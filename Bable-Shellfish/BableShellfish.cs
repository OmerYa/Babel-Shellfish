using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Dynamic;
using System.Linq.Expressions;
using System.Management.Automation;
using System.Reflection;
using System.Text;


// the following is needed to avoid compiler optimizations in the hook code
[
    module: System.Diagnostics.Debuggable(System.Diagnostics.DebuggableAttribute.DebuggingModes.None
    | System.Diagnostics.DebuggableAttribute.DebuggingModes.Default
    | System.Diagnostics.DebuggableAttribute.DebuggingModes.DisableOptimizations
    )
]


namespace BabelShellfish
{
    public class BabelShellfish
    {
        public enum ScriptType
        {
            Process,
            Command,
            Script,
            Invoke,
            Amsi
        }

        #region ScriptInvokeHandler
        public delegate void ScriptInvokeHandler(BabelShellfish sender, ScriptType type, string message);

        protected virtual void OnScriptInvoke(ScriptType type, string message)
        {
            ScriptInvokeHandler handler = ScriptScan;
            try
            {
                if (null != handler)
                {
                    handler(this, type, message);
                }
            }
            // Always log script, regardless of scanning result (scanning raises an exception if it finds mallicious content)
            finally
            {
                handler = ScriptInvoke;
                if (null != handler)
                {
                    handler(this, type, message);
                }
            }
        }

        // Split into two events so logging will always occur regardless of scanning result
        public event ScriptInvokeHandler ScriptInvoke;
        public event ScriptInvokeHandler ScriptScan;
        #endregion

        // Allows running Babel-Shellfish from unmanaged code. Installs default logger for Babel-Shellfish
        public static int Run(string input)
        {
            int success = 0;
            try
            {
                BabelShellfish instance = BabelShellfish.GetInstance();
                BabelShellfishConfiguration config = BabelShellfishConfiguration.GetInstance();

                if (config.DebugOut)
                {
                    BabelShellfishDebugString debugStringLogger = new BabelShellfishDebugString();
                    instance.ScriptInvoke += debugStringLogger.ScriptInvokeHandler;
                }
                if (!String.IsNullOrEmpty(config.LogPath))
                {
                    BabelShellfishLogger logger = new BabelShellfishLogger(config.LogPath);
                    instance.ScriptInvoke += logger.ScriptInvokeHandler;
                }
                if (config.ScanAmsi)
                {
                    BabelShellfishAmsi amsiScanner = new BabelShellfishAmsi();
                    instance.ScriptScan += amsiScanner.ScriptScanHandler;
                }
                instance.Init();

                success = 1;
            }
            catch (Exception)
            {
            }
            return success;
        }

        // Singleton
        static private BabelShellfish Instance = null;
        public static BabelShellfish GetInstance()
        {
            if (null == Instance)
            {
                Instance = new BabelShellfish();
            }
            return Instance;
        }

        #region Initialization

        public void Init()
        {
            // Instanciate dummy type from System.Management.Automation so we won't need to enumerate
            // all loaded assemblies to find it.
            System.Management.Automation.PSTokenType TokenType = PSTokenType.Attribute;
            Assembly SmaAssembly = TokenType.GetType().Assembly;

            // Force JIT SMA method we hook (in case it wasn't pre-JIT-ed)
            ForceJitMethod(SmaAssembly, "System.Management.Automation.AmsiUtils", "ScanContent");
            ForceJitMethod(SmaAssembly, "System.Management.Automation.NativeCommandProcessor", "GetProcessStartInfo");
            ForceJitMethod(SmaAssembly, "System.Management.Automation.DlrScriptCommandProcessor", "RunClause");
            ForceJitMethod(SmaAssembly, "System.Management.Automation.Cmdlet", "DoProcessRecord");
            ForceJitMethod(SmaAssembly, "System.Management.Automation.Language.PSInvokeMemberBinder", "InvokeMethod");
            
            // Find all hidden helper functions BabelShellfish needs 
            BindingFlags searchAttributes =
                BindingFlags.DeclaredOnly |
                BindingFlags.NonPublic |
                BindingFlags.Public |
                BindingFlags.Instance |
                BindingFlags.Static;

            FindExecutableMethod = SmaAssembly.GetType("System.Management.Automation.NativeCommandProcessor").GetMethod("FindExecutable", searchAttributes);
            CmdletGetCommandInfo = SmaAssembly.GetType("System.Management.Automation.Cmdlet").GetMethod("get_CommandInfo", BindingFlags.Instance | BindingFlags.NonPublic);
            CmdletMyInvocation = SmaAssembly.GetType("System.Management.Automation.Cmdlet").GetMethod("get_MyInvocation", BindingFlags.Instance | BindingFlags.NonPublic);
            BabelShellfishInspectInvoke = new List<MethodInfo>();
            for (uint i = 1; i <= 8; ++i)
            {
                BabelShellfishInspectInvoke.Add(this.GetType().GetMethod("InspectInvokeMethod" + i.ToString(), searchAttributes));
            }
            // Force JIT all methods to allow hooking engine to hook the JIT-ed code
            // This will cause the CLR Profiler Hook engine to place the hooks
            foreach (var method in this.GetType().GetMethods(searchAttributes))
            {
                System.Runtime.CompilerServices.RuntimeHelpers.PrepareMethod(method.MethodHandle);
            }
        }

