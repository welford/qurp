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

// draw.c -- this is the only file outside the refresh that touches the
// vid buffer

#include "quakedef.h"
//James Debug
#include <stdlib.h>

#define GL_COLOR_INDEX8_EXT     0x80E5

extern unsigned char d_15to8table[65536];

cvar_t		gl_nobind = {"gl_nobind", "0"};
cvar_t		gl_max_size = {"gl_max_size", "1024"};
cvar_t		gl_picmip = {"gl_picmip", "0"};
cvar_t		gl_cleanup_textures = {"gl_cleanup_textures", "0"};

byte		*draw_chars;			// 8*8 graphic characters
qpic_t		*draw_disc;				//JAMES OK
qpic_t		*draw_backtile;			//JAMES OK

int			translate_texture;
int			char_texture;

typedef struct
{
	int		texnum;
	float	sl, tl, sh, th;
} glpic_t;

byte		conback_buffer[sizeof(qpic_t) + sizeof(glpic_t)];
qpic_t		*conback = (qpic_t *)&conback_buffer;

int		gl_lightmap_format = GL_RGBA;
int		gl_solid_format = 3;
int		gl_alpha_format = 4;


#if LINEAR_TEXTURES
int		gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int		gl_filter_max = GL_LINEAR;
#else
int		gl_filter_min = GL_NEAREST;
int		gl_filter_max = GL_NEAREST;
#endif

int		texels;

typedef struct
{
	//JAMES was just an int before
	unsigned int	texnum;
	int				type;
	char			identifier[64];
	int				width, height;
	qboolean		mipmap;
} gltexture_t;

#define	MAX_GLTEXTURES	1024
gltexture_t	gltextures[MAX_GLTEXTURES];		//
int			numgltextures;

void GL_Bind (int texnum)
{
	if (gl_nobind.value)
		texnum = char_texture;
	if (currenttexture == texnum)
		return;

	currenttexture = texnum;

	//if we are changing textures we need to do a little drawing
	FlushDraw();
	glBindTexture(GL_TEXTURE_2D, texnum);
}


/*
=============================================================================

  scrap allocation

  Allocate all the little status bar obejcts into a single texture
  to crutch up stupid hardware / drivers

=============================================================================
*/

#define	MAX_SCRAPS		2
#define	BLOCK_WIDTH		256
#define	BLOCK_HEIGHT	256

int			scrap_allocated[MAX_SCRAPS][BLOCK_WIDTH];
byte		scrap_texels[MAX_SCRAPS][BLOCK_WIDTH*BLOCK_HEIGHT*4];
qboolean	scrap_dirty;

int			scrap_texnum;
unsigned int scrap_texnum_gl[MAX_SCRAPS] = {0,0};

