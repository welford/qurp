//------------------------------------------------------------
//
// vector.c 
//
// by James Anderson 2002(ish)~2007
//
// C style vector library 
//------------------------------------------------------------

#include <math.h>
#include "vector.h"

//------------------------------------------------------------
//Normalise a vector
//------------------------------------------------------------
void vecNormalise3df(Vec3df * vector)
{
	float length;

	length = (float)sqrt((vector->x*vector->x)+(vector->y*vector->y)+(vector->z*vector->z));
	if(length > 0)
	{
		//length = 1.0f/length;
		vector->x /= length;
		vector->y /= length;
		vector->z /= length;
	}

}
void vecNormalise4df(Vec4df * vector)
{
	float length;

	length = (float)sqrt((vector->x*vector->x)+(vector->y*vector->y)+(vector->z*vector->z)+(vector->t*vector->t));
	if(length > 0)
	{
		//length = 1.0f/length;
		vector->x /= length;
		vector->y /= length;
		vector->z /= length;
		vector->t /= length;
	}

}
//------------------------------------------------------------
//Add vecotos A and B result in C 
//------------------------------------------------------------
void vecAdd3df(Vec3df * vectorA, Vec3df * vectorB, Vec3df * vectorC)
{
	vectorC->x = vectorA->x + vectorB->x;
	vectorC->y = vectorA->y + vectorB->y; 
	vectorC->z = vectorA->z + vectorB->z;
}
void vecAdd4df(Vec4df * vectorA, Vec4df * vectorB, Vec4df * vectorC)
{
	vectorC->x = vectorA->x + vectorB->x;
	vectorC->y = vectorA->y + vectorB->y; 
	vectorC->z = vectorA->z + vectorB->z;
	vectorC->t = vectorA->t + vectorB->t;
}
//------------------------------------------------------------
//Subtract B from A result in C
//------------------------------------------------------------
void vecSub3df(Vec3df * vectorA, Vec3df * vectorB, Vec3df * vectorC)
{
	vectorC->x = vectorA->x - vectorB->x;
	vectorC->y = vectorA->y - vectorB->y; 
	vectorC->z = vectorA->z - vectorB->z;
}
void vecSub4df(Vec4df * vectorA, Vec4df * vectorB, Vec4df * vectorC)
{
	vectorC->x = vectorA->x - vectorB->x;
	vectorC->y = vectorA->y - vectorB->y; 
	vectorC->z = vectorA->z - vectorB->z;
	vectorC->t = vectorA->t - vectorB->t;
}
//------------------------------------------------------------
//Multiply vector A by scalar result in B 
//------------------------------------------------------------
void vecMultiplyScalar3df(Vec3df * vectorA, Vec3df * vectorB, float scalar)
{
	vectorB->x = vectorA->x*scalar;
	vectorB->y = vectorA->y*scalar; 
	vectorB->z = vectorA->z*scalar;
}
void vecMultiplyScalar4df(Vec4df * vectorA, Vec4df * vectorB, float scalar)
{
	vectorB->x = vectorA->x*scalar;
	vectorB->y = vectorA->y*scalar; 
	vectorB->z = vectorA->z*scalar;
	vectorB->t = vectorA->t*scalar;
}
//------------------------------------------------------------
//Cross product A with B result in C 
//------------------------------------------------------------
void vecCrossProduct3df(Vec3df * vectorA, Vec3df * vectorB, Vec3df * vectorC)
{
	vectorC->x = (vectorA->y*vectorB->z) - (vectorB->y*vectorA->z);
	vectorC->y = -((vectorA->z*vectorB->x) - (vectorB->z*vectorA->x));
	vectorC->z = (vectorA->x*vectorB->y) - (vectorB->x*vectorA->y);
}
void vecCrossProduct4df(Vec4df * vectorA, Vec4df * vectorB, Vec4df * vectorC)
{
	vectorC->x = (vectorA->y*vectorB->z) - (vectorB->y*vectorA->z);
	vectorC->y = -((vectorA->z*vectorB->x) - (vectorB->z*vectorA->x));
	vectorC->z = (vectorA->x*vectorB->y) - (vectorB->x*vectorA->y);
}
//------------------------------------------------------------
//Dot product 
//------------------------------------------------------------
void vecDotProduct3df(Vec3df * vectorA, Vec3df * vectorB, float * scalar)
{
	*scalar = (vectorA->x*vectorB->x)+ (vectorA->y*vectorB->y)+(vectorA->z*vectorB->z);
}
void vecDotProduct4df(Vec4df * vectorA, Vec4df * vectorB, float * scalar)
{
	*scalar = (vectorA->x*vectorB->x)+ (vectorA->y*vectorB->y)+(vectorA->z*vectorB->z)+(vectorA->t*vectorB->t);
}

