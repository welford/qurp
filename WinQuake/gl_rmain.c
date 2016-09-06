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
// r_main.c

#include "quakedef.h"

entity_t	r_worldentity;

qboolean	r_cache_thrash;		// compatability

vec3_t		modelorg, r_entorigin;
entity_t	*currententity;

int			r_visframecount;	// bumped when going to a new PVS
int			r_framecount;		// used for dlight push checking

mplane_t	frustum[4];

int			c_brush_polys, c_alias_polys;

qboolean	envmap;				// true during envmap command capture 

int			currenttexture = -1;		// to avoid unnecessary texture sets

int			cnttextures[2] = {-1, -1};     // cached

int			particletexture;	// little dot for particles
int			playertextures;		// up to 16 color translated skins
int			playertextures_gl[16];

int			mirrortexturenum;	// quake texturenum, not gltexturenum
qboolean	mirror;
mplane_t	*mirror_plane;

//
// view origin
//
vec3_t	vup;
vec3_t	vpn;
vec3_t	vright;
vec3_t	r_origin;

float	r_world_matrix[16];
float	r_base_world_matrix[16];

//
// screen size info
//
refdef_t	r_refdef;

mleaf_t		*r_viewleaf, *r_oldviewleaf;

texture_t	*r_notexture_mip;

int		d_lightstylevalue[256];	// 8.8 fraction of base light value


void R_MarkLeaves (void);

cvar_t	r_norefresh = {"r_norefresh","0"};
cvar_t	r_drawentities = {"r_drawentities","1"};
cvar_t	r_drawviewmodel = {"r_drawviewmodel","1"};
cvar_t	r_speeds = {"r_speeds","0"};
cvar_t	r_fullbright = {"r_fullbright","0"};
cvar_t	r_lightmap = {"r_lightmap","0"};
cvar_t	r_shadows = {"r_shadows","0"};
cvar_t	r_mirroralpha = {"r_mirroralpha","1"};
cvar_t	r_wateralpha = {"r_wateralpha","1"};
cvar_t	r_dynamic = {"r_dynamic","1"};
cvar_t	r_novis = {"r_novis","0"};

cvar_t	gl_finish = {"gl_finish","0"};
cvar_t	gl_clear = {"gl_clear","1"};
cvar_t	gl_cull = {"gl_cull","1"};
cvar_t	gl_smoothmodels = {"gl_smoothmodels","1"};
cvar_t	gl_affinemodels = {"gl_affinemodels","0"};
cvar_t	gl_polyblend = {"gl_polyblend","1"};
cvar_t	gl_flashblend = {"gl_flashblend","1"};
cvar_t	gl_playermip = {"gl_playermip","0"};
cvar_t	gl_nocolors = {"gl_nocolors","0"};
cvar_t	gl_keeptjunctions = {"gl_keeptjunctions","0"};
cvar_t	gl_reporttjunctions = {"gl_reporttjunctions","0"};
cvar_t	gl_doubleeyes = {"gl_doubleeys", "1"};

extern	cvar_t	gl_ztrick;
static int trickframe = 0;

/*
=================
R_CullBox

Returns true if the box is completely outside the frustom
=================
*/
qboolean R_CullBox (vec3_t mins, vec3_t maxs)
{
	int		i;

	for (i=0 ; i<4 ; i++)
		if (BoxOnPlaneSide (mins, maxs, &frustum[i]) == 2)
			return true;
	return false;
}


void R_RotateForEntity (entity_t *e)
{
	Matrix44 mtxX,mtxY,mtxZ;	
	Translate(e->origin[0],  e->origin[1],  e->origin[2]);

	matRotateZ44(DEG_TO_RAD(e->angles[1]), &mtxZ);
	matRotateY44(DEG_TO_RAD(-e->angles[0]), &mtxY);
	matRotateX44(DEG_TO_RAD(e->angles[2]), &mtxX);

	TransformMatrix(mtxZ.a);
	TransformMatrix(mtxY.a);
	TransformMatrix(mtxX.a);
	/*
    glTranslatef (e->origin[0],  e->origin[1],  e->origin[2]);
	glRotatef (e->angles[1],  0, 0, 1);
    glRotatef (-e->angles[0],  0, 1, 0);
    glRotatef (e->angles[2],  1, 0, 0);

	glGetFloatv (GL_MODELVIEW_MATRIX, r_world_matrix);
	*/
}

/*
=============================================================

  SPRITE MODELS

=============================================================
*/

