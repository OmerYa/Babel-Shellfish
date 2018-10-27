# Babel-Shellfish
Deobfuscate Powershell scripts right before execution. Babel-Shellfish allows to both log and scan through AMSI deobfuscated scripts. If a script is found mallicious it will stop its execution.

# Work In Progress
This is still a preliminary version intended as a POC. The code tested against Powershell V5.1 (only on x64 processes). I cannot recommend using it on production environment, use it on your own risk.

# Usage
 - Copy the compiled Babel-Shellfish.dll.dll and BabelShellfishProfiler.dll from /x64/Release/ to a permanent folder (make sure all users have access to it).
 - Run Install-Babel-Shellfish.bat on administrator shell (see usage below).
 - Babel-Shellfish will run on every .Net process on the system. Whenever Powershell (System.Management.Automation) runs Babel-Shellfish will run with it too.
 - Note: If you ran installation batch file from command line, you will have to start a new console for environment changes to register (running powerhsell.exe from same console as the installation won't load Babel-Shellfish).
 - You can disable Babel-Shellfish by running Disable-Babel-Shellfish.bat (run batch file as administrator)

# Installation Usage
 - Install-Babel-Shellfish.bat [DebugOut] [ScanWithAMSI] [LogAMSI] [BabelShellfish Path] [LogFolderPath]
 - Example: Install-Babel-Shellfish.bat 1 0 1 "c:\Babel-Shellfish\BabelShellfishProfiler.dll"  "c:\Babel-Shellfish\Logs\"
 - [DebugOut] - Set to 1 to send deobfuscated commands to OutputDebugString.
 - [ScanWithAMSI] - Set to 1 to scan deobfuscated scripts with AMSI.
 - [LogAMSI] - Set to 1 to log the scripts sent to AMSI (curiosity feature).
 - [BabelShellfish Path] - Path to BabelShellfishProfiler.dll
 - [LogFolderPath] - (Optional) Path to save deobfuscated scripts. Logs are saved in the same folder structure as Powershell's transcription output.

# Compilation
Project was created with Visual Studio 2013. You should install Windows Platform SDK to compile it properly. Make sure NuGet Package Manager is set to download missing packages automatically.

# Detailed Description
More info can be found on the [DerbyCon presentation](http://www.irongeek.com/i.php?page=videos/derbycon8/track-3-15-goodbye-obfuscation-hello-invisi-shell-hiding-your-powershell-script-in-plain-sight-omer-yair) by Omer Yair (October, 2018).

# Credits
 - CorProfiler by .NET Foundation
 - Eyal Ne'emany
 - Guy Franco
 - Ephraim Neuberger
 - Yossi Sassi
 - Omer Yair