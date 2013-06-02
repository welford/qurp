//------------------------------------------------------------
//
// matrix.c 
//
// by James W. Anderson 2002(ish)~2007
//
// C style vector library 
//------------------------------------------------------------


#include <math.h>
#include "vector.h"

//------------------------------------------------------------
//Multiplies 3 by 3 Matrices A and B returns result in R
//------------------------------------------------------------
void matMultiply33(Matrix33 * matA,  Matrix33 * matB, Matrix33 * matR)
{
	Matrix33 result;

	result.e.x.x = (matA->e.x.x*matB->e.x.x)+ (matA->e.x.y*matB->e.y.x)+(matA->e.x.z*matB->e.z.x);
	result.e.x.y = (matA->e.x.x*matB->e.x.y)+ (matA->e.x.y*matB->e.y.y)+(matA->e.x.z*matB->e.z.y);
	result.e.x.z = (matA->e.x.x*matB->e.x.z)+ (matA->e.x.y*matB->e.y.z)+(matA->e.x.z*matB->e.z.z);

	result.e.y.x = (matA->e.y.x*matB->e.x.x)+ (matA->e.y.y*matB->e.y.x)+(matA->e.y.z*matB->e.z.x);
	result.e.y.y = (matA->e.y.x*matB->e.x.y)+ (matA->e.y.y*matB->e.y.y)+(matA->e.y.z*matB->e.z.y);
	result.e.y.z = (matA->e.y.x*matB->e.x.z)+ (matA->e.y.y*matB->e.y.z)+(matA->e.y.z*matB->e.z.z);

	result.e.z.x = (matA->e.z.x*matB->e.x.x)+ (matA->e.z.y*matB->e.y.x)+(matA->e.z.z*matB->e.z.x);
	result.e.z.y = (matA->e.z.x*matB->e.x.y)+ (matA->e.z.y*matB->e.y.y)+(matA->e.z.z*matB->e.z.y);
	result.e.z.z = (matA->e.z.x*matB->e.x.z)+ (matA->e.z.y*matB->e.y.z)+(matA->e.z.z*matB->e.z.z);

	*matR = result;

}

//------------------------------------------------------------
// matB is inverted
//------------------------------------------------------------
void matMultiply33Inv(Matrix33 * matA,  Matrix33 * matB, Matrix33 * matR)
{
	Matrix33 result;

	result.e.x.x = (matA->e.x.x*matB->e.x.x)+ (matA->e.x.y*matB->e.x.y)+(matA->e.x.z*matB->e.x.z);
	result.e.x.y = (matA->e.x.x*matB->e.y.x)+ (matA->e.x.y*matB->e.y.y)+(matA->e.x.z*matB->e.y.z);
	result.e.x.z = (matA->e.x.x*matB->e.z.x)+ (matA->e.x.y*matB->e.z.y)+(matA->e.x.z*matB->e.z.z);

	result.e.y.x = (matA->e.y.x*matB->e.x.x)+ (matA->e.y.y*matB->e.x.y)+(matA->e.y.z*matB->e.x.z);
	result.e.y.y = (matA->e.y.x*matB->e.y.x)+ (matA->e.y.y*matB->e.y.y)+(matA->e.y.z*matB->e.y.z);
	result.e.y.z = (matA->e.y.x*matB->e.z.x)+ (matA->e.y.y*matB->e.z.y)+(matA->e.y.z*matB->e.z.z);

	result.e.z.x = (matA->e.z.x*matB->e.x.x)+ (matA->e.z.y*matB->e.x.y)+(matA->e.z.z*matB->e.x.z);
	result.e.z.y = (matA->e.z.x*matB->e.y.x)+ (matA->e.z.y*matB->e.y.y)+(matA->e.z.z*matB->e.y.z);
	result.e.z.z = (matA->e.z.x*matB->e.z.x)+ (matA->e.z.y*matB->e.z.y)+(matA->e.z.z*matB->e.z.z);

	*matR = result;
}

