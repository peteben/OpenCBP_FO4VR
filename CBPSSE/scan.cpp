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
    NiNode* node = parent->GetAsNiNode();
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

bool printStuff(NiAVObject* avObj, int depth)
{
    std::string sss = spaces(depth);
    const char* ss = sss.c_str();
    //LOG_INFO("%savObj Name = %s, RTTI = %s\n", ss, avObj->m_name, avObj->GetRTTI()->name);

    //NiNode *node = avObj->GetAsNiNode();
    //if (node) {
    //	LOG_INFO("%snode %s, RTTI %s\n", ss, node->m_name, node->GetRTTI()->name);
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

TESObjectCELL* curCell = nullptr;


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

    //LOG_ERROR("scan Cell\n");
    if (!(*g_player) || !(*g_player)->unkF0)
    {
        return;
    }

    TESObjectCELL* cell = (*g_player)->parentCell;
    if (!cell)
        return;

    // If no player then return
    auto player = DYNAMIC_CAST(LookupFormByID(0x14), TESForm, Actor);
    if (!player || !player->unkF0) return;

    float xLow = 9999999.0;
    float xHigh = -9999999.0;
    float yLow = 9999999.0;
    float yHigh = -9999999.0;
    float zLow = 9999999.0;
    float zHigh = -9999999.0;

    if (cell != curCell)
    {
        LOG_ERROR("cell change %d\n", cell->formID);
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
                bool isValid = true;
                auto actorDistance = 1024.0f; // TODO, 32 squared
                auto actorBounceDistance = 4096.0f; // TODO, 64 squared
                float actorDistSqr = 0.0;


                if (!IsActorValid(actor))
                {
                    LOG_VERBOSE("%s: Actor in cell invalid for physics, ignoring. \n", __func__);
                    continue;
                }

                if (actor->unkF0->rootNode)
                {
                    //Getting border values;
                    NiPoint3 relativeActorPos = actor->unkF0->rootNode->m_worldTransform.pos - (*g_player)->unkF0->rootNode->m_worldTransform.pos;

                    actorDistSqr = magnitudePwr2(relativeActorPos);

                    if (actorDistSqr > actorBounceDistance)
                    {
                        LOG_VERBOSE("Actor with form ID %08x too far away\n", actor->formID);
                        isValid = false;
                    }
                }

                if (!isValid)
                {
                    continue;
                }

                // If actor is not being tracked yet
                if (actors.count(actor->formID) == 0)
                {
                    // If actor should not be tracked, don't add it.
                    if (IsActorTrackable(actor))
                    {
                        LOG_VERBOSE("Tracking Actor with form ID %08x in cell %ld, race is %s, gender is %d\n",
                            actor->formID, actor->parentCell->formID,
                            actor->race->editorId.c_str(),
                            IsActorMale(actor));
                        // Make SimObj and place new element in Things
                        auto simObj = SimObj(actor);
                        //obj.actorDistSqr = actorDistSqr;
                        actors.insert(std::make_pair(actor->formID, simObj));
                        actorEntries.push_back(ActorEntry{ actor->formID, actor, actorDistSqr, actorDistSqr <= actorDistance });
                    }
                }
                else
                {
                    // If already tracked then add to entry list for this frame
                    //actors[actor->formID].actorDistSqr = actorDistSqr;
                    actorEntries.push_back(ActorEntry{ actor->formID, actor, actorDistSqr, actorDistSqr <= actorDistance });
                }
            }
        }
    }


    partitions.clear();

    LOG_VERBOSE("%s: Starting collider hashing\n", __func__);

    NiPoint3 playerPos = (*g_player)->unkF0->rootNode->m_worldTransform.pos;
    long colliderSphereCount = 0;
    long colliderCapsuleCount = 0;

    for (auto& a : actorEntries)
    {
        if (a.collisionsEnabled == true)
        {
            auto simObjIter = actors.find(a.id);
            if (simObjIter != actors.end())
            {
                UpdateColliderPositions(simObjIter->second.actorColliders, simObjIter->second.NodeCollisionSync);

                for (auto& collider : simObjIter->second.actorColliders)
                {
                    std::vector<int> ids;
                    std::vector<int> hashIdList;
                    for (int j = 0; j < collider.second.collisionSpheres.size(); j++)
                    {
                        hashIdList = GetHashIdsFromPos(collider.second.collisionSpheres[j].worldPos, collider.second.collisionSpheres[j].radius);

                        for (int m = 0; m < hashIdList.size(); m++)
                        {
                            if (std::find(ids.begin(), ids.end(), hashIdList[m]) == ids.end())
                            {
                                //LOG_INFO("ids.emplace_back(%d)", hashIdList[m]);
                                ids.emplace_back(hashIdList[m]);
                                partitions[hashIdList[m]].partitionCollisions.push_back(collider.second);
                            }
                        }
                        colliderSphereCount++;
                    }

                    for (int j = 0; j < collider.second.collisionCapsules.size(); j++)
                    {
                        hashIdList = GetHashIdsFromPos((collider.second.collisionCapsules[j].End1_worldPos + collider.second.collisionCapsules[j].End2_worldPos)
                            , (collider.second.collisionCapsules[j].End1_radius + collider.second.collisionCapsules[j].End2_radius) * 0.5f);
                        for (int m = 0; m < hashIdList.size(); m++)
                        {
                            if (std::find(ids.begin(), ids.end(), hashIdList[m]) == ids.end())
                            {
                                //LOG_INFO("ids.emplace_back(%d)", hashIdList[m]);
                                ids.emplace_back(hashIdList[m]);
                                partitions[hashIdList[m]].partitionCollisions.push_back(collider.second);
                            }
                        }
                        colliderCapsuleCount++;
                    }
                }
            }
        }
    }
    LOG_INFO("Collider sphere count = %ld ", colliderSphereCount);
    LOG_INFO("Collider capsule count = %ld\n", colliderCapsuleCount);


    //static bool done = false;
    //if (!done && player->loadedState->node) {
    //	visitObjects(player->loadedState->node, printStuff);
    //	BSFixedString cs("UUNP");
    //	auto bodyAV = player->loadedState->node->GetObjectByName(&cs.data);
    //	BSTriShape *body = bodyAV->GetAsBSTriShape();
    //	LOG_INFO("GetAsBSTriShape returned  %lld\n", body);
    //	auto geometryData = body->geometryData;
    //	//LOG_INFO("Num verts = %d\n", geometryData->m_usVertices);


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
                LOG_ERROR("Sim Object not found in tracked actors\n");
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

    LOG_VERBOSE("%s: Creating sim objs.\n", __func__);

    for (auto& a : actorEntries)
    {
        auto actorsIterator = actors.find(a.id);
        if (actorsIterator == actors.end())
        {
            //LOG_ERROR("Sim Object not found in tracked actors\n");
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
                    LOG_INFO("UpdateActors: Reset sim object\n");
                    simObj.Reset();
                }
            }
            else
            {
                auto key = BuildActorKey(a.actor);
                auto& composedConfig = BuildConfigForActor(a.actor, key);

                LOG_VERBOSE("UpdateActors: Setting key for actor %x...\n", a.id);
                simObj.SetActorKey(key);
                simObj.Bind(a.actor, boneNames, composedConfig);
            }
        }
    }

    LOG_VERBOSE("%s: Updating sim objs.\n", __func__);

    concurrency::parallel_for_each(actorEntries.begin(), actorEntries.end(), [&](const auto& a)
        {
            LOG_VERBOSE("%s: Getting actor iter.\n", __func__);

            auto actorsIterator = actors.find(a.id);
            if (actorsIterator == actors.end())
            {
                LOG_ERROR("Sim Object not found in tracked actors\n");
            }
            else
            {
                LOG_VERBOSE("%s: Getting sim obj.\n", __func__);
                auto& simObj = actorsIterator->second;

                if (simObj.IsBound())
                {
                    LOG_VERBOSE("%s: Getting sim obj key.\n", __func__);

                    UInt64 key = BuildActorKey(a.actor);
                    UInt64 simObjKey = simObj.GetActorKey();

                    LOG_VERBOSE("%s: Gotten sim obj key.\n", __func__);

                    // Detect changes in actor+slots combination
                    if (key != simObjKey)
                    {
                        LOG_ERROR("UpdateActors: Key change detected for actor %08x.\n", a.actor->formID);
                        simObj.SetActorKey(key);
                        auto& composedConfig = BuildConfigForActor(a.actor, key);
                        simObj.UpdateConfigs(composedConfig);
                    }

                    LOG_VERBOSE("%s: Finally sim obj update.\n", __func__);

                    simObj.Update(a.actor, a.collisionsEnabled);
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
        LOG_ERROR("Average Update Time in 1000 frame = %lld ns\n", totaltime.QuadPart / debugtimelog_framecount);
        totaltime.QuadPart = 0;
        debugtimelog_framecount = 0;
        //totalcallcount = 0;
    }
    debugtimelog_framecount++;
#endif

    //FAILED:
    return;
}

