#!/bin/bash
echo "============================================"
echo "  HashLatch GPU Miner - Linux Quick Start"
echo "============================================"
echo ""
echo "This script will start mining HLC using your GPU."
echo "Make sure SRBMiner is in the same folder."
echo ""
read -p "Enter your HLC wallet address: " ADDR
echo ""
echo "Starting miner..."
echo "Pool: 92.5.32.114:18767"
echo "Wallet: $ADDR"
echo ""
./srbminer --algorithm kawpow --pool 92.5.32.114:18767 --wallet "$ADDR" --password x
