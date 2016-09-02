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
// gl_warp.c -- sky and water polygons

#include "quakedef.h"

extern	model_t	*loadmodel;

int		skytexturenum;

int		solidskytexture = 0;
int		alphaskytexture = 0;
float	speedscale;		// for top sky and bottom sky

msurface_t	*warpface;

extern cvar_t gl_subdivide_size;

void BoundPoly (int numverts, float *verts, vec3_t mins, vec3_t maxs)
{
	int		i, j;
	float	*v;

	mins[0] = mins[1] = mins[2] = 9999;
	maxs[0] = maxs[1] = maxs[2] = -9999;
	v = verts;
	for (i=0 ; i<numverts ; i++)
		for (j=0 ; j<3 ; j++, v++)
		{
			if (*v < mins[j])
				mins[j] = *v;
			if (*v > maxs[j])
				maxs[j] = *v;
		}
}

void SubdividePolygon (int numverts, float *verts)
{
	int		i, j, k;
	vec3_t	mins, maxs;
	float	m;
	float	*v;
	vec3_t	front[64], back[64];
	int		f, b;
	float	dist[64];
	float	frac;
	glpoly_t	*poly;
	float	s, t;

	if (numverts > 60)
		Sys_Error ("numverts = %i", numverts);

	BoundPoly (numverts, verts, mins, maxs);

	for (i=0 ; i<3 ; i++)
	{
		m = (mins[i] + maxs[i]) * 0.5;
		m = gl_subdivide_size.value * floor (m/gl_subdivide_size.value + 0.5);
		if (maxs[i] - m < 8)
			continue;
		if (m - mins[i] < 8)
			continue;

		// cut it
		v = verts + i;
		for (j=0 ; j<numverts ; j++, v+= 3)
			dist[j] = *v - m;

		// wrap cases
		dist[j] = dist[0];
		v-=i;
		VectorCopy (verts, v);

		f = b = 0;
		v = verts;
		for (j=0 ; j<numverts ; j++, v+= 3)
		{
			if (dist[j] >= 0)
			{
				VectorCopy (v, front[f]);
				f++;
			}
			if (dist[j] <= 0)
			{
				VectorCopy (v, back[b]);
				b++;
			}
			if (dist[j] == 0 || dist[j+1] == 0)
				continue;
			if ( (dist[j] > 0) != (dist[j+1] > 0) )
			{
				// clip point
				frac = dist[j] / (dist[j] - dist[j+1]);
				for (k=0 ; k<3 ; k++)
					front[f][k] = back[b][k] = v[k] + frac*(v[3+k] - v[k]);
				f++;
				b++;
			}
		}

		SubdividePolygon (f, front[0]);
		SubdividePolygon (b, back[0]);
		return;
	}

	poly = Hunk_Alloc (sizeof(glpoly_t) + (numverts-4) * VERTEXSIZE*sizeof(float));
	poly->next = warpface->polys;
	warpface->polys = poly;
	poly->numverts = numverts;
	for (i=0 ; i<numverts ; i++, verts+= 3)
	{
		VectorCopy (verts, poly->verts[i]);
		s = DotProduct (verts, warpface->texinfo->vecs[0]);
		t = DotProduct (verts, warpface->texinfo->vecs[1]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;
	}
}

/*
================
GL_SubdivideSurface

Breaks a polygon up along axial 64 unit
boundaries so that turbulent and sky warps
can be done reasonably.
================
*/
void GL_SubdivideSurface (msurface_t *fa)
{
	vec3_t		verts[64];
	int			numverts;
	int			i;
	int			lindex;
	float		*vec;
	texture_t	*t;

	warpface = fa;

	//
	// convert edges back to a normal polygon
	//
	numverts = 0;
	for (i=0 ; i<fa->numedges ; i++)
	{
		lindex = loadmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
			vec = loadmodel->vertexes[loadmodel->edges[lindex].v[0]].position;
		else
			vec = loadmodel->vertexes[loadmodel->edges[-lindex].v[1]].position;
		VectorCopy (vec, verts[numverts]);
		numverts++;
	}

	SubdividePolygon (numverts, verts[0]);
}

//=========================================================



// speed up sin calculations - Ed
float	turbsin[] =
{
	#include "gl_warp_sin.h"
};
#define TURBSCALE (256.0 / (2 * M_PI))

/*
=============
EmitWaterPolys

Does a water warp on the pre-fragmented glpoly_t chain
=============
*/
extern void AppendGLPoly (glpoly_t *p);
void EmitWaterPolys (msurface_t *fa)
{
	glpoly_t	*p;
	float		*v,*first_vtx;
	int			i;
	float		s, t, os, ot;
	float		first_s, first_t;
	int ntriangles;

#if BATCH_BRUSH
#if SUBDIVIDE_WARP_POLYS
	for (p=fa->polys ; p ; p=p->next){
		AppendGLPoly(p);
	}
#else
	AppendGLPoly(fa->polys);
#endif
#else
	SetVertexMode(VAS_CLR_TEX);
	EnableTexture();
	BeginDrawing(RNDR_TRIANGLES);
	AddVertex4D (VTX_COLOUR, 1, 1, 1, r_wateralpha.value);	

	for (p=fa->polys ; p ; p=p->next)
	{
		ntriangles = (p->numverts-2);
		first_vtx = p->verts[0];	
		v = p->verts[0];	
		v += VERTEXSIZE;				
		
		for (i=0 ; i<ntriangles ; i++, v+= VERTEXSIZE){			
			os = first_vtx[3];	
			ot = first_vtx[4];
			s = os + turbsin[(int)((ot*0.125+realtime) * TURBSCALE) & 255];
			s *= (1.0/64);
			t = ot + turbsin[(int)((os*0.125+realtime) * TURBSCALE) & 255];
			t *= (1.0/64);
			AddVertex2D (VTX_TEXTURE, s, t);
			AddVertex3D (VTX_POSITION, first_vtx[0], first_vtx[1], first_vtx[2]);

			os = v[3];	
			ot = v[4];
			s = os + turbsin[(int)((ot*0.125+realtime) * TURBSCALE) & 255];
			s *= (1.0/64);
			t = ot + turbsin[(int)((os*0.125+realtime) * TURBSCALE) & 255];
			t *= (1.0/64);
			AddVertex2D (VTX_TEXTURE, s, t);
			AddVertex3D (VTX_POSITION, v[0], v[1], v[2]);

			os = v[10];	
			ot = v[11];
			s = os + turbsin[(int)((ot*0.125+realtime) * TURBSCALE) & 255];
			s *= (1.0/64);
			t = ot + turbsin[(int)((os*0.125+realtime) * TURBSCALE) & 255];
			t *= (1.0/64);
			AddVertex2D (VTX_TEXTURE, s, t);
			AddVertex3D (VTX_POSITION, v[7], v[8], v[9]);
		}
	}
	EndDrawing();
#endif
}




/*
=============
EmitSkyPolys
=============
*/
void EmitSkyPolys (msurface_t *fa)
{
	glpoly_t	*p;
	float		*v,*first_vtx;
	int			i;
	float	s, t;
	vec3_t	dir;
	float	length;
#if BATCH_BRUSH
	#if SUBDIVIDE_WARP_POLYS
	for (p=fa->polys ; p ; p=p->next){
		AppendGLPoly(p);
	}
	#else
	AppendGLPoly(fa->polys);
	#endif
#else
	for (p=fa->polys ; p ; p=p->next)
	{
		int ntriangles = (p->numverts-2);
		first_vtx = p->verts[0];	
		v = p->verts[0];	
		v += VERTEXSIZE;

		SetVertexMode(VAS_CLR_TEX);
		EnableTexture();
		BeginDrawing(RNDR_TRIANGLES);
		AddVertex4D (VTX_COLOUR, 1, 1, 1, 1);	

		for (i=0 ; i<ntriangles ; i++, v+= VERTEXSIZE){
		//for (i=0,v=p->verts[0] ; i<p->numverts ; i++, v+=VERTEXSIZE){
			// - - - - - - - - - - - - - - - - - - - - - - - - -

			VectorSubtract (first_vtx, r_origin, dir);
			dir[2] *= 3;	// flatten the sphere
			length = dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2];
			length = sqrt (length);
			length = 6*63/length;

			dir[0] *= length;	dir[1] *= length;
			s = (speedscale + dir[0]) * (1.0/128);		t = (speedscale + dir[1]) * (1.0/128);

			AddVertex2D (VTX_TEXTURE, s, t);
			AddVertex3D (VTX_POSITION, first_vtx[0], first_vtx[1], first_vtx[2]);

			// - - - - - - - - - - - - - - - - - - - - - - - - -
			VectorSubtract (v, r_origin, dir);
			dir[2] *= 3;	// flatten the sphere
			length = dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2];
			length = sqrt (length);
			length = 6*63/length;

			dir[0] *= length;	dir[1] *= length;
			s = (speedscale + dir[0]) * (1.0/128);		t = (speedscale + dir[1]) * (1.0/128);

			AddVertex2D (VTX_TEXTURE, s, t);
			AddVertex3D (VTX_POSITION, v[0], v[1], v[2]);

			// - - - - - - - - - - - - - - - - - - - - - - - - -

			VectorSubtract ((v+VERTEXSIZE), r_origin, dir);
			dir[2] *= 3;	// flatten the sphere
			length = dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2];
			length = sqrt (length);
			length = 6*63/length;

			dir[0] *= length;	dir[1] *= length;
			s = (speedscale + dir[0]) * (1.0/128);		t = (speedscale + dir[1]) * (1.0/128);

			AddVertex2D (VTX_TEXTURE, s, t);
			AddVertex3D (VTX_POSITION, (v+VERTEXSIZE)[0], (v+VERTEXSIZE)[1], (v+VERTEXSIZE)[2]);
		}
		EndDrawing();
		

		/*glBegin (GL_POLYGON);
		
		for (i=0,v=p->verts[0] ; i<p->numverts ; i++, v+=VERTEXSIZE)
		{
			VectorSubtract (v, r_origin, dir);
			dir[2] *= 3;	// flatten the sphere

			length = dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2];
			length = sqrt (length);
			length = 6*63/length;

			dir[0] *= length;
			dir[1] *= length;

			s = (speedscale + dir[0]) * (1.0/128);
			t = (speedscale + dir[1]) * (1.0/128);

			//glTexCoord2f (s, t);
			//glVertex3fv (v);
		}
		glEnd ();
		*/
	}
