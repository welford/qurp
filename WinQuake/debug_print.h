
#ifndef _DEBUG_CONSOLE_H_
#define _DEBUG_CONSOLE_H_

#include <stdio.h>

#ifdef W3D_PRINT
#include "viewer/w3d_console.h"
#endif

#define DEBUG_CONSOLE

#ifdef __cplusplus
	inline int null_func(const char *format, ...) { }
	#define DbgEmpty null_func
#else
	#define DbgEmpty { }
#endif// Debug Trace Enabled

#ifdef W3D_PRINT
	#define d_printf W3DPrintfConsole	
#else
	#ifdef DEBUG_CONSOLE
		#define d_printf printf
	#else
		#define d_printf DbgEmpty
	#endif
#endif

#endif _DEBUG_CONSOLE_H_
