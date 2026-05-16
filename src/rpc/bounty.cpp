#include <rpc/server.h>
#include <wallet/wallet.h>
#include <wallet/rpcwallet.h>
#include <bounty/bounty.h>
#include <chain.h>
#include <validation.h>
#include <consensus/validation.h>
#include <net.h>
#include <core_io.h>
#include <pubkey.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <script/standard.h>
#include <streams.h>
#include <base58.h>
#include <tinyformat.h>
#include <util.h>
#include <utilstrencodings.h>

#ifdef ENABLE_WALLET
#include <wallet/coincontrol.h>
#endif

static UniValue createbounty(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 2)
        throw std::runtime_error(
            "createbounty target_hash amount ( timelock )\n"
            "Create a new HLC bounty escrow.");

#ifdef ENABLE_WALLET
    CWallet* const pwallet = GetWalletForJSONRPCRequest(request);
    if (!pwallet)
        throw JSONRPCError(RPC_WALLET_ERROR, "Wallet not found");

    EnsureWalletIsUnlocked(pwallet);

    std::string targetHash = request.params[0].get_str();
    if (AmountFromValue(UniValue(request.params[1].getValStr())) < COIN) throw JSONRPCError(RPC_INVALID_PARAMETER, "Bounty amount must be at least 1 HLC");
    if (targetHash.size() != 64 || !IsHex(targetHash))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid SHA256 target hash");

    if (!request.params[1].isNum() && !request.params[1].isStr())
        throw JSONRPCError(RPC_TYPE_ERROR, "Amount must be numeric");
    CAmount amount = AmountFromValue(UniValue(request.params[1].getValStr()));
    int timelock = 10080;
    if (request.params.size() > 2) {
        std::string tl = request.params[2].getValStr();
        if (tl.empty())
            throw JSONRPCError(RPC_TYPE_ERROR, "Invalid timelock");
        timelock = std::stoi(tl);
    }

    int deadlineHeight = chainActive.Height() + timelock;

    CPubKey creatorPubKey;
    CKeyID keyID;
    {
        LOCK(pwallet->cs_wallet);
        for (const auto& entry : pwallet->mapAddressBook) {
            CTxDestination dest = entry.first;
            if (dest.type() == typeid(CKeyID)) {
                keyID = boost::get<CKeyID>(dest);
                break;
            }
        }
        CKey key;
        if (!pwallet->GetKey(keyID, key))
            throw JSONRPCError(RPC_WALLET_ERROR, "Unable to get creator key");
        creatorPubKey = key.GetPubKey();
    }

    CScript redeemScript = CScript()
        << deadlineHeight
        << OP_CHECKLOCKTIMEVERIFY
        << OP_DROP
        << ToByteVector(creatorPubKey)
        << OP_CHECKSIG;

    CScriptID scriptID(redeemScript);
    CScript scriptPubKey = GetScriptForDestination(scriptID);

    std::string metadata = BuildBountyMetadata(targetHash, deadlineHeight);
    if (metadata.size() > 128) throw JSONRPCError(RPC_INVALID_PARAMETER, "Metadata too large");
    CScript opReturnScript = CScript()
        << OP_RETURN
        << std::vector<unsigned char>(metadata.begin(), metadata.end());

    std::vector<CRecipient> recipients;
    recipients.push_back({scriptPubKey, amount, false});
    recipients.push_back({opReturnScript, 0, false});

    CReserveKey reservekey(pwallet);
    CAmount feeRequired;
    int changePos = -1;
    std::string failReason;
    CWalletTx wtx;
    CCoinControl coinControl;

    if (!pwallet->CreateTransaction(recipients, wtx, reservekey, feeRequired, changePos, failReason, coinControl))
        throw JSONRPCError(RPC_WALLET_ERROR, failReason);

    CValidationState state;
    if (!pwallet->CommitTransaction(wtx, reservekey, g_connman.get(), state))
        throw JSONRPCError(RPC_WALLET_ERROR, "CommitTransaction failed");

    BountyEntry entry;
    entry.bountyTxid = wtx.GetHash();
    entry.targetHash = targetHash;
    entry.algorithm = "SHA256";
    entry.deadlineHeight = deadlineHeight;
    entry.amount = amount;
    entry.solved = false;
    entry.reclaimed = false;
    g_bounty_index[wtx.GetHash()] = entry;

    UniValue result(UniValue::VOBJ);
    result.pushKV("txid", wtx.GetHash().GetHex());
    result.pushKV("deadline", deadlineHeight);
    result.pushKV("amount", ValueFromAmount(amount));
    result.pushKV("metadata", metadata);
    return result;
