#include "Collision.h"
#include "Utility.hpp"
#include "log.h"

Collision::Collision(NiAVObject* node, std::vector<Sphere> &colliderspheres, std::vector<Capsule> &collidercapsules)
{
	CollisionObject = node;

	collisionSpheres = colliderspheres;

	for (int j = 0; j < collisionSpheres.size(); j++)
	{
		collisionSpheres[j].offset = colliderspheres[j].offset;

		collisionSpheres[j].radius = colliderspheres[j].radius;

		collisionSpheres[j].radiuspwr2 = collisionSpheres[j].radius * collisionSpheres[j].radius;

		collisionSpheres[j].NodeName = colliderspheres[j].NodeName;
	}

	collisionCapsules = collidercapsules;
	for (int k = 0; k < collisionCapsules.size(); k++)
	{
		collisionCapsules[k].End1_offset = collidercapsules[k].End1_offset;
		
		collisionCapsules[k].End1_radius = collidercapsules[k].End1_radius;

		collisionCapsules[k].End1_radiuspwr2 = collisionCapsules[k].End1_radius * collisionCapsules[k].End1_radius;
	
		collisionCapsules[k].End2_offset = collidercapsules[k].End2_offset;

		collisionCapsules[k].End2_radius = collidercapsules[k].End2_radius;

		collisionCapsules[k].End2_radiuspwr2 = collisionCapsules[k].End2_radius * collisionCapsules[k].End2_radius;

		collisionCapsules[k].NodeName = collidercapsules[k].NodeName;
	}
}

void UpdateThingColliderPositions(NiPoint3 &Collisiondif, std::vector<Sphere>& thingCollisionSpheres, std::vector<Capsule>& thingCollisionCapsules, CollisionConfigs CollisionConfig)
{
	auto lcollisiondif = CollisionConfig.origTransWorldToLocal * Collisiondif;
	
	lcollisiondif.x = clamp(lcollisiondif.x, CollisionConfig.CollisionMinOffset.x, CollisionConfig.CollisionMaxOffset.x);
	lcollisiondif.y = clamp(lcollisiondif.y, CollisionConfig.CollisionMinOffset.y, CollisionConfig.CollisionMaxOffset.y);
	lcollisiondif.z = clamp(lcollisiondif.z, CollisionConfig.CollisionMinOffset.z, CollisionConfig.CollisionMaxOffset.z);
	
	Collisiondif = CollisionConfig.origTransLocalToWorld * lcollisiondif;
	
	for (int l = 0; l < thingCollisionSpheres.size(); l++)
	{
		thingCollisionSpheres[l].worldPos = CollisionConfig.maybePos + (CollisionConfig.objRot * thingCollisionSpheres[l].offset) + Collisiondif;
	}

	for (int m = 0; m < thingCollisionCapsules.size(); m++)
	{
		thingCollisionCapsules[m].End1_worldPos = CollisionConfig.maybePos + (CollisionConfig.objRot * thingCollisionCapsules[m].End1_offset) + Collisiondif;
		thingCollisionCapsules[m].End2_worldPos = CollisionConfig.maybePos + (CollisionConfig.objRot * thingCollisionCapsules[m].End2_offset) + Collisiondif;
	}
}

