#pragma once
#include <vector>
#include <concurrent_unordered_map.h>
#include <concurrent_vector.h>

#include "f4se/GameReferences.h"

class ActorEntry
{
public:
    UInt32 id;
    Actor* actor;
    float actorDistSqr;
    bool collisionsEnabled = true;
};
extern concurrency::concurrent_vector<ActorEntry> actorEntries;
