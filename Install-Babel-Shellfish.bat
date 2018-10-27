echo off

if '%1'=='' goto Help
if '%2'=='' goto Help
if '%3'=='' goto Help
if '%4'=='' goto Help

REG ADD "HKLM\Software\Classes\CLSID\{cf0d821e-299b-5307-a3d8-b283c03916db}\Config" /v "DebugOut" /t REG_DWORD /d %1 /f
REG ADD "HKLM\Software\Classes\CLSID\{cf0d821e-299b-5307-a3d8-b283c03916db}\Config" /v "ScanAMSI" /t REG_DWORD /d %2 /f
REG ADD "HKLM\Software\Classes\CLSID\{cf0d821e-299b-5307-a3d8-b283c03916db}\Config" /v "LogAMSI" /t REG_DWORD /d %3 /f
REG ADD "HKLM\Software\Classes\CLSID\{cf0d821e-299b-5307-a3d8-b283c03916db}\InprocServer32" /ve /t REG_SZ /d "%4" /f
REG ADD "HKLM\Software\Classes\CLSID\{cf0d821e-299b-5307-a3d8-b283c03916db}\Config" /v "LogPath"  /t REG_SZ /d "%5" /f

setx COR_PROFILER {cf0d821e-299b-5307-a3d8-b283c03916db}
setx COR_ENABLE_PROFILING 1

goto Exit

:Help

echo Usage:
echo 	Install-Babel-Shellfish.bat [DebugOut] [ScanWithAMSI] [LogAMSI] [BabelShellfish Path] [LogFolderPath] 
echo 	Example: Install-Babel-Shellfish.bat 1 0 1 "c:\Babel-Shellfish\BabelShellfishProfiler.dll" "c:\Babel-Shellfish\Logs\"

:Exit
