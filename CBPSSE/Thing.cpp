#include "f4se\NiNodes.h"
#include <time.h>

#include "log.h"
#include "Thing.h"
#include "Utility.hpp"

constexpr auto DEG_TO_RAD = 3.14159265 / 180;

// TODO Make these logger macros
//#define DEBUG 0
//#define TRANSFORM_DEBUG 0

std::unordered_map<const char*, NiPoint3> origLocalPos;
std::unordered_map<const char*, NiMatrix43> origLocalRot;

std::unordered_map<const char*, NiPoint3>::const_iterator origLocalPos_iter;
std::unordered_map<const char*, NiMatrix43>::const_iterator origLocalRot_iter;

const char * skeletonNif_boneName = "skeleton.nif";
const char* COM_boneName = "COM";

void Thing::ShowPos(NiPoint3& p) {
    logger.Info("%8.4f %8.4f %8.4f\n", p.x, p.y, p.z);
}

void Thing::ShowRot(NiMatrix43& r) {
    logger.Info("%8.4f %8.4f %8.4f %8.4f\n", r.data[0][0], r.data[0][1], r.data[0][2], r.data[0][3]);
    logger.Info("%8.4f %8.4f %8.4f %8.4f\n", r.data[1][0], r.data[1][1], r.data[1][2], r.data[1][3]);
    logger.Info("%8.4f %8.4f %8.4f %8.4f\n", r.data[2][0], r.data[2][1], r.data[2][2], r.data[2][3]);
}

Thing::Thing(Actor* actor, NiAVObject* obj, BSFixedString& name)
    : boneName(name)
    , velocity(NiPoint3(0, 0, 0)) {
    
    // Set initial positions
    oldWorldPos = obj->m_worldTransform.pos;
    time = clock();

    // Save the bones' original local values if they already haven't
    origLocalPos_iter = origLocalPos.find(boneName.c_str());
    origLocalRot_iter = origLocalRot.find(boneName.c_str());

    if (origLocalPos_iter == origLocalPos.end()) {
        logger.Error("for bone %s: ", boneName.c_str());
        logger.Error("firstRun pos Set: ");
        origLocalPos.emplace(boneName.c_str(), obj->m_localTransform.pos);
        ShowPos(obj->m_localTransform.pos);
    }
    if (origLocalRot_iter == origLocalRot.end()) {
        logger.Error("for bone %s: ", boneName.c_str());
        logger.Error("firstRun rot Set:\n");
        origLocalRot.emplace(boneName.c_str(), obj->m_localTransform.rot);
        ShowRot(obj->m_localTransform.rot);
    }

	float nodescale = 1.0f;
	if (actor)
	{
		if (actor->unkF0 && actor->unkF0->rootNode)
		{
			NiAVObject* obj = actor->unkF0->rootNode->GetObjectByName(&name);
			if (obj)
			{
				nodescale = obj->m_worldTransform.scale;
			}
		}
	}	

	thingCollisionSpheres = CreateThingCollisionSpheres(actor, std::string(name.c_str()), nodescale);

	skipFramesCount = collisionSkipFrames;
}

