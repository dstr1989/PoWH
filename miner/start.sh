#!/bin/bash
echo "=== HashLatch GPU Miner ==="
echo "Enter your HLC address:"
read addr
echo "Starting miner for address: $addr"
./srbminer --algorithm kawpow --pool 92.5.32.114:18767 --wallet $addr --password x
