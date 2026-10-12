@echo off
setlocal
cd /d "%~dp0"
if not defined JAVA_HOME (
  for /d %%J in ("%ProgramFiles%\Microsoft\jdk-21*") do set "JAVA_HOME=%%~fJ"
)
if not exist "%JAVA_HOME%\bin\java.exe" (
  echo Java 21 was not found. Set JAVA_HOME to your installed Java 21 JDK.
  pause
  exit /b 1
)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\run.ps1"
if errorlevel 1 pause
