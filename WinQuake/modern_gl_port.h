#ifndef _MODERN_GL_PORT_H_
#define _MODERN_GL_PORT_H_

#ifdef _WIN32
static const int glMajor = 4, glMinor = 2;
#include <GL/glew.h>
#include <GL/glu.h>

#define float_type float
#define float_size sizeof(float)

#define QURP_FLOAT GL_FLOAT

#else
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#ifndef GL_RED
#define GL_RED  GL_ALPHA
#endif 

#ifndef GL_RG
#define GL_RG  GL_LUMINANCE_ALPHA
#endif 

#if USE_HALF_FLOATS
#define float_type __fp16
#define float_size 2
#define QURP_FLOAT GL_HALF_FLOAT_OES
#else
#define float_type float
#define float_size sizeof(float)
#define QURP_FLOAT GL_FLOAT
#endif

#endif

typedef struct 
{
	unsigned char	st[2];
	unsigned char	pos[3];
	unsigned char	lightNormalIndex;
} glAliasData;


typedef struct 
{
	float_type	pos[3];
	float_type	st1[2];
	float_type	st2[2];
} glBrushData;


#define POSITION_LOCATION		0
#define COLOUR_LOCATION			1
#define NORMAL_LOCATION			2
#define UV_LOCATION0			3
#define UV_LOCATION1			4
#define SHADE_LOCATION			5

#define TEXT_LOCATION			0
//#define JOINT_WEIGHT_LOCATION	3
//#define JOINT_INDEX_LOCATION	4

#define LINEAR_TEXTURES			0

#define TEX_SLOT_CLR			0
#define TEX_SLOT_ANORM			1
#define TEX_SLOT_LIGHT_0		2
#define TEX_SLOT_LIGHT_1		3
#define TEX_SLOT_LIGHT_2		4
#define TEX_SLOT_LIGHT_3		5
#define TEX_SLOT_SKY			6
#define TEX_SLOT_SKY_ALPHA		7

#define SUBDIVIDE_WARP_POLYS 1

#define TEMP_INDEX_BUFFER unsigned short buffer[3*19] = {\
0, 1, 2,\
0, 2, 3,\
0, 3, 4,\
0, 4, 5,\
0, 5, 6,\
0, 6, 7,\
0, 7, 8,\
0, 8, 9,\
0, 9,10,\
0,10,11,\
0,11,12,\
0,12,13,\
0,13,14,\
0,14,15,\
0,15,16,\
0,16,17,\
0,17,18,\
0,18,19,\
0,19,20,\
}, i=0;

typedef enum {
	VAS_CLR = 0,	//vtx + clr (single colour)
	VAS_CLR_TEX,	//vtx + clr + tex (single colour)
	VAS_VTX_CLR,	//vtx + clr (per vertex colour)	
	VAS_VTX_CLR_TEX,//vtx + clr + tex(per vertex colour)	
	VAS_VTX_CLR8_TEX,//vtx + clr + tex(per vertex 8bit colour)	
	VAS_MAX,
} VertexAttributeState;

typedef enum 
{
	STREAM_CHAR = 0,
	STREAM_UCHAR,
	STREAM_SHORT,
	STREAM_INT,
	STREAM_FLOAT
}SteamType;

typedef enum 
{
	VTX_POSITION,
	VTX_COLOUR,
	VTX_TEXTURE,
}VtxDataType;

typedef enum 
{
	RNDR_POINTS = 0,
	RNDR_TRIANGLES,
	RNDR_TRIANGLE_STRIP,
	RNDR_TRIANGLE_FAN,
}RenderMode;

typedef struct _VertexAttribute
{
	unsigned int	vbo;	// if 0 use the currently bound VBO
	unsigned int	idx;	// could probably be a char?

	unsigned int	size;	// usually 3 or 4
	SteamType		type;	//also a char?
	unsigned int	normalized; //0 false, 1 true //could be a char?
	unsigned int	stride;		
	unsigned int	offset;
	unsigned int	array_offset;
	unsigned int	divisor; //for instancing	
}VertexAttribute;