/*
================
R_GetSpriteFrame
================
*/
mspriteframe_t *R_GetSpriteFrame (entity_t *currententity)
{
	msprite_t		*psprite;
	mspritegroup_t	*pspritegroup;
	mspriteframe_t	*pspriteframe;
	int				i, numframes, frame;
	float			*pintervals, fullinterval, targettime, time;

	psprite = currententity->model->cache.data;
	frame = currententity->frame;

	if ((frame >= psprite->numframes) || (frame < 0))
	{
		Con_Printf ("R_DrawSprite: no such frame %d\n", frame);
		frame = 0;
	}

	if (psprite->frames[frame].type == SPR_SINGLE)
	{
		pspriteframe = psprite->frames[frame].frameptr;
	}
	else
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes-1];

		time = cl.time + currententity->syncbase;

	// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
	// are positive, so we don't have to worry about division by 0
		targettime = time - ((int)(time / fullinterval)) * fullinterval;

		for (i=0 ; i<(numframes-1) ; i++)
		{
			if (pintervals[i] > targettime)
				break;
		}

		pspriteframe = pspritegroup->frames[i];
	}

	return pspriteframe;
}


/*
=================
R_DrawSpriteModel

=================
*/
void R_DrawSpriteModel (entity_t *e)
{
	vec3_t	point;
	mspriteframe_t	*frame;
	float		*up, *right;
	vec3_t		v_forward, v_right, v_up;
	msprite_t		*psprite;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache
	frame = R_GetSpriteFrame (e);
	psprite = currententity->model->cache.data;

	if (psprite->type == SPR_ORIENTED)
	{	// bullet marks on walls
		AngleVectors (currententity->angles, v_forward, v_right, v_up);
		up = v_up;
		right = v_right;
	}
	else
	{	// normal sprite
		up = vup;
		right = vright;
	}

	GL_DisableMultitexture();
    GL_Bind(frame->gl_texturenum);

	EnableBlending();
	SetVertexMode(VAS_CLR_TEX);

	BeginDrawing(RNDR_TRIANGLE_STRIP);	
	AddVertex4D (VTX_COLOUR, 1.0f, 1.0f, 1.0f, 1.0f);
	
	VectorMA (e->origin, frame->down, up, point);
	VectorMA (point, frame->left, right, point);
	AddVertex2D (VTX_TEXTURE, 0, 1);
	AddVertex3D (VTX_POSITION, point[0], point[1], point[2]);	

	VectorMA (e->origin, frame->up, up, point);
	VectorMA (point, frame->left, right, point);
	AddVertex2D (VTX_TEXTURE, 0, 0);
	AddVertex3D (VTX_POSITION, point[0], point[1], point[2]);	
	
	VectorMA (e->origin, frame->down, up, point);
	VectorMA (point, frame->right, right, point);
	AddVertex2D (VTX_TEXTURE, 1, 1);
	AddVertex3D (VTX_POSITION, point[0], point[1], point[2]);
	
	VectorMA (e->origin, frame->up, up, point);
	VectorMA (point, frame->right, right, point);
	AddVertex2D (VTX_TEXTURE, 1, 0);
	AddVertex3D (VTX_POSITION, point[0], point[1], point[2]);

	EndDrawing();

	DisableBlending();
}

/*
=============================================================

  ALIAS MODELS

=============================================================
*/


#define NUMVERTEXNORMALS	162

float	r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};

vec3_t	shadevector;
float	shadelight, ambientlight;

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
float	r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "anorm_dots.h"
;

int		shadedotsIndex = 0;
float	*shadedots = r_avertexnormal_dots[0];

int	lastposenum;

