@echo off
rem FlatOut Web for LAN play: the server also answers other PCs on this network (they open http://<this PC's IP>:8080).
cd /d "%~dp0"
rem a server left running from before: end it first
for /f "tokens=5" %%p in ('netstat -ano ^| findstr /r /c:":8080 .*LISTENING"') do (
  tasklist /fi "PID eq %%p" | findstr /i "node.exe" >nul && (
    echo Vypinam stary server ^(PID %%p^)...
    taskkill /PID %%p /F >nul 2>nul
  )
)
start "" http://localhost:8080/
node server.mjs --data "%~dp0.." --wasm "%~dp0flatout.wasm.gz" --dist "%~dp0dist" --port 8080 --host 0.0.0.0
pause
