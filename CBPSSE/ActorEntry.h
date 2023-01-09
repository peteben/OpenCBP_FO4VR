#pragma once
#include <vector>
#include "f4se/GameReferences.h"

class ActorEntry
{
public:
    UInt32 id;
    Actor* actor;
};
extern std::vector<ActorEntry> actorEntries;
