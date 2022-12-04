#pragma once
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <map>
#include <vector>

#include <concurrent_vector.h>
#include <concurrent_unordered_map.h>

#include "f4se/GameReferences.h"

class Configuration
{
};

struct whitelistSex
{
    bool male;
    bool female;
};

typedef concurrency::concurrent_unordered_map<std::string, float> configEntry_t; // Map settings for a particular bone
typedef concurrency::concurrent_unordered_map<std::string, configEntry_t> config_t; // Settings for a set of bones
typedef concurrency::concurrent_unordered_map<std::string, std::unordered_map<std::string, whitelistSex>> whitelist_t;

struct armorOverrideData
{
    bool isFilterInverted;
    std::unordered_set<UInt32> slots;
    std::unordered_set<UInt32> armors;
    config_t config;
};

struct actorOverrideData
{
    bool isFilterInverted;
    std::unordered_set<UInt32> actors;
    config_t config;
};

extern bool playerOnly;
extern bool femaleOnly;
extern bool maleOnly;
extern bool npcOnly;
extern bool useWhitelist;

extern int configReloadCount;
extern config_t config;
extern concurrency::concurrent_unordered_map<UInt32, armorOverrideData> configArmorOverrideMap;
extern concurrency::concurrent_unordered_map<UInt32, actorOverrideData> configActorOverrideMap;
extern whitelist_t whitelist;
extern std::vector<std::string> raceWhitelist;
extern std::unordered_set<UInt32> usedSlots;
extern std::map<std::multiset<UInt64>, config_t> cachedConfigs;
extern std::set<UInt32> priorities;

bool LoadConfig();
void DumpWhitelistToLog();