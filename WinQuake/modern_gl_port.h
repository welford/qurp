#ifndef _MODERN_GL_PORT_H_
#define _MODERN_GL_PORT_H_

#ifdef _WIN32
static const int glMajor = 3, glMinor = 2;
#include <GL/glew.h>
#include <GL/glu.h>
#else
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#endif


#define POSITION_LOCATION		0
#define COLOUR_LOCATION			1
#define NORMAL_LOCATION			2
#define UV_LOCATION0			3
#define UV_LOCATION1			4
#define TEXT_LOCATION			0
//#define JOINT_WEIGHT_LOCATION	3
//#define JOINT_INDEX_LOCATION	4


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
	unsigned int	divisor; //for instancing	
}VertexAttribute;

#ifdef _WIN32
	static GLenum texDataType[5] = {GL_RED, GL_RED, GL_RG, GL_RGB, GL_RGBA};
#else
	static GLenum texDataType[5] = {GL_ALPHA, GL_ALPHA, GL_LUMINANCE_ALPHA, GL_RGB, GL_RGBA};
#endif

extern void StartupModernGLPatch();

extern void ResetDrawCallCounter();
extern unsigned int GetDrawCallCounter();
extern unsigned int GetDrawCallSize();

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
//extern void EnableAlpha();
//extern void DisableAlpha();

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

extern void ShutdownModernGLPatch();

#endif// _MODERN_GL_PORT_H_