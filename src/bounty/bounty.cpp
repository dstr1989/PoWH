#include <sstream>
#include <bounty/bounty.h>
#include <script/script.h>
#include <utilstrencodings.h>

std::map<uint256, BountyEntry> g_bounty_index;

bool IsBountyOpReturn(const CScript& script)
{
    if (script.empty()) return false;
    return script[0] == OP_RETURN;
}

std::string BuildBountyMetadata(const std::string& targetHash, int deadlineHeight)
{
    return std::string("HLB1|SHA256|") + targetHash + "|" + std::to_string(deadlineHeight);
}

bool ParseBountyMetadata(const CScript& script, BountyEntry& entry)
{
    if (!IsBountyOpReturn(script)) return false;

    opcodetype opcode;
    std::vector<unsigned char> data;
    CScript::const_iterator pc = script.begin();

    if (!script.GetOp(pc, opcode)) return false;
    if (opcode != OP_RETURN) return false;
    if (!script.GetOp(pc, opcode, data)) return false;

    std::string metadata(data.begin(), data.end());
    if (metadata.find("HLB1|") != 0) return false;

    std::vector<std::string> parts;
    std::stringstream ss(metadata);
    std::string item;
    while (std::getline(ss, item, '|')) {
        parts.push_back(item);
    }

    if (parts.size() != 4) return false;
    entry.algorithm = parts[1];
    entry.targetHash = parts[2];
    entry.deadlineHeight = atoi(parts[3].c_str());
    return true;
}
