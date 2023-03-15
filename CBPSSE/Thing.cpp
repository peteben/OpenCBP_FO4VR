#include "f4se\NiNodes.h"
#include <time.h>

#include "ActorUtils.h"
#include "log.h"
#include "Thing.h"
#include "Utility.hpp"

constexpr auto DEG_TO_RAD = 3.14159265 / 180;
const char* skeletonNif_boneName = "skeleton.nif";
const float DIFF_LIMIT = 100.0;
const NiMatrix43 zeroMatrix = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

// TODO Make these logger macros
//#define DEBUG 1
//#define TRANSFORM_DEBUG 1
//#define COLLISION_DEBUG 1

// <BoneName <ActorFormID, data>>
pos_map Thing::origLocalPos;
rot_map Thing::origLocalRot;
rot_map Thing::origChestWorldRot;

//shared_mutex Thing::thing_map_lock;

void Thing::ShowPos(NiPoint3& p)
{
    logger.Info("%8.4f %8.4f %8.4f\n", p.x, p.y, p.z);
}

void Thing::ShowRot(NiMatrix43& r)
{
    logger.Info("%8.4f %8.4f %8.4f %8.4f\n", r.data[0][0], r.data[0][1], r.data[0][2], r.data[0][3]);
    logger.Info("%8.4f %8.4f %8.4f %8.4f\n", r.data[1][0], r.data[1][1], r.data[1][2], r.data[1][3]);
    logger.Info("%8.4f %8.4f %8.4f %8.4f\n", r.data[2][0], r.data[2][1], r.data[2][2], r.data[2][3]);
}

Thing::Thing(NiAVObject* obj, BSFixedString& name, Actor* actor)
    : thingObj(obj)
    , boneName(name)
    , velocity(NiPoint3(0, 0, 0))
    , m_actor(actor)
{
    isEnabled = true;

    auto skeletonObj = actorUtils::GetBaseSkeleton(actor);
    if (skeletonObj == NULL)
    {
        logger.Error("%s: Didn't find thing %s's base skeleton.nif for actor %08x \n", __func__, boneName.c_str(), actor->formID);
        return;
    }

    //->m_worldTransform.rot * skeletonObj->m_worldTransform.pos
    auto firstWorldPos = skeletonObj->m_worldTransform.rot * obj->m_worldTransform.pos;
    auto firstSkeletonPos = skeletonObj->m_worldTransform.rot * skeletonObj->m_worldTransform.pos;

    rightSide = (firstWorldPos.x - firstSkeletonPos.x) >= 0.0;

    // Set initial positions
    oldWorldPos = obj->m_worldTransform.pos;
    origWorldRot = obj->m_localTransform.rot.Transpose() * obj->m_worldTransform.rot;

    IsBreastBone = ContainsNoCase(std::string(boneName.c_str()), "Breast");

    time = clock();

    // TODO: check scaled motion
    float nodescale = 1.0f;

    thingCollisionSpheres = CreateThingCollisionSpheres(actor, std::string(name.c_str()));
    thingCollisionCapsules = CreateThingCollisionCapsules(actor, std::string(name.c_str()));

    collisionBuffer = NiPoint3(0, 0, 0);
    collisionSync = NiPoint3(0, 0, 0);
    skipFramesCount = collisionSkipFrames;
}

Thing::~Thing()
{
}

NiPoint3 Thing::CalculateGravitySupine(Actor* actor)
{
    NiPoint3 varGravitySupine = NiPoint3(0.0, 0.0, 0.0);

    if (!IsBreastBone) //other bones don't need to edited gravity by SPINE2 obj
    {
        //other nodes are based on parent obj
        varGravitySupine = NiPoint3(0.0, 0.0, 0.0);
        return varGravitySupine;
    }

    //Get the reference bone to know which way the breasts are orientated
    //thing_ReadNode_lock.lock();
    BSFixedString chest_str("Chest");

    NiAVObject* breastGravityReferenceBone = actor->unkF0->rootNode->GetObjectByName(&chest_str);

    //thing_ReadNode_lock.unlock();

    float gravityRatio = 0.0f;
    if (breastGravityReferenceBone != nullptr)
    {
        //auto breastRot = breastGravityReferenceBone->m_worldTransform.rot;
        //Get the orientation (here the Z element of the rotation matrix (approx 1.0 when standing up, approx -1.0 when upside down))
        auto chestOrientation = breastGravityReferenceBone->m_worldTransform.rot.data[1][2];
        gravityRatio = chestOrientation >= 0.0 ? chestOrientation : 0.0;
    }

    varGravitySupine = NiPoint3(gravitySupineX, gravitySupineY, gravitySupineZ) * gravityRatio;

    return varGravitySupine;
}

