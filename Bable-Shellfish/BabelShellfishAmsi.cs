using System;
using System.Management.Automation;


namespace BabelShellfish
{
    class BabelShellfishAmsi
    {
        public void ScriptScanHandler(BabelShellfish sender, BabelShellfish.ScriptType type, String message)
        {
            // Scan only if the source is not AMSI (to avoid endless loops)
            if (BabelShellfish.ScriptType.Amsi != type)
            {
                uint amsiResult = sender.ScanWithAmsi(message, null);

                if (1 < amsiResult)
                {
                    throw new ParseException("This script contains malicious content and has been blocked by your antivirus software.");
                }
            }

        }

    }
}
