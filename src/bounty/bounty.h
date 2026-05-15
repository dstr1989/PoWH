#ifndef HASHLOCK_BOUNTY_H
#define HASHLOCK_BOUNTY_H

#include <amount.h>
#include <uint256.h>
#include <script/script.h>
#include <primitives/transaction.h>

struct BountyEntry {
    uint256 bountyTxid;
    std::string targetHash;
    std::string algorithm;
    int deadlineHeight;
    CAmount amount;
    bool solved;
    bool reclaimed;
};

bool ParseBountyMetadata(const CScript& script, BountyEntry& entry);
std::string BuildBountyMetadata(const std::string& targetHash, int deadlineHeight);
bool IsBountyOpReturn(const CScript& script);
extern std::map<uint256, BountyEntry> g_bounty_index;

#endif

struct CommitEntry {
    uint256 bountyTxid;
    std::string commitHash;
    std::string minerAddress;
    int commitHeight;
};
extern std::map<std::string, CommitEntry> g_commit_index;