        private void ForceJitMethod(Assembly assembly, string type, string method)
        {
            BindingFlags searchAttributes =
                BindingFlags.DeclaredOnly |
                BindingFlags.NonPublic |
                BindingFlags.Public |
                BindingFlags.Instance |
                BindingFlags.Static;
            try
            {
                System.Runtime.CompilerServices.RuntimeHelpers.PrepareMethod(
                    assembly.GetType(type).GetMethod(method, searchAttributes).MethodHandle);
            }
            catch (Exception)
            {
            }

        }

        #endregion

        #region Helpers

        MethodInfo FindExecutableMethod = null;
        string FindExecutable(string filename)
        {
            if (null != FindExecutableMethod)
            {
                return FindExecutableMethod.Invoke(null, new object[] { filename }) as string;
            }

            return null;
        }


        // The compiler might remove empty functions so all our dummy functions will
        // call DummyFunc(). Please note - if DummyFunc is actually called during
        // runtime then something wrong happend with the hooking.
        void DummyFunc(string str)
        {
            Console.WriteLine(str);
        }

        void GetObjectString(StringBuilder sb, object objInput)
        {
            object o = objInput;

            PSObject pso = o as PSObject;
            if (null != pso)
            {
                o = pso.BaseObject;
            }

            if (null == o)
                sb.Append("$null");
            else
            {
                if (o.GetType().IsValueType)
                {
                    sb.Append(o.ToString());
                }
                else if (null != o as string)
                {
                    sb.Append("\"");
                    sb.Append(o.ToString());
                    sb.Append("\"");
                }
                else
                {
                    sb.Append("[");
                    sb.Append(o.GetType().FullName);
                    sb.Append("]");
                }
            }
        }
        string GetArgs(StringBuilder sb, object[] args, bool bSkipFirst)
        {
            uint skip = 0;
            if (bSkipFirst)
                ++skip;
            sb.Append("(");
            for (uint i = 0; (i + skip) < args.Length; i++)
            {
                if (i > 0)
                    sb.Append(", ");

                if (null == args[i + skip])
                    sb.Append("$null");
                else
                    GetObjectString(sb, args[i + skip]);
            }
            sb.Append(")");
            return sb.ToString();
        }


        #endregion

        #region Hooks

        ////////////////////////////////////////////////////////////////
        // System.Management.Automation.AmsiUtils
        // Monitor strings Amsi receives
        private static UInt32 ScanContent(string content, string sourceMetadata)
        {
            if (BabelShellfishConfiguration.GetInstance().LogAmsi)
                GetInstance().OnScriptInvoke(ScriptType.Amsi, content);

            return ScanContentDummy(content, sourceMetadata);

        }
        private static UInt32 ScanContentDummy(string content, string sourceMetadata)
        {
            GetInstance().DummyFunc("ScanContentDummy");

            return 1;
        }

        public UInt32 ScanWithAmsi(string content, string sourceMetadata)
        {
            return ScanContentDummy(content, sourceMetadata);
        }
        // System.Management.Automation.AmsiUtils
        ////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////
        // class System.Management.Automation.NativeCommandProcessor
        // Handling of external executables

