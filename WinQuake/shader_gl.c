#ifdef _WIN32
#pragma unmanaged
#endif

#ifdef _WIN32
#include <GL/glew.h>
#else
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#endif

#include "shader_gl.h"
#include "debug_print.h"
#include <stdlib.h>

#ifdef GLQUAKE

static const GLenum enumToGLShaderTypes[] = {GL_VERTEX_SHADER, GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER, GL_GEOMETRY_SHADER, GL_FRAGMENT_SHADER};
static const GLenum enumToGLShaderParams[] = {GL_SHADER_TYPE,GL_DELETE_STATUS,GL_COMPILE_STATUS,GL_INFO_LOG_LENGTH,GL_SHADER_SOURCE_LENGTH};
static const GLenum enumToGLBlockParams[] = {GL_UNIFORM_BLOCK_BINDING,GL_UNIFORM_BLOCK_DATA_SIZE,GL_UNIFORM_BLOCK_NAME_LENGTH,GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS,GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES,GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER,GL_UNIFORM_BLOCK_REFERENCED_BY_GEOMETRY_SHADER,GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER};
static const GLenum enumToGLProgramParams[] = {GL_DELETE_STATUS, GL_LINK_STATUS, GL_VALIDATE_STATUS, GL_INFO_LOG_LENGTH, GL_ATTACHED_SHADERS, GL_ACTIVE_ATTRIBUTES, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, GL_ACTIVE_UNIFORMS, GL_ACTIVE_UNIFORM_BLOCKS, GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH, GL_ACTIVE_UNIFORM_MAX_LENGTH, GL_PROGRAM_BINARY_LENGTH, GL_TRANSFORM_FEEDBACK_BUFFER_MODE, GL_TRANSFORM_FEEDBACK_VARYINGS, GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH, GL_GEOMETRY_VERTICES_OUT, GL_GEOMETRY_INPUT_TYPE, GL_GEOMETRY_OUTPUT_TYPE};
static const GLenum enumToGLRecordMode[] = {GL_INTERLEAVED_ATTRIBS, GL_SEPARATE_ATTRIBS};
#define GetGLShaderType(A) enumToGLShaderTypes[A]


//---------------------------------
//	Shader Object
//---------------------------------
unsigned char CreateShader(SShader* const pShader, const Type type, const char** const pSrcArray, const unsigned int n)
{
	GLint compStatus = GL_FALSE;	
	pShader->type = type;
	pShader->compiled = 0;
	pShader->handle = glCreateShader(GetGLShaderType(type));
	if(pShader->handle != INVALID_SHADER_HANDLE)
	{
		//supply the source
		glShaderSource(pShader->handle, n, pSrcArray, NULL);
		//compile the shaders
		glCompileShader(pShader->handle);		
		glGetShaderiv(pShader->handle,GL_COMPILE_STATUS, &compStatus);		
		if(compStatus != GL_TRUE)
		{
			int len = Query(pShader, LOG_LENGTH);
			char* pLog = (char*)malloc(len);
			if(pLog)
			{
				GetLog(pShader, len, pLog);
				d_printf("Compile Program Error:\n");
				d_printf(pLog);			
				free(pLog);
			}
		}
		else
			pShader->compiled = 1;
	}	
	return pShader->compiled;
}

int Query(const SShader* const pShader, const ShaderParam param)
{
	GLint params;
	glGetShaderiv(pShader->handle, enumToGLShaderParams[param], &params);
	return params;

}

void GetSource(const SShader* const pShader, const unsigned int bufSize, char* const buf)
{
	GLsizei len;
	glGetShaderSource(pShader->handle, bufSize, &len, buf);
}

void GetLog(const SShader* const pShader, const unsigned int bufSize, char* const buf)
{
	GLsizei len;
	glGetShaderInfoLog(pShader->handle, bufSize, &len, buf);
}

void DeleteShader(SShader* const pShader){
	if(pShader->handle != INVALID_SHADER_HANDLE){
		glDeleteProgram(pShader->handle);
	}
}

//-------------------------------------------------
// Program (linked set of shader objects
//-------------------------------------------------

unsigned char CreateShaderProgram(SShaderProgram* const pProgram)
{
	pProgram->handle = glCreateProgram();
	pProgram->linked = 0;
	pProgram->validate = 0;
	return  pProgram->handle != INVALID_PROGRAM_HANDLE;
}


void Start(const SShaderProgram* const pProgram)
{
	glUseProgram(pProgram->handle);	
}

void Stop(void)
{
	glUseProgram(0);	
}
	

