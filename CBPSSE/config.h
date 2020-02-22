#pragma once
#include <unordered_map>
#include <vector>

#include "f4se/GameReferences.h"
#include "f4se/GameEvents.h"

enum eLogLevels
{
	LOGLEVEL_ERR = 0,
	LOGLEVEL_WARN,
	LOGLEVEL_INFO,
};

class Configuration {
};

struct whitelistSex {
    bool male;
    bool female;
};

typedef std::unordered_map<std::string, float> configEntry_t;
typedef std::unordered_map<std::string, configEntry_t> config_t;
typedef std::unordered_map<std::string, configEntry_t> configOverrides_t;
typedef std::unordered_map<std::string, std::unordered_map<std::string, whitelistSex>> whitelist_t;

extern bool playerOnly;
extern bool femaleOnly;
extern bool maleOnly;
extern bool npcOnly;
extern bool detectArmor;
extern bool useWhitelist;

extern int configReloadCount;
extern config_t config;
extern config_t configArmor;
extern whitelist_t whitelist;
extern std::vector<std::string> raceWhitelist;
bool LoadConfig();
void DumpWhitelistToLog();


//void Log(const int msgLogLevel, const char* fmt, ...);

//#define LOG(fmt, ...) Log(LOGLEVEL_WARN, fmt, ##__VA_ARGS__)
//#define LOG_ERR(fmt, ...) Log(LOGLEVEL_ERR, fmt, ##__VA_ARGS__)
//#define LOG_INFO(fmt, ...) Log(LOGLEVEL_INFO, fmt, ##__VA_ARGS__)

//Collision Stuff

struct Sphere
{
	NiPoint3 offset0 = NiPoint3(0, 0, 0);
	NiPoint3 offset100 = NiPoint3(0, 0, 0);
	double radius0 = 4.0;
	double radius100 = 4.0;
	double radius100pwr2 = 16.0;
	NiPoint3 worldPos = NiPoint3(0, 0, 0);
	std::string NodeName;
};

struct ConfigLine
{
	std::vector<Sphere> CollisionSpheres;
	std::string NodeName;
};

struct SpecificNPCConfig
{
	std::vector<std::string> charactersList;
	std::vector<std::string> raceList;

	std::vector<std::string> AffectedNodeLines;
	std::vector<std::string> ColliderNodeLines;

	std::vector<ConfigLine> AffectedNodesList;

	std::vector<ConfigLine> ColliderNodesList;

	float bellybulge;
	float bellybulgePosition;
	float bellybulgemax;
	float bellybulgeposlowest;
	std::vector<std::string> bellybulgenodesList;
};

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