/*
=============
GL_DrawAliasFrame
=============
*/
void GL_DrawAliasFrame (aliashdr_t *paliashdr, int posenum)
{
	float	s, t;
	float 	l;
	int		i, j;
	int		index;
	trivertx_t	*v, *verts;
	int		list;
	int		*order;
	vec3_t	point;
	float	*normal;
	int		count;
	int		n_tris = 0;

#if ALIAS_VBO  
	RenderAlias(paliashdr->vbo_offset, posenum, paliashdr->numtris, shadedotsIndex, shadelight, (float)(ambientlight / 256.0f));
#else

	//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
	//super cheap speed hack to get one draw call instead of Many
	//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
	int mode = 0; //0 fan, 1 strip
	int vtx_idx = 0;
	int emitt_poly = 0;
	float vtx_history0[6];//vtx,clr,tx
	float vtx_history1[6];//vtx,clr,tx
	float vtx_history2[6];//vtx,clr,tx

	lastposenum = posenum;

	verts = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	order = (int *)((byte *)paliashdr + paliashdr->commands);

	SetVertexMode(VAS_VTX_CLR_TEX);
	BeginDrawing( RNDR_TRIANGLES);
	while (1)
	{
		// get the vertex count and primitive type
		count = *order++;
		if (!count)
			break;		// done
		if (count < 0)
		{
			count = -count;
			mode = 0;
			vtx_idx = 0;
			emitt_poly = 0;
			//BeginDrawing( RNDR_TRIANGLE_FAN );
		}
		else{
			//BeginDrawing( RNDR_TRIANGLE_STRIP );
			mode = 1;
			vtx_idx = 0;
			emitt_poly = 0;
		}
		
		do
		{
			// normals and vertexes come from the frame list
			l = shadedots[verts->lightnormalindex] * shadelight;

			//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
			//so ugly forgive me
			//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
			if(mode == 0){
				if(vtx_idx == 0){
					vtx_history0[0] = verts->v[0];	vtx_history0[1] = verts->v[1];	vtx_history0[2] = verts->v[2];
					vtx_history0[3] = l;
					vtx_history0[4] = ((float *)order)[0]; vtx_history0[5] = ((float *)order)[1];
				}
				else{
					if( (vtx_idx-1) % 2 == 0 ){
						vtx_history1[0] = verts->v[0];	vtx_history1[1] = verts->v[1];	vtx_history1[2] = verts->v[2];
						vtx_history1[3] = l;
						vtx_history1[4] = ((float *)order)[0]; vtx_history1[5] = ((float *)order)[1];
						if(vtx_idx-1 != 0){
							emitt_poly++;
						}
					}
					else if( (vtx_idx-1) % 2 == 1 ){
						vtx_history2[0] = verts->v[0];	vtx_history2[1] = verts->v[1];	vtx_history2[2] = verts->v[2];
						vtx_history2[3] = l;
						vtx_history2[4] = ((float *)order)[0]; vtx_history2[5] = ((float *)order)[1];
						emitt_poly++;
					}
				}
				vtx_idx++;
				if(vtx_idx > 2){
					if(emitt_poly % 2 == 1){
						AddVertex4D(VTX_COLOUR, vtx_history0[3], vtx_history0[3], vtx_history0[3], vtx_history0[3]);
						AddVertex2D(VTX_TEXTURE, vtx_history0[4], vtx_history0[5]);			
						AddVertex3D( VTX_POSITION, vtx_history0[0], vtx_history0[1], vtx_history0[2]);

						AddVertex4D(VTX_COLOUR, vtx_history1[3], vtx_history1[3], vtx_history1[3], vtx_history1[3]);
						AddVertex2D(VTX_TEXTURE, vtx_history1[4], vtx_history1[5]);			
						AddVertex3D( VTX_POSITION, vtx_history1[0], vtx_history1[1], vtx_history1[2]);

						AddVertex4D(VTX_COLOUR, vtx_history2[3], vtx_history2[3], vtx_history2[3], vtx_history2[3]);
						AddVertex2D(VTX_TEXTURE, vtx_history2[4], vtx_history2[5]);			
						AddVertex3D( VTX_POSITION, vtx_history2[0], vtx_history2[1], vtx_history2[2]);
					}
					else{
						AddVertex4D(VTX_COLOUR, vtx_history0[3], vtx_history0[3], vtx_history0[3], vtx_history0[3]);
						AddVertex2D(VTX_TEXTURE, vtx_history0[4], vtx_history0[5]);			
						AddVertex3D( VTX_POSITION, vtx_history0[0], vtx_history0[1], vtx_history0[2]);

						AddVertex4D(VTX_COLOUR, vtx_history2[3], vtx_history2[3], vtx_history2[3], vtx_history2[3]);
						AddVertex2D(VTX_TEXTURE, vtx_history2[4], vtx_history2[5]);			
						AddVertex3D( VTX_POSITION, vtx_history2[0], vtx_history2[1], vtx_history2[2]);

						AddVertex4D(VTX_COLOUR, vtx_history1[3], vtx_history1[3], vtx_history1[3], vtx_history1[3]);
						AddVertex2D(VTX_TEXTURE, vtx_history1[4], vtx_history1[5]);			
						AddVertex3D( VTX_POSITION, vtx_history1[0], vtx_history1[1], vtx_history1[2]);
					}
				}
				
			}
			else{
				if(vtx_idx % 3 == 0){
					vtx_history0[0] = verts->v[0];	vtx_history0[1] = verts->v[1];	vtx_history0[2] = verts->v[2];
					vtx_history0[3] = l;
					vtx_history0[4] = ((float *)order)[0]; vtx_history0[5] = ((float *)order)[1];
					if(vtx_idx > 2)
						emitt_poly++;

				}
				else if( vtx_idx % 3 == 1 ){
						vtx_history1[0] = verts->v[0];	vtx_history1[1] = verts->v[1];	vtx_history1[2] = verts->v[2];
						vtx_history1[3] = l;
						vtx_history1[4] = ((float *)order)[0]; vtx_history1[5] = ((float *)order)[1];
						if(vtx_idx > 2)
							emitt_poly++;
				}
				else if( vtx_idx % 3 == 2 ){
						vtx_history2[0] = verts->v[0];	vtx_history2[1] = verts->v[1];	vtx_history2[2] = verts->v[2];
						vtx_history2[3] = l;
						vtx_history2[4] = ((float *)order)[0]; vtx_history2[5] = ((float *)order)[1];
						emitt_poly++;
				}
				vtx_idx ++;
				if(vtx_idx > 2){
					if(emitt_poly % 2 == 1){
						AddVertex4D(VTX_COLOUR, vtx_history0[3], vtx_history0[3], vtx_history0[3], vtx_history0[3]);
						AddVertex2D(VTX_TEXTURE, vtx_history0[4], vtx_history0[5]);			
						AddVertex3D( VTX_POSITION, vtx_history0[0], vtx_history0[1], vtx_history0[2]);

						AddVertex4D(VTX_COLOUR, vtx_history1[3], vtx_history1[3], vtx_history1[3], vtx_history1[3]);
						AddVertex2D(VTX_TEXTURE, vtx_history1[4], vtx_history1[5]);			
						AddVertex3D( VTX_POSITION, vtx_history1[0], vtx_history1[1], vtx_history1[2]);

						AddVertex4D(VTX_COLOUR, vtx_history2[3], vtx_history2[3], vtx_history2[3], vtx_history2[3]);
						AddVertex2D(VTX_TEXTURE, vtx_history2[4], vtx_history2[5]);			
						AddVertex3D( VTX_POSITION, vtx_history2[0], vtx_history2[1], vtx_history2[2]);				
					}else{
						AddVertex4D(VTX_COLOUR, vtx_history0[3], vtx_history0[3], vtx_history0[3], vtx_history0[3]);
						AddVertex2D(VTX_TEXTURE, vtx_history0[4], vtx_history0[5]);			
						AddVertex3D( VTX_POSITION, vtx_history0[0], vtx_history0[1], vtx_history0[2]);

						AddVertex4D(VTX_COLOUR, vtx_history2[3], vtx_history2[3], vtx_history2[3], vtx_history2[3]);
						AddVertex2D(VTX_TEXTURE, vtx_history2[4], vtx_history2[5]);			
						AddVertex3D( VTX_POSITION, vtx_history2[0], vtx_history2[1], vtx_history2[2]);				
					
						AddVertex4D(VTX_COLOUR, vtx_history1[3], vtx_history1[3], vtx_history1[3], vtx_history1[3]);
						AddVertex2D(VTX_TEXTURE, vtx_history1[4], vtx_history1[5]);			
						AddVertex3D( VTX_POSITION, vtx_history1[0], vtx_history1[1], vtx_history1[2]);				
					}
				}
			}			
			verts++;			
			order += 2;					

		} while (--count);		
	}
	EndDrawing();
#endif //_WIN32
}


