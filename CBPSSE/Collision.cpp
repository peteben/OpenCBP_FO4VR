#include "Collision.h"
#include "Utility.hpp"

Collision::Collision(NiAVObject* node, std::vector<Sphere> spheres)
{
	CollisionObject = node;

	collisionSpheres = spheres;
	for (int j = 0; j < collisionSpheres.size(); j++)
	{
		collisionSpheres[j].offset100 = GetPointFromPercentage(spheres[j].offset0, spheres[j].offset100);

		collisionSpheres[j].radius100 = GetPercentageValue(spheres[j].radius0, spheres[j].radius100)*node->m_worldTransform.scale;

		collisionSpheres[j].radius100pwr2 = spheres[j].radius100*spheres[j].radius100;

		collisionSpheres[j].NodeName = spheres[j].NodeName;
	}
}

bool Collision::IsItColliding(NiPoint3 &collisiondif, std::vector<Sphere> thingCollisionSpheres, std::vector<Sphere> collisionSpheres, float maxOffset, bool maybe)
{	
	/*LARGE_INTEGER startingTime, endingTime, elapsedMicroseconds;
	LARGE_INTEGER frequency;

	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&startingTime);
	LOG("Collision.IsItColliding() Start");*/
	bool isItColliding = false;

	for (int j = 0; j < thingCollisionSpheres.size(); j++)
	{
		for (int i = 0; i < collisionSpheres.size(); i++)
		{			
			float limitDistance = thingCollisionSpheres[j].radius100 + collisionSpheres[i].radius100;

			NiPoint3 thingSpherePosition = thingCollisionSpheres[j].worldPos;
			NiPoint3 colSpherePosition = collisionSpheres[i].worldPos;
			
			float currentDistancePwr2 = distanceNoSqrt(thingSpherePosition, colSpherePosition);

			if (currentDistancePwr2 < limitDistance*limitDistance)
			{
				isItColliding = true;
				if (maybe)
					return true;

				float currentDistance = std::sqrt(currentDistancePwr2);
				double difPercentage = ((limitDistance - currentDistance) / currentDistance) * 100;

				collisiondif = collisiondif - thingSpherePosition;

			}			
		}
	}
	
	/*QueryPerformanceCounter(&endingTime);
	elapsedMicroseconds.QuadPart = endingTime.QuadPart - startingTime.QuadPart;
	elapsedMicroseconds.QuadPart *= 1000000000LL;
	elapsedMicroseconds.QuadPart /= frequency.QuadPart;
	LOG("Collision.IsItColliding() Update Time = %lld ns\n", elapsedMicroseconds.QuadPart);*/
	return isItColliding;
}

NiPoint3 Collision::CheckPelvisCollision(std::vector<Sphere> thingCollisionSpheres)
{
	/*LARGE_INTEGER startingTime, endingTime, elapsedMicroseconds;
	LARGE_INTEGER frequency;

	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&startingTime);
	LOG("Collision.CheckPelvisCollision() Start");*/
	NiPoint3 collisionDiff = emptyPoint;

	if (CollisionObject != nullptr)
	{
		IsItColliding(collisionDiff, thingCollisionSpheres, collisionSpheres, 99, false);
	}

	/*QueryPerformanceCounter(&endingTime);
	elapsedMicroseconds.QuadPart = endingTime.QuadPart - startingTime.QuadPart;
	elapsedMicroseconds.QuadPart *= 1000000000LL;
	elapsedMicroseconds.QuadPart /= frequency.QuadPart;
	LOG("Collision.CheckPelvisCollision() Update Time = %lld ns\n", elapsedMicroseconds.QuadPart);*/
	return collisionDiff;
}

NiPoint3 Collision::CheckCollision(bool &isItColliding, std::vector<Sphere> thingCollisionSpheres, float timeTick, long deltaT, float maxOffset, bool maybe)
{
	/*LARGE_INTEGER startingTime, endingTime, elapsedMicroseconds;
	LARGE_INTEGER frequency;

	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&startingTime);
	LOG("Collision.CheckCollision() Start");*/
	NiPoint3 collisionDiff = emptyPoint;
	bool isColliding = false;
	if (CollisionObject != nullptr)
	{
		isColliding = IsItColliding(collisionDiff, thingCollisionSpheres, collisionSpheres, maxOffset, maybe);
		if (isColliding)
		{
			isItColliding = true;
			if (maybe)
				return emptyPoint;
		}
		
		if (isItColliding)
		{
			//float timeMultiplier = timeTick / (float)deltaT;

			//collisionDiff *= timeMultiplier;

			//if (lastColliderPosition.x != 0 || lastColliderPosition.y != 0 || lastColliderPosition.z != 0)
			//{
			//	float distanceInOneCall = distance(lastColliderPosition, CollisionObject->m_worldTransform.pos);
			//	//if (distanceInOneCall > 1)
			//		collisionDiff *= distanceInOneCall;
			//}

			collisionDiff.x = clamp(collisionDiff.x, -maxOffset, maxOffset);
			collisionDiff.y = clamp(collisionDiff.y, -maxOffset, maxOffset);
			collisionDiff.z = clamp(collisionDiff.z, -maxOffset, maxOffset);
		}
		//lastColliderPosition = CollisionObject->m_worldTransform.pos;
	}

	/*QueryPerformanceCounter(&endingTime);
	elapsedMicroseconds.QuadPart = endingTime.QuadPart - startingTime.QuadPart;
	elapsedMicroseconds.QuadPart *= 1000000000LL;
	elapsedMicroseconds.QuadPart /= frequency.QuadPart;
	LOG("Collision.CheckCollision() Update Time = %lld ns\n", elapsedMicroseconds.QuadPart);*/
	return collisionDiff;
}

Collision::~Collision() {
}