#endif
}

/*
===============
EmitBothSkyLayers

Does a sky warp on the pre-fragmented glpoly_t chain
This will be called for brushmodels, the world
will have them chained together.
===============
*/
void EmitBothSkyLayers (msurface_t *fa)
{
	int			i;
	int			lindex;
	float		*vec;

	GL_DisableMultitexture();

	GL_Bind (solidskytexture);
	speedscale = realtime*8;
	speedscale -= (int)speedscale & ~127 ;

	EmitSkyPolys (fa);

	glEnable (GL_BLEND);
	GL_BindNoFlush (alphaskytexture);
	speedscale = realtime*16;
	speedscale -= (int)speedscale & ~127 ;

	EmitSkyPolys (fa);

	glDisable (GL_BLEND);
}

#ifndef QUAKE2
/*
=================
R_DrawSkyChain
=================
*/
void R_DrawSkyChain (msurface_t *s)
{
	msurface_t	*fa;

	GL_BindNoFlush(solidskytexture, TEX_SLOT_SKY);
	GL_BindNoFlush(alphaskytexture, TEX_SLOT_SKY_ALPHA);

	for (fa=s ; fa ; fa=fa->texturechain)
		EmitSkyPolys (fa);

	DrawGLPoly();
}

#endif

//===============================================================