/*
=============
GL_DrawAliasShadow
=============
*/
extern	vec3_t			lightspot;

void GL_DrawAliasShadow (aliashdr_t *paliashdr, int posenum)
{
	float	s, t, l;
	int		i, j;
	int		index;
	trivertx_t	*v, *verts;
	int		list;
	int		*order;
	vec3_t	point;
	float	*normal;
	float	height, lheight;
	int		count;

	lheight = currententity->origin[2] - lightspot[2];

	height = 0;
	verts = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	order = (int *)((byte *)paliashdr + paliashdr->commands);

	height = -lheight + 1.0;
	
	SetVertexMode(VAS_CLR);	
	AddVertex4D(VTX_COLOUR,0,0,0,1);
	while (1)
	{
		// get the vertex count and primitive type
		count = *order++;
		if (!count)
			break;		// done
		if (count < 0){
			count = -count;
			BeginDrawing( RNDR_TRIANGLE_FAN );
		}
		else{
			BeginDrawing( RNDR_TRIANGLE_STRIP );			
		}

		do
		{
			// texture coordinates come from the draw list
			// (skipped for shadows) glTexCoord2fv ((float *)order);
			order += 2;

			// normals and vertexes come from the frame list
			point[0] = verts->v[0] * paliashdr->scale[0] + paliashdr->scale_origin[0];
			point[1] = verts->v[1] * paliashdr->scale[1] + paliashdr->scale_origin[1];
			point[2] = verts->v[2] * paliashdr->scale[2] + paliashdr->scale_origin[2];

			point[0] -= shadevector[0]*(point[2]+lheight);
			point[1] -= shadevector[1]*(point[2]+lheight);
			point[2] = height;
//			height -= 0.001;
			//glVertex3fv (point);
			AddVertex3D( VTX_POSITION, point[0], point[1], point[2]);

			verts++;
		} while (--count);
		EndDrawing();
		//glEnd ();		
	}	
}



