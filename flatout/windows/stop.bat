@echo off
rem FlatOut Web: vypne server (node.exe na portu 8080).
set FOUND=
for /f "tokens=5" %%p in ('netstat -ano ^| findstr /r /c:":8080 .*LISTENING"') do (
  tasklist /fi "PID eq %%p" | findstr /i "node.exe" >nul && (
    set FOUND=1
    taskkill /PID %%p /F >nul 2>nul
    echo Server ^(PID %%p^) vypnut.
  )
)
if not defined FOUND echo Zadny server FlatOut Web nebezi.
timeout /t 3 >nul