// returns a texture number and the position inside it
int Scrap_AllocBlock (int w, int h, int *x, int *y)
{
	int		i, j;
	int		best, best2;
	int		bestx;
	int		texnum;

	for (texnum=0 ; texnum<MAX_SCRAPS ; texnum++)
	{
		best = BLOCK_HEIGHT;

		for (i=0 ; i<BLOCK_WIDTH-w ; i++)
		{
			best2 = 0;

			for (j=0 ; j<w ; j++)
			{
				if (scrap_allocated[texnum][i+j] >= best)
					break;
				if (scrap_allocated[texnum][i+j] > best2)
					best2 = scrap_allocated[texnum][i+j];
			}
			if (j == w)
			{	// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > BLOCK_HEIGHT)
			continue;

		for (i=0 ; i<w ; i++)
			scrap_allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	Sys_Error ("Scrap_AllocBlock: full");
	return 0;
}

int	scrap_uploads;

void Scrap_Upload (void)
{
	int		texnum;	

	scrap_uploads++;

	for (texnum=0 ; texnum<MAX_SCRAPS ; texnum++) {
		//GL_Bind(scrap_texnum + texnum);
		GL_Bind(scrap_texnum_gl[texnum]);
		GL_Upload8 (scrap_texels[texnum], BLOCK_WIDTH, BLOCK_HEIGHT, false, true);
	}
	scrap_dirty = false;
}

//=============================================================================
/* Support Routines */

typedef struct cachepic_s
{
	char		name[MAX_QPATH];
	qpic_t		pic;
	byte		padding[32];	// for appended glpic
} cachepic_t;

#define	MAX_CACHED_PICS		128
cachepic_t	menu_cachepics[MAX_CACHED_PICS];
int			menu_numcachepics;

byte		menuplyr_pixels[4096];

int		pic_texels;
int		pic_count;

qpic_t *Draw_PicFromWad (char *name)
{
	qpic_t	*p;
	glpic_t	*gl;

	p = W_GetLumpName (name);
	gl = (glpic_t *)p->data;

	// load little ones into the scrap
	if (p->width < 64 && p->height < 64)
	{
		int		x, y;
		int		i, j, k;
		int		texnum;

		texnum = Scrap_AllocBlock (p->width, p->height, &x, &y);
		scrap_dirty = true;
		k = 0;
		for (i=0 ; i<p->height ; i++)
			for (j=0 ; j<p->width ; j++, k++)
				scrap_texels[texnum][(y+i)*BLOCK_WIDTH + x + j] = p->data[k];
		//texnum += scrap_texnum;

		//gl->texnum = texnum;
		//JAMES
		gl->texnum = scrap_texnum_gl[texnum];
		//glGenTextures(1, &gl->texnum);

		gl->sl = (x+0.01)/(float)BLOCK_WIDTH;
		gl->sh = (x+p->width-0.01)/(float)BLOCK_WIDTH;
		gl->tl = (y+0.01)/(float)BLOCK_WIDTH;
		gl->th = (y+p->height-0.01)/(float)BLOCK_WIDTH;

		pic_count++;
		pic_texels += p->width*p->height;
	}
	else
	{
		gl->texnum = GL_LoadPicTexture (p);
		gl->sl = 0;
		gl->sh = 1;
		gl->tl = 0;
		gl->th = 1;
	}
	return p;
}


/*
================
Draw_CachePic
================
*/
qpic_t	*Draw_CachePic (char *path)
{
	cachepic_t	*pic;
	int			i;
	qpic_t		*dat;
	glpic_t		*gl;

	for (pic=menu_cachepics, i=0 ; i<menu_numcachepics ; pic++, i++)
		if (!strcmp (path, pic->name))
			return &pic->pic;

	if (menu_numcachepics == MAX_CACHED_PICS)
		Sys_Error ("menu_numcachepics == MAX_CACHED_PICS");
	menu_numcachepics++;
	strcpy (pic->name, path);

//
// load the pic from disk
//
	dat = (qpic_t *)COM_LoadTempFile (path);	
	if (!dat)
		Sys_Error ("Draw_CachePic: failed to load %s", path);
	SwapPic (dat);

	// HACK HACK HACK --- we need to keep the bytes for
	// the translatable player picture just for the menu
	// configuration dialog
	if (!strcmp (path, "gfx/menuplyr.lmp"))
		memcpy (menuplyr_pixels, dat->data, dat->width*dat->height);

	pic->pic.width = dat->width;
	pic->pic.height = dat->height;

	gl = (glpic_t *)pic->pic.data;
	gl->texnum = GL_LoadPicTexture (dat);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	return &pic->pic;
}


void Draw_CharToConback (int num, byte *dest)
{
	int		row, col;
	byte	*source;
	int		drawline;
	int		x;

	row = num>>4;
	col = num&15;
	source = draw_chars + (row<<10) + (col<<3);

	drawline = 8;

	while (drawline--)
	{
		for (x=0 ; x<8 ; x++)
			if (source[x] != 255)
				dest[x] = 0x60 + source[x];
		source += 128;
		dest += 320;
	}

}

typedef struct
{
	char *name;
	int	minimize, maximize;
} glmode_t;

glmode_t modes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};

/*
===============
Draw_TextureMode_f
===============
*/
void Draw_TextureMode_f (void)
{
	int		i;
	gltexture_t	*glt;

	if (Cmd_Argc() == 1)
	{
		for (i=0 ; i< 6 ; i++)
			if (gl_filter_min == modes[i].minimize)
			{
				Con_Printf ("%s\n", modes[i].name);
				return;
			}
		Con_Printf ("current filter is unknown???\n");
		return;
	}

	for (i=0 ; i< 6 ; i++)
	{
		if (!Q_strcasecmp (modes[i].name, Cmd_Argv(1) ) )
			break;
	}
	if (i == 6)
	{
		Con_Printf ("bad filter name\n");
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	// change all the existing mipmap texture objects
	for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
	{
		if (glt->mipmap)
		{
			GL_Bind (glt->texnum);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		}
	}
}

extern const int glMajor;
extern const int glMinor;

/*
===============
Draw_Init
===============
*/
void Draw_Init (void)
{
	int		i;
	qpic_t	*cb;
	byte	*dest, *src;
	int		x, y;
	char	ver[40];
	glpic_t	*gl;
	int		start;
	byte	*ncdata;
	int		f, fstep;


	Cvar_RegisterVariable (&gl_nobind);
	Cvar_RegisterVariable (&gl_max_size);
	Cvar_RegisterVariable (&gl_picmip);
	Cvar_RegisterVariable (&gl_cleanup_textures);

	// 3dfx can only handle 256 wide textures
	if (!Q_strncasecmp ((char *)gl_renderer, "3dfx",4) ||
		strstr((char *)gl_renderer, "Glide"))
		Cvar_Set ("gl_max_size", "256");

	Cmd_AddCommand ("gl_texturemode", &Draw_TextureMode_f);

	// load the console background and the charset
	// by hand, because we need to write the version
	// string into the background before turning
	// it into a texture
	draw_chars = W_GetLumpName ("conchars");
	for (i=0 ; i<256*64 ; i++)
		if (draw_chars[i] == 0)
			draw_chars[i] = 255;	// proper transparent color

	// now turn them into textures
	char_texture = GL_LoadTexture ("charset", 128, 128, draw_chars, false, true, TEX_TYPE_NONE);

	start = Hunk_LowMark();

	cb = (qpic_t *)COM_LoadTempFile ("gfx/conback.lmp");	
	if (!cb)
		Sys_Error ("Couldn't load gfx/conback.lmp");
	SwapPic (cb);

	// hack the version number directly into the pic
#if defined(__linux__)
	sprintf (ver, "(Raspberry Pi %2.2f, gl %4.2f) %4.2f", (float)LINUX_VERSION, (float)GLQUAKE_VERSION, (float)VERSION);
#else
	sprintf (ver, "(gl %d.%d) %4.2f", glMajor, glMinor, (float)VERSION);
#endif
	dest = cb->data + 320*186 + 320 - 11 - 8*strlen(ver);
	y = strlen(ver);
	for (x=0 ; x<y ; x++)
		Draw_CharToConback (ver[x], dest+(x<<3));

#if 0
	conback->width = vid.conwidth;
	conback->height = vid.conheight;

 	// scale console to vid size
 	dest = ncdata = Hunk_AllocName(vid.conwidth * vid.conheight, "conback");
 
 	for (y=0 ; y<vid.conheight ; y++, dest += vid.conwidth)
 	{
 		src = cb->data + cb->width * (y*cb->height/vid.conheight);
 		if (vid.conwidth == cb->width)
 			memcpy (dest, src, vid.conwidth);
 		else
 		{
 			f = 0;
 			fstep = cb->width*0x10000/vid.conwidth;
 			for (x=0 ; x<vid.conwidth ; x+=4)
 			{
 				dest[x] = src[f>>16];
 				f += fstep;
 				dest[x+1] = src[f>>16];
 				f += fstep;
 				dest[x+2] = src[f>>16];
 				f += fstep;
 				dest[x+3] = src[f>>16];
 				f += fstep;
 			}
 		}
 	}
#else
	conback->width = cb->width;
	conback->height = cb->height;
	ncdata = cb->data;
#endif

	//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	gl = (glpic_t *)conback->data;
	gl->texnum = GL_LoadTexture ("conback", conback->width, conback->height, ncdata, false, false, TEX_TYPE_NONE);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;
	conback->width = vid.width;
	conback->height = vid.height;

	// free loaded console
	Hunk_FreeToLowMark(start);

	// save a texture slot for translated picture
	//translate_texture = texture_extension_number++;
	//JAMES
	texture_extension_number++;
	glGenTextures(1, &translate_texture);
	//END

	// save slots for scraps
	//scrap_texnum = texture_extension_number;
	//JAMES
	glGenTextures(MAX_SCRAPS, scrap_texnum_gl);
	//END

	//JAMES
	//this no longer really means anything
	//but we'll keep it here
	texture_extension_number += MAX_SCRAPS;

	//
	// get the other pics we need
	//
	draw_disc = Draw_PicFromWad ("disc");
	draw_backtile = Draw_PicFromWad ("backtile");

	//exit(1);
}

/*
================
Draw_Character

Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/
void Draw_Character (int x, int y, int num)
{
	byte			*dest;
	byte			*source;
	unsigned short	*pusdest;
	int				drawline;	
	int				row, col;
	float			frow, fcol, size;

	if (num == 32)
		return;		// space

	num &= 255;
	
	if (y <= -8)
		return;			// totally off screen

	row = num>>4;
	col = num&15;

	frow = row*0.0625;
	fcol = col*0.0625;
	size = 0.0625;

	GL_Bind (char_texture);
	EnableBlending();	

	SetVertexMode(VAS_CLR_TEX);
	BeginDrawing(RNDR_TRIANGLES);

	AddVertex4D (VTX_COLOUR, 1, 1, 1, 1);
	AddVertex2D (VTX_TEXTURE, fcol, frow);
	AddVertex3D (VTX_POSITION, x, y, 0);
	
	AddVertex2D (VTX_TEXTURE, fcol + size, frow);
	AddVertex3D (VTX_POSITION, x+8, y, 0);
	
	AddVertex2D (VTX_TEXTURE, fcol + size, frow + size);
	AddVertex3D (VTX_POSITION, x+8, y+8, 0);
	
	AddVertex2D (VTX_TEXTURE, fcol, frow);
	AddVertex3D (VTX_POSITION, x, y, 0);
	
	AddVertex2D (VTX_TEXTURE, fcol + size, frow + size);
	AddVertex3D (VTX_POSITION, x+8, y+8, 0);
	
	AddVertex2D (VTX_TEXTURE, fcol, frow + size);
	AddVertex3D (VTX_POSITION, x, y+8, 0);

	EndDrawing();
}

void Draw_CharacterStrip (int x, int y, int num)
{
	byte			*dest;
	byte			*source;
	unsigned short	*pusdest;
	int				drawline;	
	int				row, col;
	float			frow, fcol, size;

	if (num == 32)
		return;		// space

	num &= 255;
	
	if (y <= -8)
		return;			// totally off screen

	row = num>>4;
	col = num&15;

	frow = row*0.0625;
	fcol = col*0.0625;
	size = 0.0625;

	GL_Bind (char_texture);
	EnableBlending();	

	SetVertexMode(VAS_CLR_TEX);
	BeginDrawing(RNDR_TRIANGLE_STRIP);
	AddVertex4D (VTX_COLOUR, 1, 1, 1, 1);

	AddVertex2D (VTX_TEXTURE, fcol, frow);
	AddVertex3D (VTX_POSITION, x, y, 0);
	
	AddVertex2D (VTX_TEXTURE, fcol, frow + size);
	AddVertex3D (VTX_POSITION, x, y+8, 0);
	
	AddVertex2D (VTX_TEXTURE, fcol + size, frow);
	AddVertex3D (VTX_POSITION, x+8, y, 0);
	
	AddVertex2D (VTX_TEXTURE, fcol + size, frow + size);
	AddVertex3D (VTX_POSITION, x+8, y+8, 0);
	
	EndDrawing();
}

/*
================
Draw_String
================
*/
void Draw_String (int x, int y, char *str)
{
	while (*str){
		Draw_Character (x, y, *str);
		str++;
		x += 8;
	}
}

/*
================
Draw_DebugChar

Draws a single character directly to the upper right corner of the screen.
This is for debugging lockups by drawing different chars in different parts
of the code.
================
*/
void Draw_DebugChar (char num)
{
}

/*
=============
Draw_AlphaPic
=============
*/
void Draw_AlphaPic (int x, int y, qpic_t *pic, float alpha)
{
	byte			*dest, *source;
	unsigned short	*pusdest;
	int				v, u;
	glpic_t			*gl;

	if (scrap_dirty)
		Scrap_Upload ();

	gl = (glpic_t *)pic->data;
	
	GL_Bind (gl->texnum);
	EnableBlending();
	SetVertexMode(VAS_CLR_TEX);

	BeginDrawing(RNDR_TRIANGLES);

	AddVertex4D (VTX_COLOUR, 1.0f, 1.0f, 1.0f, alpha);

	AddVertex2D (VTX_TEXTURE, gl->sl, gl->tl);
	AddVertex3D (VTX_POSITION, x, y, 0);

	AddVertex2D (VTX_TEXTURE, gl->sh, gl->tl);
	AddVertex3D (VTX_POSITION, x+pic->width, y, 0);
	
	AddVertex2D (VTX_TEXTURE, gl->sh, gl->th);
	AddVertex3D (VTX_POSITION, x+pic->width, y+pic->height, 0);
	
	AddVertex2D (VTX_TEXTURE, gl->sl, gl->tl);
	AddVertex3D (VTX_POSITION, x, y, 0);
	
	AddVertex2D (VTX_TEXTURE, gl->sh, gl->th);	
	AddVertex3D (VTX_POSITION, x+pic->width, y+pic->height, 0);
	
	AddVertex2D (VTX_TEXTURE, gl->sl, gl->th);
	AddVertex3D (VTX_POSITION, x, y+pic->height, 0);	

	EndDrawing();
	DisableBlending();
}


/*
=============
Draw_Pic
=============
*/
void Draw_Pic (int x, int y, qpic_t *pic)
{

	byte			*dest, *source;
	unsigned short	*pusdest;
	int				v, u;
	glpic_t			*gl;

	if (scrap_dirty)
		Scrap_Upload ();
	gl = (glpic_t *)pic->data;
	
	GL_Bind (gl->texnum);

	//JAMES	
	SetVertexMode(VAS_CLR_TEX);
	EnableBlending();
	BeginDrawing(RNDR_TRIANGLES);
	AddVertex4D (VTX_COLOUR, 1.0f, 1.0f, 1.0f, 1.0f);

	AddVertex2D (VTX_TEXTURE, gl->sl, gl->tl);
	AddVertex3D (VTX_POSITION, x, y, 0);

	AddVertex2D (VTX_TEXTURE, gl->sh, gl->tl);	
	AddVertex3D (VTX_POSITION, x+pic->width, y, 0);

	AddVertex2D (VTX_TEXTURE, gl->sh, gl->th);
	AddVertex3D (VTX_POSITION, x+pic->width, y+pic->height, 0);
	
	AddVertex2D (VTX_TEXTURE, gl->sl, gl->tl);
	AddVertex3D (VTX_POSITION, x, y, 0);
	
	AddVertex2D (VTX_TEXTURE, gl->sh, gl->th);
	AddVertex3D (VTX_POSITION, x+pic->width, y+pic->height, 0);
	
	AddVertex2D (VTX_TEXTURE, gl->sl, gl->th);
	AddVertex3D (VTX_POSITION, x, y+pic->height, 0);

	EndDrawing();
	//END
}


/*
=============
Draw_TransPic
=============
*/
void Draw_TransPic (int x, int y, qpic_t *pic)
{
	byte	*dest, *source, tbyte;
	unsigned short	*pusdest;
	int				v, u;

	if (x < 0 || (unsigned)(x + pic->width) > vid.width || y < 0 ||
		 (unsigned)(y + pic->height) > vid.height)
	{
		Sys_Error ("Draw_TransPic: bad coordinates");
	}
		
	Draw_Pic (x, y, pic);
}


/*
=============
Draw_TransPicTranslate

Only used for the player color selection menu
=============
*/
void Draw_TransPicTranslate (int x, int y, qpic_t *pic, byte *translation)
{
	int				v, u, c;
	unsigned		trans[64*64], *dest;
	byte			*src;
	int				p;

	GL_Bind (translate_texture);

	c = pic->width * pic->height;

	dest = trans;
	for (v=0 ; v<64 ; v++, dest += 64)
	{
		src = &menuplyr_pixels[ ((v*pic->height)>>6) *pic->width];
		for (u=0 ; u<64 ; u++)
		{
			p = src[(u*pic->width)>>6];
			if (p == 255)
				dest[u] = p;
			else
				dest[u] =  d_8to24table[translation[p]];
		}
	}

	//JAMES
	//glTexImage2D (GL_TEXTURE_2D, 0, texDataType[gl_alpha_format], 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);
	glTexImage2D (GL_TEXTURE_2D, 0, texDataType[gl_alpha_format], 64, 64, 0, texDataType[gl_alpha_format], GL_UNSIGNED_BYTE, trans);
	//END

#if LINEAR_TEXTURES
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#else
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
#endif

	//JAMES	
	SetVertexMode(VAS_CLR_TEX);
	BeginDrawing(RNDR_TRIANGLES);
	AddVertex4D (VTX_COLOUR, 1.0f, 1.0f, 1.0f, 1.0f);

	AddVertex2D (VTX_TEXTURE, 0.0f, 0.0f);
	AddVertex3D (VTX_POSITION, x, y, 0);
		
	AddVertex2D (VTX_TEXTURE, 0, 1);
	AddVertex3D (VTX_POSITION, x, y+pic->height, 0);
	

	AddVertex2D (VTX_TEXTURE, 1, 1);
	AddVertex3D (VTX_POSITION, x+pic->width, y+pic->height, 0);
	
	AddVertex2D (VTX_TEXTURE, 0.0f, 0.0f);
	AddVertex3D (VTX_POSITION, x, y, 0);
	
	AddVertex2D (VTX_TEXTURE, 1, 1);
	AddVertex3D (VTX_POSITION, x+pic->width, y+pic->height, 0);
	
	AddVertex2D (VTX_TEXTURE, 1, 0);
	AddVertex3D (VTX_POSITION, x+pic->width, y, 0);

	EndDrawing();
	//END
}


/*
================
Draw_ConsoleBackground

================
*/
void Draw_ConsoleBackground (int lines)
{
	int y = (vid.height * 3) >> 2;	
		
	if (lines > y){
		Draw_Pic(0, lines - vid.height, conback);
	}
	else{
		Draw_AlphaPic (0, lines - vid.height, conback, (float)(1.2 * lines)/y);
	}
}


/*
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void Draw_TileClear (int x, int y, int w, int h)
{
	GL_Bind (*(int *)draw_backtile->data);	
	
	//*
	SetVertexMode(VAS_CLR_TEX);
	BeginDrawing(RNDR_TRIANGLES);

	AddVertex4D (VTX_COLOUR, 1.0f, 1.0f, 1.0f, 1.0f);

	AddVertex2D (VTX_TEXTURE, x/64.0f, y/64.0);
	AddVertex3D (VTX_POSITION, (float)x, (float)y, 0.0f);

	AddVertex2D (VTX_TEXTURE, (x+w)/64.0f, y/64.0);	
	AddVertex3D (VTX_POSITION, (float)(x+w), (float)y, 0.0f);
	
	AddVertex2D (VTX_TEXTURE, (x+w)/64.0f, (y+h)/64.0);	
	AddVertex3D (VTX_POSITION, (float)(x+w), (float)(y+h), 0.0f);

	AddVertex2D (VTX_TEXTURE, x/64.0f, y/64.0);
	AddVertex3D (VTX_POSITION, (float)x, (float)y, 0.0f);

	AddVertex2D (VTX_TEXTURE, (x+w)/64.0f, (y+h)/64.0);	
	AddVertex3D (VTX_POSITION, (float)(x+w), (float)(y+h), 0.0f);
	
	AddVertex2D (VTX_TEXTURE, x/64.0f, (y+h)/64.0f );
	AddVertex3D (VTX_POSITION, (float)x, (float)(y+h), 0.0f);
	/*/

	SetVertexMode(VAS_CLR);
	BeginDrawing(RNDR_TRIANGLES);
	AddVertex4D (VTX_COLOUR, 1.0f, 0.0f, 0.0f, 1.0f);
	AddVertex3D (VTX_POSITION, (float)x, (float)y, 0.0f);

	AddVertex3D (VTX_POSITION, (float)(x+w), (float)y, 0.0f);
	
	AddVertex3D (VTX_POSITION, (float)(x+w), (float)(y+h), 0.0f);

	AddVertex3D (VTX_POSITION, (float)x, (float)y, 0.0f);

	AddVertex3D (VTX_POSITION, (float)(x+w), (float)(y+h), 0.0f);
	
	AddVertex3D (VTX_POSITION, (float)x, (float)(y+h), 0.0f);
	*/
	EndDrawing();	

}


/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void Draw_Fill (int x, int y, int w, int h, int c)
{
	DisableTexture();

	//JAMES	
	//SetAttributeFormat( vattr, 2, 0);
	SetVertexMode(VAS_CLR);
	BeginDrawing(RNDR_TRIANGLES);
	AddVertex4D (VTX_COLOUR, host_basepal[c*3]/255.0, host_basepal[c*3+1]/255.0, host_basepal[c*3+2]/255.0, 1.0f);

	AddVertex2D (VTX_POSITION, x, y);
	//AddVertex2D (VTX_POSITION, x+w, y);
	AddVertex2D (VTX_POSITION, x, y+h);
	AddVertex2D (VTX_POSITION, x+w, y+h);

	AddVertex2D (VTX_POSITION, x, y);
	AddVertex2D (VTX_POSITION, x+w, y+h);	
	AddVertex2D (VTX_POSITION, x+w, y);
	EndDrawing();	
	//END
	
	EnableTexture();
}
//=============================================================================

/*
================
Draw_FadeScreen

================
*/
void Draw_FadeScreen (void)
{
	//JAMES
	EnableBlending();
	DisableTexture();
	SetVertexMode(VAS_CLR);
	BeginDrawing(RNDR_TRIANGLES);
	AddVertex4D (VTX_COLOUR, 0.0f, 0.0f, 0.0f, 0.8f);

	AddVertex3D (VTX_POSITION, 0.0f, 0.0, 0.0f);
	//AddVertex3D (VTX_POSITION, vid.width, 0.0f, 0.0f);
	AddVertex3D (VTX_POSITION, 0.0f, vid.height, 0.0f);
	AddVertex3D (VTX_POSITION, vid.width, vid.height, 0.0f);

	AddVertex3D (VTX_POSITION, 0.0f, 0.0, 0.0f);
	AddVertex3D (VTX_POSITION, vid.width, vid.height, 0.0f);
	AddVertex3D (VTX_POSITION, vid.width, 0.0f, 0.0f);

	EndDrawing();	
	DisableBlending();
	//END

	Sbar_Changed();
}

//=============================================================================

/*
================
Draw_BeginDisc

Draws the little blue disc in the corner of the screen.
Call before beginning any disc IO.
================
*/
void Draw_BeginDisc (void)
{
	return;

	if (!draw_disc)
		return;
	//FlushDraw();
	//glDrawBuffer  (GL_FRONT);
	Draw_Pic (vid.width - 24, 0, draw_disc);
	//FlushDraw();
	//glDrawBuffer  (GL_BACK);

}


/*
================
Draw_EndDisc

Erases the disc icon.
Call after completing any disc IO
================
*/
void Draw_EndDisc (void)
{
}

/*
================
GL_Set2D

Setup as if the screen was 320*200
================
*/
void GL_Set2D (void)
{
	SetViewport(glx, gly, glwidth, glheight);
	SetOrthoMatrix(0.0f, (float)vid.width, (float)vid.height, 0.0f, -99999.0f, 99999.0f);
	Identity();	
	DisableDepth();
	DisableBlending();
	DisableCulling();
	
}

//====================================================================

/*
================
GL_FindTexture
================
*/
int GL_FindTexture (char *identifier)
{
	int		i;
	gltexture_t	*glt;

	for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
	{
		if (!strcmp (identifier, glt->identifier))
			return gltextures[i].texnum;
	}

	return -1;
}

/*
================
GL_ResampleTexture
================
*/
void GL_ResampleTexture (unsigned *in, int inwidth, int inheight, unsigned *out,  int outwidth, int outheight)
{
	int		i, j;
	unsigned	*inrow;
	unsigned	frac, fracstep;

	fracstep = inwidth*0x10000/outwidth;
	for (i=0 ; i<outheight ; i++, out += outwidth)
	{
		inrow = in + inwidth*(i*inheight/outheight);
		frac = fracstep >> 1;
		for (j=0 ; j<outwidth ; j+=4)
		{
			out[j] = inrow[frac>>16];
			frac += fracstep;
			out[j+1] = inrow[frac>>16];
			frac += fracstep;
			out[j+2] = inrow[frac>>16];
			frac += fracstep;
			out[j+3] = inrow[frac>>16];
			frac += fracstep;
		}
	}
}

/*
================
GL_Resample8BitTexture -- JACK
================
*/
void GL_Resample8BitTexture (unsigned char *in, int inwidth, int inheight, unsigned char *out,  int outwidth, int outheight)
{
	int		i, j;
	unsigned	char *inrow;
	unsigned	frac, fracstep;

	fracstep = inwidth*0x10000/outwidth;
	for (i=0 ; i<outheight ; i++, out += outwidth)
	{
		inrow = in + inwidth*(i*inheight/outheight);
		frac = fracstep >> 1;
		for (j=0 ; j<outwidth ; j+=4)
		{
			out[j] = inrow[frac>>16];
			frac += fracstep;
			out[j+1] = inrow[frac>>16];
			frac += fracstep;
			out[j+2] = inrow[frac>>16];
			frac += fracstep;
			out[j+3] = inrow[frac>>16];
			frac += fracstep;
		}
	}
}


/*
================
GL_MipMap

Operates in place, quartering the size of the texture
================
*/
void GL_MipMap (byte *in, int width, int height)
{
	int		i, j;
	byte	*out;

	width <<=2;
	height >>= 1;
	out = in;
	for (i=0 ; i<height ; i++, in+=width)
	{
		for (j=0 ; j<width ; j+=8, out+=4, in+=8)
		{
			out[0] = (in[0] + in[4] + in[width+0] + in[width+4])>>2;
			out[1] = (in[1] + in[5] + in[width+1] + in[width+5])>>2;
			out[2] = (in[2] + in[6] + in[width+2] + in[width+6])>>2;
			out[3] = (in[3] + in[7] + in[width+3] + in[width+7])>>2;
		}
	}
}

/*
================
GL_MipMap8Bit

Mipping for 8 bit textures
================
*/
void GL_MipMap8Bit (byte *in, int width, int height)
{
	int		i, j;
	unsigned short     r,g,b;
	byte	*out, *at1, *at2, *at3, *at4;

//	width <<=2;
	height >>= 1;
	out = in;
	for (i=0 ; i<height ; i++, in+=width)
	{
		for (j=0 ; j<width ; j+=2, out+=1, in+=2)
		{
			at1 = (byte *) (d_8to24table + in[0]);
			at2 = (byte *) (d_8to24table + in[1]);
			at3 = (byte *) (d_8to24table + in[width+0]);
			at4 = (byte *) (d_8to24table + in[width+1]);

 			r = (at1[0]+at2[0]+at3[0]+at4[0]); r>>=5;
 			g = (at1[1]+at2[1]+at3[1]+at4[1]); g>>=5;
 			b = (at1[2]+at2[2]+at3[2]+at4[2]); b>>=5;

			out[0] = d_15to8table[(r<<0) + (g<<5) + (b<<10)];
		}
	}
}

/*
===============
GL_Upload32
===============
*/
void GL_Upload32 (unsigned *data, int width, int height,  qboolean mipmap, qboolean alpha)
{
	int			samples;
	int i=0;
	unsigned char *pData;
static	unsigned	scaled[1024*512];	// [512*256];
	int			scaled_width, scaled_height;

	for (scaled_width = 1 ; scaled_width < width ; scaled_width<<=1)
		;
	for (scaled_height = 1 ; scaled_height < height ; scaled_height<<=1)
		;

	scaled_width >>= (int)gl_picmip.value;
	scaled_height >>= (int)gl_picmip.value;

	if (scaled_width > gl_max_size.value)
		scaled_width = gl_max_size.value;
	if (scaled_height > gl_max_size.value)
		scaled_height = gl_max_size.value;

	if (scaled_width * scaled_height > sizeof(scaled)/4)
		Sys_Error ("GL_LoadTexture: too big");

	samples = alpha ? gl_alpha_format : gl_solid_format;

	texels += scaled_width * scaled_height;

	if (scaled_width == width && scaled_height == height)
	{
		if (!mipmap)
		{
			//glTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
			//JAMES
			//glTexImage2D (GL_TEXTURE_2D, 0, texDataType[samples], scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
			if(alpha){
				glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);				
			}
			else{
				glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);						
			}			
			//END

			goto done;
		}
		memcpy (scaled, data, width*height*4);
	}
	else
		GL_ResampleTexture (data, width, height, scaled, scaled_width, scaled_height);

	//glTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
	//JAMES
	if(alpha){
		glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
	}
	else{
		glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
	}
	//END
	if (mipmap)
	{
		glGenerateMipmap(GL_TEXTURE_2D);		
	}
done: ;

	if (mipmap)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);		
		//JAMES
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		//END
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		//JAMES
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		//END
	}
}