unsigned char AddShaderToProgram(const SShaderProgram* const pProgram, const SShader*  pShader)
{
	if(pProgram->handle != 0)
	{
		if(pShader->compiled)
		{
			glAttachShader(pProgram->handle,pShader->handle);
			return 1;
		}
		else
		{	//should I just compile for them? seems trivial
			d_printf("compile shader before adding:\n");
		}
	}
	return 0;

}

//void RecordOutput(const SShaderProgram* const, const char**, RecordMode);
unsigned char LinkShaderProgram(SShaderProgram* const pShader)
{
	//http://www.opengl.org/sdk/docs/man4/xhtml/glLinkProgram.xml
	glLinkProgram(pShader->handle);//try and link the program
	//glGetProgramiv(m_handle, GL_LINK_STATUS, &m_linked); //check linked status
	pShader->linked = GetShaderProgramInfo(pShader, LINK_STATUS);
	if(pShader->linked != GL_TRUE)
	{
		int len = GetShaderProgramInfo(pShader, INFO_LOG_LENGTH);
		char* pLog = (char*)malloc(len);
		if(pLog)
		{
			GLsizei length = 0;
			glGetShaderInfoLog(pShader->handle, len, &length, pLog);
			d_printf("Link Program Error:\n");
			d_printf(pLog);			
			free(pLog);		
		}
	}
	glValidateProgram(pShader->handle); //validate the program

	pShader->validate = GetShaderProgramInfo(pShader, VALIDATE_STATUS);
	if(pShader->validate != GL_TRUE)
	{
		int len = GetShaderProgramInfo(pShader, INFO_LOG_LENGTH);
		char* pLog = (char*)malloc(len);
		GLsizei length = 0;
		if(pLog)
		{
			glGetShaderInfoLog(pShader->handle, len, &length, pLog);
			d_printf("Invalid Program Error:\n");
			d_printf(pLog);			
			free(pLog);				
		}
	}
	return pShader->linked && pShader->validate;
}

int GetShaderProgramInfo(const SShaderProgram*const  pProgram, const ProgramParam param)
{
	int info = 0;		

	if(pProgram->handle == INVALID_PROGRAM_HANDLE && glIsProgram (pProgram->handle))
		return 0;	
	glGetProgramiv(pProgram->handle,enumToGLProgramParams[param],&info);
	return info;
}

void DeleteShaderProgram(SShaderProgram* const pProgram)
{
	if (glIsProgram (pProgram->handle))
		glDeleteProgram(pProgram->handle);
	pProgram->handle = INVALID_PROGRAM_HANDLE;
}

void SetMRTOutput(const SShaderProgram* const pProgram, const unsigned int mrtNo, const char* name)
{
	glBindFragDataLocation(pProgram->handle, mrtNo, name);	
}


void SetAttributeLocation(const SShaderProgram* const pProgram, const unsigned int attrNum, const char* const name)
{
	glBindAttribLocation(pProgram->handle, attrNum, name);
}

void RecordOutput(SShaderProgram* const pProgram, const char** const pOP, const RecordMode rm)
{
	int count = 0;
	const char**pCnt = pOP;

	while(pCnt)
	{
		if(*pCnt == NULL)
			break;
		count++;
		pCnt++;
		//safety fallback
		if(count > 100)
			return;
	}
	glTransformFeedbackVaryings(pProgram->handle, count, pOP, enumToGLRecordMode[rm]);
	pProgram->linked = 0; //need to relink!
}

UniformHandle GetUniformLocation(const SShaderProgram* const pProgram, const char* name)
{
	if(!(pProgram->linked && pProgram->validate))
		return INVALID_UNIFORM_HANDLE;
	return glGetUniformLocation(pProgram->handle, name);
}

UniformIndex GetUniformIndex(const SShaderProgram* const pProgram, const char* name)
{
	unsigned int idx;
	if(!(pProgram->linked && pProgram->validate))
		return INVALID_UNIFORM_HANDLE;

	glGetUniformIndices(pProgram->handle, 1, &name, &idx);
	return (idx == GL_INVALID_INDEX)? INVALID_UNIFORM_INDEX : idx;
}

int GetUniformNameByIndex(const SShaderProgram* const pProgram, const unsigned int index, char* buffer, const unsigned int bufSize)
{
	int strLength = 0;
	if(!(pProgram->linked && pProgram->validate))
		return INVALID_UNIFORM_INDEX;

	glGetActiveUniformName(pProgram->handle, index, bufSize, &strLength, buffer);
	return strLength;
}

