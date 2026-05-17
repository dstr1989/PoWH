@echo off
echo ============================================
echo   HashLatch GPU Miner - Windows Quick Start
echo ============================================
echo.
echo This script will start mining HLC using your GPU.
echo Make sure SRBMiner is in the same folder.
echo.
set /p ADDR="Enter your HLC wallet address: "
echo.
echo Starting miner...
echo Pool: 92.5.32.114:18767
echo Wallet: %ADDR%
echo.
srbminer.exe --algorithm kawpow --pool 92.5.32.114:18767 --wallet %ADDR% --password x
pause
