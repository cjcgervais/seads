@echo off
rem SEADS fly launcher — double-click this. Runs the mouse-aim viewer in --fly mode from the repo
rem root (so it finds a bundled recording for bandits), and keeps the window open on exit/error.
cd /d "%~dp0"
if not exist "build-client\seads_viewer.exe" (
  echo.
  echo   seads_viewer.exe not found. Build it first:
  echo     cmake --build build-client --target seads_viewer
  echo.
  pause
  exit /b 1
)
echo Launching SEADS fly ...  (mouse aims, Shift/Ctrl throttle, Space free-look, R reset, Esc quit)
build-client\seads_viewer.exe --fly %*
echo.
echo (viewer exited with code %errorlevel%)
pause
