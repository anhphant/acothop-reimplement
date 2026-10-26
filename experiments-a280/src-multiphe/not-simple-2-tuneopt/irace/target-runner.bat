@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0target-runner.ps1" %*