int GetUniformInfo(const SShaderProgram* const pProgram, const int something, const ProgramParam param, int* const pData)
{
	if(!(pProgram->linked && pProgram->validate))
		return INVALID_UNIFORM_HANDLE;
	//TODO
	return 0;
}

int GetUniformBlockAlignment(void) 
{
	int align = 0;
	glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT,&align);
	return align;
}

int GetUniformBlockLocation(const SShaderProgram* const pProgram, const char* name)
{
	if(!(pProgram->linked && pProgram->validate))
		return INVALID_UNIFORM_HANDLE;
	return glGetUniformBlockIndex(pProgram->handle, name);	
}

void BindUniformBlock(const SShaderProgram* const pProgram, const int loc, const int binding)
{
	glUniformBlockBinding(pProgram->handle, loc, binding);
}

int GetUniformBlockBinding(const SShaderProgram* const pProgram, const int loc)
{
	return GetUniformBlockInfo(pProgram, loc, BINDING, 0);	
}


int GetUniformBlockSize(const SShaderProgram* const pProgram,const int loc)
{
	return GetUniformBlockInfo(pProgram, loc, SIZE, 0);	
}

int GetUniformBlockInfo(const SShaderProgram* const pProgram, const int loc, const BlockParam param, int* pData)
{
	if(!(pProgram->linked && pProgram->validate))
		return 0;

	if(pData == NULL)
	{
		int size = 0;		
		glGetActiveUniformBlockiv(pProgram->handle,loc,enumToGLBlockParams[param],&size);
		return size;
	}
	else
	{
		glGetActiveUniformBlockiv(pProgram->handle,loc,enumToGLBlockParams[param],pData);
		return *pData;
	}
}

void SetBoolByName(const SShaderProgram* const pProgram, const char* name, const int set)
{
	glUniform1i(glGetUniformLocation(pProgram->handle,name), set);
}

void SetTextureUnitByName(const SShaderProgram* const pProgram, const char* name, const unsigned int texUnit)
{
	glUniform1i(glGetUniformLocation(pProgram->handle, name), texUnit);	
}

void SetFloatByName(const SShaderProgram* const pProgram, const char* name, const float* const value)
{
	glUniform1f(glGetUniformLocation(pProgram->handle, name), *value);	
}

void SetVec2ByName(const SShaderProgram* const pProgram, const char* name, const float* const vec)
{
	glUniform2fv(glGetUniformLocation(pProgram->handle, name), 1, vec);	
}

void SetVec3ByName(const SShaderProgram* const pProgram, const char* name, const float* const vec)
{
	glUniform3fv(glGetUniformLocation(pProgram->handle, name), 1, vec);		
}

void SetVec4ByName(const SShaderProgram* const pProgram, const char* name, const float* const vec)
{
	glUniform4fv(glGetUniformLocation(pProgram->handle, name), 1, vec);
}

void SetMtx33ByName(const SShaderProgram* const pProgram, const char* name, const float* const mtx)
{
	glUniformMatrix3fv(glGetUniformLocation(pProgram->handle, name), 1, GL_FALSE, mtx);		
}

void SetMtx44ByName(const SShaderProgram* const pProgram, const char* name, const float* const mtx)
{
	glUniformMatrix4fv(glGetUniformLocation(pProgram->handle, name), 1, GL_FALSE, mtx);		
}

void SetBoolByLocation(int loc, const int set)
{
	glUniform1i(loc, set);
}

void SetTextureUnitByLocation(int loc, const unsigned int texUnit)
{
	glUniform1i(loc, texUnit);	
}

void SetFloatByLocation(int loc, const float* const value)
{
	glUniform1f(loc, *value);	
}

void SetVec2ByLocation(int loc, const float* const vec)
{
	glUniform2fv(loc, 1, vec);	
}

void SetVec3ByLocation(int loc, const float* const vec)
{
	glUniform3fv(loc, 1, vec);		
}

void SetVec4ByLocation(int loc, const float* const vec)
{
	glUniform4fv(loc, 1, vec);
}

void SetMtx33ByLocation(int loc, const float* const mtx)
{
	glUniformMatrix3fv(loc, 1, GL_FALSE, mtx);		
}

void SetMtx44ByLocation(int loc, const float* const mtx)
{
	glUniformMatrix4fv(loc, 1, GL_FALSE, mtx);		
}

#endif //GLQUAKE

/*
UniformHandle CreateUniformHandle(const char* name, UniformType type)
{
	return 0;
}


//--------------------------------------------


*/