#define MAX_ELEMENTS  4096
#define MAX_ELEMENT_BUFFERS  3
typedef struct _BatchElements
{
	unsigned short index;
	unsigned short bufferIndex;
	unsigned short element[MAX_ELEMENT_BUFFERS][MAX_ELEMENTS];
}BatchElements;

#ifdef _WIN32
	static GLenum texDataType[5] = {GL_RED, GL_RED, GL_RG, GL_RGB, GL_RGBA};
#else
	static GLenum texDataType[5] = {GL_ALPHA, GL_ALPHA, GL_LUMINANCE_ALPHA, GL_RGB, GL_RGBA};
#endif

extern void StartupModernGLPatch(const int width, const int height);

extern void ResetDrawCallCounter();
extern unsigned int GetDrawCallCounter();
extern unsigned int GetDrawCallSize();

extern void SetRenderSize(const unsigned int w, const unsigned int h);
extern void SetViewport(const unsigned int x, const unsigned int y, const unsigned int w, const unsigned int h);
extern void SetOrthoMatrix(const float l,const float r,const float b,const float t,const float n,const float f);
extern void SetProjectionMatrix(const float l,const float r,const float b,const float t,const float n,const float f);

extern void SetVertexMode(const VertexAttributeState state);

extern void BeginDrawing(const RenderMode rm);
extern void EndDrawing();

extern void EnableBlending();
extern void DisableBlending();
extern void EnableDepth();
extern void DisableDepth();

extern void EnableTexture();
extern void DisableTexture();
extern void EnableCulling();
extern void DisableCulling();
extern void EnableAlphaTest();
extern void DisableAlphaTest();

extern void CullFront(void);
extern void CullBack(void);

extern void SetBlendFunc(const GLenum s, const GLenum d);
extern void SetDepthMask(const int);
extern void SetDepthRange(const float, const float);
extern void SetLightmapMode(const int);

extern void AddVertex2D(const VtxDataType type, const float x, const float y);
extern void AddVertex3D(const VtxDataType type, const float x, const float y, const float z);
extern void AddVertex4D(const VtxDataType type, const float x, const float y, const float z, const float w);
extern void AddVertex4Db(const VtxDataType type, const unsigned char r, const unsigned char g, const unsigned char b, const unsigned char a);

extern void Push(void);
extern void Pop(void);
extern void Identity(void);
extern void Translate(const float, const float, const float);
extern void Scale(const float, const float, const float);
extern void SetMatrix(const float mtx[16]);
extern void TransformMatrix(const float mtx[16]);

extern void FlushDraw(void);

extern void BlitFBO(const int windowWidth, const int windowHeight);

extern void CreatAliasBuffers(int* pVboOffset, int numVerts, void * pData);
extern void StartAliasBatch(float depthmin, float depthmax);
extern void RenderAlias(const int vbo_offset, const int posenum, const int numTris, int shadeDotIndex, float shadeLight, float ambientLight);
extern void EndAliasBatch();

extern void CreateBrushBuffers(int numVerts);
extern void AddBrushData(int vertexOffset, int numVerts, void * pData);
extern void StartBrushBatch(float depthmin, float depthmax);
extern void SetupWarpBatch();
extern void SetupSkyBatch();
extern void SetupColourPass();
extern void SetupLightMapPass();
extern void RenderBrushData(int vertexOffset, int numVerts);
extern void RenderBrushDataElements(unsigned short *pIndices, int numElements);
extern void EndBrushBatch();

extern void ShutdownModernGLPatch();

extern void SpecialDebugState();
extern void UpdateTransformUBOs();
extern void SetRenderOrigin(float x, float y, float z);
extern void SetRealTime(float time);
extern void SetGamma(float gamma);

#endif// _MODERN_GL_PORT_H_