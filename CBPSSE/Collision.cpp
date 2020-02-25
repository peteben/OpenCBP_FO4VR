#include "Collision.h"
#include "Utility.hpp"

Collision::Collision(NiAVObject* node, std::vector<Sphere> spheres)
{
	CollisionObject = node;

	collisionSpheres = spheres;
	for (int j = 0; j < collisionSpheres.size(); j++)
	{
		collisionSpheres[j].offset = spheres[j].offset;

		collisionSpheres[j].radius = spheres[j].radius*node->m_worldTransform.scale;

		collisionSpheres[j].radiuspwr2 = spheres[j].radius*spheres[j].radius;

		collisionSpheres[j].NodeName = spheres[j].NodeName;
	}
}

bool Collision::IsItColliding(NiPoint3 &collisiondif, std::vector<Sphere> thingCollisionSpheres, std::vector<Sphere> collisionSpheres, bool maybe)
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
			float limitDistance = thingCollisionSpheres[j].radius + collisionSpheres[i].radius;

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
		IsItColliding(collisionDiff, thingCollisionSpheres, collisionSpheres, false);
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
		isColliding = IsItColliding(collisionDiff, thingCollisionSpheres, collisionSpheres, maybe);
		if (isColliding)
		{
			isItColliding = true;
			if (maybe)
				return emptyPoint;
		}
		
		//if (isItColliding)
		//{
		//	collisionDiff.x = clamp(collisionDiff.x, -maxOffset, maxOffset);
		//	collisionDiff.y = clamp(collisionDiff.y, -maxOffset, maxOffset);
		//	collisionDiff.z = clamp(collisionDiff.z, -maxOffset, maxOffset);
		//}
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