//------------------------------------------------------------
//Multiplies 4 by 4 Matrices A and B returns result in R
//------------------------------------------------------------
void matMultiply44(Matrix44 * matA,  Matrix44 * matB, Matrix44 * matR)
{
	Matrix44 result;

	result.e.x.x = (matA->e.x.x*matB->e.x.x)+ (matA->e.x.y*matB->e.y.x)+(matA->e.x.z*matB->e.z.x)+(matA->e.x.t*matB->e.t.x);
	result.e.x.y = (matA->e.x.x*matB->e.x.y)+ (matA->e.x.y*matB->e.y.y)+(matA->e.x.z*matB->e.z.y)+(matA->e.x.t*matB->e.t.y);
	result.e.x.z = (matA->e.x.x*matB->e.x.z)+ (matA->e.x.y*matB->e.y.z)+(matA->e.x.z*matB->e.z.z)+(matA->e.x.t*matB->e.t.z);
	result.e.x.t = (matA->e.x.x*matB->e.x.t)+ (matA->e.x.y*matB->e.y.t)+(matA->e.x.z*matB->e.z.t)+(matA->e.x.t*matB->e.t.t);

	result.e.y.x = (matA->e.y.x*matB->e.x.x)+ (matA->e.y.y*matB->e.y.x)+(matA->e.y.z*matB->e.z.x)+(matA->e.y.t*matB->e.t.x);
	result.e.y.y = (matA->e.y.x*matB->e.x.y)+ (matA->e.y.y*matB->e.y.y)+(matA->e.y.z*matB->e.z.y)+(matA->e.y.t*matB->e.t.y);
	result.e.y.z = (matA->e.y.x*matB->e.x.z)+ (matA->e.y.y*matB->e.y.z)+(matA->e.y.z*matB->e.z.z)+(matA->e.y.t*matB->e.t.z);
	result.e.y.t = (matA->e.y.x*matB->e.x.t)+ (matA->e.y.y*matB->e.y.t)+(matA->e.y.z*matB->e.z.t)+(matA->e.y.t*matB->e.t.t);

	result.e.z.x = (matA->e.z.x*matB->e.x.x)+ (matA->e.z.y*matB->e.y.x)+(matA->e.z.z*matB->e.z.x)+(matA->e.z.t*matB->e.t.x);
	result.e.z.y = (matA->e.z.x*matB->e.x.y)+ (matA->e.z.y*matB->e.y.y)+(matA->e.z.z*matB->e.z.y)+(matA->e.z.t*matB->e.t.y);
	result.e.z.z = (matA->e.z.x*matB->e.x.z)+ (matA->e.z.y*matB->e.y.z)+(matA->e.z.z*matB->e.z.z)+(matA->e.z.t*matB->e.t.z);
	result.e.z.t = (matA->e.z.x*matB->e.x.t)+ (matA->e.z.y*matB->e.y.t)+(matA->e.z.z*matB->e.z.t)+(matA->e.z.t*matB->e.t.t);

	result.e.t.x = (matA->e.t.x*matB->e.x.x)+ (matA->e.t.y*matB->e.y.x)+(matA->e.t.z*matB->e.z.x)+(matA->e.t.t*matB->e.t.x);
	result.e.t.y = (matA->e.t.x*matB->e.x.y)+ (matA->e.t.y*matB->e.y.y)+(matA->e.t.z*matB->e.z.y)+(matA->e.t.t*matB->e.t.y);
	result.e.t.z = (matA->e.t.x*matB->e.x.z)+ (matA->e.t.y*matB->e.y.z)+(matA->e.t.z*matB->e.z.z)+(matA->e.t.t*matB->e.t.z);
	result.e.t.t = (matA->e.t.x*matB->e.x.t)+ (matA->e.t.y*matB->e.y.t)+(matA->e.t.z*matB->e.z.t)+(matA->e.t.t*matB->e.t.t);

	*matR = result;
}

