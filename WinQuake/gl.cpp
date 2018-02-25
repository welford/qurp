#include "./gl.h"

//TODO: add a way for local projects to override these initialisation steps

FOREACH_OGL(GEN_DeclareGLVar);

void InitialiseOpenGLFunctionPointers(void)
{
	HMODULE MOD = LoadLibraryA("opengl32.dll");
	FOREACH_OGL(GEN_QueryGLVar);
}

#if _WIN32

FOREACH_WGL(GEN_DeclareGLVar);

void InitialiseWGLFunctionPointers(void)
{
	HMODULE MOD = LoadLibraryA("opengl32.dll");
	FOREACH_WGL(GEN_QueryGLVar);
}	
#endif //_WIN32
