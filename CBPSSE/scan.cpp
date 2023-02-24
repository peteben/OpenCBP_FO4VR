#pragma once
#pragma warning(disable : 5040)

#define DEBUG

//#define SIMPLE_BENCHMARK

#include <cstdlib>
#include <cstdio>

#include <typeinfo>

#include <memory>
#include <vector>
#include <chrono>
#include <algorithm>
#include <cassert>
#include <atomic>
#include <string>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <atomic>
#include <functional>

#include <unordered_set>

#include <concurrent_unordered_map.h>
#include <concurrent_vector.h>
#include <ppl.h>

#include "ActorEntry.h"
#include "ActorUtils.h"
#include "log.h"
#include "Thing.h"
#include "config.h"
#include "PapyrusOCBP.h"
#include "SimObj.h"
#include "Utility.hpp"
#include "f4se/GameRTTI.h"
#include "f4se/GameForms.h"
#include "f4se/GameReferences.h"
#include "f4se/NiTypes.h"
#include "f4se/NiNodes.h"
#include "f4se/BSGeometry.h"
#include "f4se/GameThreads.h"
#include "f4se/PluginAPI.h"
#include "f4se/GameStreams.h"
#include "f4se/GameExtraData.h"

#pragma warning(disable : 4996)

using actorUtils::IsActorMale;
using actorUtils::IsActorTrackable;
using actorUtils::IsActorValid;
using actorUtils::BuildActorKey;
using actorUtils::BuildConfigForActor;
using actorUtils::GetActorRaceEID;

std::atomic<TESObjectCELL*> currCell = nullptr;

extern F4SETaskInterface* g_task;

concurrency::concurrent_unordered_map<UInt32, SimObj> actors;  // Map of Actor (stored as form ID) to its Simulation Object

#ifdef SIMPLE_BENCHMARK
bool firsttimeloginit = true;
LARGE_INTEGER startingTime, endingTime, elapsedMicroseconds;
LARGE_INTEGER frequency;
LARGE_INTEGER totaltime;
int debugtimelog_framecount = 1;
#endif

//void UpdateWorldDataToChild(NiAVObject)
void DumpTransform(NiTransform t)
{
    Console_Print("%8.2f %8.2f %8.2f", t.rot.data[0][0], t.rot.data[0][1], t.rot.data[0][2]);
    Console_Print("%8.2f %8.2f %8.2f", t.rot.data[1][0], t.rot.data[1][1], t.rot.data[1][2]);
    Console_Print("%8.2f %8.2f %8.2f", t.rot.data[2][0], t.rot.data[2][1], t.rot.data[2][2]);

    Console_Print("%8.2f %8.2f %8.2f", t.pos.x, t.pos.y, t.pos.z);
    Console_Print("%8.2f", t.scale);
}


bool visitObjects(NiAVObject* parent, std::function<bool(NiAVObject*, int)> functor, int depth = 0)
{
    if (!parent) return false;
    NiNode * node = parent->GetAsNiNode();
    if (node)
    {
        if (functor(parent, depth))
            return true;

        for (UInt32 i = 0; i < node->m_children.m_emptyRunStart; i++)
        {
            NiAVObject* object = node->m_children.m_data[i];
            if (object)
            {
                if (visitObjects(object, functor, depth + 1))
                    return true;
            }
        }
    }
    else if (functor(parent, depth))
        return true;

    return false;
}

std::string spaces(int n)
{
    auto s = std::string(n, ' ');
    return s;
}

bool printStuff(NiAVObject * avObj, int depth)
{
    std::string sss = spaces(depth);
    const char * ss = sss.c_str();
    //logger.info("%savObj Name = %s, RTTI = %s\n", ss, avObj->m_name, avObj->GetRTTI()->name);

    //NiNode *node = avObj->GetAsNiNode();
    //if (node) {
    //	logger.info("%snode %s, RTTI %s\n", ss, node->m_name, node->GetRTTI()->name);
    //}
    //return false;
}

template<class T>
inline void safe_delete(T*& in)
{
    if (in)
    {
        delete in;
        in = NULL;
    }
}
bool compareActorEntries(const ActorEntry& entry1, const ActorEntry& entry2)
{
	return entry1.actorDistSqr < entry2.actorDistSqr;
}

//bool ActorIsInAngle(Actor* actor, float originalHeading, NiPoint3 cameraPosition)
//{
//	if (actor == (*g_player))
//		return true;
//
//	if (actorAngle >= 360)
//		return true;
//
//	if (actorAngle <= 0 && actor != (*g_player))
//	{
//		return false;
//	}
//
//	NiPoint3 position = actor->unkF0->rootNode->m_worldTransform.pos;
//
//	float heading = 0;
//	float attitude = 0;
//	GetAttitudeAndHeadingFromTwoPoints(cameraPosition, NiPoint3(position.x, position.y, cameraPosition.z), attitude, heading);
//	heading = heading * 57.295776f;
//
//	return AngleDifference(originalHeading, heading) <= (actorAngle * 0.5f);
//}