//------------------------------------------------------------
//Multiplies 4 by 4 Matrices A and B returns result in R
//------------------------------------------------------------
void matMultiply44IgnoreT(Matrix44 * matA,  Matrix44 * matB, Matrix44 * matR)
{
	Matrix44 result;

	result.e.x.x = (matA->e.x.x*matB->e.x.x)+ (matA->e.x.y*matB->e.y.x)+(matA->e.x.z*matB->e.z.x);
	result.e.x.y = (matA->e.x.x*matB->e.x.y)+ (matA->e.x.y*matB->e.y.y)+(matA->e.x.z*matB->e.z.y);
	result.e.x.z = (matA->e.x.x*matB->e.x.z)+ (matA->e.x.y*matB->e.y.z)+(matA->e.x.z*matB->e.z.z);
	result.e.x.t = 0.0f;

	result.e.y.x = (matA->e.y.x*matB->e.x.x)+ (matA->e.y.y*matB->e.y.x)+(matA->e.y.z*matB->e.z.x);
	result.e.y.y = (matA->e.y.x*matB->e.x.y)+ (matA->e.y.y*matB->e.y.y)+(matA->e.y.z*matB->e.z.y);
	result.e.y.z = (matA->e.y.x*matB->e.x.z)+ (matA->e.y.y*matB->e.y.z)+(matA->e.y.z*matB->e.z.z);
	result.e.y.t = 0.0f;

	result.e.z.x = (matA->e.z.x*matB->e.x.x)+ (matA->e.z.y*matB->e.y.x)+(matA->e.z.z*matB->e.z.x);
	result.e.z.y = (matA->e.z.x*matB->e.x.y)+ (matA->e.z.y*matB->e.y.y)+(matA->e.z.z*matB->e.z.y);
	result.e.z.z = (matA->e.z.x*matB->e.x.z)+ (matA->e.z.y*matB->e.y.z)+(matA->e.z.z*matB->e.z.z);
	result.e.z.t = 0.0f;

	result.e.t.x = 0.0f;
	result.e.t.y = 0.0f;
	result.e.t.z = 0.0f;
	result.e.t.t = 1.0f;

	*matR = result;
}

//------------------------------------------------------------
// matB is inverted!
//------------------------------------------------------------
void matMultiply44InvIgnoreT(Matrix44* matA,  Matrix44 * matB, Matrix44 * matR)
{
	Matrix44 result;

	result.e.x.x = (matA->e.x.x*matB->e.x.x)+ (matA->e.x.y*matB->e.x.y)+(matA->e.x.z*matB->e.x.z);
	result.e.x.y = (matA->e.x.x*matB->e.y.x)+ (matA->e.x.y*matB->e.y.y)+(matA->e.x.z*matB->e.y.z);
	result.e.x.z = (matA->e.x.x*matB->e.z.x)+ (matA->e.x.y*matB->e.z.y)+(matA->e.x.z*matB->e.z.z);
	result.e.x.t = 0.0f;

	result.e.y.x = (matA->e.y.x*matB->e.x.x)+ (matA->e.y.y*matB->e.x.y)+(matA->e.y.z*matB->e.x.z);
	result.e.y.y = (matA->e.y.x*matB->e.y.x)+ (matA->e.y.y*matB->e.y.y)+(matA->e.y.z*matB->e.y.z);
	result.e.y.z = (matA->e.y.x*matB->e.z.x)+ (matA->e.y.y*matB->e.z.y)+(matA->e.y.z*matB->e.z.z);
	result.e.y.t = 0.0f;

	result.e.z.x = (matA->e.z.x*matB->e.x.x)+ (matA->e.z.y*matB->e.x.y)+(matA->e.z.z*matB->e.x.z);
	result.e.z.y = (matA->e.z.x*matB->e.y.x)+ (matA->e.z.y*matB->e.y.y)+(matA->e.z.z*matB->e.y.z);
	result.e.z.z = (matA->e.z.x*matB->e.z.x)+ (matA->e.z.y*matB->e.z.y)+(matA->e.z.z*matB->e.z.z);
	result.e.z.t = 0.0f;

	*matR = result;

	result.e.t.x = 0.0f;
	result.e.t.y = 0.0f;
	result.e.t.z = 0.0f;
	result.e.t.t = 1.0f;

	*matR = result;
}

void matMultiplyVector33(Matrix33 * matA, Vec3df * vecA, Vec3df * vecR)
{
	Vec3df result;

	result.x = (vecA->x*matA->e.x.x)+ (vecA->y*matA->e.y.x)+(vecA->z*matA->e.z.x);
	result.y = (vecA->x*matA->e.x.y)+ (vecA->y*matA->e.y.y)+(vecA->z*matA->e.z.y);
	result.z = (vecA->x*matA->e.x.z)+ (vecA->y*matA->e.y.z)+(vecA->z*matA->e.z.z);	

	*vecR = result;
}

