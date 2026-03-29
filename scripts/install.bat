@echo off
setlocal EnableDelayedExpansion

set REPO=chrisomatic/censorman
set ASSET=censorman-v2-win-x86_64.zip
set BINARY=censorman.exe
set INSTALL_DIR=%USERPROFILE%\bin

:: ─────────────────────────────────────────────
:: Fetch latest release tag from GitHub API
:: ─────────────────────────────────────────────
echo Fetching latest release info...
for /f "tokens=* usebackq" %%F in (
    `curl -s "https://api.github.com/repos/%REPO%/releases/latest" ^| findstr "tag_name"`
) do set TAG_LINE=%%F

:: Extract the tag value from: "tag_name": "v2.0.0",
set TAG_LINE=%TAG_LINE:"=%
set TAG_LINE=%TAG_LINE:,=%
for /f "tokens=2 delims=: " %%A in ("%TAG_LINE%") do set TAG=%%A

if "%TAG%"=="" (
    echo ERROR: Could not determine the latest release tag.
    echo Make sure curl is available and you have internet access.
    goto :fail
)

echo Latest release: %TAG%

:: ─────────────────────────────────────────────
:: Download the zip asset
:: ─────────────────────────────────────────────
set DOWNLOAD_URL=https://github.com/%REPO%/releases/download/%TAG%/%ASSET%
set TMP_ZIP=%TEMP%\%ASSET%

echo Downloading %ASSET%...
curl -L -o "%TMP_ZIP%" "%DOWNLOAD_URL%"
if errorlevel 1 (
    echo ERROR: Download failed.
    goto :fail
)

:: ─────────────────────────────────────────────
:: Extract the binary (tar is built into Win10+)
:: ─────────────────────────────────────────────
set TMP_DIR=%TEMP%\censorman_install
if exist "%TMP_DIR%" rmdir /s /q "%TMP_DIR%"
mkdir "%TMP_DIR%"

echo Extracting...
tar -xf "%TMP_ZIP%" -C "%TMP_DIR%"
if errorlevel 1 (
    echo ERROR: Extraction failed.
    goto :fail
)

:: ─────────────────────────────────────────────
:: Install to %USERPROFILE%\bin
:: ─────────────────────────────────────────────
if not exist "%INSTALL_DIR%" mkdir "%INSTALL_DIR%"

copy /y "%TMP_DIR%\%BINARY%" "%INSTALL_DIR%\%BINARY%" >nul
if errorlevel 1 (
    echo ERROR: Could not copy binary to %INSTALL_DIR%.
    goto :fail
)

:: ─────────────────────────────────────────────
:: Cleanup
:: ─────────────────────────────────────────────
del /q "%TMP_ZIP%"
rmdir /s /q "%TMP_DIR%"

echo.
echo censorman installed to %INSTALL_DIR%\%BINARY%
echo.

:: ─────────────────────────────────────────────
:: Optionally add install dir to user PATH
:: ─────────────────────────────────────────────
echo %PATH% | findstr /i /c:"%INSTALL_DIR%" >nul
if errorlevel 1 (
    echo NOTE: %INSTALL_DIR% is not in your PATH.
    echo To add it permanently, run the following command:
    echo.
    echo   setx PATH "%%PATH%%;%INSTALL_DIR%"
    echo.
    echo Then restart your Command Prompt.
)

echo Done! Run: censorman --help
goto :eof

:fail
echo Installation failed.
exit /b 1