float vecLength3df(Vec3df * vectorA)
{
	return (float)sqrt((vectorA->x*vectorA->x) + (vectorA->y*vectorA->y) + (vectorA->z*vectorA->z));
}
float vecLength4df(Vec4df * vectorA)
{
	return (float)sqrt((vectorA->x*vectorA->x) + (vectorA->y*vectorA->y) + (vectorA->z*vectorA->z));
}

void vecRotX3df(Vec3df * vectorA, float theta)
{
	vectorA->x =  (vectorA->x*1);
	vectorA->y =  (vectorA->y*(float)cos(theta))+(vectorA->z*(float)sin(theta)) ;
	vectorA->z =  (vectorA->y*-(float)sin(theta))+(vectorA->z*(float)cos(theta));
}
void vecRotX4df(Vec4df * vectorA, float theta)
{
	vectorA->x =  (vectorA->x*1);
	vectorA->y =  (vectorA->y*(float)cos(theta))+(vectorA->z*(float)sin(theta)) ;
	vectorA->z =  (vectorA->y*-(float)sin(theta))+(vectorA->z*(float)cos(theta));
	vectorA->t =  vectorA->t;
}

void vecRotY3df(Vec3df * vectorA, float theta)
{
	vectorA->x =  (vectorA->x*(float)cos(theta))+(vectorA->z*(float)sin(theta));
	vectorA->y =  (vectorA->y*1) ;
	vectorA->z =  (vectorA->x*-(float)sin(theta))+(vectorA->z*(float)cos(theta));
}
void vecRotY4df(Vec4df * vectorA, float theta)
{
	vectorA->x =  (vectorA->x*(float)cos(theta))+(vectorA->z*(float)sin(theta));
	vectorA->y =  (vectorA->y*1) ;
	vectorA->z =  (vectorA->x*-(float)sin(theta))+(vectorA->z*(float)cos(theta));
	vectorA->t =  vectorA->t;
}

void vecRotZ3df(Vec3df * vectorA, float theta)
{
	vectorA->x =  (vectorA->x*(float)cos(theta))+(vectorA->y*(float)sin(theta)) ;
	vectorA->y =  (vectorA->x*-(float)sin(theta))+(vectorA->y*(float)cos(theta));
	vectorA->z =  (vectorA->z*1);
}
void vecRotZ4df(Vec4df * vectorA, float theta)
{
	vectorA->x =  (vectorA->x*(float)cos(theta))+(vectorA->y*(float)sin(theta)) ;
	vectorA->y =  (vectorA->x*-(float)sin(theta))+(vectorA->y*(float)cos(theta));
	vectorA->z =  (vectorA->z*1);
	vectorA->t =  (vectorA->t*1);
}

//Rotates Vector A around Vector B by angle theta
void vecRotVec3df(Vec3df * vectorA, Vec3df * vectorB, float theta)
{
	float c,s,t;
	Vec3df vectorNew;

	// Calculate the sine and cosine of the angle once
	c = (float)cos(theta);
	s = (float)sin(theta);
	t = 1 - c;

	// Find the new x position for the new rotated point
	vectorNew.x  = (c + (t * vectorB->x * vectorB->x))		* vectorA->x;
	vectorNew.x += ((t * vectorB->x * vectorB->y) - vectorB->z * s)	* vectorA->y;
	vectorNew.x += ((t * vectorB->x * vectorB->z) + vectorB->y * s)	* vectorA->z;

	// Find the new y position for the new rotated point
	vectorNew.y  = ((t * vectorB->x * vectorB->y) + vectorB->z * s)	* vectorA->x;
	vectorNew.y += (c + (t * vectorB->y * vectorB->y))		* vectorA->y;
	vectorNew.y += ((t * vectorB->y * vectorB->z) - vectorB->x * s)	* vectorA->z;

	// Find the new z position for the new rotated point
	vectorNew.z  = ((t * vectorB->x * vectorB->z) - vectorB->y * s)	* vectorA->x;
	vectorNew.z += ((t * vectorB->y * vectorB->z) + vectorB->x * s)	* vectorA->y;
	vectorNew.z += (c + (t * vectorB->z * vectorB->z))		* vectorA->z;

	vectorA->x = vectorNew.x;
	vectorA->y = vectorNew.y;
	vectorA->z = vectorNew.z;
}