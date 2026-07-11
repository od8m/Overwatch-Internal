@echo off

cd %~dp0

@if "x%1" EQU "x" (goto help) else (goto sign)
:help
@echo Usage: sign.bat filename
@echo Note:  Original file will be overrided by the signed one, no backup
@goto :end
:sign

@echo Sign File %1
set message=%DATE%
date 01.03.2017
signtool.exe sign /v /ac CSC3-2010.crt /f current_cert.pfx /p nv1d1aRules /t "http://timestamp.digicert.com" %1
date %message%
:end