void matMultiplyVector33Inv(Matrix33 * matA, Vec3df * vecA, Vec3df * vecR)
{
	Vec3df result;

	result.x = (vecA->x*matA->e.x.x)+ (vecA->y*matA->e.x.y)+(vecA->z*matA->e.x.z);
	result.y = (vecA->x*matA->e.y.x)+ (vecA->y*matA->e.y.y)+(vecA->z*matA->e.y.z);
	result.z = (vecA->x*matA->e.z.x)+ (vecA->y*matA->e.z.y)+(vecA->z*matA->e.z.z);	

	*vecR = result;
}

void matMultiplyVector44(Matrix44 * matA, Vec3df * vecA, Vec3df * vecR)
{
	Vec3df result;

	result.x = (vecA->x*matA->e.x.x)+ (vecA->y*matA->e.y.x)+(vecA->z*matA->e.z.x);
	result.y = (vecA->x*matA->e.x.y)+ (vecA->y*matA->e.y.y)+(vecA->z*matA->e.z.y);
	result.z = (vecA->x*matA->e.x.z)+ (vecA->y*matA->e.y.z)+(vecA->z*matA->e.z.z);	

	*vecR = result;
}

void matMultiplyVector44Inv(Matrix44 * matA, Vec3df * vecA, Vec3df * vecR)
{
	Vec3df result;

	result.x = (vecA->x*matA->e.x.x)+ (vecA->y*matA->e.x.y)+(vecA->z*matA->e.x.z);
	result.y = (vecA->x*matA->e.y.x)+ (vecA->y*matA->e.y.y)+(vecA->z*matA->e.y.z);
	result.z = (vecA->x*matA->e.z.x)+ (vecA->y*matA->e.z.y)+(vecA->z*matA->e.z.z);	

	*vecR = result;
}
//------------------------------------------------------------
//Returns a 3 by 3 rotation Matrix by angle theta on x axis
//------------------------------------------------------------
void matRotateX33(float theta, Matrix33 * matR)
{
	matR->e.x.x = 1;
	matR->e.x.y = 0;
	matR->e.x.z = 0;

	matR->e.y.x = 0;
	matR->e.y.y = (float)cos(theta);
	matR->e.y.z = (float)sin(theta);

	matR->e.z.x = 0;
	matR->e.z.y = (float)-sin(theta);
	matR->e.z.z = (float)cos(theta);	

}
//------------------------------------------------------------
//Returns a 4 by 4 rotation Matrix by angle theta on x axis
//------------------------------------------------------------
void matRotateX44(float theta, Matrix44 * matR)
{
	matR->e.x.x = 1;
	matR->e.x.y = 0;
	matR->e.x.z = 0;
	matR->e.x.t = 0;

	matR->e.y.x = 0;
	matR->e.y.y = (float)cos(theta);
	matR->e.y.z = (float)sin(theta);
	matR->e.y.t = 0;

	matR->e.z.x = 0;
	matR->e.z.y = (float)-sin(theta);
	matR->e.z.z = (float)cos(theta);	
	matR->e.z.t= 0;

	matR->e.t.x = 0;
	matR->e.t.y = 0;
	matR->e.t.z = 0;	
	matR->e.t.t = 1;
}
//------------------------------------------------------------
//Returns a 3 by 3 rotation Matrix by angle theta on Y axis
//------------------------------------------------------------
void matRotateY33(float theta, Matrix33 * matR)
{
	matR->e.x.x = (float)cos(theta);
	matR->e.x.y = 0;
	matR->e.x.z = (float)-sin(theta);
	
	matR->e.y.x = 0;
	matR->e.y.y = 1;
	matR->e.y.z = 0;
	
	matR->e.z.x = (float)sin(theta);
	matR->e.z.y = 0;
	matR->e.z.z = (float)cos(theta);	
	
}
//------------------------------------------------------------
//Returns a 4 by 4 rotation Matrix by angle theta on Y axis
//------------------------------------------------------------
void matRotateY44(float theta, Matrix44 * matR)
{
	matR->e.x.x = (float)cos(theta);
	matR->e.x.y = 0;
	matR->e.x.z = (float)-sin(theta);
	matR->e.x.t = 0;

	matR->e.y.x = 0;
	matR->e.y.y = 1;
	matR->e.y.z = 0;
	matR->e.y.t = 0;

	matR->e.z.x = (float)sin(theta);
	matR->e.z.y = 0;
	matR->e.z.z = (float)cos(theta);	
	matR->e.z.t = 0;

	matR->e.t.x = 0;
	matR->e.t.y = 0;
	matR->e.t.z = 0;	
	matR->e.t.t = 1;

}
//------------------------------------------------------------
//Returns a 3 by 3 rotation Matrix by angle theta on Z axis
//------------------------------------------------------------
extern void matRotateZ33(float theta, Matrix33 * matR)
{
	matR->e.x.x = (float)cos(theta);
	matR->e.x.y = (float)sin(theta);
	matR->e.x.z = 0;

	matR->e.y.x = (float)-sin(theta);
	matR->e.y.y = (float)cos(theta);
	matR->e.y.z = 0;

	matR->e.z.x = 0;
	matR->e.z.y = 0;
	matR->e.z.z = 1;	

}
//------------------------------------------------------------
//Returns a 4 by 4 rotation Matrix by angle theta on Z axis
//------------------------------------------------------------
extern void matRotateZ44(float theta, Matrix44 * matR)
{

	matR->e.x.x = (float)cos(theta);
	matR->e.x.y = (float)sin(theta);
	matR->e.x.z = 0;
	matR->e.x.t = 0;

	matR->e.y.x = (float)-sin(theta);
	matR->e.y.y = (float)cos(theta);
	matR->e.y.z = 0;
	matR->e.y.t = 0;

	matR->e.z.x = 0;
	matR->e.z.y = 0;
	matR->e.z.z = 1;	
	matR->e.z.t = 0;

	matR->e.t.x = 0;
	matR->e.t.y = 0;
	matR->e.t.z = 0;	
	matR->e.t.t = 1;

}
void matSetIdentity33(Matrix33 * matR)
{
	matR->e.x.x = 1;matR->e.x.y = 0;matR->e.x.z = 0;

	matR->e.y.x = 0;matR->e.y.y = 1;matR->e.y.z = 0;

	matR->e.z.x = 0;matR->e.z.y = 0;matR->e.z.z = 1;
}

