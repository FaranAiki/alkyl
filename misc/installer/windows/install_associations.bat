@echo off
setlocal

:: Get the directory where this script is located
set "INSTALL_DIR=%~dp0..\.."
:: Resolve absolute path
for %%I in ("%INSTALL_DIR%") do set "INSTALL_DIR=%%~fI"

set "ICON_PATH=%INSTALL_DIR%\asset\script_logo.ico"

echo Registering AlkylScript file type...
reg add "HKEY_CLASSES_ROOT\AlkylScript" /ve /d "Alkyl Source File" /f
reg add "HKEY_CLASSES_ROOT\AlkylScript\DefaultIcon" /ve /d "\"%ICON_PATH%\",0" /f

echo Associating .aky extension...
reg add "HKEY_CLASSES_ROOT\.aky" /ve /d "AlkylScript" /f

echo Associating .hky extension...
reg add "HKEY_CLASSES_ROOT\.hky" /ve /d "AlkylScript" /f

echo Done! You may need to restart Windows Explorer for icons to update.
pause
