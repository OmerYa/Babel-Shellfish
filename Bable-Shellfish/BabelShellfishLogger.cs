using System;
using System.Globalization;
using System.IO;

namespace BabelShellfish
{
    public class BabelShellfishLogger
    {

        private string LogPath = null;
        public BabelShellfishLogger(string logFolderPath)
        {
            try
            {
                // Code copied from Powershell to get the transcription file path logic:
                // https://github.com/PowerShell/PowerShell/blob/4ee1973d976d66078fcffc54208accab0ec9d8c9/src/System.Management.Automation/engine/hostifaces/MshHostUserInterface.cs
                string logPath = Path.Combine(logFolderPath, DateTime.Now.ToString("yyyyMMdd", CultureInfo.InvariantCulture));
                byte[] randomBytes = new byte[6];
                System.Security.Cryptography.RandomNumberGenerator.Create().GetBytes(randomBytes);
                string filename = String.Format(
                            CultureInfo.InvariantCulture,
                            "BabelShellfish.{0}.{1}.{2:yyyyMMddHHmmss}.txt",
                            Environment.MachineName,
                            Convert.ToBase64String(randomBytes).Replace('/', '_'),
                            DateTime.Now);

                logPath = System.IO.Path.Combine(logPath, filename);

                string baseDirectory = Path.GetDirectoryName(logPath);
                if (Directory.Exists(logPath) || (String.Equals(baseDirectory, logPath.TrimEnd(Path.DirectorySeparatorChar), StringComparison.Ordinal)))
                {
                    logPath = null;
                }
                else
                {
                    if (!Directory.Exists(baseDirectory))
                    {
                        Directory.CreateDirectory(baseDirectory);
                    }
                    if (!File.Exists(logPath))
                    {
                        File.Create(logPath).Dispose();
                    }
                }
                LogPath = logPath;
            }
            catch (Exception)
            {
            }
        }

        public void ScriptInvokeHandler(BabelShellfish sender, BabelShellfish.ScriptType type, String message)
        {
            if (!String.IsNullOrEmpty(this.LogPath))
            {
                string logString = String.Format(
                    CultureInfo.InvariantCulture,
                    "{0:yyyy-MM-dd-HH:mm:ss} - {1}: {2}",
                    DateTime.Now,
                    type.ToString(),
                    message);

                try
                {
                    StreamWriter contentWriter;
                    try
                    {
                        contentWriter = new StreamWriter(
                                new FileStream(this.LogPath, FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.Read));
                        contentWriter.BaseStream.Seek(0, SeekOrigin.End);
                    }
                    catch
                    {
                        contentWriter = new StreamWriter(
                                new FileStream(this.LogPath, FileMode.Append, FileAccess.Write, FileShare.Read));
                    }
                    contentWriter.AutoFlush = true;
                    contentWriter.WriteLine(logString);
                    contentWriter.Flush();
                    contentWriter.Close();
                }
                catch (Exception)
                {
                }
            }
        }
    }
}
