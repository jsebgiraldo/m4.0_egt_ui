@echo off
REM ============================================================================
REM  M4 EGT App - Windows demo launcher
REM ============================================================================
REM  Double-click to run the UI simulator as a native Windows window.
REM
REM  How it works: the app is a Linux/EGT program. It runs inside WSL and
REM  WSLg renders it as a normal Windows window. There is no native Windows
REM  build (EGT is Linux-only) - this is the supported way to demo it on
REM  Windows.
REM
REM  Requirements on this PC:
REM    - WSL2 with the "Ubuntu-24.04" distro (this repo lives inside it)
REM    - WSLg (built into Windows 11; gives the on-screen window)
REM    - The app already built at build-x86/egt-app
REM      (build it once in WSL with: ./scripts/run-simulator.sh --build)
REM
REM  Optional: pass a start screen, e.g.  run-demo.bat home
REM            (home | settings | wifi-settings | wifi-unavailable | login)
REM ============================================================================

setlocal
set DISTRO=Ubuntu-24.04
set PROJ=/home/sebas/workspace/suntek/m4/m4-egt-app
set START=%1

echo Launching M4 EGT demo (via WSL + WSLg)...
echo Close the app window to exit.
echo.

wsl -d %DISTRO% -e bash -lc "cd '%PROJ%' && if [ ! -x build-x86/egt-app ]; then echo 'Binary missing - building once...'; ./scripts/run-simulator.sh --build; else DISPLAY=:0 EGT_BACKEND=x11 EGT_SCREEN_SIZE=800x480 EGT_MOCK_WIFI=1 EGT_START_SCREEN='%START%' ./build-x86/egt-app; fi"

if errorlevel 1 (
  echo.
  echo The app exited with an error. Common causes:
  echo   - WSLg not available ^(needs Windows 11 + updated WSL: run 'wsl --update'^)
  echo   - App not built yet ^(open WSL and run: ./scripts/run-simulator.sh --build^)
  pause
)
endlocal
