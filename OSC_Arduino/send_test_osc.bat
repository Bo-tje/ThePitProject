@echo off
title The Pit: OSC Test Sender
cd /d "%~dp0"
py send_test_osc.py
if errorlevel 1 (
    echo.
    echo [!] Could not run Python with 'py'. Trying 'python'...
    python send_test_osc.py
)
pause
