@echo off
cd /d "%~dp0"
py -3 command_notepad.py
if errorlevel 1 pause
