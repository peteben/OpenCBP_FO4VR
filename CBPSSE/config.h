#pragma once
#include <unordered_set>
#include <set>
#include <map>
#include <vector>

#include <concurrent_vector.h>
#include <concurrent_unordered_map.h>
#include <concurrent_unordered_set.h>

#include "f4se/GameReferences.h"
#include "f4se/GameEvents.h"

#include "unordered_dense.h"

#pragma warning(disable : 4996)

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

//Collision Stuff

struct Sphere
{
	NiPoint3 offset = NiPoint3(0, 0, 0);
	double radius = 4.0;
	double radiuspwr2 = 16.0;
	NiPoint3 worldPos = NiPoint3(0, 0, 0);
	std::string NodeName;
};

struct ConfigLine
{
	std::vector<Sphere> CollisionSpheres;
	std::string NodeName;
};

extern int collisionSkipFrames;

extern int gridsize;
extern int adjacencyValue;
extern int tuningModeCollision;
extern float actorDistance;

extern float collisionX;
extern float collisionY;
extern float collisionZ;

extern std::vector<std::string> AffectedNodeLines;
extern std::vector<std::string> ColliderNodeLines;

extern std::vector<ConfigLine> AffectedNodesList; //Nodes that can be collided with
extern std::vector<ConfigLine> ColliderNodesList; //Nodes that can collide nodes

void LoadCollisionConfig();

void ConfigLineSplitter(std::string &line, Sphere &newSphere);

int GetConfigSettingsValue(std::string line, std::string &variable);
std::string GetConfigSettingsStringValue(std::string line, std::string &variable);

float GetConfigSettingsFloatValue(std::string line, std::string &variable);

void printSpheresMessage(std::string message, std::vector<Sphere> spheres);

std::vector<std::string> ConfigLineVectorToStringVector(std::vector<ConfigLine> linesList);

void DumpCollisionConfigsToLog();

//bool IsActorMale(Actor* actor);

//bool RegisterFuncs(VMClassRegistry* registry);
//BSFixedString GetVersion(StaticFunctionTag* base);
//BSFixedString GetVersionMinor(StaticFunctionTag* base);
//BSFixedString GetVersionBeta(StaticFunctionTag* base);