#else
    throw JSONRPCError(RPC_WALLET_ERROR, "Wallet disabled");
#endif
}

static UniValue solvebounty(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 3)
        throw std::runtime_error(
            "solvebounty bounty_txid solution payout_address\n"
            "Solve an HLC bounty. Reward is sent to payout_address.");

#ifdef ENABLE_WALLET
    CWallet* const pwallet = GetWalletForJSONRPCRequest(request);
    if (!pwallet)
        throw JSONRPCError(RPC_WALLET_ERROR, "Wallet not found");

    EnsureWalletIsUnlocked(pwallet);

    uint256 txid = ParseHashV(request.params[0], "bounty_txid");
    std::string solution = request.params[1].get_str();
    if (solution.size() > 1024) throw JSONRPCError(RPC_INVALID_PARAMETER, "Solution too large (max 1KB)");
    std::string payoutAddress = request.params[2].get_str();

    auto it = g_bounty_index.find(txid);
    if (it == g_bounty_index.end())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Unknown bounty");

    BountyEntry& entry = it->second;
    if (entry.solved)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Bounty already solved");

    unsigned char hash[CSHA256::OUTPUT_SIZE];
    CSHA256().Write((const unsigned char*)solution.data(), solution.size()).Finalize(hash);
    std::string computedHash = HexStr(hash, hash + CSHA256::OUTPUT_SIZE);

    if (computedHash != entry.targetHash)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid solution");

    entry.solved = true;

    CTxDestination dest = DecodeDestination(payoutAddress);
    if (!IsValidDestination(dest))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid payout address");

    CAmount reward = entry.amount;
    CAmount nFeeRet = 0;
    int nChangePosInOut = -1;
    std::string error;
    CReserveKey reserveKey(pwallet);
    CWalletTx wtxPayout;
    CCoinControl coinControl;

    std::vector<CRecipient> recipients;
    recipients.push_back({GetScriptForDestination(dest), reward, false});

    if (!pwallet->CreateTransaction(recipients, wtxPayout, reserveKey, nFeeRet, nChangePosInOut, error, coinControl))
        throw JSONRPCError(RPC_WALLET_ERROR, "Payout transaction creation failed: " + error);

    CValidationState state;
    if (!pwallet->CommitTransaction(wtxPayout, reserveKey, g_connman.get(), state))
        throw JSONRPCError(RPC_WALLET_ERROR, "Payout commit failed");

    UniValue result(UniValue::VOBJ);
    result.pushKV("status", "solved");
    result.pushKV("bounty_txid", txid.GetHex());
    result.pushKV("payout_address", payoutAddress);
    result.pushKV("payout_txid", wtxPayout.GetHash().GetHex());
    result.pushKV("reward", ValueFromAmount(reward));
    return result;
#else
    throw JSONRPCError(RPC_WALLET_ERROR, "Wallet disabled");
#endif
}

static UniValue reclaimbounty(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1)
        throw std::runtime_error(
            "reclaimbounty bounty_txid\n"
            "Reclaim expired HLC bounty.");

    uint256 txid = ParseHashV(request.params[0], "bounty_txid");
    auto it = g_bounty_index.find(txid);
    if (it == g_bounty_index.end())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Unknown bounty");

    BountyEntry& entry = it->second;
    if (chainActive.Height() < entry.deadlineHeight)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Timelock not expired");
    if (entry.reclaimed)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Already reclaimed");

    entry.reclaimed = true;

    UniValue result(UniValue::VOBJ);
    result.pushKV("status", "reclaimed");
    result.pushKV("bounty_txid", txid.GetHex());
    return result;
}