void Thing::StoreOriginalTransforms(Actor* actor)
{
    // Save the bones' original local values, per actor, if they already haven't
    auto origLocalPos_iter = origLocalPos.find(boneName.c_str());
    auto origLocalRot_iter = origLocalRot.find(boneName.c_str());

    auto obj = thingObj;

    //thing_map_lock.lock();
    if (IsBreastBone)
    {
        BSFixedString chest_name("Chest");
        NiAVObject* chestObj = actor->unkF0->rootNode->GetObjectByName(&chest_name);

        if (chestObj && chestObj->m_parent)
        {
            if (origLocalRot_iter == origChestWorldRot.end())
            {
                origChestWorldRot[boneName.c_str()][actor->formID] = chestObj->m_parent->m_worldTransform.rot * chestObj->m_localTransform.rot;
            }
            else
            {
                auto &actorRotMap = origChestWorldRot.at(boneName.c_str());
                auto actor_iter = actorRotMap.find(actor->formID);
                if (actor_iter == actorRotMap.end())
                {
                    origChestWorldRot[boneName.c_str()][actor->formID] = chestObj->m_parent->m_worldTransform.rot * chestObj->m_localTransform.rot;
                }
            }
        }
    }

    // Original Bone Position
    if (origLocalPos_iter == origLocalPos.end())
    {
        origLocalPos[boneName.c_str()][actor->formID] = obj->m_localTransform.pos;
#ifdef DEBUG
        logger.Error("for bone %s, actor %08x: \n", boneName.c_str(), actor->formID);
        logger.Error("firstRun pos Set: \n");
        ShowPos(obj->m_localTransform.pos);
#endif
    }
    else
    {
        auto &actorPosMap = origLocalPos.at(boneName.c_str());
        auto actor_iter = actorPosMap.find(actor->formID);
        if (actor_iter == actorPosMap.end())
        {
            origLocalPos[boneName.c_str()][actor->formID] = obj->m_localTransform.pos;
#ifdef DEBUG
            logger.Error("for bone %s, actor %08x: \n", boneName.c_str(), actor->formID);
            logger.Error("firstRun pos Set: \n");
            ShowPos(obj->m_localTransform.pos);
#endif
        }
    }

    // Original Bone Rotation
    if (origLocalRot_iter == origLocalRot.end())
    {
        origLocalRot[boneName.c_str()][actor->formID] = obj->m_localTransform.rot;
#ifdef DEBUG
        logger.Error("for bone %s, actor %08x: \n", boneName.c_str(), actor->formID);
        logger.Error("firstRun rot Set:\n");
        ShowRot(obj->m_localTransform.rot);
#endif
    }
    else
    {
        auto &actorRotMap = origLocalRot.at(boneName.c_str());
        auto actor_iter = actorRotMap.find(actor->formID);
        if (actor_iter == actorRotMap.end())
        {
            origLocalRot[boneName.c_str()][actor->formID] = obj->m_localTransform.rot;
#ifdef DEBUG
            logger.Error("for bone %s, actor %08x: \n", boneName.c_str(), actor->formID);
            logger.Error("firstRun rot Set: \n");
            ShowRot(obj->m_localTransform.rot);
#endif
        }
    }
    //thing_map_lock.unlock();
}

// TODO: this copies an entire vector...
std::vector<Sphere> Thing::CreateThingCollisionSpheres(Actor* actor, std::string nodeName)
{
    concurrency::concurrent_vector<ConfigLine>* affectedNodesListPtr;

    // TODO: snc for later
    //SpecificNPCConfig snc;

    //bool success = GetSpecificNPCConfigForActor(actor, snc);

    //if (success)
    //{
    //	affectedNodesListPtr = &(snc.AffectedNodesList);
    //}
    //else
    //{
    affectedNodesListPtr = &AffectedNodesList;
    //}

    std::vector<Sphere> spheres;

    for (int i = 0; i < affectedNodesListPtr->size(); i++)
    {
        if (affectedNodesListPtr->at(i).NodeName == nodeName)
        {
            spheres = affectedNodesListPtr->at(i).CollisionSpheres;
            IgnoredCollidersList = affectedNodesListPtr->at(i).IgnoredColliders;
            IgnoredSelfCollidersList = affectedNodesListPtr->at(i).IgnoredSelfColliders;
            IgnoreAllSelfColliders = affectedNodesListPtr->at(i).IgnoreAllSelfColliders;
            for (int j = 0; j < spheres.size(); j++)
            {
                spheres[j].offset = spheres[j].offset;

                spheres[j].radius = spheres[j].radius;

                spheres[j].radiuspwr2 = spheres[j].radius * spheres[j].radius;
            }
            break;
        }
    }
    return spheres;
}

std::vector<Capsule> Thing::CreateThingCollisionCapsules(Actor* actor, std::string nodeName)
{
    concurrency::concurrent_vector<ConfigLine>* AffectedNodesListPtr;

    //SpecificNPCConfig snc;

    //bool success = GetSpecificNPCConfigForActor(actor, snc);

    //if (success)
    //{
    //	AffectedNodesListPtr = &(snc.AffectedNodesList);
    //}
    //else
    //{
    AffectedNodesListPtr = &AffectedNodesList;
    //}

    std::vector<Capsule> capsules;

    concurrency::parallel_for(size_t(0), AffectedNodesListPtr->size(), [&](size_t i)
        {
            if (AffectedNodesListPtr->at(i).NodeName == nodeName)
            {
                capsules = AffectedNodesListPtr->at(i).CollisionCapsules;
                IgnoredCollidersList = AffectedNodesListPtr->at(i).IgnoredColliders;
                IgnoredSelfCollidersList = AffectedNodesListPtr->at(i).IgnoredSelfColliders;
                IgnoreAllSelfColliders = AffectedNodesListPtr->at(i).IgnoreAllSelfColliders;
                for (int j = 0; j < capsules.size(); j++)
                {
                    capsules[j].End1_offset = capsules[j].End1_offset;

                    capsules[j].End1_radius = capsules[j].End1_radius;

                    capsules[j].End1_radiuspwr2 = capsules[j].End1_radius * capsules[j].End1_radius;

                    capsules[j].End2_offset = capsules[j].End2_offset;

                    capsules[j].End2_radius = capsules[j].End2_radius;

                    capsules[j].End2_radiuspwr2 = capsules[j].End2_radius * capsules[j].End2_radius;
                }
            }
        });
    return capsules;
}

