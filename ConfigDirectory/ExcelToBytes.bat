@echo off
setlocal
chcp 65001 >nul

if "%~1"=="" (
    echo 用法: %~nx0 ^<Excel文件^> [输出目录] [模式]
    exit /b 1
)

set "EXCEL=%~1"
if "%~2"=="" (set "OUT=./config/") else (set "OUT=%~2")
if "%~3"=="" (set "MODE=1") else (set "MODE=%~3")

.\Tools.exe "%EXCEL%" "%OUT%" %MODE%
pause