void matSetIdentity44(Matrix44 * matR)
{
	matR->e.x.x = 1;matR->e.x.y = 0;matR->e.x.z = 0;matR->e.x.t = 0;

	matR->e.y.x = 0;matR->e.y.y = 1;matR->e.y.z = 0;matR->e.y.t = 0;

	matR->e.z.x = 0;matR->e.z.y = 0;matR->e.z.z = 1;matR->e.z.t = 0;

	matR->e.t.x = 0;matR->e.t.y = 0;matR->e.t.z = 0;matR->e.t.t = 1;
}
//------------------------------------------------------------
//create a matrix from the vectors given
//------------------------------------------------------------
void matSetMatrix33(Vec3df * vecX, Vec3df * vecY, Vec3df * vecZ, Matrix33 * matR)
{
	(*matR).e.x = *vecX;
	(*matR).e.y = *vecY;
	(*matR).e.z = *vecZ;
}
//------------------------------------------------------------
//create a matrix from the vectors given
//------------------------------------------------------------
void matSetMatrix44(Vec4df * vecX, Vec4df * vecY, Vec4df * vecZ, Vec4df * vecT, Matrix44 * matR)
{
	matR->e.x = *vecX;
	matR->e.y = *vecY;
	matR->e.z = *vecZ;	
	matR->e.t = *vecT;	
}
//------------------------------------------------------------
//Set the Rotation only in a 4by4 Matrix
//------------------------------------------------------------
void matSetMatrixRotation44(Vec4df * vecX, Vec4df * vecY, Vec4df * vecZ, Matrix44 * matR)
{
	matR->e.x = *vecX;
	matR->e.y = *vecY;
	matR->e.z = *vecZ;
}