/*
=================
R_SetupAliasFrame

=================
*/
void R_SetupAliasFrame (int frame, aliashdr_t *paliashdr)
{
	int				pose, numposes;
	float			interval;

	if ((frame >= paliashdr->numframes) || (frame < 0))
	{
		Con_DPrintf ("R_AliasSetupFrame: no such frame %d\n", frame);
		frame = 0;
	}

	pose = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	if (numposes > 1)
	{
		interval = paliashdr->frames[frame].interval;
		pose += (int)(cl.time / interval) % numposes;
	}

	GL_DrawAliasFrame (paliashdr, pose);
}



/*
=================
R_DrawAliasModel

=================
*/
//static int n_alias_draw = 0;
void R_DrawAliasModel (entity_t *e)
{
	int			i, j;
	int			lnum;
	vec3_t		dist;
	float		add;
	model_t		*clmodel;
	vec3_t		mins, maxs;
	aliashdr_t	*paliashdr;
	trivertx_t	*verts, *v;
	int			index;
	float		s, t, an;
	int			anim;

	clmodel = currententity->model;

	VectorAdd (currententity->origin, clmodel->mins, mins);
	VectorAdd (currententity->origin, clmodel->maxs, maxs);

	if (R_CullBox (mins, maxs))
		return;

	//n_alias_draw++;

	VectorCopy (currententity->origin, r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);

	//
	// get lighting information
	//

	ambientlight = shadelight = R_LightPoint (currententity->origin);

	// allways give the gun some light
	if (e == &cl.viewent && ambientlight < 24)
		ambientlight = shadelight = 24;

	for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
	{
		if (cl_dlights[lnum].die >= cl.time)
		{
			VectorSubtract (currententity->origin,
							cl_dlights[lnum].origin,
							dist);
			add = cl_dlights[lnum].radius - Length(dist);

			if (add > 0) {
				ambientlight += add;
				//ZOID models should be affected by dlights as well
				//shadelight += add;
			}
		}
	}

	// clamp lighting so it doesn't overbright as much
	if (ambientlight > 128)
		ambientlight = 128;
	if (ambientlight + shadelight > 192)
		shadelight = 192 - ambientlight;

	// ZOID: never allow players to go totally black
	i = currententity - cl_entities;
	if (i >= 1 && i<=cl.maxclients /* && !strcmp (currententity->model->name, "progs/player.mdl") */)
		if (ambientlight < 8)
			ambientlight = shadelight = 8;

	shadedotsIndex = ((int)(e->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1);
	shadedots = r_avertexnormal_dots[shadedotsIndex];
	shadelight = shadelight / 200.0;
	
	an = e->angles[1]/180*M_PI;
	shadevector[0] = cos(-an);
	shadevector[1] = sin(-an);
	shadevector[2] = 1;
	VectorNormalize (shadevector);

	//
	// locate the proper data
	//
	paliashdr = (aliashdr_t *)Mod_Extradata (currententity->model);

	c_alias_polys += paliashdr->numtris;

	//
	// draw all the triangles
	//

	GL_DisableMultitexture();

	Push();
	R_RotateForEntity (e);

	if (!strcmp (clmodel->name, "progs/eyes.mdl") && gl_doubleeyes.value) {
		Translate(paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2] - (22 + 8));
		// double size of eyes, since they are really hard to see in gl
		Scale(paliashdr->scale[0]*2, paliashdr->scale[1]*2, paliashdr->scale[2]*2);		
	} else {		
		Translate(paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
		Scale(paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);
	}

	anim = (int)(cl.time*10) & 3;
    GL_Bind(paliashdr->gl_texturenum[currententity->skinnum][anim]);

	// we can't dynamically colormap textures, so they are cached
	// seperately for the players.  Heads are just uncolored.
	if (currententity->colormap != vid.colormap && !gl_nocolors.value)
	{
		i = currententity - cl_entities;
		if (i >= 1 && i<=cl.maxclients /* && !strcmp (currententity->model->name, "progs/player.mdl") */){
		   // GL_Bind(playertextures - 1 + i);
			GL_Bind(playertextures_gl[- 1 + i]);
		}
	}

	R_SetupAliasFrame (currententity->frame, paliashdr);

	Pop();

	if (r_shadows.value)
	{
		/*
		Push();
		R_RotateForEntity (e);
		DisableBlending();
		DisableTexture();
		GL_DrawAliasShadow (paliashdr, lastposenum);
		EnableBlending();
		EnableTexture();
		Pop();
		*/
	}

}

//==================================================================================

/*
=============
R_DrawEntitiesOnList
=============
*/
void R_DrawEntitiesOnList (void)
{
	int		i;

	if (!r_drawentities.value)
		return;

	//n_alias_draw = 0;

	// draw sprites seperately, because of alpha blending
#
	//alias first as they will eventuall all be in the 
	//same VBO
	StartAliasBatch(gldepthmin, gldepthmax);
	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = cl_visedicts[i];
		switch (currententity->model->type)
		{
		case mod_alias:
			R_DrawAliasModel (currententity);
			break;
		default:
			break;
		}
	}
	EndAliasBatch();

	StartBrushBatch(gldepthmin, gldepthmax);
	for (i = 0; i<cl_numvisedicts; i++)
	{
		currententity = cl_visedicts[i];
		switch (currententity->model->type)
		{
		case mod_brush:
			R_DrawBrushModel(currententity);
			break;
		default:
			break;
		}
	}
	EndBrushBatch();

	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = cl_visedicts[i];

		switch (currententity->model->type)
		{
		case mod_sprite:
			R_DrawSpriteModel (currententity);
			break;
		}
	}

	//printf("number of entities called : %d", n_alias_draw);
}