void Thing::UpdateConfig(configEntry_t& centry)
{
    stiffness = centry["stiffness"];
    stiffness2 = centry["stiffness2"];
    damping = centry["damping"];

    maxOffsetX = centry["maxoffsetX"];
    maxOffsetY = centry["maxoffsetY"];
    maxOffsetZ = centry["maxoffsetZ"];

    linearX = centry["linearX"];
    linearY = centry["linearY"];
    linearZ = centry["linearZ"];

    rotationalX = centry["rotationalX"];
    rotationalY = centry["rotationalY"];
    rotationalZ = centry["rotationalZ"];

    rotateLinearX = centry["rotateLinearX"];
    rotateLinearY = centry["rotateLinearY"];
    rotateLinearZ = centry["rotateLinearZ"];

    rotateRotationX = centry["rotateRotationX"];
    rotateRotationY = centry["rotateRotationY"];
    rotateRotationZ = centry["rotateRotationZ"];

    timeTick = centry["timetick"];

    if (centry.find("timeStep") != centry.end())
        timeStep = centry["timeStep"];
    else
        timeStep = 0.016f;

    gravityBias = centry["gravityBias"];
    gravityCorrection = centry["gravityCorrection"];
    cogOffsetY = centry["cogOffsetY"];
    cogOffsetX = centry["cogOffsetX"];
    cogOffsetZ = centry["cogOffsetZ"];
    if (timeTick <= 1)
        timeTick = 1;

    absRotX = centry["absRotX"] != 0.0;

    linearSpreadforceXtoY = centry["linearSpreadforceXtoY"];
    linearSpreadforceXtoZ = centry["linearSpreadforceXtoZ"];
    linearSpreadforceYtoX = centry["linearSpreadforceYtoX"];
    linearSpreadforceYtoZ = centry["linearSpreadforceYtoZ"];
    linearSpreadforceZtoX = centry["linearSpreadforceZtoX"];
    linearSpreadforceZtoY = centry["linearSpreadforceZtoY"];

    gravitySupineX = centry["gravitySupineX"];
    gravitySupineY = centry["gravitySupineY"];
    gravitySupineZ = centry["gravitySupineZ"];

    collisionFriction = centry["collisionFriction"];
    collisionPenetration = centry["collisionPenetration"];
    collisionMultipler = centry["collisionMultipler"];
    collisionMultiplerRot = centry["collisionMultiplerRot"];

    collisionElastic = centry["collisionElastic"] != 0.0;
    collisionElasticConstraints = centry["collisionElasticConstraints"] != 0.0;

    CollisionConfig.IsElasticCollision = collisionElastic;

    //CollisionConfig.CollisionMaxOffset = NiPoint3(collisionXmaxOffset, collisionYmaxOffset, collisionZmaxOffset);
    //CollisionConfig.CollisionMinOffset = NiPoint3(collisionXminOffset, collisionYminOffset, collisionZminOffset);
}

void Thing::Reset(Actor* actor)
{
    auto loadedState = actor->unkF0;
    if (!loadedState || !loadedState->rootNode)
    {
        logger.Error("No loaded state for actor %08x\n", actor->formID);
        return;
    }
    auto obj = loadedState->rootNode->GetObjectByName(&boneName);

    if (!obj)
    {
        logger.Error("Couldn't get name for loaded state for actor %08x\n", actor->formID);
        return;
    }

    obj->m_localTransform.pos = origLocalPos[boneName.c_str()][actor->formID];
    obj->m_localTransform.rot = origLocalRot[boneName.c_str()][actor->formID];
}

// Returns 
template <typename T> int sgn(T val)
{
    return (T(0) < val) - (val < T(0));
}

NiAVObject* Thing::IsThingActorValid(Actor* actor)
{
    if (!actorUtils::IsActorValid(actor))
    {
        //logger.Error("%s: No valid actor in Thing::Update\n", __func__);
        return NULL;
    }
    auto loadedState = actor->unkF0;
    if (!loadedState || !loadedState->rootNode)
    {
        //logger.Error("%s: No loaded state for actor %08x\n", __func__, actor->formID);
        return NULL;
    }
    auto obj = loadedState->rootNode->GetObjectByName(&boneName);

    if (!obj)
    {
        //logger.Error("%s: Couldn't get name for loaded state for actor %08x\n", __func__, actor->formID);
        return NULL;
    }

    if (!obj->m_parent)
    {
        //logger.Error("%s: Couldn't get bone %s parent for actor %08x\n", __func__, boneName.c_str(), actor->formID);
        return NULL;
    }

    return obj;
}

