# HashLatch (HLC)

**Decentralized L1 blockchain for useful GPU mining and cryptographic bounties.**

HashLatch combines KawPow GPU mining with a free-market bounty system for solving real-world cryptographic tasks: password recovery, wallet restoration, and penetration testing.

## Key Features

- ⛏ **KawPow Mining** — ASIC-resistant GPU mining (Ravencoin fork)
- 💰 **Cryptographic Bounties** — Create tasks, lock rewards in P2SH escrow
- 🔒 **Commit-Reveal** — Anti-frontrunning protection for miners
- ⏱ **Timelock Recovery** — Unclaimed bounties return to creators after deadline
- 🛡 **No Premine, No ICO** — Fair launch, pure community-driven

## Quick Start

```bash
git clone https://github.com/dstr1989/PoWH.git
cd PoWH
./autogen.sh
./configure --disable-tests --without-gui --with-incompatible-bdb
make -j$(nproc)
./src/hashlatchd -regtest -daemon
./src/hashlatch-cli -regtest generate 101
## Quick Start

git clone https://github.com/dstr1989/PoWH.git
cd PoWH
./autogen.sh
./configure --disable-tests --without-gui --with-incompatible-bdb
make -j$(nproc)
./src/hashlatchd -regtest -daemon
./src/hashlatch-cli -regtest generate 101

## Bounty RPC Commands

| Command | Description |
|---------|-------------|
| createbounty | Create a new bounty |
| solvebounty | Solve a bounty |
| commitbounty | Commit solution (hidden) |
| revealbounty | Reveal after commit |
| reclaimbounty | Reclaim after deadline |
| listbounties | List all bounties |

## Tokenomics

- Max Supply: 21,000,000 HLC
- Block Time: ~2 minutes
- Dev Fee: 2%
- Miner Reward: 98%

## Testnet

Live testnet: http://92.5.32.114:5000/api

## Links

- Website: hashlatch.network
- Discord: discord.gg/hashlatch
- Twitter: @HashLatchCoin

## License

MIT License (c) 2026 HashLatch