/*
=============
R_DrawViewModel
=============
*/
void R_DrawViewModel (void)
{
	int			j;
	int			lnum;
	vec3_t		dist;
	float		add;
	dlight_t	*dl;
	//int			ambientlight, shadelight;

	if (!r_drawviewmodel.value)
		return;

	if (chase_active.value)
		return;

	if (envmap)
		return;

	if (!r_drawentities.value)
		return;

	if (cl.items & IT_INVISIBILITY)
		return;

	if (cl.stats[STAT_HEALTH] <= 0)
		return;

	currententity = &cl.viewent;
	if (!currententity->model)
		return;

	j = R_LightPoint (currententity->origin);

	if (j < 24)
		j = 24;		// allways give some light on gun

	ambientlight = j;
	shadelight = j;

	// add dynamic lights
	for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
	{
		dl = &cl_dlights[lnum];
		if (!dl->radius)
			continue;
		if (!dl->radius)
			continue;
		if (dl->die < cl.time)
			continue;

		VectorSubtract (currententity->origin, dl->origin, dist);
		add = dl->radius - Length(dist);
		if (add > 0)
			ambientlight += add;
	}

	// hack the depth range to prevent view model from poking into walls
	StartAliasBatch(gldepthmin, gldepthmin + 0.3*( gldepthmax - gldepthmin ));
	R_DrawAliasModel (currententity);	
	EndAliasBatch();
	SetDepthRange (gldepthmin, gldepthmax);
}


/*
============
R_PolyBlend
============
*/
int R_PolyBlend (void)
{
	Matrix44 mtxX,mtxZ;

	if (!gl_polyblend.value)
		return 0;
	if (!v_blend[3])
		return 0;

	FlushDraw();
	GL_DisableMultitexture();

	DisableAlphaTest();
	EnableBlending();
	DisableDepth();
	DisableTexture();

	Identity();
	matRotateX44(DEG_TO_RAD(-90), &mtxX);
	matRotateZ44(DEG_TO_RAD(90), &mtxZ);
	TransformMatrix(mtxX.a);
	TransformMatrix(mtxZ.a);	

	SetVertexMode(VAS_CLR);

	BeginDrawing(RNDR_TRIANGLE_STRIP);
	AddVertex4D (VTX_COLOUR, v_blend[0], v_blend[1], v_blend[2], v_blend[3]);	 
	
	int width = -(vid.width), height = vid.height;
	
	AddVertex3D (VTX_POSITION, 10, 0,		height);
	AddVertex3D (VTX_POSITION, 10, width,	height);	
	AddVertex3D (VTX_POSITION, 10, 0,		0);
	AddVertex3D (VTX_POSITION, 10, width,	0);

	EndDrawing();
	FlushDraw();

	EnableAlphaTest();
	DisableBlending();
	EnableTexture();
	EnableDepth();
	return 1;
}


int SignbitsForPlane (mplane_t *out)
{
	int	bits, j;

	// for fast box on planeside test

	bits = 0;
	for (j=0 ; j<3 ; j++)
	{
		if (out->normal[j] < 0)
			bits |= 1<<j;
	}
	return bits;
}


void R_SetFrustum (void)
{
	int		i;

	if (r_refdef.fov_x == 90) 
	{
		// front side is visible

		VectorAdd (vpn, vright, frustum[0].normal);
		VectorSubtract (vpn, vright, frustum[1].normal);

		VectorAdd (vpn, vup, frustum[2].normal);
		VectorSubtract (vpn, vup, frustum[3].normal);
	}
	else
	{
		// rotate VPN right by FOV_X/2 degrees
		RotatePointAroundVector( frustum[0].normal, vup, vpn, -(90-r_refdef.fov_x / 2 ) );
		// rotate VPN left by FOV_X/2 degrees
		RotatePointAroundVector( frustum[1].normal, vup, vpn, 90-r_refdef.fov_x / 2 );
		// rotate VPN up by FOV_X/2 degrees
		RotatePointAroundVector( frustum[2].normal, vright, vpn, 90-r_refdef.fov_y / 2 );
		// rotate VPN down by FOV_X/2 degrees
		RotatePointAroundVector( frustum[3].normal, vright, vpn, -( 90 - r_refdef.fov_y / 2 ) );
	}

	for (i=0 ; i<4 ; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}
}