        // GetProcessStartInfo is called right before executing an external process
        private static ProcessStartInfo GetProcessStartInfo(object ths, bool redirectOutput, bool redirectError, bool redirectInput, bool soloCommand)
        {
            ProcessStartInfo info = GetProcessStartInfoDummy(ths, redirectOutput, redirectError, redirectInput, soloCommand);
            String exe = GetInstance().FindExecutable(info.FileName);

            StringBuilder sb = new StringBuilder();
            if (String.IsNullOrEmpty(exe))
            {
                sb.Append(info.FileName);
            }
            else
            {
                sb.Append(exe);
                sb.Append(" \"");
                sb.Append(info.FileName);
                sb.Append("\"");
            }
            if (!String.IsNullOrEmpty(info.Arguments))
            {
                sb.Append(" ");
                sb.Append(info.Arguments);
            }
            GetInstance().OnScriptInvoke(ScriptType.Process, sb.ToString());

            return info;
        }
        private static ProcessStartInfo GetProcessStartInfoDummy(object ths, bool redirectOutput, bool redirectError, bool redirectInput, bool soloCommand)
        {
            GetInstance().DummyFunc("GetProcessStartInfoDummy");
            return null;
        }
        //// class System.Management.Automation.NativeCommandProcessor
        ////////////////////////////////////////////////////////////////


        ////////////////////////////////////////////////////////////////
        // class System.Management.Automation.DlrScriptCommandProcessor
        // Handling Textual Scripts
        static void RunClause(object ths, Delegate clause, object dollarUnderbar, object inputToProcess)
        {
            Action<object> wrapped = delegate(Object obj)
            {
                GetInstance().OnScriptInvoke(ScriptType.Script, ths.ToString());
                clause.DynamicInvoke(new object[] { obj });
            };
            RunClauseDummy(ths, wrapped, dollarUnderbar, inputToProcess);

        }
        static void RunClauseDummy(object ths, Delegate clause, object dollarUnderbar, object inputToProcess)
        {
            GetInstance().DummyFunc("RunClauseDummy");
        }
        // class System.Management.Automation.DlrScriptCommandProcessor
        ////////////////////////////////////////////////////////////////


        ////////////////////////////////////////////////////////////////
        /// class System.Management.Automation.Cmdlet

        // Instead of hooking class System.Management.Automation.CommandProcessor, hooking Cmdlet
        // instead since it will eventually be called by CommandProcessor
        static void DoProcessRecord(object ths)
        {
            GetInstance().DoAction(ths as Cmdlet);

            DoProcessRecordDummy(ths);
        }

        static void DoProcessRecordDummy(object ths)
        {
            GetInstance().DummyFunc("DoProcessRecordDummy");
        }

        MethodInfo CmdletGetCommandInfo = null;
        MethodInfo CmdletMyInvocation = null;
        void DoAction(Cmdlet ths)
        {
            if (null == ths)
                return;

            try
            {
                CommandInfo cmdInfo = CmdletGetCommandInfo.Invoke(ths, null) as CommandInfo;

                if (null == cmdInfo)
                    return;

                String cmdName = cmdInfo.Name;

                StringBuilder sb = new StringBuilder();
                sb.Append(cmdName);

                InvocationInfo invocationInfo = CmdletMyInvocation.Invoke(ths, null) as InvocationInfo;

                if (null == invocationInfo)
                    return;
                
                foreach(string paramName in invocationInfo.BoundParameters.Keys)
                {
                    if (null != invocationInfo.BoundParameters[paramName])
                    {
                        sb.Append(" -");
                        sb.Append(paramName);
                        sb.Append(" ");
                        sb.Append(invocationInfo.BoundParameters[paramName].ToString());
                    }
                }

                foreach (object arg in invocationInfo.UnboundArguments)
                {
                    if (null != arg)
                    {
                        sb.Append(" ");
                        sb.Append(arg.ToString());
                    }
                }

                OnScriptInvoke(ScriptType.Command, sb.ToString());
            }
            catch(Exception)
            {

            }
        }
        /// class System.Management.Automation.Cmdlet
        ////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////
        /// class System.Management.Automation.Language.PSInvokeMemberBinder

        // Handles direct call to .Net methods. The hook replaces the compilre's
        // output. It adds a call to BabelShellfish inspection method before
        // calling the .Net method.
        internal enum MethodInvocationType
        {
            Ordinary,
            Setter,
            Getter,
            BaseCtor,
            NonVirtual,
        }
        
