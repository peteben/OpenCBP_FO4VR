#include "f4se/GameRTTI.h"
#include "f4se_common/Utilities.h"

#include "CollisionConfig.h"
#include "CollisionHub.h"
#include "hash.h"
#include "log.h"
#include "Utility.hpp"

PartitionMap partitions;


//debug variable
long callCount = 0;
bool CreateActorColliders(Actor * actor, concurrency::concurrent_unordered_map<std::string, Collision> &actorCollidersList)
{
	bool GroundCollisionEnabled = false;
	NiNode* mostInterestingRoot;

	if (actor && actor->unkF0 && actor->unkF0->rootNode)
	{
    	mostInterestingRoot = actor->unkF0->rootNode;
	}
  	else
	{
    	return false;
	}
	
	auto actorRef = DYNAMIC_CAST(actor, Actor, TESObjectREFR);
	float actorBaseScale = 1.0f;
	//if (actorRef)
	//{
	//	actorBaseScale = CALL_MEMBER_FN(actorRef, GetBaseScale)();
	//}
	
	concurrency::concurrent_vector<ConfigLine>* ColliderNodesListPtr;
	
	//SpecificNPCConfig snc;

	if (actor)
	{
		//if (GetSpecificNPCConfigForActor(actor))
		//{
		//	ColliderNodesListPtr = &(snc.ColliderNodesList);
		//}
		//else
		//{
			ColliderNodesListPtr = &ColliderNodesList;
		//}
	}
	else
	{
		ColliderNodesListPtr = &ColliderNodesList;
	}

	//std::shared_mutex CH_read_lock;

	//concurrency::parallel_for (size_t(0), ColliderNodesListPtr->size(), [&](size_t j)
	//{
	for (size_t j = 0; j < ColliderNodesListPtr->size(); j++)
	{
		//if (ColliderNodesListPtr->at(j).NodeName.compare(GroundReferenceBone) == 0) //detecting NPC Root [Root] node for ground collision
		//{
		//	GroundCollisionEnabled = true;
		//}
		//else
		//{
			BSFixedString fs(ColliderNodesListPtr->at(j).NodeName.c_str());
			//CH_read_lock.lock();
			NiAVObject* node = mostInterestingRoot->GetObjectByName(&fs);
			//CH_read_lock.unlock();
			if (node)
			{
				Collision newCol = Collision::Collision(node, ColliderNodesListPtr->at(j).CollisionSpheres, ColliderNodesListPtr->at(j).CollisionCapsules);
				newCol.colliderActor = actor;
				newCol.colliderNodeName = ColliderNodesListPtr->at(j).NodeName;
				newCol.actorBaseScale = actorBaseScale;

				actorCollidersList.insert(std::make_pair(ColliderNodesListPtr->at(j).NodeName, newCol));
			}
		//}
	}
	//});
	return GroundCollisionEnabled;
}


void UpdateColliderPositions(concurrency::concurrent_unordered_map<std::string, Collision> &colliderList, concurrency::concurrent_unordered_map<std::string, NiPoint3> NodeCollisionSyncList)
{
	//concurrency::parallel_for_each(colliderList.begin(), colliderList.end(), [&](auto& collider)
	//{
	for (auto& collider : colliderList)
	{
		NiPoint3 VirtualOffset = emptyPoint;

		if (NodeCollisionSyncList.find(collider.second.colliderNodeName) != NodeCollisionSyncList.end())
			VirtualOffset = NodeCollisionSyncList[collider.second.colliderNodeName];

		float colliderNodescale = 1.0f;// -((1.0f - (collider.second.CollisionObject->m_worldTransform.scale / collider.second.actorBaseScale)));

		for (int j = 0; j < collider.second.collisionSpheres.size(); j++)
		{
			collider.second.collisionSpheres[j].worldPos = collider.second.CollisionObject->m_worldTransform.pos + (collider.second.CollisionObject->m_worldTransform.rot * collider.second.collisionSpheres[j].offset) + VirtualOffset;
			collider.second.collisionSpheres[j].radiuspwr2 = collider.second.collisionSpheres[j].radius * collider.second.collisionSpheres[j].radius;
		}

		for (int k = 0; k < collider.second.collisionCapsules.size(); k++)
		{
			collider.second.collisionCapsules[k].End1_worldPos = collider.second.CollisionObject->m_worldTransform.pos + (collider.second.CollisionObject->m_worldTransform.rot * collider.second.collisionCapsules[k].End1_offset) + VirtualOffset;
			collider.second.collisionCapsules[k].End1_radiuspwr2 = collider.second.collisionCapsules[k].End1_radius * collider.second.collisionCapsules[k].End1_radius;
			collider.second.collisionCapsules[k].End2_worldPos = collider.second.CollisionObject->m_worldTransform.pos + (collider.second.CollisionObject->m_worldTransform.rot * collider.second.collisionCapsules[k].End2_offset) + VirtualOffset;
			collider.second.collisionCapsules[k].End2_radiuspwr2 = collider.second.collisionCapsules[k].End2_radius * collider.second.collisionCapsules[k].End2_radius;
		}
	}
	//});
}