Thing::~Thing() {
}
std::vector<Sphere> Thing::CreateThingCollisionSpheres(Actor * actor, std::string nodeName, float nodescale)
{
	auto actorRef = DYNAMIC_CAST(actor, Actor, TESObjectREFR);

	std::vector<ConfigLine>* AffectedNodesListPtr;

	const char * actorrefname = "";

	if (actor->formID == 0x14) //If Player
	{
		actorrefname = "Player";
	}
	else
	{
		actorrefname = CALL_MEMBER_FN(actorRef, GetReferenceName)();
	}

	AffectedNodesListPtr = &AffectedNodesList;


	std::vector<Sphere> spheres;

	for (int i = 0; i < AffectedNodesListPtr->size(); i++)
	{
		if (AffectedNodesListPtr->at(i).NodeName == nodeName)
		{
			spheres = AffectedNodesListPtr->at(i).CollisionSpheres;

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

void Thing::UpdateConfig(configEntry_t & centry) {
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
}

//static float clamp(float val, float min, float max) {
//    if (val < min) return min;
//    else if (val > max) return max;
//    return val;
//}

void Thing::Reset(Actor *actor) {
    auto loadedState = actor->unkF0;
    if (!loadedState || !loadedState->rootNode) {
        logger.Error("No loaded state for actor %08x\n", actor->formID);
        return;
    }
    auto obj = loadedState->rootNode->GetObjectByName(&boneName);

    if (!obj) {
        logger.Error("Couldn't get name for loaded state for actor %08x\n", actor->formID);
        return;
    }

    obj->m_localTransform.pos = origLocalPos.at(boneName.c_str());
    obj->m_localTransform.rot = origLocalRot.at(boneName.c_str());
}

// Returns 
template <typename T> int sgn(T val) {
    return (T(0) < val) - (val < T(0));
}

NiAVObject* Thing::IsActorValid(Actor* actor) {
    auto loadedState = actor->unkF0;
    if (!loadedState || !loadedState->rootNode) {
        logger.Error("No loaded state for actor %08x\n", actor->formID);
        return NULL;
    }
    auto obj = loadedState->rootNode->GetObjectByName(&boneName);

    if (!obj) {
        logger.Error("Couldn't get name for loaded state for actor %08x\n", actor->formID);
        return NULL;
    }

    if (!obj->m_parent) {
        logger.Error("Couldn't get bone %s parent for actor %08x\n", boneName.c_str(), actor->formID);
        return NULL;
    }

    return obj;
}

void Thing::Update(Actor *actor) {

	bool collisionsOn = true;
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

    /*LARGE_INTEGER startingTime, endingTime, elapsedMicroseconds;
    LARGE_INTEGER frequency;

    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&startingTime);*/

    auto newTime = clock();
    auto deltaT = newTime - time;

    time = newTime;
    if (deltaT > 64) deltaT = 64;
    if (deltaT < 8) deltaT = 8;
    
	NiMatrix43 objRotation;

    auto obj = IsActorValid(actor);
    if (!obj) {
        return;
    }

    objRotation = obj->m_worldTransform.rot;

	bool IsThereCollision = false;
	NiPoint3 collisionDiff = emptyPoint;
	long originalDeltaT = deltaT;
	NiPoint3 collisionVector = emptyPoint;

	float varCogOffsetX = cogOffsetX;
    float varCogOffsetY = cogOffsetY;
    float varCogOffsetZ = cogOffsetZ;
	float varGravityCorrection = -1*gravityCorrection;
	float varGravityBias = gravityBias;

#if TRANSFORM_DEBUG
    auto sceneObj = obj;
    while (sceneObj->m_parent && sceneObj->m_name != "skeleton.nif")
    {
        logger.info(sceneObj->m_name);
        logger.info("\n---\n");
        logger.error("Actual m_localTransform.pos: ");
        showPos(sceneObj->m_localTransform.pos);
        logger.error("Actual m_worldTransform.pos: ");
        showPos(sceneObj->m_worldTransform.pos);
        logger.info("---\n");
        //logger.error("Actual m_localTransform.rot Matrix:\n");
        showRot(sceneObj->m_localTransform.rot);
        //logger.error("Actual m_worldTransform.rot Matrix:\n");
        showRot(sceneObj->m_worldTransform.rot);
        logger.info("---\n");
        //if (sceneObj->m_parent) {
        //	logger.error("Calculated m_worldTransform.pos: ");
        //	showPos((sceneObj->m_parent->m_worldTransform.rot.Transpose() * sceneObj->m_localTransform.pos) + sceneObj->m_parent->m_worldTransform.pos);
        //	logger.error("Calculated m_worldTransform.rot Matrix:\n");
        //	showRot(sceneObj->m_localTransform.rot * sceneObj->m_parent->m_worldTransform.rot);
        //}
        sceneObj = sceneObj->m_parent;
    }
#endif

    auto skeletonObj = obj;
    NiAVObject* comObj;
    bool skeletonFound = false;
    while (skeletonObj->m_parent)
    {
        if (skeletonObj->m_parent->m_name == BSFixedString(COM_boneName)) {
            comObj = skeletonObj->m_parent;
        }
        else if (skeletonObj->m_parent->m_name == BSFixedString(skeletonNif_boneName)) {
            skeletonObj = skeletonObj->m_parent;
            skeletonFound = true;
            break;
        }
        skeletonObj = skeletonObj->m_parent;
    }
    if (skeletonFound == false) {
        logger.Error("Couldn't find skeleton for actor %08x\n", actor->formID);
        return;
    }

    objRotation = skeletonObj->m_worldTransform.rot.Transpose();

	std::vector<long> thingIdList;
	std::vector<long> hashIdList;
    logger.Info("Collisions on: %d, hashSize: %d\n", collisionsOn, hashSize);
	if (collisionsOn && hashSize>0)
	{
		logger.Info("Before Collision Stuff Start\n");
		// Collision Stuff Start
		for (int i = 0; i < thingCollisionSpheres.size(); i++)
		{
			thingCollisionSpheres[i].worldPos = oldWorldPos + (objRotation*thingCollisionSpheres[i].offset);
			//printNiPointMessage("thingCollisionSpheres[i].worldPos", thingCollisionSpheres[i].worldPos);
			hashIdList = GetHashIdsFromPos(thingCollisionSpheres[i].worldPos, thingCollisionSpheres[i].radius, hashSize);
			for(int m=0; m<hashIdList.size(); m++)
			{
				if (!(std::find(thingIdList.begin(), thingIdList.end(), hashIdList[m]) != thingIdList.end()))
				{
					thingIdList.emplace_back(hashIdList[m]);
				}
			}
		}

		NiPoint3 lastcollisionVector = emptyPoint;

		for (int j = 0; j < thingIdList.size(); j++)
		{
			long id = thingIdList[j];
			if (partitions.find(id) != partitions.end())
			{
				for (int i = 0; i < partitions[id].partitionCollisions.size(); i++)
				{
					if (partitions[id].partitionCollisions[i].colliderActor == actor && partitions[id].partitionCollisions[i].colliderNodeName.find("Genital") != std::string::npos)
						continue;

					if (partitions[id].partitionCollisions[i].colliderActor == actor && std::strcmp(partitions[id].partitionCollisions[i].colliderNodeName.c_str(), boneName.c_str())==0 )
						continue;

					callCount++;

					if (!CompareNiPoints(lastcollisionVector, collisionVector))
				{
						for (int l = 0; l < thingCollisionSpheres.size(); l++)
						{
							thingCollisionSpheres[l].worldPos = oldWorldPos + (objRotation*thingCollisionSpheres[l].offset) + collisionVector;
						}
					}
					lastcollisionVector = collisionVector;

					bool colliding = false;
					collisionDiff = partitions[id].partitionCollisions[i].CheckCollision(colliding, thingCollisionSpheres, timeTick, originalDeltaT, maxOffsetX, false); // TODO fix this
					if (colliding)
						IsThereCollision = true;

					collisionVector = collisionVector + collisionDiff*movementMultiplier;
					collisionVector.x = clamp(collisionVector.x, -maxOffsetX, maxOffsetX);
					collisionVector.y = clamp(collisionVector.y, -maxOffsetY, maxOffsetY);
					collisionVector.z = clamp(collisionVector.z, -maxOffsetZ, maxOffsetZ);
				}
			}
		}
		if (IsThereCollision)
		{
			float timeMultiplier = timeTick / (float)deltaT;

			collisionVector *= timeMultiplier;

			varCogOffsetX = 0;
            varCogOffsetY = 0;
            varCogOffsetZ = 0;
            varGravityCorrection = 0;
			varGravityBias = 0;
		}
		//LOG("After Collision Stuff");
	}

	NiPoint3 newPos = oldWorldPos;

	NiPoint3 posDelta = emptyPoint;

#if DEBUG
    logger.error("bone %s for actor %08x with parent %s\n", boneName.c_str(), actor->formID, skeletonObj->m_name.c_str());
    showRot(skeletonObj->m_worldTransform.rot);
    //showPos(obj->m_parent->m_worldTransform.rot.Transpose() * obj->m_localTransform.pos);
#endif
    NiMatrix43 targetRot = skeletonObj->m_localTransform.rot.Transpose();
    NiPoint3 origWorldPos = (obj->m_parent->m_worldTransform.rot.Transpose() * origLocalPos.at(boneName.c_str())) +  obj->m_parent->m_worldTransform.pos;
    // Offset to move Center of Mass make rotational motion more significant
    NiPoint3 target = (targetRot * NiPoint3(varCogOffsetX, varCogOffsetY, varCogOffsetZ)) + origWorldPos;

#if DEBUG
    logger.error("World Position: ");
    showPos(obj->m_worldTransform.pos);
    //logger.error("Parent World Position difference: ");
    //showPos(obj->m_worldTransform.pos - obj->m_parent->m_worldTransform.pos);
    logger.error("Target Rotation * cogOffsetY %8.4f: ", cogOffsetY);
    showPos(targetRot * NiPoint3(cogOffsetX, cogOffsetY, cogOffsetZ));
    //logger.error("Target Rotation:\n");
    //showRot(targetRot);
    logger.error("cogOffset x Transformation:");
    showPos(targetRot * NiPoint3(cogOffsetX, 0, 0));
    logger.error("cogOffset y Transformation:");
    showPos(targetRot * NiPoint3(0, cogOffsetY, 0));
    logger.error("cogOffset z Transformation:");
    showPos(targetRot * NiPoint3(0, 0, cogOffsetZ));
#endif

    if (!IsThereCollision)
    {
        // diff is Difference in position between old and new world position
        NiPoint3 diff = target - oldWorldPos;

        // Move up in for gravity correction
        diff += targetRot * NiPoint3(0, 0, varGravityCorrection);

#if DEBUG
        logger.error("Diff after gravity correction %f: ", gravityCorrection);
        showPos(diff);
#endif

        if (fabs(diff.x) > 100 || fabs(diff.y) > 100 || fabs(diff.z) > 100) {
            logger.Error("transform reset\n");
            obj->m_localTransform.pos = origLocalPos.at(boneName.c_str());
            oldWorldPos = target;
            velocity = NiPoint3(0, 0, 0);
            time = clock();
            return;
        }

        float timeMultiplier = timeTick / (float)deltaT;
        diff *= timeMultiplier;
        NiPoint3 posDelta = NiPoint3(0, 0, 0);

        // Compute the "Spring" Force
        NiPoint3 diff2(diff.x * diff.x * sgn(diff.x), diff.y * diff.y * sgn(diff.y), diff.z * diff.z * sgn(diff.z));
        NiPoint3 force = (diff * stiffness) + (diff2 * stiffness2) - (targetRot * NiPoint3(0, 0, gravityBias));

#if DEBUG
        logger.error("Diff2: ");
        showPos(diff2);
        logger.error("Force with stiffness %f, stiffness2 %f, gravity bias %f: ", stiffness, stiffness2, gravityBias);
        showPos(force);
#endif

        do {
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
                thingCollisionSpheres[i].worldPos = (objRotation * thingCollisionSpheres[i].offset) + maybePos;
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
            collisionVector = emptyPoint;
            NiPoint3 lastcollisionVector = emptyPoint;
            for (int j = 0; j < thingIdList.size(); j++)
            {
                long id = thingIdList[j];
                //LOG_INFO("Thing hashId=%d", id);
                if (partitions.find(id) != partitions.end())
                {
                    for (int i = 0; i < partitions[id].partitionCollisions.size(); i++)
                    {
                        if (partitions[id].partitionCollisions[i].colliderActor == actor && partitions[id].partitionCollisions[i].colliderNodeName.find("Genital") != std::string::npos)
                            continue;

                        if (partitions[id].partitionCollisions[i].colliderActor == actor && std::strcmp(partitions[id].partitionCollisions[i].colliderNodeName.c_str(), boneName.c_str()) == 0)
                            continue;

                        callCount++;
                        //partitions[id].partitionCollisions[i].CollidedWeight = actorWeight;

                        if (!CompareNiPoints(lastcollisionVector, collisionVector))
                        {
                            for (int l = 0; l < thingCollisionSpheres.size(); l++)
                            {
                                thingCollisionSpheres[l].worldPos = (objRotation * thingCollisionSpheres[l].offset) + maybePos + collisionVector;
                            }
                        }
                        lastcollisionVector = collisionVector;

                        bool colliding = false;
                        collisionDiff = partitions[id].partitionCollisions[i].CheckCollision(colliding, thingCollisionSpheres, timeTick, originalDeltaT, maxOffsetX, false);
                        if (colliding)
                        {
                            velocity = emptyPoint;
                            maybeNot = true;
                            collisionVector = collisionVector + collisionDiff;
                            collisionVector.x = clamp(collisionVector.x, -maxOffsetX, maxOffsetX);
                            collisionVector.y = clamp(collisionVector.y, -maxOffsetY, maxOffsetY);
                            collisionVector.z = clamp(collisionVector.z, -maxOffsetZ, maxOffsetZ);
                        }
                    }
                }
            }

            if (!maybeNot)
                newPos = newPos + posDelta;
            else
            {
                varGravityCorrection = 0;
                collisionVector *= timeMultiplier;
                newPos = maybePos + collisionVector;
            }

            //LOG("After Maybe Collision Stuff End");
        }
        else
            newPos = newPos + posDelta;
    }
	else
	{
		newPos = newPos + collisionVector;
		collisionOnLastFrame = true;
	}	
        //oldWorldPos = newPos - target;

#if DEBUG
        //logger.error("posDelta: ");
        //showPos(posDelta);
        logger.error("newPos: ");
        showPos(newPos);
#endif
        // clamp the difference to stop the breast severely lagging at low framerates
        NiPoint3 diff = newPos - target;

        diff.x = clamp(diff.x, -maxOffsetX, maxOffsetX);
        diff.y = clamp(diff.y, -maxOffsetY, maxOffsetY);
        diff.z = clamp(diff.z - gravityCorrection, -maxOffsetZ, maxOffsetZ) + gravityCorrection;

        //oldWorldPos = target + diff;
        oldWorldPos = diff + target;

#if DEBUG
        logger.error("diff from newPos: ");
        showPos(diff);
        //logger.error("oldWorldPos: ");
        //showPos(oldWorldPos);
#endif

        // move the bones based on the supplied weightings
        // Convert the world translations into local coordinates

        NiMatrix43 invRot;
        NiMatrix43 rotateLinear;
        rotateLinear.SetEulerAngles(rotateLinearX * DEG_TO_RAD,
                                    rotateLinearY * DEG_TO_RAD,
                                    rotateLinearZ * DEG_TO_RAD);

        invRot = rotateLinear * obj->m_parent->m_worldTransform.rot;

        auto localDiff = diff;
        localDiff = skeletonObj->m_localTransform.rot * localDiff;
        localDiff.x *= linearX;
        localDiff.y *= linearY;
        localDiff.z *= linearZ;

        auto rotDiff = localDiff;
        localDiff = skeletonObj->m_localTransform.rot.Transpose() * localDiff;

        localDiff = invRot * localDiff;
        oldWorldPos = diff + target;
#if DEBUG
        logger.error("invRot x=10 Transformation:");
        showPos(invRot * NiPoint3(10, 0, 0));
        logger.error("invRot y=10 Transformation:");
        showPos(invRot * NiPoint3(0, 10, 0));
        logger.error("invRot z=10 Transformation:");
        showPos(invRot * NiPoint3(0, 0, 10));
        logger.error("oldWorldPos: ");
        showPos(oldWorldPos);
        logger.error("localTransform.pos: ");
        showPos(obj->m_localTransform.pos);
        logger.error("localDiff: ");
        showPos(localDiff);
        logger.error("rotDiff: ");
        showPos(rotDiff);

#endif
        // scale positions from config
        NiPoint3 newLocalPos = NiPoint3((localDiff.x) + origLocalPos.at(boneName.c_str()).x,
                                        (localDiff.y) + origLocalPos.at(boneName.c_str()).y,
                                        (localDiff.z) + origLocalPos.at(boneName.c_str()).z
        );
        obj->m_localTransform.pos = newLocalPos;
        
        if (absRotX) rotDiff.x = fabs(rotDiff.x);

        rotDiff.x *= rotationalX;
        rotDiff.y *= rotationalY;
        rotDiff.z *= rotationalZ;


#if DEBUG
        logger.error("localTransform.pos after: ");
        showPos(obj->m_localTransform.pos);
        logger.error("origLocalPos:");
        showPos(origLocalPos.at(boneName.c_str()));
        logger.error("origLocalRot:");
        showRot(origLocalRot.at(boneName.c_str()));

#endif
        // Do rotation.
        NiMatrix43 rotateRotation;
        rotateRotation.SetEulerAngles(rotateRotationX * DEG_TO_RAD,
                                      rotateRotationY * DEG_TO_RAD,
                                      rotateRotationZ * DEG_TO_RAD);

        NiMatrix43 standardRot;

        rotDiff = rotateRotation * rotDiff;
        standardRot.SetEulerAngles(rotDiff.x, rotDiff.y, rotDiff.z);
        obj->m_localTransform.rot = standardRot * origLocalRot.at(boneName.c_str());
#if DEBUG
    logger.error("end update()\n");
#endif

    //logger.error("end update()\n");
    /*QueryPerformanceCounter(&endingTime);
    elapsedMicroseconds.QuadPart = endingTime.QuadPart - startingTime.QuadPart;
    elapsedMicroseconds.QuadPart *= 1000000000LL;
    elapsedMicroseconds.QuadPart /= frequency.QuadPart;
    _MESSAGE("Thing.update() Update Time = %lld ns\n", elapsedMicroseconds.QuadPart);*/

}