#pragma once

#include <unordered_map>
#include <vector>
#include "amp.h"
#include <ppl.h>

#include "f4se/GameReferences.h"
#include "Thing.h"
#include "config.h"

class SimObj
{

public:
    enum class Gender
    {
        Male,
        Female,
        Unassigned
    };

    concurrency::concurrent_unordered_map<std::string, Thing> things;
    SimObj(Actor* actor);
    SimObj() {}
    ~SimObj();
    bool AddBonesToThings(Actor* actor, std::vector<std::string>& boneNames);
    bool Bind(Actor* actor, std::vector<std::string>& boneNames, config_t& config);
    Gender GetGender();
    std::string GetRaceEID();
    void Reset();
    void Update(Actor* actor);
    bool UpdateConfigs(config_t& config);
    bool IsBound() { return bound; }
private:
    UInt32 id = 0;
    bool bound = false;
    Gender gender;
    std::string raceEid;
};

extern std::vector<std::string> boneNames;