TESObjectCELL * curCell = nullptr;


void UpdateActors()
{
#ifdef SIMPLE_BENCHMARK
    if (firsttimeloginit)
    {
        firsttimeloginit = false;
        totaltime.QuadPart = 0;

    }

    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&startingTime);
#endif

    // We scan the cell and build the list every time - only look up things by ID once
    // we retain all state by actor ID, in a map - it's cleared on cell change
    actorEntries.clear();

    //logger.error("scan Cell\n");
	if (!(*g_player) || !(*g_player)->unkF0)
	{
		return;
	}
		
	TESObjectCELL* cell = (*g_player)->parentCell;
	if (!cell)
		return;
		
    NiPoint3 actorPos;

    // If no player then return
    auto player = DYNAMIC_CAST(LookupFormByID(0x14), TESForm, Actor);
    if (!player || !player->unkF0) goto FAILED;

    // If player has no cell then return
    auto cell = player->parentCell;
    if (!cell) goto FAILED;

    float xLow = 9999999.0; 
    float xHigh = -9999999.0;
    float yLow = 9999999.0;
    float yHigh = -9999999.0;
    float zLow = 9999999.0;
    float zHigh = -9999999.0;
    
    if (cell != curCell)
    {
        logger.Error("cell change %d\n", cell->formID);
        curCell = cell;
        actors.clear();
        actorEntries.clear();
    }
    else
    {
        // Attempt to get cell's objects
        for (UInt32 i = 0; i < cell->objectList.count; i++)
        {
            auto ref = cell->objectList[i];
            if (ref)
            {
                // Attempt to get actors
                auto actor = DYNAMIC_CAST(ref, TESObjectREFR, Actor);
                if (actor && actor->unkF0)
                {

                    if (actor->unkF0->rootNode)
                    {
                        //Getting border values;
                        actorPos = actor->unkF0->rootNode->m_worldTransform.pos;

                        if (distanceNoSqrt((*g_player)->unkF0->rootNode->m_worldTransform.pos, actorPos) > actorDistance) {
                            logger.Info("Actor with form ID %08x too far away\n", actor->formID);
                            continue;
                        }

                    }
                    
                    // If actor is not being tracked yet
                    if (actors.count(actor->formID) == 0)
                    {
                        // If actor should not be tracked, don't add it.
                        if (IsActorTrackable(actor))
                        {
                            //logger.Info("Tracking Actor with form ID %08x in cell %ld, race is %s, gender is %d\n", 
                            //    actor->formID, actor->parentCell->formID,
                            //    actor->race->editorId.c_str(),
                            //    IsActorMale(actor));
                            // Make SimObj and place new element in Things
                            auto obj = SimObj(actor);
							obj.actorDistSqr = actorDistSqr;
                            actors.insert(std::make_pair(actor->formID, obj));
                            actorEntries.push_back(ActorEntry{ actor->formID, actor, actorDistSqr, actorDistSqr <= actorDistance });
                        }
                    }
                    else if (IsActorValid(actor) && isValid)
                    {
                        // If already tracked then add to entry list for this frame
						actors[actor->formID].actorDistSqr = actorDistSqr;
                        actorEntries.push_back(ActorEntry{ actor->formID, actor });
                    }
                }
            }
        }
    }


    partitions.clear();

    //logger.Info("Starting collider hashing\n");


	NiPoint3 playerPos = (*g_player)->unkF0->rootNode->m_worldTransform.pos;
	long colliderSphereCount = 0;
	long colliderCapsuleCount = 0;

        std::vector<long> ids;
        std::vector<long> hashIdList;
        for (int i = 0; i < otherColliders.size(); i++)
        {
            //LOG_INFO("otherColliders[%d]: %s",i, otherColliders[i].CollisionObject->m_name);

            ids.clear();
            for (int j = 0; j < otherColliders[i].collisionSpheres.size(); j++)
            {
                hashIdList = GetHashIdsFromPos(otherColliders[i].collisionSpheres[j].worldPos, otherColliders[i].collisionSpheres[j].radius, hashSize);

                for (int m = 0; m < hashIdList.size(); m++)
                {
                    // Place all unfound IDs into ids and partitions
                    if (std::find(ids.begin(), ids.end(), hashIdList[m]) != ids.end())
                        continue;
                    else
                    {
                        //LOG_INFO("ids.emplace_back(%d)", hashIdList[m]);
                        ids.emplace_back(hashIdList[m]);
                        partitions[hashIdList[m]].partitionCollisions.emplace_back(otherColliders[i]);
                    }
                }
            }
        }
        //LOG_INFO("End of collider hashing");
        //LOG_INFO("Partitions size=%d", partitions.size());

    //static bool done = false;
    //if (!done && player->loadedState->node) {
    //	visitObjects(player->loadedState->node, printStuff);
    //	BSFixedString cs("UUNP");
    //	auto bodyAV = player->loadedState->node->GetObjectByName(&cs.data);
    //	BSTriShape *body = bodyAV->GetAsBSTriShape();
    //	logger.info("GetAsBSTriShape returned  %lld\n", body);
    //	auto geometryData = body->geometryData;
    //	//logger.info("Num verts = %d\n", geometryData->m_usVertices);


    //	done = true;
    //}

    // Reload config
    static int count = 0;
    if (configReloadCount && count++ > configReloadCount)
    {
        count = 0;
        auto reloadActors = LoadConfig();
        for (auto& a : actorEntries)
        {
            auto actorsIterator = actors.find(a.id);
            if (actorsIterator == actors.end())
            {
                //logger.error("Sim Object not found in tracked actors\n");
            }
            else
            {
                auto& simObj = actorsIterator->second;
                auto key = BuildActorKey(a.actor);
                auto& composedConfig = BuildConfigForActor(a.actor, key);

                simObj.SetActorKey(key);
                simObj.AddBonesToThings(a.actor, boneNames);
                simObj.UpdateConfigs(composedConfig);
            }
        }

        // Clear actors
        if (reloadActors)
        {
            actors.clear();
            actorEntries.clear();
        }
    }

    for (auto& a : actorEntries)
    {
        auto actorsIterator = actors.find(a.id);
        if (actorsIterator == actors.end())
        {
            //logger.error("Sim Object not found in tracked actors\n");
        }
        else
        {
            auto& simObj = actorsIterator->second;

            SimObj::Gender gender = IsActorMale(a.actor) ? SimObj::Gender::Male : SimObj::Gender::Female;

            // Check if gender and/or race have changed
            if (simObj.IsBound())
            {
                if (gender != simObj.GetGender() ||
                    GetActorRaceEID(a.actor) != simObj.GetRaceEID())
                {
                    logger.Info("UpdateActors: Reset sim object\n");
                    simObj.Reset();
                }
            }
            else
            {
                auto key = BuildActorKey(a.actor);
                auto& composedConfig = BuildConfigForActor(a.actor, key);

                //logger.Error("UpdateActors: Setting key for actor %x...\n", a.id);
                simObj.SetActorKey(key);
                simObj.Bind(a.actor, boneNames, composedConfig);
            }
        }
    }

    concurrency::parallel_for_each(actorEntries.begin(), actorEntries.end(), [&](const auto& a)
        {
            auto actorsIterator = actors.find(a.id);
            if (actorsIterator == actors.end())
            {
                //logger.error("Sim Object not found in tracked actors\n");
            }
            else
            {
                auto& simObj = actorsIterator->second;

                if (simObj.IsBound())
                {
                    UInt64 key = BuildActorKey(a.actor);
                    UInt64 simObjKey = simObj.GetActorKey();

                    // Detect changes in actor+slots combination
                    if (key != simObjKey)
                    {
                        logger.Error("UpdateActors: Key change detected for actor %08x.\n", a.actor->formID);
                        simObj.SetActorKey(key);
                        auto& composedConfig = BuildConfigForActor(a.actor, key);
                        simObj.UpdateConfigs(composedConfig);
                    }
                    simObj.Update(a.actor);
                }
            }
        });



#ifdef SIMPLE_BENCHMARK
    QueryPerformanceCounter(&endingTime);
    elapsedMicroseconds.QuadPart = endingTime.QuadPart - startingTime.QuadPart;
    elapsedMicroseconds.QuadPart *= 1000000000LL;
    elapsedMicroseconds.QuadPart /= frequency.QuadPart;
    //long long avg = elapsedMicroseconds.QuadPart / callCount;
    totaltime.QuadPart += elapsedMicroseconds.QuadPart;
    //LOG_ERR("Collider Check Call Count: %d - Update Time = %lld ns", callCount, elapsedMicroseconds.QuadPart);
    if (debugtimelog_framecount % 1000 == 0)
    {
        logger.Error("Average Update Time in 1000 frame = %lld ns\n", totaltime.QuadPart / debugtimelog_framecount);
        totaltime.QuadPart = 0;
        debugtimelog_framecount = 0;
        //totalcallcount = 0;
    }
    debugtimelog_framecount++;
#endif

FAILED:
    return;
}