void Thing::UpdateThing(Actor* actor)
{
    bool collisionsOn = true;
    //float friction = collisionOnLastFrame ? collisionFriction : 1.0f;

    if (skipFramesCount > 0)
    {
        skipFramesCount--;
        collisionsOn = false;
        if (collisionOnLastFrame)
        {
            return;
        }
    }
    else
    {
        skipFramesCount = collisionSkipFrames;
        collisionOnLastFrame = false;
    }

    // Check for valid actor
    thingObj = IsThingActorValid(actor);

    if (!thingObj)
    {
        return;
    }

    auto newTime = clock();
    auto deltaT = newTime - time;

    time = newTime;
    if (deltaT > 64) deltaT = 64;
    if (deltaT < 8) deltaT = 8;

    bool IsThereCollision = false;
    bool maybeNot = false;
    NiPoint3 collisionDiff = NiPoint3(0, 0, 0);
    long originalDeltaT = deltaT;
    NiPoint3 collisionVector = NiPoint3(0, 0, 0);

    float varCogOffsetX = cogOffsetX;
    float varCogOffsetY = cogOffsetY;
    float varCogOffsetZ = cogOffsetZ;
    float varGravityCorrection = gravityCorrection;
    float varGravityBias = gravityBias;

    auto forceMultiplier = 1.0f;

    int collisionCheckCount = 0;

    StoreOriginalTransforms(actor);

#if TRANSFORM_DEBUG
    auto sceneObj = thingObj;
    while (sceneObj->m_parent && sceneObj->m_name != "skeleton.nif")
    {
        logger.Info(sceneObj->m_name);
        logger.Info("\n---\n");
        logger.Error("Actual m_localTransform.pos: ");
        ShowPos(sceneObj->m_localTransform.pos);
        logger.Error("Actual m_worldTransform.pos: ");
        ShowPos(sceneObj->m_worldTransform.pos);
        logger.Info("---\n");
        //logger.Error("Actual m_localTransform.rot Matrix:\n");
        ShowRot(sceneObj->m_localTransform.rot);
        //logger.Error("Actual m_worldTransform.rot Matrix:\n");
        ShowRot(sceneObj->m_worldTransform.rot);
        logger.Info("---\n");
        //if (sceneObj->m_parent) {
        //	logger.Error("Calculated m_worldTransform.pos: ");
        //	ShowPos((sceneObj->m_parent->m_worldTransform.rot.Transpose() * sceneObj->m_localTransform.pos) + sceneObj->m_parent->m_worldTransform.pos);
        //	logger.Error("Calculated m_worldTransform.rot Matrix:\n");
        //	ShowRot(sceneObj->m_localTransform.rot * sceneObj->m_parent->m_worldTransform.rot);
        //}
        sceneObj = sceneObj->m_parent;
}
#endif

    auto skeletonObj = actorUtils::GetBaseSkeleton(actor);
    if (skeletonObj == NULL)
    {
        logger.Error("%s: Didn't find thing %s's base skeleton.nif for actor %08x \n", __func__, boneName.c_str(), actor->formID);
        return;
    }

    const NiMatrix43 TransformWorldToSkel = skeletonObj->m_localTransform.rot;
    const NiMatrix43 TransformSkelToWorld = skeletonObj->m_localTransform.rot.Transpose();
    const NiMatrix43 TransformWorldToLocal = thingObj->m_parent->m_worldTransform.rot;
    const NiMatrix43 TransformLocalToWorld = thingObj->m_parent->m_worldTransform.rot.Transpose();

#if DEBUG
    logger.Error("bone %s for actor %08x with parent %s\n", boneName.c_str(), actor->formID, skeletonObj->m_name.c_str());
    //    ShowRot(skeletonObj->m_worldTransform.rot);
    //    //ShowPos(TransformLocalToWorld * thingObj->m_localTransform.pos);
#endif

    NiPoint3 origWorldPos = (TransformLocalToWorld * origLocalPos[boneName.c_str()][actor->formID]) + thingObj->m_parent->m_worldTransform.pos;
    //NiPoint3 origWorldPosRot = (TransformLocalToWorld * origLocalPos[boneName.c_str()][actor->formID]) + thingObj->m_parent->m_worldTransform.pos;
    float nodeParentInvScale = 1.0f;// / thingObj->m_parent->m_worldTransform.scale;

    // Cog offset is offset to move Center of Mass make rotational motion more significant
    NiPoint3 worldTarget = (TransformSkelToWorld * NiPoint3(cogOffsetX, cogOffsetY, cogOffsetZ)) + origWorldPos;

    // worldDiff is Difference in position between old and new world position
    NiPoint3 worldDiff = (worldTarget - oldWorldPos) * forceMultiplier;

    if (fabs(worldDiff.x) > DIFF_LIMIT || fabs(worldDiff.y) > DIFF_LIMIT || fabs(worldDiff.z) > DIFF_LIMIT)
    {
        logger.Error("%s: bone %s transform reset for actor %x\n", __func__, boneName.c_str(), actor->formID);
        thingObj->m_localTransform.pos = origLocalPos[boneName.c_str()][actor->formID];
        thingObj->m_localTransform.rot = origLocalRot[boneName.c_str()][actor->formID];

        oldWorldPos = worldTarget;
        oldWorldPosRot = origWorldPos;

        velocity = NiPoint3(0, 0, 0);
        time = clock();
        return;
    }

    NiPoint3 newWorldPos = oldWorldPos;
    NiPoint3 posDelta = NiPoint3(0, 0, 0);


    // Rotation for transforming gravityBias back to world coordinates
    auto newRotation = TransformWorldToLocal * origWorldRot.Transpose();

    // move the bones based on the supplied weightings
    // Convert the world translations into local coordinates

    NiMatrix43 rotateLinear = zeroMatrix;
    rotateLinear.SetEulerAngles(rotateLinearX * DEG_TO_RAD, rotateLinearY * DEG_TO_RAD, rotateLinearZ * DEG_TO_RAD);

    auto timeMultiplier = timeTick / (float)deltaT;

    worldDiff = (worldDiff * timeMultiplier);

    // Compute the "Spring" Force

    NiPoint3 diff2(worldDiff.x * worldDiff.x * sgn(worldDiff.x),
        worldDiff.y * worldDiff.y * sgn(worldDiff.y),
        worldDiff.z * worldDiff.z * sgn(worldDiff.z));

    NiPoint3 force = (worldDiff * stiffness) + (diff2 * stiffness2) - (newRotation * TransformSkelToWorld * NiPoint3(0, 0, gravityBias));

#if DEBUG
    logger.Error("Diff2: ");
    ShowPos(diff2);
    logger.Error("Force with stiffness %f, stiffness2 %f, gravity bias %f: ", stiffness, stiffness2, varGravityBias);
    ShowPos(force);
#endif

    do
    {
        // Assume mass is 1, so Accelleration is Force, can vary mass by changinf force
        //velocity = (velocity + (force * timeStep)) * (1 - (damping * timeStep));
        velocity = (velocity + (force * timeStep)) - (velocity * (damping * timeStep));

        // New position accounting for time
        posDelta += (velocity * timeStep);
        deltaT -= timeTick;
    } while (deltaT >= timeTick);

    velocity = velocity;

    //newWorldPos = oldWorldPos + posDelta;
    //NiPoint3 newPosRot = origWorldPosRot;

    oldWorldPos = worldDiff + worldTarget;

#if DEBUG
    logger.Error("posDelta: ");
    ShowPos(posDelta);
    logger.Error("newWorldPos: ");
    ShowPos(newWorldPos);
#endif
    // clamp the difference to stop the breast severely lagging at low framerates
    worldDiff = newWorldPos - worldTarget;

    worldDiff.x = clamp(worldDiff.x, -maxOffsetX, maxOffsetX);
    worldDiff.y = clamp(worldDiff.y, -maxOffsetY, maxOffsetY);
    worldDiff.z = clamp(worldDiff.z, -maxOffsetZ, maxOffsetZ);

#if DEBUG
    logger.Error("worldDiff from newWorldPos: ");
    ShowPos(worldDiff);
    //logger.Error("oldWorldPos: ");
    //ShowPos(oldWorldPos);
#endif

    // Transform worldDiff to skeleton space for config'd calculations
    auto skelDiff = TransformWorldToSkel * worldDiff;

    auto scaleMultiplier = 1.0;
    skelDiff.x *= scaleMultiplier;
    skelDiff.y *= scaleMultiplier;
    skelDiff.z *= scaleMultiplier;

    auto beforeLocalDiff = skelDiff;

    // Clamp against settings (which are in skeleton space)
    skelDiff.x = clamp(skelDiff.x, -maxOffsetX, maxOffsetX);
    skelDiff.y = clamp(skelDiff.y, -maxOffsetY, maxOffsetY);
    skelDiff.z = clamp(skelDiff.z, -maxOffsetZ, maxOffsetZ);

    beforeLocalDiff = beforeLocalDiff - skelDiff;

    skelDiff.x += (beforeLocalDiff.y * linearSpreadforceXtoY) + (beforeLocalDiff.z * linearSpreadforceXtoZ);
    skelDiff.y += (beforeLocalDiff.x * linearSpreadforceYtoX) + (beforeLocalDiff.z * linearSpreadforceYtoZ);
    skelDiff.z += (beforeLocalDiff.x * linearSpreadforceZtoX) + (beforeLocalDiff.y * linearSpreadforceZtoY);

    // Clamp against settings (which are in skeleton space)
    skelDiff.x = clamp(skelDiff.x, -maxOffsetX, maxOffsetX);
    skelDiff.y = clamp(skelDiff.y, -maxOffsetY, maxOffsetY);
    skelDiff.z = clamp(skelDiff.z, -maxOffsetZ, maxOffsetZ);

    skelDiff.x *= linearX;
    skelDiff.y *= linearY;
    skelDiff.z *= linearZ;

    // Store a copy of skelDiff for later for transforming rotation motions
    auto rotDiff = skelDiff;

    auto varGravitySupine = CalculateGravitySupine(actor);

    // Transform skelDiff to world coordinates
    skelDiff = TransformSkelToWorld * skelDiff;

    newWorldPos = skelDiff;

    newWorldPos.x += varGravitySupine.x * linearX;
    newWorldPos.y += varGravitySupine.y * linearY;
    newWorldPos.z += varGravitySupine.z * linearZ;

    oldWorldPos = worldDiff + worldTarget;

    // Create the rotated world space transformation matrix
    NiMatrix43 rotatedTransformWorldToLocal = rotateLinear * newRotation.Transpose() * TransformWorldToLocal;

    // Transform skelDiff to a settings-rotated local space
    //newWorldPos = rotatedInvWorldTrans * newWorldPos;

    auto newLocalPos = origLocalPos[boneName.c_str()][actor->formID] + (rotatedTransformWorldToLocal * newWorldPos);

    // Apply gravityCorrection, which will always point downward
    // newLocalPos += rotateLinear * TransformWorldToLocal * skeletonObj->m_localTransform.rot.Transpose() * NiPoint3(0, 0, gravityCorrection * linearZ);

    //if (ContainsNoCase(std::string(boneName.c_str()), "Breast_CBP_R_02") ||
    //    ContainsNoCase(std::string(boneName.c_str()), "Breast_CBP_L_02")
    //    )
    //{
    //    //logger.Error("skeletonObj->m_localTransform.rot.Transpose()\n");
    //    //ShowRot(skeletonObj->m_localTransform.rot.Transpose());
    //    //logger.Error("rotatedInvWorldTrans\n");
    //    //ShowRot(rotatedInvWorldTrans);
    //    logger.Error("newWorldPos: ");
    //    ShowPos(newWorldPos);
    //    logger.Error("newLocalPos: ");
    //    ShowPos(newLocalPos);
    //}

#if DEBUG
    logger.Error("rotatedInvWorldTrans x=10 Transformation:");
    ShowPos(rotatedInvWorldTrans * NiPoint3(10, 0, 0));
    logger.Error("rotatedInvWorldTrans y=10 Transformation:");
    ShowPos(rotatedInvWorldTrans * NiPoint3(0, 10, 0));
    logger.Error("rotatedInvWorldTrans z=10 Transformation:");
    ShowPos(rotatedInvWorldTrans * NiPoint3(0, 0, 10));
    logger.Error("oldWorldPos: ");
    ShowPos(oldWorldPos);
    logger.Error("localTransform.pos: ");
    ShowPos(thingObj->m_localTransform.pos);
    logger.Error("rotDiff: ");
    ShowPos(rotDiff);

#endif

    //thingObj->m_localTransform.pos = newLocalPos;

    // Calculate rotational motion
    if (absRotX) rotDiff.x = fabs(rotDiff.x);

    // Rotational motion scale
    rotDiff.x *= rotationalX;
    rotDiff.y *= rotationalY;
    rotDiff.z *= rotationalZ;


#if DEBUG
    logger.Error("localTransform.pos after: ");
    ShowPos(thingObj->m_localTransform.pos);
    logger.Error("origLocalPos:");
    ShowPos(origLocalPos[boneName.c_str()][actor->formID]);
    logger.Error("origLocalRot:");
    ShowRot(origLocalRot[boneName.c_str()][actor->formID]);

#endif
    // Rotate rotation according to settings.
    NiMatrix43 rotateRotation = zeroMatrix;
    rotateRotation.SetEulerAngles(rotateRotationX * DEG_TO_RAD,
        rotateRotationY * DEG_TO_RAD,
        rotateRotationZ * DEG_TO_RAD);

    NiMatrix43 standardRot = zeroMatrix;

    rotDiff = rotateRotation * rotDiff;
    standardRot.SetEulerAngles(rotDiff.x, rotDiff.y, rotDiff.z);
    // Calculate the new local rot as an offset from the original local rot
    //thingObj->m_localTransform.rot = standardRot * origLocalRot[boneName.c_str()][actor->formID];

#if DEBUG
    logger.Error("end update()\n");
#endif

    ///#### physics calculate done
    ///#### collision calculate start

    NiPoint3 worldDiffColl = NiPoint3(0, 0, 0);
    NiPoint3 worldDiffGroundColl = NiPoint3(0, 0, 0);
    NiPoint3 maybeWorldDiffColl = NiPoint3(0, 0, 0);

    if (collisionsOn && ActorCollisionsEnabled)
    {
        std::vector<int> thingIdList;
        std::vector<int> hashIdList;
        NiPoint3 GroundCollisionVector = emptyPoint;
        auto actorBaseScale = 1.0f;
        auto nodeScale = 1.0f;

        //The rotation of the previous frame due to collisions should not be used.
        // Calculate the new rot and transform it to world space
        NiMatrix43 objRotation = TransformLocalToWorld * (standardRot * origLocalRot[boneName.c_str()][actor->formID]);

        //LOG("Before Maybe Collision Stuff Start");
        auto maybeldiff = newLocalPos;
        //maybeldiff.x = maybeldiff.x * linearX;
        //maybeldiff.y = maybeldiff.y * linearY;
        //maybeldiff.z = maybeldiff.z * linearZ;

        //(TransformLocalToWorld * origLocalPos[boneName.c_str()][actor->formID]) + thingObj->m_parent->m_worldTransform.pos
        //NiAVObject * playerObj = (*g_player)->unkF0->rootNode;

        // MaybePos = worldTarget + worldDiff + worldPos
        // MaybePos is in world coordinate space
        NiPoint3 maybePos = worldTarget + (TransformLocalToWorld * (maybeldiff + (origLocalPos[boneName.c_str()][actor->formID] * nodeScale))); //add missing local pos

        float colliderNodescale = nodeScale / actorBaseScale; //Calibrate the scale gap between collider and actual mesh caused by bone weight

        //After cbp movement collision detection
        for (int i = 0; i < thingCollisionSpheres.size(); i++)
        {
            //thingCollisionSpheres[i].offset = thingCollisionSpheres[i].offset * actorBaseScale * colliderNodescale;
            thingCollisionSpheres[i].worldPos = maybePos + (objRotation * thingCollisionSpheres[i].offset);
            //thingCollisionSpheres[i].radius = thingCollisionSpheres[i].radius * actorBaseScale * colliderNodescale;
            thingCollisionSpheres[i].radiuspwr2 = thingCollisionSpheres[i].radius * thingCollisionSpheres[i].radius;
            hashIdList = GetHashIdsFromPos(thingCollisionSpheres[i].worldPos, thingCollisionSpheres[i].radius);
            for (int m = 0; m < hashIdList.size(); m++)
            {
                if (!(std::find(thingIdList.begin(), thingIdList.end(), hashIdList[m]) != thingIdList.end()))
                {
                    thingIdList.emplace_back(hashIdList[m]);
                }
            }
        }
        for (int i = 0; i < thingCollisionCapsules.size(); i++)
        {
            //thingCollisionCapsules[i].End1_offset = thingCollisionCapsules[i].End1_offset * actorBaseScale * colliderNodescale;
            thingCollisionCapsules[i].End1_worldPos = maybePos + (objRotation * thingCollisionCapsules[i].End1_offset);
            //thingCollisionCapsules[i].End1_radius = thingCollisionCapsules[i].End1_radius * actorBaseScale * colliderNodescale;
            thingCollisionCapsules[i].End1_radiuspwr2 = thingCollisionCapsules[i].End1_radius * thingCollisionCapsules[i].End1_radius;
            //thingCollisionCapsules[i].End2_offset = thingCollisionCapsules[i].End2_offset * actorBaseScale * colliderNodescale;
            thingCollisionCapsules[i].End2_worldPos = maybePos + (objRotation * thingCollisionCapsules[i].End2_offset);
            //thingCollisionCapsules[i].End2_radius = thingCollisionCapsules[i].End2_radius * actorBaseScale * colliderNodescale;
            thingCollisionCapsules[i].End2_radiuspwr2 = thingCollisionCapsules[i].End2_radius * thingCollisionCapsules[i].End2_radius;
            hashIdList = GetHashIdsFromPos((thingCollisionCapsules[i].End2_worldPos + thingCollisionCapsules[i].End2_worldPos) * 0.5f
                , (thingCollisionCapsules[i].End1_radius + thingCollisionCapsules[i].End2_radius) * 0.5f);
            for (int m = 0; m < hashIdList.size(); m++)
            {
                if (!(std::find(thingIdList.begin(), thingIdList.end(), hashIdList[m]) != thingIdList.end()))
                {
                    thingIdList.emplace_back(hashIdList[m]);
                }
            }
        }

        //Prevent normal movement to cause collision (This prevents shakes)			
        collisionVector = emptyPoint;
        NiPoint3 lastcollisionVector = emptyPoint;
        CollisionConfig.maybePos = maybePos;
        CollisionConfig.origTransLocalToWorld = TransformLocalToWorld;
        CollisionConfig.objRot = objRotation;
        CollisionConfig.origTransWorldToLocal = TransformWorldToLocal;

        // Find collisions
        for (int j = 0; j < thingIdList.size(); j++)
        {
            int id = thingIdList[j];
            //LOG_INFO("Thing hashId=%d", id);
            if (partitions.find(id) != partitions.end())
            {
                for (int i = 0; i < partitions[id].partitionCollisions.size(); i++)
                {
                    if (IgnoreAllSelfColliders && partitions[id].partitionCollisions[i].colliderActor == actor)
                    {
                        //LOG("Ignoring collision between %s and %s because IgnoreAllSelfColliders", partitions[id].partitionCollisions[i].colliderNodeName.c_str(), boneName.data);
                        continue;
                    }
                    if (partitions[id].partitionCollisions[i].colliderActor == actor && std::find(IgnoredSelfCollidersList.begin(), IgnoredSelfCollidersList.end(), partitions[id].partitionCollisions[i].colliderNodeName) != IgnoredSelfCollidersList.end())
                    {
                        //LOG("Ignoring collision between %s and %s because IgnoredSelfCollidersList", partitions[id].partitionCollisions[i].colliderNodeName.c_str(), boneName.data);
                        continue;
                    }

                    if (std::find(IgnoredCollidersList.begin(), IgnoredCollidersList.end(), partitions[id].partitionCollisions[i].colliderNodeName) != IgnoredCollidersList.end())
                    {
                        //LOG("Ignoring collision between %s and %s because IgnoredCollidersList", partitions[id].partitionCollisions[i].colliderNodeName.c_str(), boneName.data);
                        continue;
                    }

                    //Actor's own same obj is ignored, of course
                    if (partitions[id].partitionCollisions[i].colliderActor == actor && std::strcmp(partitions[id].partitionCollisions[i].colliderNodeName.c_str(), boneName.c_str()) == 0)
                        continue;

                    //if (debugtimelog || logging)
                    //    InterlockedIncrement(&callCount);

                    bool colliding = false;
                    
                    colliding = partitions[id].partitionCollisions[i].CheckCollision(collisionVector, thingCollisionSpheres, thingCollisionCapsules, CollisionConfig, false);
                    if (colliding)
                    {
                        maybeNot = true;

                        std::string actorNodeString = GetActorNodeString(actor, boneName);

                        //if (std::find(PlayerCollisionEventNodes.begin(), PlayerCollisionEventNodes.end(), partitions[id].partitionCollisions[i].colliderNodeName) != PlayerCollisionEventNodes.end())
                        //{
                        //    ActorNodePlayerCollisionEventMap[actorNodeString].collisionInThisCycle = true;
                        //}
                    }

                    collisionCheckCount++;
                }
            }
        }

        //ground collision	
/* 		if (GroundCollisionEnabled)
        {
            float bottomPos = groundPos;
            float bottomRadius = 0.0f;

            for (int l = 0; l < thingCollisionSpheres.size(); l++)
            {
                if (thingCollisionSpheres[l].worldPos.z - thingCollisionSpheres[l].radius100 < bottomPos - bottomRadius)
                {
                    bottomPos = thingCollisionSpheres[l].worldPos.z;
                    bottomRadius = thingCollisionSpheres[l].radius100;
                }
            }

            for (int m = 0; m < thingCollisionCapsules.size(); m++)
            {

                if (thingCollisionCapsules[m].End1_worldPos.z - thingCollisionCapsules[m].End1_radius100 < thingCollisionCapsules[m].End2_worldPos.z - thingCollisionCapsules[m].End2_radius100)
                {
                    if (thingCollisionCapsules[m].End1_worldPos.z - thingCollisionCapsules[m].End1_radius100 < bottomPos - bottomRadius)
                    {
                        bottomPos = thingCollisionCapsules[m].End1_worldPos.z;
                        bottomRadius = thingCollisionCapsules[m].End1_radius100;
                    }
                }
                else
                {
                    if (thingCollisionCapsules[m].End2_worldPos.z - thingCollisionCapsules[m].End2_radius100 < bottomPos - bottomRadius)
                    {
                        bottomPos = thingCollisionCapsules[m].End2_worldPos.z;
                        bottomRadius = thingCollisionCapsules[m].End2_radius100;
                    }
                }
            }

            if (bottomPos - bottomRadius < groundPos)
            {
                maybeNot = true;

                float Scalar = groundPos - (bottomPos - bottomRadius);

                //it can allow only force up to the radius for doesn't get crushed by the ground
                if (Scalar > bottomRadius)
                    Scalar = 0;

                GroundCollisionVector = NiPoint3(0, 0, Scalar);
            }
        } */

        // Collision found
        if (maybeNot)
        {
            collisionOnLastFrame = true;

            worldDiffColl = collisionVector * collisionPenetration;
            //localDiffGroundCol = TransformWorldToLocal * (GroundCollisionVector * collisionPenetration);

            //Add more collision force for weak bone weights but virtually for maintain collision by obj position
            //For example, if a obj has a bone weight value of about 0.1, that shape seems actually moves by 0.1 even if the obj moves by 1
            //However, simply applying the multipler then changes the actual obj position,so that's making the collisions out of sync
            //Therefore to make perfect collision
            //it seems to be pushed out as much as colliding to the naked eye, but the actual position of the colliding obj must be maintained original position
            maybeWorldDiffColl = (worldDiffColl + worldDiffGroundColl) * collisionMultipler / thingObj->m_parent->m_worldTransform.scale;

            //add collision vector buffer of one frame to some reduce jitter and add softness by collision
            //be particularly useful for both nodes colliding that defined in both affected and collider nodes
            auto maybeWorldDiffCollAdjust = maybeWorldDiffColl;
            maybeWorldDiffColl = (maybeWorldDiffColl + collisionBuffer) * 0.5;
            collisionBuffer = maybeWorldDiffCollAdjust;

            //set to collision sync for the obj that has both affected obj and collider obj
            collisionSync = worldDiffColl + worldDiffGroundColl - maybeWorldDiffColl;

            //auto rcoldiffXnew = (localDiffCol + localDiffGroundCol) * collisionMultiplerRot * varRotationalXnew;
            //auto rcoldiffYnew = (localDiffCol + localDiffGroundCol) * collisionMultiplerRot * varRotationalYnew;
            //auto rcoldiffZnew = (localDiffCol + localDiffGroundCol) * collisionMultiplerRot * varRotationalZnew;

            //rcoldiffXnew.x *= linearXrotationX;
            //rcoldiffXnew.y *= linearYrotationX;
            //rcoldiffXnew.z *= linearZrotationX; //1

            //rcoldiffYnew.x *= linearXrotationY; //1
            //rcoldiffYnew.y *= linearYrotationY;
            //rcoldiffYnew.z *= linearZrotationY;

            //rcoldiffZnew.x *= linearXrotationZ;
            //rcoldiffZnew.y *= linearYrotationZ; //1
            //rcoldiffZnew.z *= linearZrotationZ;

            //NiMatrix43 newcolRot;
            //newcolRot.SetEulerAngles(rcoldiffYnew.x + rcoldiffYnew.y + rcoldiffYnew.z, rcoldiffZnew.x + rcoldiffZnew.y + rcoldiffZnew.z, rcoldiffXnew.x + rcoldiffXnew.y + rcoldiffXnew.z);

            ////newRot = newRot * newcolRot;
        }
        else
        {
            collisionSync = emptyPoint;
        }
        ///#### collision calculate done

        //LOG("After Maybe Collision Stuff End");
    }
    //the collision accuracy is now almost perfect except for the rotation
    //well, I don't have an idea to be performance-friendly about the accuracy of collisions rotation
    //

        //logger.error("set positions\n");

    //If put the result of collision into the next frame, the quality of collision and movement will improve
    //but if that part is almost exclusively for collisions like vagina, it's better not to reflect the result of collision into physics
    //### To be free from unstable FPS, have to remove the varGravityCorrection from the next frame
    if (collisionElastic && maybeNot)
    {
        oldWorldPos = worldDiff + worldTarget + worldDiffColl + worldDiffGroundColl + worldTarget;
        //oldWorldPosRot = (TransformLocalToWorld * (ldiffRot + (ldiffcol + ldiffGcol) * collisionMultiplerRot)) + target - NiPoint3(0, 0, varGravityCorrection);
        //for update oldWorldPos&Rot when frame gap
        //oldLocalPos = ldiff + ldiffcol + ldiffGcol - (invRot * NiPoint3(0, 0, varGravityCorrection));
        //oldLocalPosRot = (ldiffRot + (ldiffcol + ldiffGcol) * collisionMultiplerRot) - (invRot * NiPoint3(0, 0, varGravityCorrection));

        if (collisionElasticConstraints)
        {
            collisionInertia += worldDiffColl + worldDiffGroundColl;
            collisionInertiaRot += ((worldDiffColl + worldDiffGroundColl) * collisionMultiplerRot);

            multiplerInertia = 1.0f;
            multiplerInertiaRot = 1.0f;
        }
    }
    else
    {
        oldWorldPos = worldDiff + worldTarget;
        //oldWorldPosRot = (TransLocalToWorld * ldiffRot) + target - NiPoint3(0, 0, varGravityCorrection);

        //for update oldWorldPos&Rot when frame gap
        //oldLocalPos = ldiff - (invRot * NiPoint3(0, 0, varGravityCorrection));
        //oldLocalPosRot = ldiffRot - (invRot * NiPoint3(0, 0, varGravityCorrection));
    }

    thingObj->m_localTransform.pos.x = newLocalPos.x + (maybeWorldDiffColl.x) * nodeParentInvScale;
    thingObj->m_localTransform.pos.y = newLocalPos.y + (maybeWorldDiffColl.y) * nodeParentInvScale;
    thingObj->m_localTransform.pos.z = newLocalPos.z + (maybeWorldDiffColl.z) * nodeParentInvScale;

    thingObj->m_localTransform.rot = standardRot * origLocalRot[boneName.c_str()][actor->formID];

    //RefreshNode(obj);

    //logger.Error("end update()\n");
    /*QueryPerformanceCounter(&endingTime);
    elapsedMicroseconds.QuadPart = endingTime.QuadPart - startingTime.QuadPart;
    elapsedMicroseconds.QuadPart *= 1000000000LL;
    elapsedMicroseconds.QuadPart /= frequency.QuadPart;
    _MESSAGE("Thing.update() Update Time = %lld ns\n", elapsedMicroseconds.QuadPart);*/

}