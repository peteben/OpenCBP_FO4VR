#pragma once
#include <concurrent_unordered_map.h>
#include <concurrent_unordered_set.h>
#include <concurrent_vector.h>
#include <vector>

#include "f4se/GameReferences.h"

struct Sphere
{
	NiPoint3 offset = NiPoint3(0, 0, 0);
	double radius = 4.0;
	double radiuspwr2 = 16.0;
	NiPoint3 worldPos = NiPoint3(0, 0, 0);
	std::string NodeName;
	UInt32 index = -1;
};

struct Capsule
{
	NiPoint3 End1_offset = NiPoint3(0, 0, 0);
	NiPoint3 End1_worldPos = NiPoint3(0, 0, 0);
	double End1_radius = 4.0;
	double End1_radiuspwr2 = 16.0;
	NiPoint3 End2_offset = NiPoint3(0, 0, 0);
	NiPoint3 End2_worldPos = NiPoint3(0, 0, 0);
	double End2_radius = 4.0;
	double End2_radiuspwr2 = 16.0;

	std::string NodeName;
	UInt32 index = -1;
};

struct ConfigLine
{
	std::vector<Sphere> CollisionSpheres;
	std::vector<Capsule> CollisionCapsules;
	std::string NodeName;
	std::vector<std::string> IgnoredColliders;
	std::vector<std::string> IgnoredSelfColliders;
	bool IgnoreAllSelfColliders = false;
	float scaleWeight = 1.0f;
};

enum ConditionType
{
	IsRaceFormId,
	IsRaceName,
	ActorFormId,
	ActorName,
	ActorWeightGreaterThan,
	IsMale,
	IsFemale,
	IsPlayer,
	IsInFaction,
	HasKeywordId,
	HasKeywordName,
	RaceHasKeywordId,
	RaceHasKeywordName,
	IsActorBase,
	IsPlayerTeammate,
	IsUnique,
	IsVoiceType,
	IsCombatStyle,
	IsClass
};

struct ConditionItem
{
	//multiple items
	std::vector<ConditionItem> OrItems;

	//single item
	bool single = true;
	bool Not = false;
	ConditionType type;
	UInt32 id;
	std::string str;
};

struct Conditions
{
	std::vector<ConditionItem> AndItems;
};

struct SpecificNPCConfig
{
	Conditions conditions;
	int ConditionPriority = 50;

	std::vector<std::string> AffectedNodeLines;
	std::vector<std::string> ColliderNodeLines;

	std::vector<ConfigLine> AffectedNodesList;

	concurrency::concurrent_vector<ConfigLine> ColliderNodesList;

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

extern concurrency::concurrent_vector<ConfigLine> AffectedNodesList; //Nodes that can be collided with
extern concurrency::concurrent_vector<ConfigLine> ColliderNodesList; //Nodes that can collide nodes

extern std::string GroundReferenceBone;
extern std::string HighheelReferenceBone;

void LoadCollisionConfig();

bool GetSpecificNPCConfigForActor(Actor* actor);

void ConfigLineSplitter(std::string& line, Sphere& newSphere);

int GetConfigSettingsValue(std::string line, std::string& variable);
std::string GetConfigSettingsStringValue(std::string line, std::string& variable);

float GetConfigSettingsFloatValue(std::string line, std::string& variable);

void printSpheresMessage(std::string message, std::vector<Sphere> spheres);

std::vector<std::string> ConfigLineVectorToStringVector(std::vector<ConfigLine> linesList);

void DumpCollisionConfigsToLog();

bool ConditionCheck(Actor* actor, ConditionItem& condition);
bool CheckActorForConditions(Actor* actor, Conditions& conditions);

//extern concurrency::concurrent_vector<std::string> PlayerCollisionEventNodes;
//
//extern float MinimumCollisionDurationForEvent;
//
//struct PlayerCollisionEvent
//{
//	bool collisionInThisCycle = false;
//	float durationFilled = 0.0f;
//	float totalDurationFilled = 0.0f;
//};
//
//extern concurrency::concurrent_unordered_map<std::string, PlayerCollisionEvent> ActorNodePlayerCollisionEventMap;