//------------------------------------------------------------
//Set the Translation only in a 4by4 matrix
//------------------------------------------------------------
void matMatrixTranslate44(const float x, const float y, const float z,  Matrix44 * matR)
{
	int j = 0;
	for (j=0;j<4;j++){
		matR->a[12+j] += x * matR->a[j] + y * matR->a[4+j] + z * matR->a[8+j]; 
	}	 	
}

//------------------------------------------------------------
//Return a Matrix into an array
//------------------------------------------------------------
void matReturnArray33(Matrix33 * matA, float m[9])
{
	m[0] = matA->e.x.x;
	m[3] = matA->e.x.y;
	m[6] = matA->e.x.z;

	m[1] = matA->e.y.x;
	m[4] = matA->e.y.y;
	m[7] = matA->e.y.z;

	m[2] = matA->e.z.x;
	m[5] = matA->e.z.y;
	m[8] = matA->e.z.z;
}
//------------------------------------------------------------
//Return a Matrix into an array
//------------------------------------------------------------
void matReturnInverseArray33(Matrix33 * matA, float m[9])
{
	m[0] = matA->e.x.x;
	m[1] = matA->e.x.y;
	m[2] = matA->e.x.z;

	m[3] = matA->e.y.x;
	m[4] = matA->e.y.y;
	m[5] = matA->e.y.z;

	m[6] = matA->e.z.x;
	m[7] = matA->e.z.y;
	m[8] = matA->e.z.z;
	
}
//------------------------------------------------------------
//Return a Matrix into an array
//------------------------------------------------------------
void matReturnArray44(Matrix44 * matA, float m[16])
{	
	m[0] = matA->e.x.x;
	m[4] = matA->e.x.y;
	m[8] = matA->e.x.z;
	m[12] = matA->e.x.t;	

	m[1] = matA->e.y.x;
	m[5] = matA->e.y.y;
	m[9] = matA->e.y.z;
	m[13] = matA->e.y.t;
	

	m[2] = matA->e.z.x;
	m[6] = matA->e.z.y;
	m[10] = matA->e.z.z;
	m[14] = matA->e.z.t;
	
	m[3] = matA->e.t.x;
	m[7] = matA->e.t.y;
	m[11] = matA->e.t.z;
	m[15] = matA->e.t.t;
}

//------------------------------------------------------------
//Return a Matrix into an array
//------------------------------------------------------------
void matReturnInverseArray44(Matrix44 * matA, float m[16])
{
	m[0] = matA->e.x.x;
	m[1] = matA->e.x.y;
	m[2] = matA->e.x.z;
	m[3] = matA->e.x.t;	

	m[4] = matA->e.y.x;
	m[5] = matA->e.y.y;
	m[6] = matA->e.y.z;
	m[7] = matA->e.y.t;
	

	m[8] = matA->e.z.x;
	m[9] = matA->e.z.y;
	m[10] = matA->e.z.z;
	m[11] = matA->e.z.t;
	
	m[12] = matA->e.t.x;
	m[13] = matA->e.t.y;
	m[14] = matA->e.t.z;
	m[15] = matA->e.t.t;				
}

//------------------------------------------------------------
//Set a Matrix from an array
//------------------------------------------------------------
void matSetFromArray44(float m[16],Matrix44 * matA)
{
	matA->e.x.x = m[0];
	matA->e.x.y = m[4];
	matA->e.x.z = m[8];
	matA->e.x.t = m[12];

	matA->e.y.x = m[1];
	matA->e.y.y = m[5];
	matA->e.y.z = m[9];
	matA->e.y.t = m[13];

	matA->e.z.x = m[2];
	matA->e.z.y = m[6];
	matA->e.z.z = m[10];
	matA->e.z.t = m[14];

	matA->e.t.x = m[3];
	matA->e.t.y = m[7];
	matA->e.t.z = m[11];
	matA->e.t.t = m[15];
}
void matSetFromArray33(float m[9],Matrix33 * matA)
{
	matA->e.x.x = m[0];
	matA->e.x.y = m[3];
	matA->e.x.z = m[6];
	
	matA->e.y.x = m[1];
	matA->e.y.y = m[4];
	matA->e.y.z = m[7];
	
	matA->e.z.x = m[2];
	matA->e.z.y = m[5];
	matA->e.z.z = m[8];
}


