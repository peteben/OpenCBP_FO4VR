#pragma once
#include "f4se/GameReferences.h"
#include "config.h"

namespace actorUtils
{
    struct EquippedArmor
    {
        const TESForm* armor;
        const TESForm* model;
    };

    std::string GetActorRaceEID(Actor* actor);
    NiAVObject* GetBaseSkeleton(Actor* actor);
    bool IsActorPriorityBlacklisted(Actor* actor, UInt32 priority);
    bool IsActorInPowerArmor(Actor* actor);
    bool IsActorPriorityWhitelisted(Actor* actor, UInt32 priority);
    bool IsActorMale(Actor* actor);
    bool IsActorTrackable(Actor* actor);
    bool IsActorValid(Actor* actor);
    bool IsArmorPriorityBlacklisted(Actor* actor, UInt32 priority);
    bool IsActorInPowerArmor(Actor* actor);
    bool IsArmorPriorityWhitelisted(Actor* actor, UInt32 priority);
    bool IsBoneInWhitelist(Actor* actor, std::string boneName);

    const EquippedArmor GetActorEquippedArmor(Actor* actor, UInt32 slot);
    config_t BuildConfigForActor(Actor* actor);
}