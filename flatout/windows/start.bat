@echo off
rem FlatOut Web: starts the local server for the FlatOut folder one level up and opens the game in Chrome.
cd /d "%~dp0"
where node >nul 2>nul || (
  echo Node.js neni nainstalovany. Stahni "LTS" z https://nodejs.org a spust tento soubor znovu.
  pause
  exit /b 1
)
rem a server left running from before (window closed badly, "stop" not possible): end it first
for /f "tokens=5" %%p in ('netstat -ano ^| findstr /r /c:":8080 .*LISTENING"') do (
  tasklist /fi "PID eq %%p" | findstr /i "node.exe" >nul && (
    echo Vypinam stary server ^(PID %%p^)...
    taskkill /PID %%p /F >nul 2>nul
  )
)
start "" http://localhost:8080/
node server.mjs --data "%~dp0.." --wasm "%~dp0flatout.wasm.gz" --dist "%~dp0dist" --port 8080
pause