/*
=============
R_InitSky

A sky texture is 256*128, with the right side being a masked overlay
==============
*/
void R_InitSky (texture_t *mt)
{
	int			i, j, p;
	byte		*src;
	unsigned	trans[128*128];
	unsigned	transpix;
	int			r, g, b;
	unsigned	*rgba;
	extern	int			skytexturenum;

	src = (byte *)mt + mt->offsets[0];

	// make an average value for the back to avoid
	// a fringe on the top level

	r = g = b = 0;
	for (i=0 ; i<128 ; i++)
		for (j=0 ; j<128 ; j++)
		{
			p = src[i*256 + j + 128];
			rgba = &d_8to24table[p];
			trans[(i*128) + j] = *rgba;
			r += ((byte *)rgba)[0];
			g += ((byte *)rgba)[1];
			b += ((byte *)rgba)[2];
		}

	((byte *)&transpix)[0] = r/(128*128);
	((byte *)&transpix)[1] = g/(128*128);
	((byte *)&transpix)[2] = b/(128*128);
	((byte *)&transpix)[3] = 0;


	if (!solidskytexture){
		glGenTextures(1, &solidskytexture); 
	}
	GL_Bind (solidskytexture );
	//glTexImage2D (GL_TEXTURE_2D, 0, texDataType[gl_solid_format], 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);
	glTexImage2D (GL_TEXTURE_2D, 0, texDataType[gl_solid_format], 128, 128, 0, texDataType[gl_solid_format], GL_UNSIGNED_BYTE, trans);
#if LINEAR_TEXTURES
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#else
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
#endif


	for (i=0 ; i<128 ; i++)
		for (j=0 ; j<128 ; j++)
		{
			p = src[i*256 + j];
			if (p == 0)
				trans[(i*128) + j] = transpix;
			else
				trans[(i*128) + j] = d_8to24table[p];
		}

	if (!alphaskytexture){
		glGenTextures(1, &alphaskytexture); 
	}

	GL_Bind(alphaskytexture);
	//glTexImage2D (GL_TEXTURE_2D, 0, texDataType[gl_alpha_format], 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);
	glTexImage2D (GL_TEXTURE_2D, 0, texDataType[gl_alpha_format], 128, 128, 0, texDataType[gl_alpha_format], GL_UNSIGNED_BYTE, trans);
#if LINEAR_TEXTURES
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#else
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
#endif
}

