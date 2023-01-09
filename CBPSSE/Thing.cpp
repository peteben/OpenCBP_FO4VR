#include "f4se\NiNodes.h"
#include <time.h>

#include "ActorUtils.h"
#include "log.h"
#include "Thing.h"
#include "Utility.hpp"

constexpr auto DEG_TO_RAD = 3.14159265 / 180;
const char* skeletonNif_boneName = "skeleton.nif";
const float DIFF_LIMIT = 100.0;

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

    //logger.Error("obj->m_worldTransform.rot.Transpose():");
    //ShowRot(obj->m_worldTransform.rot.Transpose());
    //logger.Error("obj->m_localTransform.rot:");
    //ShowRot(obj->m_localTransform.rot);
    origWorldRot = obj->m_localTransform.rot.Transpose() * obj->m_worldTransform.rot;

    time = clock();

	float nodescale = 1.0f;
	//if (actor)
	//{
	//	if (actor->unkF0 && actor->unkF0->rootNode)
	//	{
	//		NiAVObject* obj = actor->unkF0->rootNode->GetObjectByName(&name);
	//		if (obj)
	//		{
	//			nodescale = obj->m_worldTransform.scale;
	//		}
	//	}
	//}	

	thingCollisionSpheres = CreateThingCollisionSpheres(actor, std::string(name.c_str()), nodescale);

	skipFramesCount = collisionSkipFrames;
    IsBreastBone = ContainsNoCase(std::string(boneName.c_str()), "Breast");
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
                auto actorRotMap = origChestWorldRot.at(boneName.c_str());
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
        auto actorPosMap = origLocalPos.at(boneName.c_str());
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
        auto actorRotMap = origLocalRot.at(boneName.c_str());
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
std::vector<Sphere> Thing::CreateThingCollisionSpheres(Actor * actor, std::string nodeName, float nodescale)
{
	auto actorRef = DYNAMIC_CAST(actor, Actor, TESObjectREFR);

	std::vector<ConfigLine>* affectedNodesListPtr;

	const char * actorrefname = "";

	if (actor->formID == 0x14) //If Player
	{
		actorrefname = "Player";
	}
	else
	{
		actorrefname = CALL_MEMBER_FN(actorRef, GetReferenceName)();
	}

	affectedNodesListPtr = &AffectedNodesList;


	std::vector<Sphere> spheres;

	for (int i = 0; i < affectedNodesListPtr->size(); i++)
	{
		if (affectedNodesListPtr->at(i).NodeName == nodeName)
		{
			spheres = affectedNodesListPtr->at(i).CollisionSpheres;

			for(int j=0; j<spheres.size(); j++)
			{
				spheres[j].offset = spheres[j].offset;

				spheres[j].radius = spheres[j].radius * nodescale;

				spheres[j].radiuspwr2 = spheres[j].radius * spheres[j].radius;
			}
			break;
		}
	}
	return spheres;
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
}

//static float clamp(float val, float min, float max) {
//    if (val < min) return min;
//    else if (val > max) return max;
//    return val;
//}