bool Collision::IsItColliding(NiPoint3 &collisionDif, std::vector<Sphere> &thingCollisionSpheres, std::vector<Sphere> &collisionSpheres, std::vector<Capsule> &thingCollisionCapsules, std::vector<Capsule> &collisionCapsules, CollisionConfigs CollisionConfig, bool maybe)
{	
	/*LARGE_INTEGER startingTime, endingTime, elapsedMicroseconds;
	LARGE_INTEGER frequency;

	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&startingTime);
	LOG("Collision.IsItColliding() Start");*/
	bool isItColliding = false;

	for (int j = 0; j < thingCollisionSpheres.size(); j++)
	{
		NiPoint3 thingSpherePosition = thingCollisionSpheres[j].worldPos;

		for (int i = 0; i < collisionSpheres.size(); i++) //sphere X sphere
		{			
			float limitDistance = thingCollisionSpheres[j].radius + collisionSpheres[i].radius;

			NiPoint3 colSpherePosition = collisionSpheres[i].worldPos;
			
			float currentDistancePwr2 = distanceNoSqrt(thingSpherePosition, colSpherePosition);

			if (currentDistancePwr2 < limitDistance * limitDistance)
			{
				isItColliding = true;
				if (maybe)
					return true;

				float currentDistance = std::sqrt(currentDistancePwr2);
				double Scalar = limitDistance - currentDistance; //Get vector scalar

				collisionDif += GetVectorFromCollision(colSpherePosition, thingSpherePosition, Scalar, currentDistance); //Get collision vector
				//LOG_INFO("%s is colliding with %s with percentage %f!\n", thingCollisionSpheres[j].NodeName.c_str(), collisionSpheres[i].NodeName.c_str(), difPercentage);

				UpdateThingColliderPositions(collisionDif, thingCollisionSpheres, thingCollisionCapsules, CollisionConfig);
			}			
		}
		
		for (int k = 0; k < collisionCapsules.size(); k++) //sphere X capsule
		{
			NiPoint3 bestPoint = ClosestPointOnLineSegment(collisionCapsules[k].End1_worldPos, collisionCapsules[k].End2_worldPos, thingCollisionSpheres[j].worldPos);

			float twoPointDistancePwr2 = distanceNoSqrt(collisionCapsules[k].End1_worldPos, collisionCapsules[k].End2_worldPos);

			float bestPointDistancePwr2 = distanceNoSqrt(collisionCapsules[k].End1_worldPos, bestPoint);

			float PointWeight = bestPointDistancePwr2 / twoPointDistancePwr2 * 100;

			float limitDistance = GetPercentageValue(collisionCapsules[k].End1_radius, collisionCapsules[k].End2_radius, PointWeight) + thingCollisionSpheres[j].radius;

			float currentDistancePwr2 = distanceNoSqrt(thingCollisionSpheres[j].worldPos, bestPoint);

			if (currentDistancePwr2 < limitDistance * limitDistance)
			{
				isItColliding = true;
				if (maybe)
					return true;

				float currentDistance = std::sqrt(currentDistancePwr2);
				double Scalar = limitDistance - currentDistance; //Get vector scalar

				collisionDif += GetVectorFromCollision(bestPoint, thingCollisionSpheres[j].worldPos, Scalar, currentDistance); //Get collision vector

				UpdateThingColliderPositions(collisionDif, thingCollisionSpheres, thingCollisionCapsules, CollisionConfig);
			}
		}
	}
	
	for (int n = 0; n < thingCollisionCapsules.size(); n++)
	{
		for (int l = 0; l < collisionSpheres.size(); l++) //capsule X sphere
		{
			NiPoint3 bestPoint = ClosestPointOnLineSegment(thingCollisionCapsules[n].End1_worldPos, thingCollisionCapsules[n].End2_worldPos, collisionSpheres[l].worldPos);

			float twoPointDistancePwr2 = distanceNoSqrt(thingCollisionCapsules[n].End1_worldPos, thingCollisionCapsules[n].End2_worldPos);

			float bestPointDistancePwr2 = distanceNoSqrt(thingCollisionCapsules[n].End1_worldPos, bestPoint);

			float PointWeight = bestPointDistancePwr2 / twoPointDistancePwr2 * 100;

			float limitDistance = GetPercentageValue(thingCollisionCapsules[n].End1_radius, thingCollisionCapsules[n].End2_radius, PointWeight) + collisionSpheres[l].radius;

			float currentDistancePwr2 = distanceNoSqrt(bestPoint, collisionSpheres[l].worldPos);

			if (currentDistancePwr2 < limitDistance * limitDistance)
			{
				isItColliding = true;
				if (maybe)
					return true;

				float currentDistance = std::sqrt(currentDistancePwr2);
				double Scalar = limitDistance - currentDistance; //Get vector scalar

				collisionDif += GetVectorFromCollision(collisionSpheres[l].worldPos, bestPoint, Scalar, currentDistance); //Get collision vector
			
				UpdateThingColliderPositions(collisionDif, thingCollisionSpheres, thingCollisionCapsules, CollisionConfig);
			}
		}

		for (int m = 0; m < collisionCapsules.size(); m++) //capsule X capsule
		{
			NiPoint3 v0 = collisionCapsules[m].End1_worldPos - thingCollisionCapsules[n].End1_worldPos;
			NiPoint3 v1 = collisionCapsules[m].End2_worldPos - thingCollisionCapsules[n].End1_worldPos;
			NiPoint3 v2 = collisionCapsules[m].End1_worldPos - thingCollisionCapsules[n].End2_worldPos;
			NiPoint3 v3 = collisionCapsules[m].End2_worldPos - thingCollisionCapsules[n].End2_worldPos;

			float d0 = Dot(v0, v0);
			float d1 = Dot(v1, v1);
			float d2 = Dot(v2, v2);
			float d3 = Dot(v3, v3);
			
			NiPoint3 bestPointA;
			if (d2 < d0 || d2 < d1 || d3 < d0 || d3 < d1)
			{
				bestPointA = thingCollisionCapsules[n].End2_worldPos;
			}
			else
			{
				bestPointA = thingCollisionCapsules[n].End1_worldPos;
			}

			NiPoint3 bestPointB = ClosestPointOnLineSegment(collisionCapsules[m].End1_worldPos, collisionCapsules[m].End2_worldPos, bestPointA);
			bestPointA = ClosestPointOnLineSegment(thingCollisionCapsules[n].End1_worldPos, thingCollisionCapsules[n].End2_worldPos, bestPointB);

			float twoPointADistancePwr2 = distanceNoSqrt(thingCollisionCapsules[n].End1_worldPos, thingCollisionCapsules[n].End2_worldPos);
			float twoPointBDistancePwr2 = distanceNoSqrt(collisionCapsules[m].End1_worldPos, collisionCapsules[m].End2_worldPos);

			float bestPointADistancePwr2 = distanceNoSqrt(thingCollisionCapsules[n].End1_worldPos, bestPointA);
			float bestPointBDistancePwr2 = distanceNoSqrt(collisionCapsules[m].End1_worldPos, bestPointB);

			float PointAWeight = bestPointADistancePwr2 / twoPointADistancePwr2 * 100;
			float PointBWeight = bestPointBDistancePwr2 / twoPointBDistancePwr2 * 100;

			float limitDistance = GetPercentageValue(thingCollisionCapsules[n].End1_radius, thingCollisionCapsules[n].End2_radius, PointAWeight)
				+ GetPercentageValue(collisionCapsules[m].End1_radius, collisionCapsules[m].End2_radius, PointBWeight);

			float currentDistancePwr2 = distanceNoSqrt(bestPointA, bestPointB);

			if (currentDistancePwr2 < limitDistance * limitDistance)
			{
				isItColliding = true;
				if (maybe)
				{
					return true;
				}

				float currentDistance = std::sqrt(currentDistancePwr2);
				double Scalar = limitDistance - currentDistance; //Get vector scalar

				collisionDif += GetVectorFromCollision(bestPointB, bestPointA, Scalar, currentDistance); //Get collision vector

				UpdateThingColliderPositions(collisionDif, thingCollisionSpheres, thingCollisionCapsules, CollisionConfig);
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

bool Collision::CheckCollision(NiPoint3 &collisionDiff, std::vector<Sphere> &thingCollisionSpheres, std::vector<Capsule>& thingCollisionCapsules, CollisionConfigs CollisionConfig, bool maybe)
{
	/*LARGE_INTEGER startingTime, endingTime, elapsedMicroseconds;
	LARGE_INTEGER frequency;

	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&startingTime);
	LOG("Collision.CheckCollision() Start");*/
	bool isColliding = false;
	if (CollisionObject != nullptr)
	{
		isColliding = IsItColliding(collisionDiff, thingCollisionSpheres, collisionSpheres, thingCollisionCapsules, collisionCapsules, CollisionConfig, maybe);
		if (isColliding)
		{
			if (maybe)
			{
				return true;
			}
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
	return isColliding;
}

// Dot product of 2 vectors 
float Collision::Dot(NiPoint3 A, NiPoint3 B)
{
	float x1, y1, z1;
	x1 = A.x * B.x;
	y1 = A.y * B.y;
	z1 = A.z * B.z;
	return (x1 + y1 + z1);
}

NiPoint3 Collision::ClosestPointOnLineSegment(NiPoint3 lineStart, NiPoint3 lineEnd, NiPoint3 point)
{
	auto v = lineEnd - lineStart;
	auto t = Dot(point - lineStart, v) / Dot(v, v);
	t = max(t, 0.0f);
	t = min(t, 1.0f);
	return lineStart + v * t;
}

Collision::~Collision() {
}