/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// sys_null.h -- null system driver to aid porting efforts

#include "quakedef.h"
#include "errno.h"
#include "platform.h"
#include <sys/time.h>
#include <signal.h>

//JAMES
//my simple video and input setup
CPlatform		platform;
qboolean		isDedicated;

extern void IN_KB_CALLBACK(unsigned int code, int press);

/*
===============================================================================

FILE IO

===============================================================================
*/

#define MAX_HANDLES             10
FILE    *sys_handles[MAX_HANDLES];

int             findhandle (void)
{
	int             i;
	
	for (i=1 ; i<MAX_HANDLES ; i++)
		if (!sys_handles[i])
			return i;
	Sys_Error ("out of handles");
	return -1;
}

/*
================
filelength
================
*/
int filelength (FILE *f)
{
	int             pos;
	int             end;

	pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	fseek (f, pos, SEEK_SET);

	return end;
}

int Sys_FileOpenRead (char *path, int *hndl)
{
	FILE    *f;
	int             i;
	
	i = findhandle ();

	f = fopen(path, "rb");
	if (!f)
	{
		*hndl = -1;
		return -1;
	}
	sys_handles[i] = f;
	*hndl = i;
	
	return filelength(f);
}

int Sys_FileOpenWrite (char *path)
{
	FILE    *f;
	int             i;
	
	i = findhandle ();

	f = fopen(path, "wb");
	if (!f)
		Sys_Error ("Error opening %s: %s", path,strerror(errno));
	sys_handles[i] = f;
	
	return i;
}

void Sys_FileClose (int handle)
{
	fclose (sys_handles[handle]);
	sys_handles[handle] = NULL;
}

void Sys_FileSeek (int handle, int position)
{
	fseek (sys_handles[handle], position, SEEK_SET);
}

int Sys_FileRead (int handle, void *dest, int count)
{
	return fread (dest, 1, count, sys_handles[handle]);
}

int Sys_FileWrite (int handle, void *data, int count)
{
	return fwrite (data, 1, count, sys_handles[handle]);
}

int     Sys_FileTime (char *path)
{
	FILE    *f;
	
	f = fopen(path, "rb");
	if (f)
	{
		fclose(f);
		return 1;
	}
	
	return -1;
}

void Sys_mkdir (char *path)
{
}


/*
===============================================================================

SYSTEM IO

===============================================================================
*/

void Sys_MakeCodeWriteable (unsigned long startaddr, unsigned long length)
{
}


void Sys_Error (char *error, ...)
{
	va_list         argptr;

	printf ("Sys_Error: ");   
	va_start (argptr,error);
	vprintf (error,argptr);
	va_end (argptr);
	printf ("\n");

	exit (1);
}

void Sys_Printf (char *fmt, ...)
{
	va_list         argptr;
	
	va_start (argptr,fmt);
	vprintf (fmt,argptr);
	va_end (argptr);
}

void Sys_Quit (void)
{
	printf("\nSys_Quit\n");

	// FIXME should we call Host_Shutdown() here instead?
	S_Shutdown();		// Shutdown SDL cleanly
	exit (0);
}

double Sys_FloatTime (void)
{
	/*
	static double t;
	
	t += 0.1;
	
	return t;
	*/
	struct timeval tp;
    struct timezone tzp; 
    static int      secbase = 0;    	
    gettimeofday(&tp, &tzp); 

    if (!secbase)
    {
        secbase = tp.tv_sec;
        return tp.tv_usec/1000000.0;
    }

    return (tp.tv_sec - secbase) + tp.tv_usec/1000000.0;
}

char *Sys_ConsoleInput (void)
{
	return NULL;
}

void Sys_Sleep (void)
{
}

void Sys_SendKeyEvents (void)
{
	// Called from SCR_ModalMessage in gl_screen.c (called from menu.c)
	// also Con_NotifyBox in console.c (called from cd_audio.c)

	// Added to fix hang when prompted to start new game (menu.c)
	Tick(&platform, IN_KB_CALLBACK);
}

void Sys_HighFPPrecision (void)
{
}

void Sys_LowFPPrecision (void)
{
}

//=============================================================================

void rpi_sighandler(int sig)
{
	// Only handles SIGTERM and SIGINT (ctrl-c)
	signal(SIGINT, SIG_DFL);	// reset to default
	signal(SIGTERM, SIG_DFL);	// reset to default

	printf("\nReceived signal %d, exiting...\n", sig);
	fflush(stdout);

	Sys_Quit();
}

void main (int argc, char **argv)
{
	int i=0;
	int j = 0;
	double		time, oldtime, newtime, sec_counter=0;
	int frame_counter = 0;
	static quakeparms_t    parms;
	extern int vcrFile;
	extern int recording;
	extern unsigned int dcc;
	extern unsigned int dcs;

	// Setup signal handlers to shutdown SDL cleanly
	// signal(SIGINT, rpi_sighandler);	// Currently DISABLED
	// signal(SIGTERM, rpi_sighandler);	// Currently DISABLED

	COM_InitArgv (argc, argv);

	parms.argc = com_argc;
	parms.argv = com_argv;
#ifdef GLQUAKE
	parms.memsize = 16*1024*1024;
#else
	parms.memsize = 8*1024*1024;
#endif

	j = COM_CheckParm("-mem");
	if (j)
		parms.memsize = (int) (Q_atof(com_argv[j+1]) * 1024 * 1024);	
	parms.membase = malloc (parms.memsize);
	parms.basedir = "./";

	Host_Init (&parms);

	glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	
	//not needed seems to do nothing
	//Sys_Init();

	oldtime = Sys_FloatTime () - 0.1;
	while (1){		
		// find time spent rendering last frame
        newtime = Sys_FloatTime ();		
        time = newtime - oldtime;

		sec_counter += time;
		frame_counter++;
		if(sec_counter > 1.0f){
			sec_counter -= 1.0f;
			//printf("fps : %d\n", frame_counter);			
			//Con_Printf("total calls : %d, data : %dkb\n", dcc, dcs/1024);
			//Con_Printf("fps : %d\n", frame_counter);
			dcc = 0;
			dcs = 0;
			frame_counter = 0;
		}


		Tick(&platform, IN_KB_CALLBACK);
		
		if (cls.state == ca_dedicated)
        {   // play vcrfiles at max speed
            if (time < sys_ticrate.value && (vcrFile == -1 || recording) )
            {
				usleep(1);
                continue;       // not time to run a server only tic yet
            }
            time = sys_ticrate.value;
        }

        if (time > sys_ticrate.value*2)
            oldtime = newtime;
        else
            oldtime += time;
		
		Host_Frame (time);

		i++;
		//if(IS_BUTTON_PRESSED(platform.m_keyboard.key[KB_A])){
		if(i%10 == 0){
			//SCR_ScreenShot_f();
		}
	}
	
	free (parms.membase);
}