// Returns list of collision hash ids given a position and the radius
// Hash ids are for collisions at pos within the radius
std::vector<int> GetHashIdsFromPos(NiPoint3 pos, float radiusplus)
{
	//float radiusplus = radius + 1.0f; //1 is enough now.

	std::vector<int> hashIdList;
	
	int hashId = CreateHashId(pos.x, pos.y, pos.z, gridsize, actorDistance);
	//LOG_INFO("hashId=%d", hashId);
	if (hashId >= 0)
		hashIdList.emplace_back(hashId);

	bool xPlus = false;
	bool xMinus = false;
	bool yPlus = false;
	bool yMinus = false;
	bool zPlus = false;
	bool zMinus = false;

	hashId = CreateHashId(pos.x + radiusplus, pos.y, pos.z, gridsize, actorDistance);
	//LOG_INFO("hashId=%d", hashId);
	if (hashId >= 0)
	{
		if (!(std::find(hashIdList.begin(), hashIdList.end(), hashId) != hashIdList.end()))
		{
			xPlus = true;
			hashIdList.emplace_back(hashId);
		}
	}

	hashId = CreateHashId(pos.x - radiusplus, pos.y, pos.z, gridsize, actorDistance);
	//LOG_INFO("hashId=%d", hashId);
	if (hashId >= 0)
	{
		if (!(std::find(hashIdList.begin(), hashIdList.end(), hashId) != hashIdList.end()))
		{
			xMinus = true;
			hashIdList.emplace_back(hashId);
		}
	}

	hashId = CreateHashId(pos.x, pos.y + radiusplus, pos.z, gridsize, actorDistance);
	//LOG_INFO("hashId=%d", hashId);
	if (hashId >= 0)
	{
		if (!(std::find(hashIdList.begin(), hashIdList.end(), hashId) != hashIdList.end()))
		{
			yPlus = true;
			hashIdList.emplace_back(hashId);
		}
	}

	hashId = CreateHashId(pos.x, pos.y - radiusplus, pos.z, gridsize, actorDistance);
	//LOG_INFO("hashId=%d", hashId);
	if (hashId >= 0)
	{
		if (!(std::find(hashIdList.begin(), hashIdList.end(), hashId) != hashIdList.end()))
		{
			yMinus = true;
			hashIdList.emplace_back(hashId);
		}
	}

	hashId = CreateHashId(pos.x, pos.y, pos.z + radiusplus, gridsize, actorDistance);
	//LOG_INFO("hashId=%d", hashId);
	if (hashId >= 0)
	{
		if (!(std::find(hashIdList.begin(), hashIdList.end(), hashId) != hashIdList.end()))
		{
			zPlus = true;
			hashIdList.emplace_back(hashId);
		}
	}

	hashId = CreateHashId(pos.x, pos.y, pos.z - radiusplus, gridsize, actorDistance);
	//LOG_INFO("hashId=%d", hashId);
	if (hashId >= 0)
	{
		if (!(std::find(hashIdList.begin(), hashIdList.end(), hashId) != hashIdList.end()))
		{
			zMinus = true;
			hashIdList.emplace_back(hashId);
		}
	}

	if (xPlus && yPlus)
	{
		hashId = CreateHashId(pos.x + radiusplus, pos.y + radiusplus, pos.z, gridsize,  actorDistance);
		//LOG_INFO("hashId=%d", hashId);
		if (hashId >= 0)
		{
			if (!(std::find(hashIdList.begin(), hashIdList.end(), hashId) != hashIdList.end()))
			{
				hashIdList.emplace_back(hashId);
			}
		}

		if (xPlus && yPlus && zPlus)
		{
			hashId = CreateHashId(pos.x + radiusplus, pos.y + radiusplus, pos.z + radiusplus, gridsize, actorDistance);
			//LOG_INFO("hashId=%d", hashId);
			if (hashId >= 0)
			{
				if (!(std::find(hashIdList.begin(), hashIdList.end(), hashId) != hashIdList.end()))
				{
					hashIdList.emplace_back(hashId);
				}
			}
		}
		else if (xPlus && yPlus && zMinus)
		{
			hashId = CreateHashId(pos.x + radiusplus, pos.y + radiusplus, pos.z - radiusplus, gridsize, actorDistance);
			//LOG_INFO("hashId=%d", hashId);
			if (hashId >= 0)
			{
				if (!(std::find(hashIdList.begin(), hashIdList.end(), hashId) != hashIdList.end()))
				{
					hashIdList.emplace_back(hashId);
				}
			}
		}
	}
	else if (xMinus && yPlus)
	{
		hashId = CreateHashId(pos.x - radiusplus, pos.y + radiusplus, pos.z, gridsize, actorDistance);
		//LOG_INFO("hashId=%d", hashId);
		if (hashId >= 0)
		{
			if (!(std::find(hashIdList.begin(), hashIdList.end(), hashId) != hashIdList.end()))
			{
				hashIdList.emplace_back(hashId);
			}
		}

		if (xMinus && yPlus && zMinus)
		{
			hashId = CreateHashId(pos.x - radiusplus, pos.y + radiusplus, pos.z - radiusplus, gridsize, actorDistance);
			//LOG_INFO("hashId=%d", hashId);
			if (hashId >= 0)
			{
				if (!(std::find(hashIdList.begin(), hashIdList.end(), hashId) != hashIdList.end()))
				{
					hashIdList.emplace_back(hashId);
				}
			}
		}
		else if (xMinus && yPlus && zPlus)
		{
			hashId = CreateHashId(pos.x - radiusplus, pos.y + radiusplus, pos.z + radiusplus, gridsize, actorDistance);
			//LOG_INFO("hashId=%d", hashId);
			if (hashId >= 0)
			{
				if (!(std::find(hashIdList.begin(), hashIdList.end(), hashId) != hashIdList.end()))
				{
					hashIdList.emplace_back(hashId);
				}
			}
		}
	}
	else if (xPlus && yMinus)
	{
		hashId = CreateHashId(pos.x + radiusplus, pos.y - radiusplus, pos.z, gridsize,  actorDistance);
		//LOG_INFO("hashId=%d", hashId);
		if (hashId >= 0)
		{
			if (!(std::find(hashIdList.begin(), hashIdList.end(), hashId) != hashIdList.end()))
			{
				hashIdList.emplace_back(hashId);
			}
		}

		if (xPlus && yMinus && zMinus)
		{
			hashId = CreateHashId(pos.x + radiusplus, pos.y - radiusplus, pos.z - radiusplus, gridsize, actorDistance);
			//LOG_INFO("hashId=%d", hashId);
			if (hashId >= 0)
			{
				if (!(std::find(hashIdList.begin(), hashIdList.end(), hashId) != hashIdList.end()))
				{
					hashIdList.emplace_back(hashId);
				}
			}
		}
		else if (xPlus && yMinus && zPlus)
		{
			hashId = CreateHashId(pos.x + radiusplus, pos.y - radiusplus, pos.z + radiusplus, gridsize, actorDistance);
			//LOG_INFO("hashId=%d", hashId);
			if (hashId >= 0)
			{
				if (!(std::find(hashIdList.begin(), hashIdList.end(), hashId) != hashIdList.end()))
				{
					hashIdList.emplace_back(hashId);
				}
			}
		}
	}
	else if (xMinus && yMinus)
	{
		hashId = CreateHashId(pos.x - radiusplus, pos.y - radiusplus, pos.z, gridsize, actorDistance);
		//LOG_INFO("hashId=%d", hashId);
		if (hashId >= 0)
		{
			if (!(std::find(hashIdList.begin(), hashIdList.end(), hashId) != hashIdList.end()))
			{
				hashIdList.emplace_back(hashId);
			}
		}

		if (xMinus && yMinus && zMinus)
		{
			hashId = CreateHashId(pos.x - radiusplus, pos.y - radiusplus, pos.z - radiusplus, gridsize, actorDistance);
			//LOG_INFO("hashId=%d", hashId);
			if (hashId >= 0)
			{
				if (!(std::find(hashIdList.begin(), hashIdList.end(), hashId) != hashIdList.end()))
				{
					hashIdList.emplace_back(hashId);
				}
			}
		}
		else if (xMinus && yMinus && zPlus)
		{
			hashId = CreateHashId(pos.x - radiusplus, pos.y - radiusplus, pos.z + radiusplus, gridsize, actorDistance);
			//LOG_INFO("hashId=%d", hashId);
			if (hashId >= 0)
			{
				if (!(std::find(hashIdList.begin(), hashIdList.end(), hashId) != hashIdList.end()))
				{
					hashIdList.emplace_back(hashId);
				}
			}
		}
	}

	if (xPlus && zPlus)
	{
		hashId = CreateHashId(pos.x + radiusplus, pos.y, pos.z + radiusplus, gridsize, actorDistance);
		//LOG_INFO("hashId=%d", hashId);
		if (hashId >= 0)
		{
			if (!(std::find(hashIdList.begin(), hashIdList.end(), hashId) != hashIdList.end()))
			{
				hashIdList.emplace_back(hashId);
			}
		}
	}
	else if (xMinus && zPlus)
	{
		hashId = CreateHashId(pos.x - radiusplus, pos.y, pos.z + radiusplus, gridsize, actorDistance);
		//LOG_INFO("hashId=%d", hashId);
		if (hashId >= 0)
		{
			if (!(std::find(hashIdList.begin(), hashIdList.end(), hashId) != hashIdList.end()))
			{
				hashIdList.emplace_back(hashId);
			}
		}
	}
	else if (xPlus && zMinus)
	{
		hashId = CreateHashId(pos.x + radiusplus, pos.y, pos.z - radiusplus, gridsize, actorDistance);
		//LOG_INFO("hashId=%d", hashId);
		if (hashId >= 0)
		{
			if (!(std::find(hashIdList.begin(), hashIdList.end(), hashId) != hashIdList.end()))
			{
				hashIdList.emplace_back(hashId);
			}
		}
	}
	else if (xMinus && zMinus)
	{
		hashId = CreateHashId(pos.x - radiusplus, pos.y, pos.z - radiusplus, gridsize, actorDistance);
		//LOG_INFO("hashId=%d", hashId);
		if (hashId >= 0)
		{
			if (!(std::find(hashIdList.begin(), hashIdList.end(), hashId) != hashIdList.end()))
			{
				hashIdList.emplace_back(hashId);
			}
		}
	}

	if (yPlus && zPlus)
	{
		hashId = CreateHashId(pos.x, pos.y + radiusplus, pos.z + radiusplus, gridsize, actorDistance);
		//LOG_INFO("hashId=%d", hashId);
		if (hashId >= 0)
		{
			if (!(std::find(hashIdList.begin(), hashIdList.end(), hashId) != hashIdList.end()))
			{
				hashIdList.emplace_back(hashId);
			}
		}
	}
	else if (yMinus && zPlus)
	{
		hashId = CreateHashId(pos.x, pos.y - radiusplus, pos.z + radiusplus, gridsize, actorDistance);
		//LOG_INFO("hashId=%d", hashId);
		if (hashId >= 0)
		{
			if (!(std::find(hashIdList.begin(), hashIdList.end(), hashId) != hashIdList.end()))
			{
				hashIdList.emplace_back(hashId);
			}
		}
	}
	else if (yPlus && zMinus)
	{
		hashId = CreateHashId(pos.x, pos.y + radiusplus, pos.z - radiusplus, gridsize, actorDistance);
		//LOG_INFO("hashId=%d", hashId);
		if (hashId >= 0)
		{
			if (!(std::find(hashIdList.begin(), hashIdList.end(), hashId) != hashIdList.end()))
			{
				hashIdList.emplace_back(hashId);
			}
		}
	}
	else if (yMinus && zMinus)
	{
		hashId = CreateHashId(pos.x, pos.y - radiusplus, pos.z - radiusplus, gridsize, actorDistance);
		//LOG_INFO("hashId=%d", hashId);
		if (hashId >= 0)
		{
			if (!(std::find(hashIdList.begin(), hashIdList.end(), hashId) != hashIdList.end()))
			{
				hashIdList.emplace_back(hashId);
			}
		}
	}

	return hashIdList;
}

int GetHashIdFromPos(NiPoint3 pos)
{	
	int hashId = CreateHashId(pos.x, pos.y, pos.z, gridsize, actorDistance);
	if (hashId >= 0)
		return hashId;
	else
		return -1;

	/*hashId = unsigned(floor((pos.x+radius) / gridsize)*a + floor(pos.y / gridsize)*b + floor(pos.z / gridsize)*c) % size;
	if (hashId < size && hashId >= 0)
		hashIdList.emplace_back(hashId);

	hashId = unsigned(floor((pos.x - radius) / gridsize)*a + floor(pos.y / gridsize)*b + floor(pos.z / gridsize)*c) % size;
	if (hashId < size && hashId >= 0)
		hashIdList.emplace_back(hashId);*/


}