#pragma once
#include <atomic>
#include <concurrent_unordered_map.h>
#include <concurrent_unordered_set.h>
#include <concurrent_vector.h>
#include <map>
#include <set>
#include <unordered_set>
#include <vector>

#include "f4se/GameEvents.h"
#include "f4se/GameReferences.h"

#include "CollisionConfig.h"
#include "unordered_dense.h"

#pragma warning(disable : 4996)

struct whitelistSex
{
    bool male;
    bool female;
};

typedef concurrency::concurrent_unordered_map<std::string, float> configEntry_t; // Map settings for a particular bone
typedef concurrency::concurrent_unordered_map<std::string, configEntry_t> config_t; // Settings for a set of bones
typedef concurrency::concurrent_unordered_map<std::string, concurrency::concurrent_unordered_map<std::string, whitelistSex>> whitelist_t;

struct armorOverrideData
{
    bool isFilterInverted;
    concurrency::concurrent_unordered_set<UInt32> slots;
    concurrency::concurrent_unordered_set<UInt32> armors;
    config_t config;
};

struct actorOverrideData
{
    bool isFilterInverted;
    concurrency::concurrent_unordered_set<UInt32> actors;
    config_t config;
};


struct SpecificNPCBounceConfig
{
    Conditions conditions;
    int ConditionPriority = 50;

    config_t config;
};

extern std::vector<SpecificNPCBounceConfig> specificNPCBounceConfigList;

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
extern concurrency::concurrent_unordered_set<UInt32> usedSlots;
extern concurrency::concurrent_unordered_map<UInt64, config_t> cachedConfigs;
extern std::set<UInt32> priorities;

bool LoadConfig();
void DumpWhitelistToLog();


//void Log(const int msgLogLevel, const char* fmt, ...);

//#define LOG(fmt, ...) Log(LOGLEVEL_WARN, fmt, ##__VA_ARGS__)
//#define LOG_ERR(fmt, ...) Log(LOGLEVEL_ERR, fmt, ##__VA_ARGS__)
//#define LOG_INFO(fmt, ...) Log(LOGLEVEL_INFO, fmt, ##__VA_ARGS__)