/*
===============
R_SetupFrame
===============
*/
void R_SetupFrame (void)
{
	int				edgecount;
	vrect_t			vrect;
	float			w, h;

// don't allow cheats in multiplayer
	if (cl.maxclients > 1)
		Cvar_Set ("r_fullbright", "0");

	R_AnimateLight ();

	r_framecount++;

// build the transformation matrix for the given view angles
	VectorCopy (r_refdef.vieworg, r_origin);

	AngleVectors (r_refdef.viewangles, vpn, vright, vup);

// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_viewleaf = Mod_PointInLeaf (r_origin, cl.worldmodel);

	V_SetContentsColor (r_viewleaf->contents);
	V_CalcBlend ();

	r_cache_thrash = false;

	c_brush_polys = 0;
	c_alias_polys = 0;

}


void MYgluPerspective( double fovy, double aspect,
		     double zNear, double zFar )
{
   double xmin, xmax, ymin, ymax;

   ymax = zNear * tan( fovy * M_PI / 360.0 );
   ymin = -ymax;

   xmin = ymin * aspect;
   xmax = ymax * aspect;

   //glFrustum( xmin, xmax, ymin, ymax, zNear, zFar );
   SetProjectionMatrix(xmin, xmax, ymin, ymax, zNear, zFar );
}


/*
=============
R_SetupGL
=============
*/
void R_SetupGL (void)
{
	float	screenaspect;
	float	yfov;
	int		i;
	extern	int glwidth, glheight;
	int		x, x2, y2, y, w, h;
	//JAMES
	Matrix44 mtxX,mtxY,mtxZ;	
	float mtx_z_up[16] = {
		0.0f, 0.0f, -1.0f, 0.0f,
		-1.0, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	};
	//END

	//
	// set up viewpoint
	//
	//glMatrixMode(GL_PROJECTION);
    //glLoadIdentity ();
	x = r_refdef.vrect.x * glwidth/vid.width;
	x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * glwidth/vid.width;
	y = (vid.height-r_refdef.vrect.y) * glheight/vid.height;
	y2 = (vid.height - (r_refdef.vrect.y + r_refdef.vrect.height)) * glheight/vid.height;

	// fudge around because of frac screen scale
	if (x > 0)
		x--;
	if (x2 < glwidth)
		x2++;
	if (y2 < 0)
		y2--;
	if (y < glheight)
		y++;

	w = x2 - x;
	h = y - y2;

	if (envmap)
	{
		x = y2 = 0;
		w = h = 256;
	}

	//glViewport (glx + x, gly + y2, w, h);
	SetViewport(glx + x, gly + y2, w, h);
	glClear( GL_DEPTH_BUFFER_BIT );
    screenaspect = (float)r_refdef.vrect.width/r_refdef.vrect.height;
//	yfov = 2*atan((float)r_refdef.vrect.height/r_refdef.vrect.width)*180/M_PI;
    MYgluPerspective (r_refdef.fov_y,  screenaspect,  4,  4096);

	if (mirror)
	{
		if (mirror_plane->normal[2])
			glScalef (1, -1, 1);
		else
			glScalef (-1, 1, 1);
		//glCullFace(GL_BACK);
		CullBack();
	}
	else{
		CullFront();

		//glCullFace(GL_FRONT);
	}
	
	
	/*
	glMatrixMode(GL_MODELVIEW);
    glLoadIdentity ();
    glRotatef (-90,  1, 0, 0);
    glRotatef (90,  0, 0, 1);
	
	//rotate camera 
    glRotatef (-r_refdef.viewangles[2],  1, 0, 0);
    glRotatef (-r_refdef.viewangles[0],  0, 1, 0);
    glRotatef (-r_refdef.viewangles[1],  0, 0, 1);	

	//translate out by camera distance
    glTranslatef (-r_refdef.vieworg[0],  -r_refdef.vieworg[1],  -r_refdef.vieworg[2]);

	glGetFloatv (GL_MODELVIEW_MATRIX, r_world_matrix);
	*/
		
	matRotateX44(DEG_TO_RAD(-r_refdef.viewangles[2]), &mtxX);
	matRotateY44(DEG_TO_RAD(-r_refdef.viewangles[0]), &mtxY);
	matRotateZ44(DEG_TO_RAD(-r_refdef.viewangles[1]), &mtxZ);

	SetMatrix(mtx_z_up);
	TransformMatrix(mtxX.a);
	TransformMatrix(mtxY.a);
	TransformMatrix(mtxZ.a);
	Translate(-r_refdef.vieworg[0],  -r_refdef.vieworg[1],  -r_refdef.vieworg[2]);
	SetRenderOrigin(r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]);
	SetRealTime(realtime);
	//
	// set drawing parms
	//
	if (gl_cull.value)
		EnableCulling( );
	else
		DisableCulling( );

	DisableAlphaTest();
	DisableBlending();
	EnableDepth();
}