void matNormalise33(Matrix33 * matR)
{
	vecNormalise3df(&matR->e.x);
	vecNormalise3df(&matR->e.y);
	vecNormalise3df(&matR->e.z);
}
void matNormalise44(Matrix44 * matR)
{
	vecNormalise4df(&matR->e.x);
	vecNormalise4df(&matR->e.y);
	vecNormalise4df(&matR->e.z);
	vecNormalise4df(&matR->e.t);
}
void matNormaliseRotation44(Matrix44 * matR)
{
	vecNormalise4df(&matR->e.x);
	vecNormalise4df(&matR->e.y);
	vecNormalise4df(&matR->e.z);
}

//------------------------------------------------------------
//Copy a matrix O to C
//------------------------------------------------------------
void matCopy44(Matrix44 * matO,Matrix44 * matC)
{
	matC->e.x.x = matO->e.x.x;
	matC->e.x.y = matO->e.x.y;
	matC->e.x.z = matO->e.x.z;
	matC->e.x.t = matO->e.x.t;

	matC->e.y.x = matO->e.y.x;
	matC->e.y.y = matO->e.y.y;
	matC->e.y.z = matO->e.y.z;
	matC->e.y.t = matO->e.y.t;

	matC->e.z.x = matO->e.z.x;
	matC->e.z.y = matO->e.z.y;
	matC->e.z.z = matO->e.z.z;
	matC->e.z.t = matO->e.z.t;

	matC->e.t.x = matO->e.t.x;
	matC->e.t.y = matO->e.t.y;
	matC->e.t.z = matO->e.t.z;
	matC->e.t.t = matO->e.t.t;
}
void matCopyInverse44(Matrix44 * matO,Matrix44 * matC)
{
	matC->e.x.x = matO->e.x.x;
	matC->e.x.y = matO->e.y.x;
	matC->e.x.z = matO->e.z.x;
	matC->e.x.t = matO->e.t.x;

	matC->e.y.x = matO->e.x.y;
	matC->e.y.y = matO->e.y.y;
	matC->e.y.z = matO->e.z.y;
	matC->e.y.t = matO->e.t.y;

	matC->e.z.x = matO->e.x.z;
	matC->e.z.y = matO->e.y.z;
	matC->e.z.z = matO->e.z.z;
	matC->e.z.t = matO->e.t.z;

	matC->e.t.x = matO->e.x.t;
	matC->e.t.y = matO->e.y.t;
	matC->e.t.z = matO->e.z.t;
	matC->e.t.t = matO->e.t.t;
}

void matCopyInverse44IgnoreT(Matrix44 * matO,Matrix44 * matC)
{
	matC->e.x.x = matO->e.x.x;
	matC->e.x.y = matO->e.y.x;
	matC->e.x.z = matO->e.z.x;
	matC->e.x.t = 0.0f;

	matC->e.y.x = matO->e.x.y;
	matC->e.y.y = matO->e.y.y;
	matC->e.y.z = matO->e.z.y;
	matC->e.y.t = 0.0f;

	matC->e.z.x = matO->e.x.z;
	matC->e.z.y = matO->e.y.z;
	matC->e.z.z = matO->e.z.z;
	matC->e.z.t = 0.0f;

	matC->e.t.x = 0.0f;
	matC->e.t.y = 0.0f;
	matC->e.t.z = 0.0;
	matC->e.t.t = 1.0f;
}

void matCopy33(Matrix33 * matO,Matrix33 * matC)
{
	matC->e.x.x = matO->e.x.x;
	matC->e.x.y = matO->e.x.y;
	matC->e.x.z = matO->e.x.z;

	matC->e.y.x = matO->e.y.x;
	matC->e.y.y = matO->e.y.y;
	matC->e.y.z = matO->e.y.z;

	matC->e.z.x = matO->e.z.x;
	matC->e.z.y = matO->e.z.y;
	matC->e.z.z = matO->e.z.z;
}

