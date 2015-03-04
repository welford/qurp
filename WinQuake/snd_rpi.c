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
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <stdio.h>
#include "quakedef.h"
#include <SDL/SDL.h>

int audio_fd;
int snd_inited;

static int tryrates[] = { 11025, 22051, 44100, 8000 };

static unsigned volatile int SDL_bufpos;
static unsigned volatile int SDL_buflen;

void SDL_callback_fill_audio(void *udata, Uint8 *stream, int len)
{
	// This runs in a separate thread, so ideally we'd mutex
	// access to shm->buffer and SDL_bufpos, but it seems OK without.
	// FIXED: SDL_bufpos read is now mutexed in SNDDMA_GetDMAPos(), which
	// is all that really matters (a little bit of sound data corruption
	// would not be a problem, I guess).

	// shm-> buffer is filled via snd_dma.c S_Update() calls S_Update_()
	// calls snd_mix.c S_PaintChannels() then S_TransferPaintBuffer() then
	// S_TransferStereo16() then Snd_WriteLinearBlastStereo16() 

	// DEBUG - we get called with len=4096, ie the whole buffer, so	
	// we need to make the DMA buffer larger (use four times)
	// Con_Printf("\nAudio callback len=%d\n",len);	// DEBUG

	if (!shm)	// This would be bad
	{
		// Con_Printf("\nAudio callback shm is NULL !!\n"); // DEBUG
		return;	
	}

#if 0
	// slow version
	unsigned int i;
	for (i=0; i<len; i++)
	{
		stream[i] = shm->buffer[SDL_bufpos++];
		if (SDL_bufpos >= SDL_buflen)
			SDL_bufpos = 0;
	}
#else
	// better version uses memcpy and updates SDL_bufpos once only
	if (SDL_bufpos + len > SDL_buflen)
	{
		// This normally does not happen if shm->buffer is a
		// multiple of the SDL audio buffer, but we should cope anyway
		// Con_Printf("\nAudio callback buffer mis-aligned!!\n"); // DEBUG
		unsigned int start = SDL_bufpos;
		unsigned int count = SDL_buflen - SDL_bufpos;
		memcpy(stream, shm->buffer+SDL_bufpos, count);
		memcpy(stream + count, shm->buffer, len - count);
		SDL_bufpos = len - count;
	}
	else
	{
		memcpy(stream, shm->buffer+SDL_bufpos, len);
		if (SDL_bufpos + len == SDL_buflen)
			SDL_bufpos = 0;
		else
			SDL_bufpos += len;
	}
#endif
}


qboolean SNDDMA_Init(void)
{

	int rc;
    int fmt;
	int tmp;
    int i;
    char *s;
	int caps;

	SDL_AudioSpec wanted;
	SDL_AudioSpec obtained;

    /* Set default audio format */
    wanted.freq = 22050;
    wanted.format = AUDIO_S16;
    wanted.channels = 2;    /* 1 = mono, 2 = stereo */
    wanted.samples = 1024;
    wanted.callback = SDL_callback_fill_audio;
    wanted.userdata = NULL;	// callback references shm directly

	snd_inited = 0;

	Con_Printf("\nSNDDMA_Init Entered\n");

	if (SDL_Init(SDL_INIT_AUDIO) < 0)
	{
		perror("SDL_INIT_AUDIO");
		Con_Printf("SDL_INIT_AUDIO failed\n");
		return 0;
	}

	Con_Printf("\nSNDDMA_Init SDL_INIT_AUDIO succeeded\n");

	shm = &sn;		// See snd_dma.c, sn is global volatile dma_t
				// defined in sound.h

	shm->splitbuffer = 0;

	// set sample bits & speed to match SDL wanted

        shm->samplebits = 16;
        // shm->speed = tryrates[1];	// FIXME ??? 22051 cf 22050 in wanted
        shm->speed = wanted.freq;	// 22050 is fine
	shm->channels = wanted.channels;
   

	// Don't bother with config , always use 22050 16 bit stereo
#if 0
    s = getenv("QUAKE_SOUND_SAMPLEBITS");
    if (s) shm->samplebits = atoi(s);
	else if ((i = COM_CheckParm("-sndbits")) != 0)
		shm->samplebits = atoi(com_argv[i+1]);
	if (shm->samplebits != 16 && shm->samplebits != 8)
    {
        shm->samplebits = 16;
    }

    s = getenv("QUAKE_SOUND_SPEED");
    if (s) shm->speed = atoi(s);
	else if ((i = COM_CheckParm("-sndspeed")) != 0)
		shm->speed = atoi(com_argv[i+1]);
    else
    {
        shm->speed = 22051;	// 22051 from tryrates, not 22050
    }

    s = getenv("QUAKE_SOUND_CHANNELS");
    if (s) shm->channels = atoi(s);
	else if ((i = COM_CheckParm("-sndmono")) != 0)
		shm->channels = 1;
	else if ((i = COM_CheckParm("-sndstereo")) != 0)
		shm->channels = 2;
    else shm->channels = 2;
#endif

	// Make the DMA buffer four times the size of the SDL buffer

	// samples is mono samples (see dma_t in sound.h), so times channels
	shm->samples = 4 * wanted.samples * shm->channels;
	SDL_buflen = shm->samples * (shm->samplebits/8);
	shm->buffer = (unsigned char*) malloc(SDL_buflen);

	shm->submission_chunk = 1;
	shm->samplepos = 0;

	if ( SDL_OpenAudio(&wanted, &obtained) < 0 ) {
		perror("SDL_OpenAudio");
		Con_Printf("SDL_OpenAudio failed\n");
		return 0;
	}

	SDL_PauseAudio(0);

	snd_inited = 1;
	return 1;
}

int SNDDMA_GetDMAPos(void)
{

	if (!snd_inited) return 0;

	// Lockout the callback before getting position. Works OK without
	// doing this (SDL_bufpos read is likely atomic), but best to be safe

	SDL_LockAudio();
	// samplepos counts mono samples (see dma_t in sound.h)
	shm->samplepos = SDL_bufpos / (shm->samplebits / 8);
	SDL_UnlockAudio();

	return shm->samplepos;

}

void SNDDMA_Shutdown(void)
{
	// TODO ought to set SIGINT and SIGTERM handlers in sys_rpi.c
	// to call SNDDMA_Shutdown(), but it seems OK without it

	Con_Printf("\nSNDDMA_Shutdown entered\n");	// DEBUG
	if (snd_inited)
	{
		Con_Printf("SDL_CloseAudio\n");
		SDL_CloseAudio();
		Con_Printf("SDL_Quit\n");
		SDL_Quit();
		snd_inited = 0;
	}
	Con_Printf("SNDDMA_Shutdown complete\n");
}

/*
==============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
===============
*/
void SNDDMA_Submit(void)
{
}