void GL_Upload8_EXT (byte *data, int width, int height,  qboolean mipmap, qboolean alpha) 
{
	/*
	int			i, s;
	qboolean	noalpha;
	int			p;
	static unsigned j;
	int			samples;
    static	unsigned char scaled[1024*512];	// [512*256];
	int			scaled_width, scaled_height;

	s = width*height;
	// if there are no transparent pixels, make it a 3 component
	// texture even if it was specified as otherwise
	if (alpha)
	{
		noalpha = true;
		for (i=0 ; i<s ; i++)
		{
			if (data[i] == 255)
				noalpha = false;
		}

		if (alpha && noalpha)
			alpha = false;
	}
	for (scaled_width = 1 ; scaled_width < width ; scaled_width<<=1)
		;
	for (scaled_height = 1 ; scaled_height < height ; scaled_height<<=1)
		;

	scaled_width >>= (int)gl_picmip.value;
	scaled_height >>= (int)gl_picmip.value;

	if (scaled_width > gl_max_size.value)
		scaled_width = gl_max_size.value;
	if (scaled_height > gl_max_size.value)
		scaled_height = gl_max_size.value;

	if (scaled_width * scaled_height > sizeof(scaled))
		Sys_Error ("GL_LoadTexture: too big");

	samples = 1; // alpha ? gl_alpha_format : gl_solid_format;

	texels += scaled_width * scaled_height;

	if (scaled_width == width && scaled_height == height)
	{
		if (!mipmap)
		{
			glTexImage2D (GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, scaled_width, scaled_height, 0, GL_COLOR_INDEX , GL_UNSIGNED_BYTE, data);			
			goto done;
		}
		memcpy (scaled, data, width*height);
	}
	else
		GL_Resample8BitTexture (data, width, height, scaled, scaled_width, scaled_height);

	glTexImage2D (GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, scaled_width, scaled_height, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, scaled);
	if (mipmap)
	{
		int		miplevel;

		miplevel = 0;
		while (scaled_width > 1 || scaled_height > 1)
		{
			GL_MipMap8Bit ((byte *)scaled, scaled_width, scaled_height);
			scaled_width >>= 1;
			scaled_height >>= 1;
			if (scaled_width < 1)
				scaled_width = 1;
			if (scaled_height < 1)
				scaled_height = 1;
			miplevel++;
			glTexImage2D (GL_TEXTURE_2D, miplevel, GL_COLOR_INDEX8_EXT, scaled_width, scaled_height, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, scaled);
		}
	}
done: ;


	if (mipmap)
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	*/
}