void matCopyInverse33(Matrix33 * matO,Matrix33 * matC)
{
	matC->e.x.x = matO->e.x.x;
	matC->e.x.y = matO->e.y.x;
	matC->e.x.z = matO->e.z.x;

	matC->e.y.x = matO->e.x.y;
	matC->e.y.y = matO->e.y.y;
	matC->e.y.z = matO->e.z.y;

	matC->e.z.x = matO->e.x.z;
	matC->e.z.y = matO->e.y.z;
	matC->e.z.z = matO->e.z.z;
}

//------------------------------------------------------------
//Copy a matrix O to C dropping the T components
//------------------------------------------------------------
void matCopy44to33(Matrix44 * matO,Matrix33 * matC)
{
	matC->e.x.x = matO->e.x.x;
	matC->e.x.y = matO->e.x.y;
	matC->e.x.z = matO->e.x.z;
	
	matC->e.y.x = matO->e.y.x;
	matC->e.y.y = matO->e.y.y;
	matC->e.y.z = matO->e.y.z;
	
	matC->e.z.x = matO->e.z.x;
	matC->e.z.y = matO->e.z.y;
	matC->e.z.z = matO->e.z.z;	
}

//------------------------------------------------------------
// Scale Matrix O by n, copy to C
//------------------------------------------------------------
void matScale44(Matrix44 * matO,float n, Matrix44 * matC)
{
	matC->e.x.x = n*matO->e.x.x;
	matC->e.x.y = n*matO->e.x.y;
	matC->e.x.z = n*matO->e.x.z;
	matC->e.x.t = n*matO->e.x.t;

	matC->e.y.x = n*matO->e.y.x;
	matC->e.y.y = n*matO->e.y.y;
	matC->e.y.z = n*matO->e.y.z;
	matC->e.y.t = n*matO->e.y.t;

	matC->e.z.x = n*matO->e.z.x;
	matC->e.z.y = n*matO->e.z.y;
	matC->e.z.z = n*matO->e.z.z;
	matC->e.z.t = n*matO->e.z.t;

	matC->e.t.x = n*matO->e.t.x;
	matC->e.t.y = n*matO->e.t.y;
	matC->e.t.z = n*matO->e.t.z;
	matC->e.t.t = n*matO->e.t.t;
}

//------------------------------------------------------------
// Scale Matrix O by n, copy to C
//------------------------------------------------------------
void matScaleXYZ44(const float sx, const float sy, const float sz, Matrix44 * matC)
{
	int x;
	for (x = 0; x <  4; x++) matC->a[x]*=sx;
	for (x = 4; x <  8; x++) matC->a[x]*=sy;
	for (x = 8; x < 12; x++) matC->a[x]*=sz;
}

//------------------------------------------------------------
// Scale Matrix O by n, copy to C
//------------------------------------------------------------
void matScale33(Matrix33 * matO,float n, Matrix33 * matC)
{
	matC->e.x.x = n*matO->e.x.x;
	matC->e.x.y = n*matO->e.x.y;
	matC->e.x.z = n*matO->e.x.z;
	
	matC->e.y.x = n*matO->e.y.x;
	matC->e.y.y = n*matO->e.y.y;
	matC->e.y.z = n*matO->e.y.z;

	matC->e.z.x = n*matO->e.z.x;
	matC->e.z.y = n*matO->e.z.y;
	matC->e.z.z = n*matO->e.z.z;
}

//------------------------------------------------------------
// Scale Matrix44 O by n, copy to MATrix 33, C
//------------------------------------------------------------
void matScale44to33(Matrix44 * matO,float n, Matrix33 * matC)
{
	matC->e.x.x = n*matO->e.x.x;
	matC->e.x.y = n*matO->e.x.y;
	matC->e.x.z = n*matO->e.x.z;
	
	matC->e.y.x = n*matO->e.y.x;
	matC->e.y.y = n*matO->e.y.y;
	matC->e.y.z = n*matO->e.y.z;

	matC->e.z.x = n*matO->e.z.x;
	matC->e.z.y = n*matO->e.z.y;
	matC->e.z.z = n*matO->e.z.z;
}
