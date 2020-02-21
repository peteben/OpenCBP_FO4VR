#pragma once
#include <vector>
#include "f4se/GameReferences.h"

struct ActorEntry {
    UInt32 id;
    Actor* actor;
};
extern std::vector<ActorEntry> actorEntries;
