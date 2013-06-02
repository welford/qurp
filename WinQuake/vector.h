//------------------------------------------------------------
// vector.c 
//
// By James Anderson 2002(ish)~2007
//
// C style vector library 
//------------------------------------------------------------


#ifndef _VECTOR_H_
#define _VECTOR_H_

#ifdef __cplusplus 
extern "C" {
#endif

#define V_PI 3.14159265359
#define DEG_TO_RAD(x) ((x*V_PI)/180.0f)

//------------------------------------------------------------
//Vectors
//------------------------------------------------------------
typedef struct
{
	float x,y,z,t;
} Vec4df;

typedef struct
{
	float x,y,z;
} Vec3df;

typedef struct
{
	float x,y;
} Vec2df;

typedef struct
{
	long x,y;
} Vec2di;

//------------------------------------------------------------
//Matrices
//------------------------------------------------------------
typedef union
{
	struct 
	{
		Vec2df x,y;
	}e;
	float a[4];
}Matrix22;

typedef union
{
	struct 
	{
		Vec3df x,y,z;
	}e;
	float a[9];
}Matrix33;

typedef union
{
	struct 
	{
		Vec4df x,y,z,t;
	}e;
	float a[16];
}Matrix44;

//------------------------------------------------------------
//Quaternions
//------------------------------------------------------------
typedef union _Quaternion
{
	float x,y,z,w;
	float a[4];
}Quaternion;

//Vector Functions
extern float vecLength3df(Vec3df * vector);

extern void vecNormalise3df(Vec3df * vector);
extern void vecNormalise4df(Vec4df * vector);

extern void vecAdd3df(Vec3df * vectorA, Vec3df * vectorB, Vec3df * vectorC);
extern void vecAdd4df(Vec4df * vectorA, Vec4df * vectorB, Vec4df * vectorC);

extern void vecSub3df(Vec3df * vectorA, Vec3df * vectorB, Vec3df * vectorC);
extern void vecSub4df(Vec4df * vectorA, Vec4df * vectorB, Vec4df * vectorC);

extern void vecMultiplyScalar3df(Vec3df * vectorA, Vec3df * vectorB, float scalar);
extern void vecMultiplyScalar4df(Vec4df * vectorA, Vec4df * vectorB, float scalar);

extern void vecCrossProduct3df(Vec3df * vectorA, Vec3df * vectorB, Vec3df * vectorC);
extern void vecCrossProduct4df(Vec4df * vectorA, Vec4df * vectorB, Vec4df * vectorC);

extern void vecDotProduct3df(Vec3df * vectorA, Vec3df * vectorB, float * scalar);
extern void vecDotProduct4df(Vec4df * vectorA, Vec4df * vectorB, float * scalar);

extern float vecLength3df(Vec3df * vectorA);
extern float vecLength4df(Vec4df * vectorA);

extern void vecRotX3df(Vec3df * vectorA, float theta);
extern void vecRotY3df(Vec3df * vectorA, float theta);
extern void vecRotZ3df(Vec3df * vectorA, float theta);

extern void vecRotX4df(Vec4df * vectorA, float theta);
extern void vecRotY4df(Vec4df * vectorA, float theta);
extern void vecRotZ4df(Vec4df * vectorA, float theta);

extern void vecRotVec3df(Vec3df * vectorA, Vec3df * vectorB, float theta);

//Matrix Functions
extern void matMultiply22(Matrix22 * matA,  Matrix22 * matB, Matrix22 * matR);
extern void matMultiply33(Matrix33 * matA,  Matrix33 * matB, Matrix33 * matR);
extern void matMultiply33Inv(Matrix33 * matA,  Matrix33 * matB, Matrix33 * matR);
extern void matMultiply44(Matrix44 * matA,  Matrix44 * matB, Matrix44 * matR);
extern void matMultiply44IgnoreT(Matrix44 * matA,  Matrix44 * matB, Matrix44 * matR);
extern void matMultiply44InvIgnoreT(Matrix44 * matA,  Matrix44 * matB, Matrix44 * matR);

extern void matMultiplyVector33(Matrix33 * matA, Vec3df * vecA, Vec3df * vecR);
extern void matMultiplyVector33Inv(Matrix33 * matA, Vec3df * vecA, Vec3df * vecR);
extern void matMultiplyVector44(Matrix44 * matA, Vec3df * vecA, Vec3df * vecR);
extern void matMultiplyVector44Inv(Matrix44 * matA, Vec3df * vecA, Vec3df * vecR);

extern void matRotateX22(float theta, Matrix22 * matR);
extern void matRotateX33(float theta, Matrix33 * matR);
extern void matRotateX44(float theta, Matrix44 * matR);

extern void matRotateY22(float theta, Matrix22 * matR);
extern void matRotateY33(float theta, Matrix33 * matR);
extern void matRotateY44(float theta, Matrix44 * matR);

extern void matRotateZ33(float theta, Matrix33 * matR);
extern void matRotateZ44(float theta, Matrix44 * matR);

extern void matSetIdentity33(Matrix33 * matR);
extern void matSetIdentity44(Matrix44 * matR);

extern void matNormalise33(Matrix33 * matR);
extern void matNormalise44(Matrix44 * matR);
extern void matNormaliseRotation44(Matrix44 * matR);

extern void matSetMatrix33(Vec3df * vecX, Vec3df * vecY, Vec3df * vecZ, Matrix33 * matR);
extern void matSetMatrix44(Vec4df * vecX, Vec4df * vecY, Vec4df * vecZ, Vec4df * vecT, Matrix44 * matR);
extern void matSetFromArray33(float m[9],Matrix33 * matA);
extern void matSetFromArray44(float m[16],Matrix44 * matA);
extern void matSetMatrixRotation44(Vec4df * vecX, Vec4df * vecY, Vec4df * vecZ, Matrix44 * matR);
extern void matSetMatrixTranslation44(Vec4df * vecT, Matrix44 * matR);
extern void matMatrixTranslate44(const float x, const float y, const float z, Matrix44 * matR);

extern void matReturnArray33(Matrix33 * matA, float m[9]);
extern void matReturnArray44(Matrix44 * matA, float m[16]);

extern void matReturnInverseArray33(Matrix33 * matA, float m[9]);
extern void matReturnInverseArray44(Matrix44 * matA, float m[16]);

extern void matCopy44(Matrix44 * matO,Matrix44 * matC);
extern void matCopyInverse44(Matrix44 * matO,Matrix44 * matC);
extern void matCopyInverse44IgnoreT(Matrix44 * matO,Matrix44 * matC);
extern void matCopy33(Matrix33 * matO,Matrix33 * matC);
extern void matCopyInverse33(Matrix33 * matO,Matrix33 * matC);
extern void matCopy44to33(Matrix44 * matO,Matrix33 * matC);

extern void matScale44(Matrix44 * matO,float n,Matrix44 * matC);
extern void matScaleXYZ44(const float sx, const float sy, const float sz, Matrix44 * matC);
extern void matScale33(Matrix33 * matO,float n,Matrix33 * matC);
extern void matScale44to33(Matrix44 * matO,float n,Matrix33 * matC);

#ifdef __cplusplus 
}
#endif

#endif _VECTOR_H_