static float remap(float value, float start1, float stop1, float start2, float stop2)
{
    return start2 + (stop2 - start2) * ((value - start1) / (stop1 - start1));
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
    auto forceMultiplier = 1.0;

    thingObj = IsThingActorValid(actor);
    auto obj = thingObj;

    if (!obj)
    {
        return;
    }

    auto newTime = clock();
    auto deltaT = newTime - time;

    time = newTime;
    if (deltaT > 64) deltaT = 64;
    if (deltaT < 8) deltaT = 8;

	NiMatrix43 objRotation;

    objRotation = obj->m_worldTransform.rot;

	bool IsThereCollision = false;
	NiPoint3 collisionDiff = zeroVector;
	long originalDeltaT = deltaT;
	NiPoint3 collisionVector = zeroVector;

	float varCogOffsetX = cogOffsetX;
    float varCogOffsetY = cogOffsetY;
    float varCogOffsetZ = cogOffsetZ;
	float varGravityCorrection = gravityCorrection;
	float varGravityBias = gravityBias;

#if TRANSFORM_DEBUG
    auto sceneObj = obj;
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

    StoreOriginalTransforms(actor);

    auto skeletonObj = actorUtils::GetBaseSkeleton(actor);
    if (skeletonObj == NULL)
    {
        logger.Error("%s: Didn't find thing %s's base skeleton.nif for actor %08x \n", __func__, boneName.c_str(), actor->formID);
        return;
    }

	std::vector<long> thingIdList;
	std::vector<long> hashIdList;
    //logger.Info("Collisions on: %d, hashSize: %d\n", collisionsOn, hashSize);
	if (collisionsOn && hashSize>0)
	{
		//logger.Info("Before Collision Stuff Start\n");
		// Collision Stuff Start
		for (int i = 0; i < thingCollisionSpheres.size(); i++)
		{
			thingCollisionSpheres[i].worldPos = obj->m_worldTransform.pos + (objRotation*thingCollisionSpheres[i].offset);
			hashIdList = GetHashIdsFromPos(thingCollisionSpheres[i].worldPos, thingCollisionSpheres[i].radius, hashSize);
			for(int m=0; m<hashIdList.size(); m++)
			{
				if (!(std::find(thingIdList.begin(), thingIdList.end(), hashIdList[m]) != thingIdList.end()))
				{
					thingIdList.emplace_back(hashIdList[m]);
				}
			}
		}

		NiPoint3 lastcollisionVector = zeroVector;

		for (int j = 0; j < thingIdList.size(); j++)
		{
			long id = thingIdList[j];
			if (partitions.find(id) != partitions.end())
			{
                for (int i = 0; i < partitions[id].partitionCollisions.size(); i++)
                {
                    // Skip collision with itself?
                    if (partitions[id].partitionCollisions[i].colliderActor == actor && std::strcmp(partitions[id].partitionCollisions[i].colliderNodeName.c_str(), boneName.c_str()) == 0)
                        continue;

                    callCount++;

                    // Check if collision vector has changed
                    if (!CompareNiPoints(lastcollisionVector, collisionVector))
                    {
                        // Build thing's collision spheres for current frame
                        for (auto thingCollisSpheres : thingCollisionSpheres)
                        {
                            thingCollisSpheres.worldPos = obj->m_worldTransform.pos + (objRotation * thingCollisSpheres.offset) + collisionVector;
                        }
                    }
                    lastcollisionVector = collisionVector;

                    bool colliding = false;
                    collisionDiff = partitions[id].partitionCollisions[i].CheckCollision(colliding, thingCollisionSpheres, timeTick, originalDeltaT, maxOffsetX, false);
                    if (colliding) {
                        logger.Info("Collision 1 detected!\n");
                        IsThereCollision = true;
                    }   

                    //velocity = velocity + collisionDiff;
					collisionVector = collisionVector + collisionDiff;
				}
			}
		}
		if (IsThereCollision)
		{
			float timeMultiplier = timeTick / (float)deltaT;

            collisionVector.x *= collisionX / linearX;
            collisionVector.y *= collisionY / linearY;
            collisionVector.z *= collisionZ / linearZ;
			collisionVector *= timeMultiplier;
            collisionVector.x = clamp(collisionVector.x, -maxOffsetX, maxOffsetX);
            collisionVector.y = clamp(collisionVector.y, -maxOffsetY, maxOffsetY);
            collisionVector.z = clamp(collisionVector.z, -maxOffsetZ, maxOffsetZ);
            velocity = collisionVector * timeStep;
        }
		//LOG("After Collision Stuff");
	}

	NiPoint3 newPos = oldWorldPos;
	NiPoint3 posDelta = zeroVector;

#if DEBUG
    logger.Error("bone %s for actor %08x with parent %s\n", boneName.c_str(), actor->formID, skeletonObj->m_name.c_str());
//    ShowRot(skeletonObj->m_worldTransform.rot);
//    //ShowPos(obj->m_parent->m_worldTransform.rot.Transpose() * obj->m_localTransform.pos);
#endif

    NiMatrix43 skelSpaceInvTransform = skeletonObj->m_localTransform.rot.Transpose();
    NiPoint3 origWorldPos = (obj->m_parent->m_worldTransform.rot.Transpose() * origLocalPos[boneName.c_str()][actor->formID]) + obj->m_parent->m_worldTransform.pos;
    NiPoint3 origWorldPosRot = (obj->m_parent->m_worldTransform.rot.Transpose() * origLocalPos[boneName.c_str()][actor->formID]) + obj->m_parent->m_worldTransform.pos;
    //float nodeParentInvScale = 1.0f / obj->m_parent->m_worldTransform.scale;

    // Cog offset is offset to move Center of Mass make rotational motion more significant
    // First transform the cog offset (which is in skeleton space) to world space
    // target is in world space
    NiPoint3 target = (skelSpaceInvTransform * NiPoint3(cogOffsetX, cogOffsetY, cogOffsetZ)) + origWorldPos;

    // move the bones based on the supplied weightings
    // Convert the world translations into local coordinates
    NiMatrix43 rotateLinear;
    rotateLinear.SetEulerAngles(rotateLinearX * DEG_TO_RAD, rotateLinearY * DEG_TO_RAD, rotateLinearZ * DEG_TO_RAD);

#if DEBUG
    logger.Error("World Position: ");
    ShowPos(obj->m_worldTransform.pos);
    logger.Error("oldWorldPos: ");
    ShowPos(oldWorldPos);
    logger.Error("target: ");
    ShowPos(target);
    //logger.Error("Parent World Position difference: ");
    //ShowPos(obj->m_worldTransform.pos - obj->m_parent->m_worldTransform.pos);
    ShowPos(skelSpaceInvTransform * NiPoint3(cogOffsetX, cogOffsetY, cogOffsetZ));
    //logger.Error("Target Rotation:\n");
    //ShowRot(skelSpaceInvTransform);
    logger.Error("cogOffset x Transformation:");
    ShowPos(skelSpaceInvTransform * NiPoint3(cogOffsetX, 0, 0));
    logger.Error("cogOffset y Transformation:");
    ShowPos(skelSpaceInvTransform * NiPoint3(0, cogOffsetY, 0));
    logger.Error("cogOffset z Transformation:");
    ShowPos(skelSpaceInvTransform * NiPoint3(0, 0, cogOffsetZ));
#endif

    if (!IsThereCollision)
    {
        // diff is Difference in position between old and new world position
        // diff is world space
        NiPoint3 diff = (target - oldWorldPos) * forceMultiplier;

        // Move up in for gravity correction
        diff += skelSpaceInvTransform * NiPoint3(0, 0, varGravityCorrection);

        //diff += collisionVector;
#if DEBUG
        logger.Error("Diff after gravity correction %f: ", varGravityCorrection);
        ShowPos(diff);
#endif

        if (fabs(diff.x) > DIFF_LIMIT || fabs(diff.y) > DIFF_LIMIT || fabs(diff.z) > DIFF_LIMIT)
        {
	        logger.Error("%s: bone %s transform reset for actor %x\n", __func__, boneName.c_str(), actor->formID);
	        obj->m_localTransform.pos = origLocalPos[boneName.c_str()][actor->formID];
	        obj->m_localTransform.rot = origLocalRot[boneName.c_str()][actor->formID];

	        oldWorldPos = target;
	        oldWorldPosRot = origWorldPos;

	        velocity = NiPoint3(0, 0, 0);
	        time = clock();
	        return;
	    }

	    // Rotation for transforming gravityBias back to world coordinates
	    auto newRotation = obj->m_parent->m_worldTransform.rot * origWorldRot.Transpose();

        float timeMultiplier = timeTick / (float)deltaT;
	    // Transform to local space
	    //diff = obj->m_parent->m_worldTransform.rot * (diff * timeMultiplier);
	    diff = (diff * timeMultiplier);
        diff *= timeMultiplier;


    NiPoint3 posDelta = NiPoint3(0, 0, 0);

    // Compute the "Spring" Force

    NiPoint3 diff2(diff.x * diff.x * sgn(diff.x),
        diff.y * diff.y * sgn(diff.y),
        diff.z * diff.z * sgn(diff.z));

    NiPoint3 force = (diff * stiffness) + (diff2 * stiffness2) - (newRotation * skelSpaceInvTransform * NiPoint3(0, 0, gravityBias));

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

        if (collisionsOn && hashSize > 0)
        {
            //LOG("Before Maybe Collision Stuff Start");
            NiPoint3 maybePos = newPos + posDelta;

            bool maybeNot = false;

            //After cbp movement collision detection
            thingIdList.clear();
            for (int i = 0; i < thingCollisionSpheres.size(); i++)
            {
                thingCollisionSpheres[i].worldPos = obj->m_worldTransform.pos + (objRotation * thingCollisionSpheres[i].offset) + posDelta;
                hashIdList = GetHashIdsFromPos(thingCollisionSpheres[i].worldPos, thingCollisionSpheres[i].radius, hashSize);
                for (int m = 0; m < hashIdList.size(); m++)
                {
                    if (!(std::find(thingIdList.begin(), thingIdList.end(), hashIdList[m]) != thingIdList.end()))
                    {
                        thingIdList.emplace_back(hashIdList[m]);
                    }
                }
            }
            //Prevent normal movement to cause collision (This prevents shakes)			
            collisionVector = zeroVector;
            NiPoint3 lastcollisionVector = zeroVector;
            for (int j = 0; j < thingIdList.size(); j++)
            {
                long id = thingIdList[j];
                //LOG_INFO("Thing hashId=%d", id);
                if (partitions.find(id) != partitions.end())
                {
                    for (int i = 0; i < partitions[id].partitionCollisions.size(); i++)
                    {
                        if (partitions[id].partitionCollisions[i].colliderActor == actor && std::strcmp(partitions[id].partitionCollisions[i].colliderNodeName.c_str(), boneName.c_str()) == 0)
                            continue;

                        callCount++;

                        if (!CompareNiPoints(lastcollisionVector, collisionVector))
                        {
                            // Build thing's collision spheres for current frame after move changes
                            for (auto thingCollisSpheres : thingCollisionSpheres)
                            {
                                thingCollisSpheres.worldPos = obj->m_worldTransform.pos + (objRotation * thingCollisSpheres.offset) + maybePos + collisionVector;
                            }
                        }
                        lastcollisionVector = collisionVector;

                        bool colliding = false;
                        collisionDiff = partitions[id].partitionCollisions[i].CheckCollision(colliding, thingCollisionSpheres, timeTick, originalDeltaT, maxOffsetX, false);
                        if (colliding)
                        {
                            IsThereCollision = true;
                            logger.Info("Collision 2 detected!\n");

                            maybeNot = true;
                            collisionVector = collisionVector + collisionDiff;
                        }
                    }
                }
            }

            if (!maybeNot) {
                logger.Info("Collision 2 didnt happen!\n");
                newPos = maybePos;
            }
            else
            {
                collisionVector.x *= collisionX / linearX;
                collisionVector.y *= collisionY / linearY;
                collisionVector.z *= collisionZ / linearZ;
                collisionVector *= timeMultiplier;
                collisionVector.x = clamp(collisionVector.x, -maxOffsetX, maxOffsetX);
                collisionVector.y = clamp(collisionVector.y, -maxOffsetY, maxOffsetY);
                collisionVector.z = clamp(collisionVector.z, -maxOffsetZ, maxOffsetZ);
                velocity = collisionVector * timeStep;
                newPos = maybePos + collisionVector;
            }

            //LOG("After Maybe Collision Stuff End");
        }
        else {
            logger.Info("Collision 2 fall through!\n");
            newPos = newPos + posDelta;
        }
    }
	else
	{
        logger.Info("Collision 2 skip from Collision 1!\n");
		newPos = newPos + collisionVector;
		collisionOnLastFrame = true;
	}	
        //oldWorldPos = newPos - target;

#if DEBUG
    logger.Error("posDelta: ");
    ShowPos(posDelta);
    logger.Error("newPos: ");
    ShowPos(newPos);
#endif
    // clamp the difference to stop the breast severely lagging at low framerates
    auto diff = newPos - target;

    oldWorldPos = diff + target;
//velocity = /*obj->m_parent->m_worldTransform.rot.Transpose() * */velocity;

    newPos = oldWorldPos + /*obj->m_parent->m_worldTransform.rot.Transpose() **/ posDelta;
    NiPoint3 newPosRot = origWorldPosRot;

    diff.x = clamp(diff.x, -maxOffsetX, maxOffsetX);
    diff.y = clamp(diff.y, -maxOffsetY, maxOffsetY);
    diff.z = clamp(diff.z, -maxOffsetZ, maxOffsetZ);

#if DEBUG
    logger.Error("diff from newPos: ");
    ShowPos(diff);
    //logger.Error("oldWorldPos: ");
    //ShowPos(oldWorldPos);
#endif

    auto localDiff = diff;

    // Transform localDiff (which is in world space) to skeleton space
    localDiff = skeletonObj->m_localTransform.rot * localDiff;
 
    auto scaleMultiplier = 1.0;
    localDiff.x *= scaleMultiplier;
    localDiff.y *= scaleMultiplier;
    localDiff.z *= scaleMultiplier;

    auto beforeLocalDiff = localDiff;

    // Clamp against settings (which are in skeleton space)
    localDiff.x = clamp(localDiff.x, -maxOffsetX, maxOffsetX);
    localDiff.y = clamp(localDiff.y, -maxOffsetY, maxOffsetY);
    localDiff.z = clamp(localDiff.z, -maxOffsetZ, maxOffsetZ);

    beforeLocalDiff = beforeLocalDiff - localDiff;

    localDiff.x += (beforeLocalDiff.y * linearSpreadforceXtoY) + (beforeLocalDiff.z * linearSpreadforceXtoZ);
    localDiff.y += (beforeLocalDiff.x * linearSpreadforceYtoX) + (beforeLocalDiff.z * linearSpreadforceYtoZ);
    localDiff.z += (beforeLocalDiff.x * linearSpreadforceZtoX) + (beforeLocalDiff.y * linearSpreadforceZtoY);

    // Clamp against settings (which are in skeleton space)
    localDiff.x = clamp(localDiff.x, -maxOffsetX, maxOffsetX);
    localDiff.y = clamp(localDiff.y, -maxOffsetY, maxOffsetY);
    localDiff.z = clamp(localDiff.z, -maxOffsetZ, maxOffsetZ);

    localDiff.x *= linearX;
    localDiff.y *= linearY;
    localDiff.z *= linearZ;

    // Store a copy of localDiff for later for transforming rotation motions
    auto rotDiff = localDiff;

    auto varGravitySupine = CalculateGravitySupine(actor);

    // Transform localDiff to world coordinates
    localDiff = skeletonObj->m_localTransform.rot.Transpose() * localDiff;

    auto newWorldPos = localDiff;

    newWorldPos.x += varGravitySupine.x * linearX;
    newWorldPos.y += varGravitySupine.y * linearY;
    newWorldPos.z += varGravitySupine.z * linearZ;

    oldWorldPos = diff + target;

    // Rotation for transforming gravityBias back to world coordinates
    auto newRotation = obj->m_parent->m_worldTransform.rot * origWorldRot.Transpose();

    // Create the rotated world space transformation matrix
    NiMatrix43 rotatedInvWorldTrans = rotateLinear * newRotation.Transpose() * obj->m_parent->m_worldTransform.rot;

    // Transform localDiff to a settings-rotated local space
    //newWorldPos = rotatedInvWorldTrans * newWorldPos;

    auto newLocalPos = origLocalPos[boneName.c_str()][actor->formID] + (rotatedInvWorldTrans * newWorldPos);

    // Apply gravityCorrection, which will always point downward
    newLocalPos += rotateLinear * obj->m_parent->m_worldTransform.rot * skeletonObj->m_localTransform.rot.Transpose() * NiPoint3(0, 0, gravityCorrection * linearZ);

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
    ShowPos(obj->m_localTransform.pos);
    logger.Error("rotDiff: ");
    ShowPos(rotDiff);

#endif

    obj->m_localTransform.pos = newLocalPos;

    // Calculate rotational motion
    if (absRotX) rotDiff.x = fabs(rotDiff.x);

    // Rotational motion scale
    rotDiff.x *= rotationalX;
    rotDiff.y *= rotationalY;
    rotDiff.z *= rotationalZ;


#if DEBUG
    logger.Error("localTransform.pos after: ");
    ShowPos(obj->m_localTransform.pos);
    logger.Error("origLocalPos:");
    ShowPos(origLocalPos[boneName.c_str()][actor->formID]);
    logger.Error("origLocalRot:");
    ShowRot(origLocalRot[boneName.c_str()][actor->formID]);

#endif
    // Rotate rotation according to settings.
    NiMatrix43 rotateRotation;
    rotateRotation.SetEulerAngles(rotateRotationX * DEG_TO_RAD,
        rotateRotationY * DEG_TO_RAD,
        rotateRotationZ * DEG_TO_RAD);

    NiMatrix43 standardRot;

    rotDiff = rotateRotation * rotDiff;
    standardRot.SetEulerAngles(rotDiff.x, rotDiff.y, rotDiff.z);
    // Calculate the new local rot as an offset from the original local rot
    obj->m_localTransform.rot = standardRot * origLocalRot[boneName.c_str()][actor->formID];

#if DEBUG
    logger.Error("end update()\n");
#endif

    //logger.Error("end update()\n");
    /*QueryPerformanceCounter(&endingTime);
    elapsedMicroseconds.QuadPart = endingTime.QuadPart - startingTime.QuadPart;
    elapsedMicroseconds.QuadPart *= 1000000000LL;
    elapsedMicroseconds.QuadPart /= frequency.QuadPart;
    _MESSAGE("Thing.update() Update Time = %lld ns\n", elapsedMicroseconds.QuadPart);*/

}