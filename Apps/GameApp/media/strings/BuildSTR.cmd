@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

for %%F in (*.txt) do (
    echo Compiling: %%F
    stringTool.exe -pc "%%~nF.STRINGBIN" "%%~F"
)

echo.
echo All STRINGBIN-files compiled
pause