/*
===============
GL_Upload8
===============
*/
void GL_Upload8 (byte *data, int width, int height,  qboolean mipmap, qboolean alpha)
{
static	unsigned	trans[640*480];		// FIXME, temporary
	int			i, s;
	qboolean	noalpha;
	int			p;

	s = width*height;
	// if there are no transparent pixels, make it a 3 component
	// texture even if it was specified as otherwise
	if (alpha)
	{
		noalpha = true;
		for (i=0 ; i<s ; i++)
		{
			p = data[i];
			if (p == 255)
				noalpha = false;
			trans[i] = d_8to24table[p];
		}

		if (alpha && noalpha)
			alpha = false;
	}
	else
	{
		if (s&3)
			Sys_Error ("GL_Upload8: s&3");
		for (i=0 ; i<s ; i+=4)
		{
			trans[i] = d_8to24table[data[i]];
			trans[i+1] = d_8to24table[data[i+1]];
			trans[i+2] = d_8to24table[data[i+2]];
			trans[i+3] = d_8to24table[data[i+3]];
		}
	}

 	//if (VID_Is8bit() && !alpha && (data!=scrap_texels[0])) {
 		//GL_Upload8_EXT (data, width, height, mipmap, alpha);
 	//	return;
	//}
	GL_Upload32 (trans, width, height, mipmap, alpha);
}

