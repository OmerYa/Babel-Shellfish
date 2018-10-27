using System;
using System.Globalization;
using System.Runtime.InteropServices;

namespace BabelShellfish
{
    class BabelShellfishDebugString
    {
        [DllImport("kernel32.dll", CharSet = CharSet.Auto)]
        private static extern void OutputDebugString(string message);
        public void ScriptInvokeHandler(BabelShellfish sender, BabelShellfish.ScriptType type, String message)
        {
            string logString = String.Format(
                            CultureInfo.InvariantCulture,
                            "{0:yyyy-MM-dd-HH:mm:ss} - {1}: {2}\n",
                            DateTime.Now,
                            type.ToString(),
                            message);

            OutputDebugString(logString);
        }
    }
}
