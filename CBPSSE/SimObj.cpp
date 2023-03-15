#pragma warning(disable : 5040)

#include "f4se/NiNodes.h"
#include "f4se/GameForms.h"
#include "f4se/GameRTTI.h"

#include "ActorUtils.h"
#include "config.h"
#include "log.h"
#include "PapyrusOCBP.h"
#include "SimObj.h"

using actorUtils::GetBaseSkeleton;
using actorUtils::IsBoneInWhitelist;
using actorUtils::IsActorInPowerArmor;
using papyrusOCBP::boneIgnores;

// Note we don't ref count the nodes becasue it's ignored when the Actor is deleted, and calling Release after that can corrupt memory
std::vector<std::string> boneNames;

SimObj::SimObj(Actor* actor)
    : things(4)
{
    id = actor->formID;
    gender = Gender::Unassigned;
    raceEid = std::string("");
}

SimObj::~SimObj()
{
}

bool SimObj::AddBonesToThings(Actor* actor, std::vector<std::string>& boneNames)
{
    //logger.Error("%s\n", __func__);
    if (!actor)
    {
        return false;
    }
    auto loadedData = actor->unkF0;

    if (loadedData && loadedData->rootNode)
    {
        for (std::string & b : boneNames)
        {
            if (!useWhitelist || (IsBoneInWhitelist(actor, b) && useWhitelist))
            {
                //logger.Error("%s: adding Bone %s for actor %08x\n", __func__, b.c_str(), actor->formID);
                BSFixedString cs(b.c_str());
                auto bone = loadedData->rootNode->GetObjectByName(&cs);
                auto findBone = things.find(b);
                if (!bone)
                {
                    logger.Info("%s: Failed to find Bone %s for actor %08x\n", __func__, b.c_str(), actor->formID);
                }
                else if (findBone == things.end())
                {
                    //logger.info("Doing Bone %s for actor %08x\n", b, actor->formID);
                    things.insert(std::make_pair(b, Thing(bone, cs, actor)));
                }
            }
        }
    }
    return true;
}

bool SimObj::Bind(Actor* actor, std::vector<std::string>& boneNames, config_t& config)
{
    if (!actorUtils::IsActorValid(actor))
    {
        return false;
    }
    auto loadedData = actor->unkF0;
    if (loadedData && loadedData->rootNode)
    {
        bound = true;

        things.clear();
        actorColliders.clear();

        if (actorUtils::IsActorMale(actor))
        {
            gender = Gender::Male;
        }
        else
        {
            gender = Gender::Female;
        }

        raceEid = actorUtils::GetActorRaceEID(actor);

        auto success = AddBonesToThings(actor, boneNames);
        if (!success)
        {
            return false;
        }

        //GroundCollisionEnabled = CreateActorColliders(actor, actorColliders);
        UpdateConfigs(config);
        return  true;
    }
    return false;
}

UInt64 SimObj::GetActorKey()
{
    return currentActorKey;
}

SimObj::Gender SimObj::GetGender()
{
    return gender;
}

std::string SimObj::GetRaceEID()
{
    return raceEid;
}

void SimObj::Reset()
{
    bound = false;
    things.clear();
}

void SimObj::SetActorKey(UInt64 key)
{
    currentActorKey = key;
}

void SimObj::Update(Actor* actor, bool collisionsEnabled)
{
    bool stopPhysics = false;

    if (!bound ||
        IsActorInPowerArmor(actor) ||
        NULL == GetBaseSkeleton(actor))
    {
        return;
    }

    if (!actorUtils::IsActorValid(actor))
    {
        return;
    }

    for (auto& t : things)
    {

        // Might be a better way to do this
        auto actorBoneMapIter = boneIgnores.find(actor->formID);
        if (actorBoneMapIter != boneIgnores.end())
        {
            auto & actorBoneMap = actorBoneMapIter->second;
            auto boneDisabledIter = actorBoneMap.find(t.first);
            if (boneDisabledIter != actorBoneMap.end())
            {
                if (true == boneDisabledIter->second)
                {
                    continue;
                }
            }
        }

        if (t.second.isEnabled)
        {
            t.second.ActorCollisionsEnabled = collisionsEnabled;
            t.second.groundPos = this->groundPos;
            t.second.UpdateThing(actor);

            // Update collision sync
            if (t.second.VirtualCollisionEnabled)
            {
                std::string boneName = std::string(t.second.boneName.c_str());
                NodeCollisionSync[boneName] = t.second.collisionSync;
            }
        }
    }
}

bool SimObj::UpdateConfigs(config_t& config)
{
    for (auto & thing : things)
    {
        //concurrency::parallel_for_each(things.begin(), things.end(), [&](auto& thing)
        //    {
                //logger.Info("%s: Updating config for Thing %s\n", __func__, thing.first.c_str());

        if (config.count(thing.first) > 0)
        {
            thing.second.UpdateConfig(config[thing.first]);
            thing.second.isEnabled = true;
        }
        else
        {
            thing.second.isEnabled = false;
        }
        //});
    }
    return true;
}