static UniValue listbounties(const JSONRPCRequest& request)
{
    UniValue result(UniValue::VARR);
    for (const auto& pair : g_bounty_index) {
        const BountyEntry& entry = pair.second;
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("txid", entry.bountyTxid.GetHex());
        obj.pushKV("target_hash", entry.targetHash);
        obj.pushKV("algorithm", entry.algorithm);
        obj.pushKV("deadline", entry.deadlineHeight);
        obj.pushKV("amount", ValueFromAmount(entry.amount));
        obj.pushKV("solved", entry.solved);
        obj.pushKV("reclaimed", entry.reclaimed);
        result.push_back(obj);
    }
    return result;
}

static UniValue commitbounty(const JSONRPCRequest& request);
static UniValue revealbounty(const JSONRPCRequest& request);
static const CRPCCommand commands[] = {
    { "bounty", "createbounty", &createbounty, {} },
    { "bounty", "solvebounty", &solvebounty, {} },
    { "bounty", "reclaimbounty", &reclaimbounty, {} },
    { "bounty", "listbounties", &listbounties, {} },
    { "bounty", "commitbounty", &commitbounty, {} },
    { "bounty", "revealbounty", &revealbounty, {} },
};

void RegisterBountyRPCCommands(CRPCTable& t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}

static UniValue commitbounty(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 3)
        throw std::runtime_error(
            "commitbounty bounty_txid solution miner_address\n"
            "Commit a solution without revealing it.");

    uint256 txid = ParseHashV(request.params[0], "bounty_txid");
    std::string solution = request.params[1].get_str();
    std::string minerAddress = request.params[2].get_str();

    auto it = g_bounty_index.find(txid);
    if (it == g_bounty_index.end())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Unknown bounty");

    BountyEntry& entry = it->second;
    if (entry.solved)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Bounty already solved");

    unsigned char hash[CSHA256::OUTPUT_SIZE];
    CSHA256().Write((const unsigned char*)solution.data(), solution.size()).Finalize(hash);
    std::string computedHash = HexStr(hash, hash + CSHA256::OUTPUT_SIZE);

    if (computedHash != entry.targetHash)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid solution");

    std::string nonce = std::to_string(GetRand(1000000000));
    std::string commitData = solution + minerAddress + nonce;
    unsigned char commitHash[CSHA256::OUTPUT_SIZE];
    CSHA256().Write((const unsigned char*)commitData.data(), commitData.size()).Finalize(commitHash);
    std::string commitHex = HexStr(commitHash, commitHash + CSHA256::OUTPUT_SIZE);

    CommitEntry centry;
    centry.bountyTxid = txid;
    centry.commitHash = commitHex;
    centry.minerAddress = minerAddress;
    centry.commitHeight = chainActive.Height();
    g_commit_index[commitHex] = centry;

    UniValue result(UniValue::VOBJ);
    result.pushKV("commit_hash", commitHex);
    result.pushKV("miner_address", minerAddress);
    result.pushKV("commit_height", centry.commitHeight);
    result.pushKV("nonce", nonce);
    result.pushKV("wait_blocks", 6);
    return result;
}

static UniValue revealbounty(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 4)
        throw std::runtime_error(
            "revealbounty bounty_txid solution nonce payout_address\n"
            "Reveal previously committed solution.");

    uint256 txid = ParseHashV(request.params[0], "bounty_txid");
    std::string solution = request.params[1].get_str();
    std::string nonce = request.params[2].get_str();
    std::string payoutAddress = request.params[3].get_str();

    auto it = g_bounty_index.find(txid);
    if (it == g_bounty_index.end())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Unknown bounty");

    BountyEntry& entry = it->second;
    if (entry.solved)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Bounty already solved");

    std::string commitData = solution + payoutAddress + nonce;
    unsigned char commitHash[CSHA256::OUTPUT_SIZE];
    CSHA256().Write((const unsigned char*)commitData.data(), commitData.size()).Finalize(commitHash);
    std::string computedCommit = HexStr(commitHash, commitHash + CSHA256::OUTPUT_SIZE);

    auto cit = g_commit_index.find(computedCommit);
    if (cit == g_commit_index.end())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "No matching commit found - solution must be committed first");

    if (chainActive.Height() < cit->second.commitHeight + 6)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Reveal too early - wait 6 blocks after commit");

    entry.solved = true;

    UniValue result(UniValue::VOBJ);
    result.pushKV("status", "solved");
    result.pushKV("bounty_txid", txid.GetHex());
    result.pushKV("payout_address", payoutAddress);
    return result;
}