/*
================
R_RenderScene

r_refdef must be set before the first call
================
*/
void R_RenderScene (void)
{
	R_SetupFrame ();
	
	R_SetFrustum ();
	
	R_SetupGL ();
	//
	R_MarkLeaves ();	// done here so we know if we're in water
	//
	R_DrawWorld ();		// adds static entities to the list
	//
	S_ExtraUpdate ();	// don't let sound get messed up if going slow
	//
	R_DrawEntitiesOnList ();
	//
	R_RenderDlights ();
	//
	R_DrawParticles ();	

	R_UpdateLightmaps();

#ifdef GLTEST
	Test_Draw ();
#endif
}

/*
=============
R_Clear
=============
*/
void R_Clear (void)
{
	/*
	Here we just flush amy remaining draw called and set the depthfunctions instantly
	-JWA
	*/
	FlushDraw();
	if (r_mirroralpha.value != 1.0)
	{
		if (gl_clear.value)
			glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		else
			glClear (GL_DEPTH_BUFFER_BIT);
		gldepthmin = 0;
		gldepthmax = 0.5;
		glDepthFunc (GL_LEQUAL);
	}
	else if (gl_ztrick.value)
	{
		if (gl_clear.value){
			glClear (GL_COLOR_BUFFER_BIT);
		}
		trickframe++;
		if (trickframe & 1)
		{
			gldepthmin = 0;
			gldepthmax = 0.49999;
			glDepthFunc (GL_LEQUAL);
		}
		else
		{
			gldepthmin = 1;
			gldepthmax = 0.5;
			glDepthFunc (GL_GEQUAL);
		}
	}
	else
	{
		glDepthMask(1); 
		//if (gl_clear.value)
		//	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		//else
		glClear (GL_DEPTH_BUFFER_BIT);
		gldepthmin = 0;
		gldepthmax = 1;
		glDepthFunc (GL_LEQUAL);
	}

	SetDepthRange (gldepthmin, gldepthmax);
}

/*
=============
R_Mirror
=============
*/
void R_Mirror (void)
{

	float		d;
	msurface_t	*s;
	entity_t	*ent;

	if (!mirror)
		return;

	return;

	memcpy (r_base_world_matrix, r_world_matrix, sizeof(r_base_world_matrix));

	d = DotProduct (r_refdef.vieworg, mirror_plane->normal) - mirror_plane->dist;
	VectorMA (r_refdef.vieworg, -2*d, mirror_plane->normal, r_refdef.vieworg);

	d = DotProduct (vpn, mirror_plane->normal);
	VectorMA (vpn, -2*d, mirror_plane->normal, vpn);

	r_refdef.viewangles[0] = -asin (vpn[2])/M_PI*180;
	r_refdef.viewangles[1] = atan2 (vpn[1], vpn[0])/M_PI*180;
	r_refdef.viewangles[2] = -r_refdef.viewangles[2];

	ent = &cl_entities[cl.viewentity];
	if (cl_numvisedicts < MAX_VISEDICTS)
	{
		cl_visedicts[cl_numvisedicts] = ent;
		cl_numvisedicts++;
	}

	gldepthmin = 0.5;
	gldepthmax = 1;

	SetDepthRange (gldepthmin, gldepthmax);
	glDepthFunc (GL_LEQUAL);

	R_RenderScene ();
	R_DrawWaterSurfaces ();
	
	FlushDraw();

}

/*================
R_RenderView

r_refdef must be set before the first call
================*/
void R_RenderView (void)
{
	double	time1, time2;
	
	if (r_norefresh.value)
		return;

	if (!r_worldentity.model || !cl.worldmodel)
		Sys_Error ("R_RenderView: NULL worldmodel");

	if (r_speeds.value)
	{
		FlushDraw();
		time1 = Sys_FloatTime ();
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

	mirror = false;

	if (gl_finish.value)
	{
		FlushDraw();
	}
	
	R_Clear ();
	
	// render normal view
	R_RenderScene();
	R_DrawViewModel();	
	R_DrawWaterSurfaces ();	

	// render mirror view
	//R_Mirror ();

	//the flashes on top of the screen
	//R_PolyBlend ();

	if (r_speeds.value)
	{
		time2 = Sys_FloatTime ();
		Con_Printf ("%3i ms  %4i wpoly %4i epoly\n", (int)((time2-time1)*1000), c_brush_polys, c_alias_polys); 
	}
	
}
