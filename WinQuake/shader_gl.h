#ifndef _SHADER_GL_H_
#define _SHADER_GL_H_

 #ifdef __cplusplus
 extern "C" {
 #endif

#ifdef GLQUAKE
/*
Shader
*/
typedef enum 
{
	VERT = 0,
	CTRL,
	EVAL,
	GEOM,
	FRAG,		
	INVALID = -1,
}Type;

typedef enum 
{
	TYPE,
	DELETE_STATUS,
	COMPILE_STATUS,
	LOG_LENGTH,
	SOURCE_LENGTH
}ShaderParam;

typedef unsigned int Handle;
static const Handle INVALID_SHADER_HANDLE = 0;

typedef struct
{	
	Type			type;
	unsigned char 	compiled;
	Handle		 	handle;	
}SShader;

unsigned char CreateShader(SShader* const pShader, const Type type, const char** const pSrcArray, const unsigned int n);	
void DeleteShader(SShader* const pShader);	
int Query(const SShader* const pShader, const ShaderParam param);
void GetSource(const SShader* const pShader, const unsigned int bufSize, char*const buf);
void GetLog(const SShader* const pShader, const unsigned int bufSize, char*const buf);

/*
Shader
*/
typedef enum
{
	BOOL_,
	TEXTURE,
	INT_1,
	INT_2,
	INT_3,
	INT_4,
	FLOAT_1,
	FLOAT_2,
	FLOAT_3,
	FLOAT_4,
	FLOAT_9,
	FLOAT_16
}UniformType;

typedef enum 
{
	PROGRAM_DELETE_STATUS, 
	LINK_STATUS, 
	VALIDATE_STATUS, 
	INFO_LOG_LENGTH, 
	ATTACHED_SHADERS, 
	ACTIVE_ATTRIBUTES, 
	ACTIVE_ATTRIBUTE_MAX_LENGTH, 
	ACTIVE_UNIFORMS, 
	ACTIVE_UNIFORM_BLOCKS, 
	ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH, 
	ACTIVE_UNIFORM_MAX_LENGTH, 
	PROGRAM_BINARY_LENGTH, 
	TRANSFORM_FEEDBACK_BUFFER_MODE, 
	TRANSFORM_FEEDBACK_VARYINGS, 
	TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH, 
	GEOMETRY_VERTICES_OUT, 
	GEOMETRY_INPUT_TYPE, 
	GEOMETRY_OUTPUT_TYPE
}ProgramParam;

typedef enum
{
	BINDING=0,
	SIZE_,
	NAME_LENGTH,
	BLK_ACTIVE_UNIFORMS,
	BLK_ACTIVE_UNIFORMS_INDICES,
	REFERENCED_BY_VERT_SHADER,
	REFERENCED_BY_FRAG_SHADER,
	REFERENCED_BY_GEOM_SHADER
}BlockParam;

typedef enum
{
	INTERLEAVED = 0,
	SEPARATE			
}RecordMode;


typedef unsigned int ProgramHandle;
static const ProgramHandle INVALID_PROGRAM_HANDLE = 0;
typedef int UniformHandle;	
static const UniformHandle INVALID_UNIFORM_HANDLE = -1;
typedef unsigned int UniformIndex;	
static const UniformIndex INVALID_UNIFORM_INDEX = -1;

typedef struct
{
	int				linked;
	int				validate;
	Handle			handle;
}SShaderProgram;

unsigned char CreateShaderProgram(SShaderProgram* const);
void Start(const SShaderProgram* const);
void Stop(void);
void DeleteShaderProgram(SShaderProgram* const);

unsigned char AddShaderToProgram(const SShaderProgram* const, const SShader*  pShader);
unsigned char LinkShaderProgram(SShaderProgram* const);
int GetShaderProgramInfo(const SShaderProgram* const, const ProgramParam);
void SetMRTOutput(const SShaderProgram* const, const unsigned int mrtNo, const char* name);
void SetAttributeLocation(const SShaderProgram* const, const unsigned int attrNum, const char* const name);
void RecordOutput(SShaderProgram* const, const char** const, const RecordMode);

UniformHandle GetUniformLocation(const SShaderProgram* const,const char*);
UniformIndex GetUniformIndex(const SShaderProgram* const,const char* );
int GetUniformNameByIndex(const SShaderProgram* const,const unsigned int, char*, const unsigned int);
int GetUniformInfo(const SShaderProgram* const, const int, const ProgramParam, int* const pData);


static int GetUniformBlockAlignment(void); //currently bound buffer
int GetUniformBlockLocation(const SShaderProgram* const pProgram, const char*);
void BindUniformBlock(const SShaderProgram* const pProgram, const int loc, const int binding);
int GetUniformBlockBinding(const SShaderProgram* const pProgram, const int);
int GetUniformBlockSize(const SShaderProgram* const pProgram, const int);
int GetUniformBlockInfo(const SShaderProgram* const pProgram, const int loc, const BlockParam , int* pData);


void SetTextureUnitByName(const SShaderProgram* const pProgram, const char*, const unsigned int);
void SetBoolByName(const SShaderProgram* const pProgram, const char*, const int);
void SetFloatByName(const SShaderProgram* const pProgram, const char* varName, const float* const);
void SetVec2ByName(const SShaderProgram* const pProgram, const char* varName, const float* const);
void SetVec3ByName(const SShaderProgram* const pProgram, const char* varName, const float* const);
void SetVec4ByName(const SShaderProgram* const pProgram, const char* varName, const float* const);
void SetMtx33ByName(const SShaderProgram* const pProgram, const char* varName, const float* const);
void SetMtx44ByName(const SShaderProgram* const pProgram, const char* varName, const float* const);

//already have the location
void SetTextureUnitByLocation(int loc, const unsigned int unit);
void SetBoolByLocation(int loc, const int);
void SetFloatByLocation(int loc, const float* const value);
void SetVec2ByLocation(int loc, const float* const);
void SetVec3ByLocation(int loc, const float* const);
void SetVec4ByLocation(int loc, const float* const);
void SetMtx33ByLocation(int loc, const float* const);
void SetMtx44ByLocation(int loc, const float* const);

#endif //GLQUAKE

#ifdef __cplusplus
}
#endif

#endif