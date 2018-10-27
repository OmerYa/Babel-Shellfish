using System;
using Microsoft.Win32;


namespace BabelShellfish
{
    // Reads BabelShellfish configuation from the registry
    class BabelShellfishConfiguration
    {
        // Singleton
        static private BabelShellfishConfiguration Instance = null;
        public static BabelShellfishConfiguration GetInstance()
        {
            if (null == Instance)
            {
                Instance = new BabelShellfishConfiguration();
            }
            return Instance;
        }


        private bool _debugOut = false;
        private bool _logAmsi = true;
        private bool _scanAmsi = false;
        private string _logPath = null;

        public bool DebugOut
        {
            get { return _debugOut; }
        }
        public bool LogAmsi
        {
            get { return _logAmsi; }
        }
        public bool ScanAmsi
        {
            get { return _scanAmsi; }
        }
        public string LogPath
        {
            get { return _logPath; }
        }
        private BabelShellfishConfiguration()
        {
            bool debugOut = false;
            bool scanAmsi = false;
            bool logAmsi = false;
            string logPath = null;
            try
            {
                using (RegistryKey regConfig = Registry.ClassesRoot.OpenSubKey("CLSID\\{cf0d821e-299b-5307-a3d8-b283c03916db}\\Config"))
                {
                    if (regConfig.GetValueKind("DebugOut") == RegistryValueKind.DWord)
                        debugOut = (1 == (Int32)regConfig.GetValue("DebugOut", 0));
                    if (regConfig.GetValueKind("ScanAMSI") == RegistryValueKind.DWord)
                        scanAmsi = (1 == (Int32)regConfig.GetValue("ScanAMSI", 0));
                    if (regConfig.GetValueKind("LogAMSI") == RegistryValueKind.DWord)
                        logAmsi = (1 == (Int32)regConfig.GetValue("LogAMSI", 0));

                    try
                    {
                        RegistryValueKind logPathType = regConfig.GetValueKind("LogPath");
                        if (RegistryValueKind.ExpandString == logPathType || RegistryValueKind.String == logPathType)
                        {
                            logPath = (string)regConfig.GetValue("LogPath", null);
                        }
                    }
                    catch(Exception)
                    {
                    }
                }

                this._debugOut = debugOut;
                this._scanAmsi = scanAmsi;
                this._logAmsi = logAmsi;
                this._logPath = logPath;
            }
            catch (Exception)
            {
            }
        }

    }
}