        static void InspectInvokeMethod(object m, params object[] args)
        {
           
            if (null == m)
                return;

            MethodInfo mi = m as MethodInfo;
            if (null == mi)
                return;

            StringBuilder sb = new StringBuilder();
            bool bSkipFirst = false;
            if (mi.IsStatic)
            {
                sb.Append("[");
                sb.Append(mi.DeclaringType.FullName);
                sb.Append("]::");
                sb.Append(mi.Name);
            }
            else
            {
                if (1 <= args.Length && null != args[0])
                {
                    GetInstance().GetObjectString(sb, args[0]);
                    sb.Append(".");
                    sb.Append(mi.Name);
                    bSkipFirst = true;
                }
            }
            if (null != args && 0 < args.Length)
            {
                GetInstance().GetArgs(sb, args, bSkipFirst);
            }

            GetInstance().OnScriptInvoke(ScriptType.Invoke, sb.ToString());
        }
        #region PowershellCompilerHookUglyMagic
        // Powershell assigns parameters based on the method's parameters list. Had
        // we passed InspectInvokeMethod directly to Powershell it will always call
        // it with two parameters regardless of the actual parameters passed.
        static void InspectInvokeMethod1(object arg1)
        {
            InspectInvokeMethod(arg1);
        }
        static void InspectInvokeMethod2(object arg1, object arg2)
        {
            InspectInvokeMethod(arg1, arg2);
        }
        static void InspectInvokeMethod3(object arg1, object arg2, object arg3)
        {
            InspectInvokeMethod(arg1, arg2, arg3);
        }
        static void InspectInvokeMethod4(object arg1, object arg2, object arg3, object arg4)
        {
            InspectInvokeMethod(arg1, arg2, arg3, arg4);
        }
        static void InspectInvokeMethod5(object arg1, object arg2, object arg3, object arg4, object arg5)
        {
            InspectInvokeMethod(arg1, arg2, arg3, arg4, arg5);
        }
        static void InspectInvokeMethod6(object arg1, object arg2, object arg3, object arg4, object arg5, object arg6)
        {
            InspectInvokeMethod(arg1, arg2, arg3, arg4, arg5, arg6);
        }
        static void InspectInvokeMethod7(object arg1, object arg2, object arg3, object arg4, object arg5, object arg6, object arg7)
        {
            InspectInvokeMethod(arg1, arg2, arg3, arg4, arg5, arg6, arg7);
        }
        static void InspectInvokeMethod8(object arg1, object arg2, object arg3, object arg4, object arg5, object arg6, object arg7, object arg8)
        {
            InspectInvokeMethod(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
        }

        List<MethodInfo> BabelShellfishInspectInvoke = null;

        #endregion

        // Original function receives a MethodBase (either a method or a constructor) and
        // builds an Expression object that calls it with supplied parameters. BabelShellfish
        // uses this behavior to add another call to its own inspection method before the
        // requested method is called.
        static Expression InvokeMethod(
            MethodBase mi,
            DynamicMetaObject target,
            DynamicMetaObject[] originalArgs,
            bool expandParameters,
            MethodInvocationType invocationType)
        {
            Expression exprOriginal = InvokeMethodDummy(mi, target, originalArgs, expandParameters, invocationType);

            if (GetInstance().BabelShellfishInspectInvoke.Count <= originalArgs.Length)
                return exprOriginal;

            List<DynamicMetaObject> inspectArgs = new List<DynamicMetaObject>();

            inspectArgs.Add(DynamicMetaObject.Create(mi, Expression.Constant(mi)));

            if (false == mi.IsStatic)
            {
                inspectArgs.Add(target);
            }

            for (int i = 0; i < originalArgs.Length; i++)
            {
                inspectArgs.Add(originalArgs[i]);
            }

            Expression exprInpect = InvokeMethodDummy(
                GetInstance().BabelShellfishInspectInvoke[inspectArgs.Count - 1],
                target,
                inspectArgs.ToArray(),
                expandParameters,
                MethodInvocationType.Ordinary);

            return Expression.Block(exprInpect, exprOriginal);
        }

        static Expression InvokeMethodDummy(
            MethodBase mi,
            DynamicMetaObject target,
            DynamicMetaObject[] args,
            bool expandParameters,
            MethodInvocationType invocationType)
        {
            GetInstance().DummyFunc("InvokeMethodDummy");

            return null;
        }
        /// class System.Management.Automation.Language.PSInvokeMemberBinder
        ////////////////////////////////////////////////////////////////

        #endregion
    }
}