//see http://forums.inside3d.com/viewtopic.php?f=12&t=1899&view=previous
void GL_CleanupTextures (void)
{
	int i = 0, j = 0;
	if (gl_cleanup_textures.value == 0)	{		
		return;
	}

	for (i = j = 0; i < numgltextures; ++i, ++j)
	{
		if (gltextures[i].type & TEX_TYPE_MAP)
		{
			Con_DPrintf("GL_FreeTextures: Clearing texture %s\n", gltextures[i].identifier);
			glDeleteTextures(1, &gltextures[i].texnum);
			--j;
		}
		else if (j < i)
			gltextures[j] = gltextures[i];
	}

	Cvar_Set("gl_cleanup_textures", "0");

	numgltextures = j;

}

/*
================
GL_LoadTexture
================
*/
int GL_LoadTexture (char *identifier, int width, int height, byte *data, qboolean mipmap, qboolean alpha, const int type)
{
	//static int only_one = 0;
	qboolean	noalpha;
	int			i, p, s;
	gltexture_t	*glt;


	// see if the texture is allready present
	if (identifier[0])
	{
		for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
		{
			if (!strcmp (identifier, glt->identifier))
			{
				if (width != glt->width || height != glt->height)
					Sys_Error ("GL_LoadTexture: cache mismatch");
				return gltextures[i].texnum;
			}
		}
	}
	else {
		glt = &gltextures[numgltextures];
		glt->type = type;
		numgltextures++;
	}

	strcpy (glt->identifier, identifier);
	//glt->texnum = texture_extension_number;
	//JAMES
	glGenTextures(1, &glt->texnum);
	//END

	glt->width = width;
	glt->height = height;
	glt->mipmap = mipmap;

	GL_Bind(glt->texnum );
	GL_Upload8 (data, width, height, mipmap, alpha);
	GL_Bind(0);

	texture_extension_number++;

	return glt->texnum;
}

/*
================
GL_LoadPicTexture
================
*/
int GL_LoadPicTexture (qpic_t *pic)
{
	return GL_LoadTexture ("", pic->width, pic->height, pic->data, false, true, TEX_TYPE_NONE);
}

/****************************************/

static GLenum oldtarget = TEXTURE0_SGIS;

void GL_SelectTexture (GLenum target) 
{
	if (!gl_mtexable)
		return;
	qglSelectTextureSGIS(target);
	if (target == oldtarget) 
		return;
	cnttextures[oldtarget-TEXTURE0_SGIS] = currenttexture;
	currenttexture = cnttextures[target-TEXTURE0_SGIS];
	oldtarget = target;
}
