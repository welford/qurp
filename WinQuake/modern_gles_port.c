#include "modern_gl_port.h"
#ifdef USE_GLES
#include "transforms.h"
#include <signal.h>

#ifdef _WIN32
#include <GL/glew.h>
#include <GL/glew.h>
#include <glsw.h>
#include <shader_gl.h>
#include <vector++\vector\vector.h>
#else
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include "transforms.h"
//#include "glsw.h"
#include "quake_shaders.h"
#include "shader_gles.h"
#include "vector.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define STRINGIFY_NAME(s) #s
#define STRINGIFY_VALUE(x) STRINGIFY_NAME(x)
#define HASH_DEFINE_VALUE(x) "#define "#x" "STRINGIFY_VALUE(x)
#define HASH_DEFINE(x) "#define "#x

#define MAX_DIRECTION_LIGHTS	4
#define MAX_POINT_LIGHTS		5
#define MAX_SPOT_LIGHTS			5

//we render to an internal buffer, then blit it to screen
//---------------------------------------------------
//if we had more memory this would be feasible
//maybe for a rpi2 version!
//---------------------------------------------------
#define USE_FBO 0 //see not above, off for now, never fully finished.

static unsigned int fbo_handle[2];
static unsigned int fbo_colour_texture[2];
static unsigned int fbo_depth_texture;
static unsigned int renderWidth = 0, renderHeight = 0;


typedef struct {
	int	num_attr;
	VertexAttribute* p_a_vtx_attr;
}_VAS;


//VAS_CLR
VertexAttribute vas_clr[] = {
	{0, COLOUR_LOCATION,	4, STREAM_FLOAT, 0, float_size*7,			 0,	 0,	0},
	{0, POSITION_LOCATION,	3, STREAM_FLOAT, 0, float_size*7, float_size*4,  4,	0},		
};
//VAS_CLR_TEX
VertexAttribute vas_clr_tx[] = {
	{0, COLOUR_LOCATION,	4, STREAM_FLOAT, 0, float_size*9,			 0,	0, 0},
	{0, UV_LOCATION0,		2, STREAM_FLOAT, 0, float_size*9, float_size*4,	4, 0},
	{0, POSITION_LOCATION,	3, STREAM_FLOAT, 0, float_size*9, float_size*6,	6, 0}	
};
//VAS_VTX_CLR
VertexAttribute vas_vtx_clr[] = {
	{0, COLOUR_LOCATION,	4, STREAM_FLOAT, 0, float_size*7,			 0,	0, 0},
	{0, POSITION_LOCATION,	3, STREAM_FLOAT, 0, float_size*7, float_size*4,	4, 0}
};
//VAS_VTX_CLR_TEX
VertexAttribute vas_vtx_clr_tx[] = {
	{0, COLOUR_LOCATION,	4, STREAM_FLOAT, 0, float_size*9,			 0,	0, 0},
	{0, UV_LOCATION0,		2, STREAM_FLOAT, 0, float_size*9, float_size*4,	4, 0},
	{0, POSITION_LOCATION,	3, STREAM_FLOAT, 0, float_size*9, float_size*6,	6, 0}
};
//VAS_VTX_CLR8_TEX
#if USE_HALF_FLOATS
VertexAttribute vas_vtx_clr8_tx[] = {
	{0, COLOUR_LOCATION,	4, STREAM_UCHAR, 1, float_size*7,			 0,	0, 0},
	{0, UV_LOCATION0,		2, STREAM_FLOAT, 0, float_size*7, float_size*2,	2, 0},
	{0, POSITION_LOCATION,	3, STREAM_FLOAT, 0, float_size*7, float_size*4,	4, 0}
};
#else
VertexAttribute vas_vtx_clr8_tx[] = {
	{0, COLOUR_LOCATION,	4, STREAM_UCHAR, 1, float_size*6,			 0,	0, 0},
	{0, UV_LOCATION0,		2, STREAM_FLOAT, 0, float_size*6, float_size*1,	1, 0},
	{0, POSITION_LOCATION,	3, STREAM_FLOAT, 0, float_size*6, float_size*3,	3, 0}
};
#endif


static _VAS vas[VAS_MAX] = {
	{2, vas_clr},
	{3, vas_clr_tx},	
	{2, vas_vtx_clr},
	{3, vas_vtx_clr_tx},
	{3, vas_vtx_clr8_tx},
};

typedef struct 
{
	float direction[4];		
	float diffuse[4];
}DirectionLight;

typedef struct 
{
	float position[4];
	float diffuse[4];
	float intensity;
	float half_intensity_distance;
	float linear_attenuation;
	float quadratic_attenuation;
}PointLight;

typedef struct 
{
	float position[4];		//4th 
	float direction[4];		//should be 3 plus a buffer
	float diffuse[4];		//diffuse alpha is not used right now, there as a dummy buffer for UBO alignment

	float intensity;
	float half_intensity_distance;
	float linear_attenuation;
	float quadratic_attenuation;

	float cosine_cutoff;
	float dummy[3];
}SpotLight;

typedef struct {
	Matrix44 mvp;
	Matrix44 proj; 
	Matrix44 mv;
	float normalMin;
	float normalRange;
	float shadeIndex;
	float shadeLight;
	float ambientLight;
	float realtime;
	float gamma;
	float r_origin_shade[3];
	float waterwarp;
}UBOTransforms;

typedef struct {
	int				number_of_direction_lights;
	int				number_of_point_lights;
	int				number_of_spot_lights;
	int				dummy;
	DirectionLight	direction_light[MAX_DIRECTION_LIGHTS];
	PointLight		point_light[MAX_POINT_LIGHTS];
	SpotLight		spot_light[MAX_SPOT_LIGHTS];
}UBOLights;

static const GLenum enumToGLAttribType[] = { GL_BYTE, GL_UNSIGNED_BYTE, GL_SHORT, GL_INT, QURP_FLOAT };

typedef enum{
	FRONT=0,
	BACK,
}CullFace;

typedef struct _RenderState{
	RenderMode render_mode;
	int enable_blend;
	int enable_texture;
	int enable_cull;
	int enable_depth;

	GLenum	src, dst;

	int	depth_mask;
	float depth_min, depth_max;

	CullFace  cull_mode;

	float red, green, blue, alpha;

	int lightmap_mode;

	int enable_alpha_test;
	
}RenderState;

static unsigned int current_shader_idx = 0;

typedef struct _VtxData{
	VertexAttributeState vertex_state;  
	int transform_uniform[7];

	//unsigned int vao_handle;
	unsigned int vbo_handle;

	unsigned int vbo_capacity;		//bytes, constant

	unsigned int vbo_num_floats;	//number of floats currently in the buffer
	unsigned int vbo_size;			//suze in bytes of the number of floats in buffer
	
	unsigned int n_vertices;		//number of vertices
	unsigned int has_colour;
	unsigned int has_texture;

	int normalMin, normalRange, shadeIndex, shadeLight, ambientLight, aliasRealtime, brushRealtime, warpRealtime, skyRealtime, r_origin_shade, brushWaterwarp, aliasWaterwarp, warpWaterwarp;
	int gammaBrush, gammaAlias, gammaSky, gammaWarp, gammaTexture;
	float_type *	p_pre_gl_buffer;
}VtxData;

static VtxData vtx = {	VAS_CLR,0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0, 0};
static VtxData vtx_fallback = {	VAS_CLR,0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0, 0};

typedef struct _VtxOffsets{
	int clr;
	int position;
	int uv;
	unsigned int total;
}VtxOffsets;
static VtxOffsets vtx_offsets = {0, 4, -1, 0};

int inbetween_start_end				= 0;
int transform_dirty = 0;
static int force_render_state_change = 0;
RenderState current_render_state = {
	RNDR_TRIANGLES, 
	0,0,0,0, 
	GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, 
	1, 0.0f, 1.0f, 
	BACK, 
	0,0,0,0, 
	0,
	0};
RenderState next_render_state = {
	RNDR_TRIANGLES, 
	0,0,0,0, 
	GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, 
	1, 0.0f, 1.0f, 
	BACK, 
	0,0,0,0, 
	0,
	0};
static UBOTransforms transforms;
static UBOLights lights;

static const int GRANULARITY = 16384*4*float_size; //probably too small (half floats)

static const int transform_stack_size = 10;

static int alias_vbo = -1;				//
static int alias_vert_offset = 0;			//
static const int ALIAS_BUFFER_SIZE  = (2048 * 1024 * 12);

static int brush_vbo = -1;				//
static int brush_vbo_size = -1;			//


SShaderProgram texture_shader;
SShaderProgram colour_shader;
SShaderProgram light_map_shader;
SShaderProgram alias_shader;
SShaderProgram brush_shader;
SShaderProgram warp_shader;
SShaderProgram sky_shader;

void TransferAndDraw(void);

#define EPSILON 0.0001f//complete hack
static int HasRenderStateChanged(){
	int i=0;
	if(force_render_state_change)
	{
		force_render_state_change = 0;
		return 1;
	}
	if(current_render_state.render_mode != next_render_state.render_mode)
		return 1;
	if(current_render_state.enable_blend != next_render_state.enable_blend)
		return 1;
	if(current_render_state.src != next_render_state.src)
		return 1;
	if(current_render_state.dst != next_render_state.dst)
		return 1;
	if(current_render_state.enable_texture != next_render_state.enable_texture)
		return 1;
	if(current_render_state.enable_cull != next_render_state.enable_cull)
		return 1;
	if(current_render_state.enable_depth != next_render_state.enable_depth)
		return 1;
	if(current_render_state.depth_mask != next_render_state.depth_mask)
		return 1;

	if(current_render_state.depth_min > next_render_state.depth_min + EPSILON 
		|| current_render_state.depth_min < next_render_state.depth_min - EPSILON)
		return 1;
	if(current_render_state.depth_max > next_render_state.depth_max + EPSILON 
		|| current_render_state.depth_max < next_render_state.depth_max - EPSILON)
		return 1;

	if(current_render_state.cull_mode != next_render_state.cull_mode)
		return 1;

	if(current_render_state.lightmap_mode != next_render_state.lightmap_mode)
		return 1;

	if(current_render_state.enable_alpha_test != next_render_state.enable_alpha_test)
		return 1;
	
	return 0;
}

static int SameColour(){
	if(current_render_state.red > next_render_state.red + EPSILON 
		|| current_render_state.red < next_render_state.red - EPSILON)
		return 0;
	if(current_render_state.green > next_render_state.green + EPSILON 
		|| current_render_state.green < next_render_state.green - EPSILON)
		return 0;
	if(current_render_state.blue > next_render_state.blue + EPSILON 
		|| current_render_state.blue < next_render_state.blue - EPSILON)
		return 0;
	if(current_render_state.alpha > next_render_state.alpha + EPSILON 
		|| current_render_state.alpha < next_render_state.alpha - EPSILON)
		return 0;
	return 1;
}

unsigned int GetCurrentBuffer(GLenum buffer_type){
	GLint id = 0;
	glGetIntegerv(buffer_type, &id);
	return id;
}

void BindBuffer(GLenum buffer_type, unsigned int id){	glBindBuffer(buffer_type, id);}
void BindVAO(void){	/*glBindVertexArray( vtx.vao_handle );*/ }
void UnbindVAO(void){	/*glBindVertexArray( 0 );*/ }

#define BUFFER_OFFSET(i) ((char *)0 + (i))
static void SetAttributeFormat( const VertexAttribute* pAttr, unsigned int numAttr, unsigned int v_offset)
{
	//unsigned int currentVBO = GetCurrentBuffer(GL_ARRAY_BUFFER_BINDING);
	unsigned int i;

	TransferAndDraw();

	if(vtx.n_vertices != 0){
		//printf("ermergerd");
	}

	BindVAO();

	for(i=0;i<10;i++)glDisableVertexAttribArray(i);

	for(i=0; i<numAttr; i++)
	{
		/*
		if(pAttr[i].vbo != 0){
			BindBuffer(GL_ARRAY_BUFFER, pAttr[i].vbo);
		}*/

		glEnableVertexAttribArray(pAttr[i].idx);
		
		switch(pAttr[i].type)
		{
		default:
		case STREAM_FLOAT:			
		case STREAM_UCHAR:			
		case STREAM_CHAR:
		case STREAM_SHORT:
		case STREAM_INT:
			glVertexAttribPointer(pAttr[i].idx, pAttr[i].size, enumToGLAttribType[ pAttr[i].type ], pAttr[i].normalized, pAttr[i].stride, BUFFER_OFFSET(v_offset + pAttr[i].offset));
			break;		
			//glVertexAttribIPointer(pAttr[i].idx, pAttr[i].size, enumToGLAttribType[ pAttr[i].type ], pAttr[i].stride, BUFFER_OFFSET(v_offset + pAttr[i].offset));
			//break;
		}
		//if(pAttr[i].vbo != 0)
		//	BindBuffer(GL_ARRAY_BUFFER, currentVBO);
	}
	UnbindVAO();
}

void SetVertexMode(const VertexAttributeState state){
	int i = 0;
	if(vtx.vertex_state != state){
		//update the state
		SetAttributeFormat(vas[state].p_a_vtx_attr, vas[state].num_attr, 0);
		vtx.vertex_state = state;

		//- - - - - - - - - - - - - - 
		//set the vertex offsets
		//- - - - - - - - - - - - - - 
		vtx.vertex_state = state;
		vtx_offsets.clr = -1;
		vtx_offsets.position = -1;
		vtx_offsets.uv = -1;
		vtx_offsets.total = 0;

		for(i=0;i<vas[state].num_attr;i++){
			if(vas[state].p_a_vtx_attr[i].idx ==COLOUR_LOCATION){
				vtx_offsets.clr = vas[state].p_a_vtx_attr[i].array_offset;				
			}
			if(vas[state].p_a_vtx_attr[i].idx ==POSITION_LOCATION){
				vtx_offsets.position = vas[state].p_a_vtx_attr[i].array_offset;
				//position is always last so it sets the total offset
				vtx_offsets.total = vas[state].p_a_vtx_attr[i].stride / float_size;
			}
			if(vas[state].p_a_vtx_attr[i].idx ==UV_LOCATION0){
				vtx_offsets.uv = vas[state].p_a_vtx_attr[i].array_offset;
			}
		}
	}
}


#define DIM 16
#define WIDTH DIM
#define HEIGHT DIM

unsigned int debug_texture = 0,normal_texture = 0,light_texture = {0};

static void CreateDebugTextures(){
	int black=0;
	unsigned char * pTexture = 0;
	unsigned int h, w;
	pTexture = (unsigned char*)malloc(WIDTH * HEIGHT * 3);
	for(h=0;h<HEIGHT;h++)
	{
		for(w=0;w<WIDTH;w++)
		{
			if((w > WIDTH/2 && h< HEIGHT/2) || (w < WIDTH/2 && h> HEIGHT/2))
				black = 0;
			else
				black = 1;

			pTexture[(h*WIDTH*3)+(w*3)+0] = (black)?0:255;	//R
			pTexture[(h*WIDTH*3)+(w*3)+1] = (black)?0:255;	//G
			pTexture[(h*WIDTH*3)+(w*3)+2] = (black)?0:255;	//B
		}	
	}
	
	glGenTextures(1, &debug_texture);	
	//debug_texture = 13;
	glBindTexture(GL_TEXTURE_2D, debug_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, WIDTH, HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, pTexture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBindTexture(GL_TEXTURE_2D, 0);
	free(pTexture);
}

#define SHADEDOT_QUANT 16
float	_r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "anorm_dots.h"
;

static void CreateANormTextures(){
	unsigned char * pTexture = 0;
	unsigned int h, w;
	float max=-1.0f,min=99.0f;
	float scaler = 0;
	pTexture = (unsigned char*)malloc(16 * 256);

	//get range clamps
	for(h=0;h<SHADEDOT_QUANT;h++)
	{
		for(w=0;w<256;w++)
		{
			if (_r_avertexnormal_dots[h][w] > max)
				max = _r_avertexnormal_dots[h][w];
			if (_r_avertexnormal_dots[h][w] < min)
				min = _r_avertexnormal_dots[h][w];
		}	
	}

	scaler = 255.0f / (max - min);

	transforms.normalRange = max - min;
	transforms.normalMin = min;

	//clamp between 0~255
	for(h=0;h<16;h++)
	{
		for(w=0;w<256;w++)
		{
			pTexture[(h*256)+w] = (unsigned char) ((_r_avertexnormal_dots[h][w] - min) * scaler);
		}
	}

	glGenTextures(1, &normal_texture);	
	//debug_texture = 13;
	glActiveTexture(GL_TEXTURE0 + TEX_SLOT_ANORM);
	glBindTexture(GL_TEXTURE_2D, normal_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 256, 16, 0, GL_RED, GL_UNSIGNED_BYTE, pTexture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glActiveTexture(GL_TEXTURE0 + TEX_SLOT_CLR);
	free(pTexture);
}

//creates a fake light textute
static void CreateLightTexture(){
	int black=0;
	unsigned char * pTexture = 0;
	unsigned int h, w;
	pTexture = (unsigned char*)malloc(WIDTH * HEIGHT * 3);
	for(h=0;h<HEIGHT;h++)
	{
		for(w=0;w<WIDTH;w++)
		{
			if((w > WIDTH/2 && h< HEIGHT/2) || (w < WIDTH/2 && h> HEIGHT/2))
				black = 0;
			else
				black = 1;

			pTexture[(h*WIDTH*3)+(w*3)+0] = (black)?0:255;	//R
			pTexture[(h*WIDTH*3)+(w*3)+1] = (black)?0:255;	//G
			pTexture[(h*WIDTH*3)+(w*3)+2] = (black)?0:255;	//B
		}	
	}
	
	glGenTextures(1, &light_texture);	
	glActiveTexture(GL_TEXTURE0 + TEX_SLOT_LIGHT_0);
	glBindTexture(GL_TEXTURE_2D, light_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, WIDTH, HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, pTexture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glActiveTexture(GL_TEXTURE0 + TEX_SLOT_CLR);

	free(pTexture);
}

static void checkfbo(void)
{
	switch(glCheckFramebufferStatus(GL_FRAMEBUFFER)) 
	{
	case GL_FRAMEBUFFER_COMPLETE: 
		printf("GL_FRAMEBUFFER_COMPLETE\n");
		return;
		break;
	case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
		printf("GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT\n");
		break;
	case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
		printf("GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT\n");
		break;
	case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS:
		printf("GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS\n");
		break;
	//case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
	//	printf("GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER\n");
	//	break;
	//case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
	//	printf("GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER\n");
	//	break;
	case GL_FRAMEBUFFER_UNSUPPORTED:
		printf("GL_FRAMEBUFFER_UNSUPPORTED\n");
		break;
	default:
		printf("FBO Status Error!\n");
		break;
	}
}

GLuint depthRenderbuffer;

static void SetupFBO(const int width, const int height){
#if USE_FBO
	//glGenTextures(2, fbo_colour_texture);	
	//glBindTexture(GL_TEXTURE_2D, fbo_colour_texture[0]);
	//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 512, 512, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	//
	//glBindTexture(GL_TEXTURE_2D, fbo_colour_texture[1]);
	//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 512, 512, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	//
	//glBindTexture(GL_TEXTURE_2D, fbo_colour_texture[0]);
	//
	//glGenRenderbuffers(1, &depthRenderbuffer);
	//glBindRenderbuffer(GL_RENDERBUFFER, depthRenderbuffer);
	//glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, 512, 512);
	//
	//glBindTexture(GL_TEXTURE_2D, 0);
	//
	//glGenFramebuffers(2, fbo_handle);
	//glBindFramebuffer(GL_FRAMEBUFFER, fbo_handle[0]);
	//glFramebufferTexture2D(GL_FRAMEBUFFER , GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo_colour_texture[0], 0);
	//glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRenderbuffer);
	//
	//glBindFramebuffer(GL_FRAMEBUFFER, fbo_handle[1]);
	//glFramebufferTexture2D(GL_FRAMEBUFFER , GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo_colour_texture[1], 0);
	//glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRenderbuffer);
	//
	//
	//checkfbo();	
	//
	//glBindFramebuffer(GL_FRAMEBUFFER, fbo_handle[0]);
#endif
}

static void SetupShaders(void){
	int got = 0;
	SShader vtxTxShdr, frgTxShdr;
	SShader vtxAlShdr, frgAlShdr;
	SShader vtxClrShdr, frgClrShdr;
	SShader vtxLightShdr, frgLightShdr;
	SShader vtxBrushShdr, frgBrushShdr;
	SShader frgWarpShdr;
	SShader vtxSkyShdr, frgSkyShdr;
	const char *pDVertStr[3] = { 0, 0, 0 }, *pDFragStr[3] = { 0, 0, 0 };

	//shader (TEXTURED)	
	pDVertStr[0] = header_vertex;
	pDVertStr[1] = header_shared;	
	pDVertStr[2] = txt_clr_vertex;
	CreateShader(&vtxTxShdr, VERT, pDVertStr, 3);
	
	pDFragStr[0] = header_fragment;
	pDFragStr[1] = header_shared;
	pDFragStr[2] = txt_clr_frag;

	CreateShader(&frgTxShdr, FRAG, pDFragStr, 3);

	CreateShaderProgram(&texture_shader);
	AddShaderToProgram(&texture_shader,&vtxTxShdr);
	AddShaderToProgram(&texture_shader,&frgTxShdr);
	SetAttributeLocation(&texture_shader, POSITION_LOCATION, "inVertex");
	SetAttributeLocation(&texture_shader, COLOUR_LOCATION, "inColour");
	SetAttributeLocation(&texture_shader, UV_LOCATION0, "inUV");	
	LinkShaderProgram(&texture_shader);
	Start(&texture_shader);
	SetTextureUnitByName(&texture_shader,"tex0", TEX_SLOT_CLR);
	SetTextureUnitByName(&texture_shader,"anorm", TEX_SLOT_ANORM);
	Stop();

	//shader (BRUSH)	
	pDVertStr[0] = header_vertex;
	pDVertStr[1] = header_shared;
	pDVertStr[2] = brush_clr_vertex;

	CreateShader(&vtxBrushShdr, VERT, pDVertStr, 3);
	
	pDFragStr[0] = header_fragment;
	pDFragStr[1] = header_shared;
	pDFragStr[2] = brush_clr_frag;

	CreateShader(&frgBrushShdr, FRAG, pDFragStr, 3);

	CreateShaderProgram(&brush_shader);
	AddShaderToProgram(&brush_shader,&vtxBrushShdr);
	AddShaderToProgram(&brush_shader,&frgBrushShdr);
	SetAttributeLocation(&brush_shader, POSITION_LOCATION, "inVertex");
	//SetAttributeLocation(&brush_shader, COLOUR_LOCATION, "inColour");
	SetAttributeLocation(&brush_shader, UV_LOCATION0, "inUV");	
	SetAttributeLocation(&brush_shader, UV_LOCATION1, "inUVLightmap");	
	LinkShaderProgram(&brush_shader);
	Start(&brush_shader);
	SetTextureUnitByName(&brush_shader,"tex0", TEX_SLOT_CLR);
	SetTextureUnitByName(&brush_shader, "texLightMap", TEX_SLOT_LIGHT_0);
	Stop();

	//shader (WARP)	
	pDFragStr[0] = header_fragment;
	pDFragStr[1] = header_shared;
	pDFragStr[2] = warp_clr_frag;

	CreateShader(&frgWarpShdr, FRAG, pDFragStr, 3);

	CreateShaderProgram(&warp_shader);
	AddShaderToProgram(&warp_shader, &vtxBrushShdr);
	AddShaderToProgram(&warp_shader, &frgWarpShdr);
	SetAttributeLocation(&warp_shader, POSITION_LOCATION, "inVertex");
	SetAttributeLocation(&warp_shader, UV_LOCATION0, "inUV");
	SetAttributeLocation(&warp_shader, UV_LOCATION1, "inUVLightmap");
	LinkShaderProgram(&warp_shader);
	Start(&warp_shader);
	SetTextureUnitByName(&warp_shader, "tex0", TEX_SLOT_CLR);
	Stop();

	//shader (SKY)	
	pDVertStr[0] = header_vertex;
	pDVertStr[1] = header_shared;
	pDVertStr[2] = sky_vertex;

	CreateShader(&vtxSkyShdr, VERT, pDVertStr, 3);

	pDFragStr[0] = header_fragment;
	pDFragStr[1] = header_shared;
	pDFragStr[2] = sky_frag;

	CreateShader(&frgSkyShdr, FRAG, pDFragStr, 3);

	CreateShaderProgram(&sky_shader);
	AddShaderToProgram(&sky_shader, &vtxSkyShdr);
	AddShaderToProgram(&sky_shader, &frgSkyShdr);
	SetAttributeLocation(&sky_shader, POSITION_LOCATION, "inVertex");
	SetAttributeLocation(&sky_shader, UV_LOCATION0, "inUV");
	SetAttributeLocation(&sky_shader, UV_LOCATION1, "inUVLightmap");
	LinkShaderProgram(&sky_shader);
	Start(&sky_shader);
	SetTextureUnitByName(&sky_shader, "sky", TEX_SLOT_SKY);
	SetTextureUnitByName(&sky_shader, "skyAlpha", TEX_SLOT_SKY_ALPHA);
	Stop();

	//shader (LIGHTMAP)
	pDVertStr[0] = header_vertex;
	pDVertStr[1] = header_shared;
	pDVertStr[2] = lgt_vertex;
	CreateShader(&vtxLightShdr, VERT, pDVertStr, 3);

	pDFragStr[0] = header_fragment;
	pDFragStr[1] = header_shared;
	pDFragStr[2] = lgt_frag;
	CreateShader(&frgLightShdr, FRAG, pDFragStr, 3);

	CreateShaderProgram(&light_map_shader);
	AddShaderToProgram(&light_map_shader, &vtxLightShdr);
	AddShaderToProgram(&light_map_shader, &frgLightShdr);
	SetAttributeLocation(&light_map_shader, POSITION_LOCATION, "inVertex");
	SetAttributeLocation(&light_map_shader, UV_LOCATION0, "inUV");
	SetAttributeLocation(&light_map_shader, UV_LOCATION1, "inUVLightmap");
	LinkShaderProgram(&light_map_shader);
	Start(&light_map_shader);
	SetTextureUnitByName(&light_map_shader, "texLightMap", TEX_SLOT_LIGHT_0);	
	Stop();

	//shader (COLOUR)	
	pDVertStr[0] = header_vertex;
	pDVertStr[1] = header_shared;
	pDVertStr[2] = clr_vertex;
	CreateShader(&vtxClrShdr, VERT, pDVertStr, 3);

	pDFragStr[0] = header_fragment;
	pDFragStr[1] = header_shared;
	pDFragStr[2] = clr_frag;
	CreateShader(&frgClrShdr, FRAG, pDFragStr, 3);

	CreateShaderProgram(&colour_shader);
	AddShaderToProgram(&colour_shader,&vtxClrShdr);
	AddShaderToProgram(&colour_shader,&frgClrShdr);
	SetAttributeLocation(&colour_shader, POSITION_LOCATION, "inVertex");
	SetAttributeLocation(&colour_shader, COLOUR_LOCATION, "inColour");
	SetAttributeLocation(&colour_shader, UV_LOCATION0, "inUV");	
	LinkShaderProgram(&colour_shader);	
	Start(&colour_shader);
	SetTextureUnitByName(&colour_shader, "tex0", TEX_SLOT_CLR);
	SetTextureUnitByName(&colour_shader,"anorm", TEX_SLOT_ANORM);
	Stop();

	//shader (ALIAS)	
	pDVertStr[0] = header_vertex;
	pDVertStr[1] = header_shared;
	pDVertStr[2] = alias_vertex;
	CreateShader(&vtxAlShdr, VERT, pDVertStr, 3);

	pDFragStr[0] = header_fragment;
	pDFragStr[1] = header_shared;
	pDFragStr[2] = alias_frag;
	CreateShader(&frgAlShdr, FRAG, pDFragStr, 3);

	CreateShaderProgram(&alias_shader);
	AddShaderToProgram(&alias_shader, &vtxAlShdr);
	AddShaderToProgram(&alias_shader, &frgAlShdr);
	SetAttributeLocation(&alias_shader, POSITION_LOCATION, "inVertex");
	SetAttributeLocation(&alias_shader, SHADE_LOCATION, "inShadeIndex");
	SetAttributeLocation(&alias_shader, UV_LOCATION0, "inUV");
	LinkShaderProgram(&alias_shader);
	Start(&alias_shader);
	SetTextureUnitByName(&alias_shader, "tex0", TEX_SLOT_CLR);
	SetTextureUnitByName(&alias_shader, "anorm", TEX_SLOT_ANORM);
	Stop();

	//this will delete them after we have deleted the program associated with them
	DeleteShader(&vtxTxShdr);
	DeleteShader(&frgTxShdr);	
	DeleteShader(&vtxClrShdr);
	DeleteShader(&frgClrShdr);	
	DeleteShader(&vtxLightShdr);
	DeleteShader(&frgLightShdr);
	DeleteShader(&vtxAlShdr);
	DeleteShader(&frgAlShdr);
}

static void SetupUBOs(){
	current_shader_idx = 0;
	//probably sort this out! a bit random
	vtx.transform_uniform[0] = glGetUniformLocation(texture_shader.handle, "trans.mvp");
	vtx.transform_uniform[1] = glGetUniformLocation(colour_shader.handle, "trans.mvp");
	vtx.transform_uniform[2] = glGetUniformLocation(light_map_shader.handle, "trans.mvp");
	vtx.transform_uniform[3] = glGetUniformLocation(alias_shader.handle, "trans.mvp");
	vtx.transform_uniform[4] = glGetUniformLocation(brush_shader.handle, "trans.mvp");
	vtx.transform_uniform[5] = glGetUniformLocation(warp_shader.handle, "trans.mvp");
	vtx.transform_uniform[5] = glGetUniformLocation(sky_shader.handle, "trans.mvp");

	vtx.normalMin = glGetUniformLocation(alias_shader.handle, "trans.normalMin");
	vtx.normalRange = glGetUniformLocation(alias_shader.handle, "trans.normalRange");
	vtx.shadeIndex = glGetUniformLocation(alias_shader.handle, "trans.shadeIndex");
	vtx.shadeLight = glGetUniformLocation(alias_shader.handle, "trans.shadeLight");
	vtx.ambientLight = glGetUniformLocation(alias_shader.handle, "trans.ambientLight");

	vtx.brushRealtime = glGetUniformLocation(brush_shader.handle, "trans.realtime");
	vtx.aliasRealtime = glGetUniformLocation(alias_shader.handle, "trans.realtime");
	vtx.warpRealtime = glGetUniformLocation(warp_shader.handle, "trans.realtime");
	vtx.skyRealtime = glGetUniformLocation(sky_shader.handle, "trans.realtime");
	vtx.r_origin_shade = glGetUniformLocation(sky_shader.handle, "trans.r_origin_shade");

	vtx.brushWaterwarp = glGetUniformLocation(brush_shader.handle, "trans.waterwarp");
	vtx.aliasWaterwarp = glGetUniformLocation(alias_shader.handle, "trans.waterwarp");
	vtx.warpWaterwarp = glGetUniformLocation(warp_shader.handle, "trans.waterwarp");

	vtx.gammaBrush = glGetUniformLocation(brush_shader.handle, "trans.gamma");
	vtx.gammaAlias = glGetUniformLocation(alias_shader.handle, "trans.gamma");
	vtx.gammaSky = glGetUniformLocation(sky_shader.handle, "trans.gamma");
	vtx.gammaWarp = glGetUniformLocation(warp_shader.handle, "trans.gamma");
	vtx.gammaTexture = glGetUniformLocation(texture_shader.handle, "trans.gamma");
}

void UpdateTransformUBOs(){	
	float *tmp = StackGetTop();
	int i=0;
	for(i=0;i<16;i++){
		transforms.mv.a[i] = tmp[i];
	}
	matMultiply44(&transforms.mv, &transforms.proj, &transforms.mvp);
	SetMtx44ByLocation(vtx.transform_uniform[current_shader_idx], transforms.mvp.a);
	transform_dirty = 0;
}

//w and h are not used int he RPI version, we render to fullscreen only
void StartupModernGLPatch(int w, int h){
	float * tmp;
	glActiveTexture(GL_TEXTURE0 + TEX_SLOT_CLR);

	//setup a global VAO/VBO for rendering everything
	//hopefully this will be good enough
	//if(vtx.vao_handle == 0){
	if( 1 ){
		glGenBuffers(1, &vtx.vbo_handle);
		glBindBuffer(GL_ARRAY_BUFFER, vtx.vbo_handle);
		glBufferData(GL_ARRAY_BUFFER, GRANULARITY, 0, GL_DYNAMIC_DRAW);

		tmp = (float*)glMapBufferOES(GL_ARRAY_BUFFER, GL_WRITE_ONLY_OES);
		glUnmapBufferOES(GL_ARRAY_BUFFER);

		vtx.vbo_capacity = GRANULARITY + 4;
		vtx.vbo_size = 0;
	}

	// - - - - - - - - - - - - - - - -
	//setup the transforms
	// - - - - - - - - - - - - - - - -	
	matSetIdentity44(&transforms.mv);
	matSetIdentity44(&transforms.proj);

	// - - - - - - - - - - - - - - - -
	//setup the shaders library
	// - - - - - - - - - - - - - - - -
	SetupShaders();	
	SetupUBOs();

	// - - - - - - - - - - - - - - - -
	//Allocate float data 
	// - - - - - - - - - - - - - - - -
	vtx.p_pre_gl_buffer = (float_type*)malloc(vtx.vbo_capacity);
	//vtx.p_buffer_loc = vtx.p_pre_gl_buffer;

	// - - - - - - - - - - - - - - - -
	//
	// - - - - - - - - - - - - - - - -
	InitialiseStack(transform_stack_size);

	SetRenderSize(w, h);

	//CreateDebugTextures();
	CreateANormTextures();
	//CreateLightTexture();
}

static unsigned int num_draw_calls = 0;
static unsigned int size_data_transfer = 0;
void ResetDrawCallCounter(){
	num_draw_calls = 0;
	size_data_transfer = 0;
}

unsigned int GetDrawCallCounter(){
	return num_draw_calls;
}

unsigned int GetDrawCallSize(){
	return size_data_transfer;
}

void EnableBlending(){
	if(inbetween_start_end || next_render_state.enable_blend == 1)
		return;
	next_render_state.enable_blend = 1;
}

void DisableBlending(){
	if(inbetween_start_end || next_render_state.enable_blend == 0)
		return;
	next_render_state.enable_blend = 0;
}

void EnableDepth(){
	if(inbetween_start_end || next_render_state.enable_depth == 1)
		return;
	next_render_state.enable_depth = 1;
}

void DisableDepth(){
	if(inbetween_start_end || next_render_state.enable_depth == 0)
		return;	
	next_render_state.enable_depth = 0;
}

void EnableTexture(){
	if(inbetween_start_end || next_render_state.enable_texture == 1)
		return;
	next_render_state.enable_texture = 1;
}

void DisableTexture(){
	if(inbetween_start_end || next_render_state.enable_texture == 0)
		return;	
	next_render_state.enable_texture = 0;
}

void EnableCulling(){
	if(inbetween_start_end || next_render_state.enable_cull == 1)
		return;
	next_render_state.enable_cull = 1;
}

void DisableCulling(){
	if(inbetween_start_end || next_render_state.enable_cull == 0)
		return;	
	next_render_state.enable_cull = 0;
}

void SetBlendFunc(const GLenum s, const GLenum d){
	if(inbetween_start_end || (next_render_state.src == s && next_render_state.dst == d))
		return;
	next_render_state.src= s;
	next_render_state.dst= d;	
}

void SetDepthMask(const int dm){
	if(inbetween_start_end || next_render_state.depth_mask == dm)
		return;
	next_render_state.depth_mask = dm;	
}

void SetDepthRange(const float min, const float max){
	if(inbetween_start_end)
		return;
	next_render_state.depth_min = min;
	next_render_state.depth_max = max;
}

void SetLightmapMode(const int active){
	if(inbetween_start_end)
		return;
	next_render_state.lightmap_mode = active;

}

void EnableAlphaTest(){
	if(inbetween_start_end)
		return;
	next_render_state.enable_alpha_test = 1;
}

void DisableAlphaTest(){
	if(inbetween_start_end)
		return;
	next_render_state.enable_alpha_test = 0;
}

void CullFront(void){
	if (inbetween_start_end || current_render_state.cull_mode == FRONT)
		return;

	next_render_state.cull_mode = FRONT;
}

void CullBack(void){
	if (inbetween_start_end || current_render_state.cull_mode == BACK)
		return;

	next_render_state.cull_mode = BACK;
}

void BeginDrawing(const RenderMode rm){
	next_render_state.render_mode = rm;
	//reset some stuff
	if(HasRenderStateChanged() || transform_dirty == 1){
		TransferAndDraw();
	}
	inbetween_start_end = 1;
}

void EndDrawing(){
	if(next_render_state.render_mode==RNDR_TRIANGLE_FAN || next_render_state.render_mode==RNDR_TRIANGLE_STRIP){
		TransferAndDraw();
	}

	inbetween_start_end = 0;
	vtx_fallback = vtx;
}

static int PreAddVertexCheck(const VtxDataType type){
	if(type == VTX_POSITION){
		int n_verts = vtx.n_vertices+1;
		//set the next color, uv etc to the default values
		if(vtx_offsets.clr >= 0){
			vtx.p_pre_gl_buffer[(n_verts * vtx_offsets.total) + vtx_offsets.clr + 0] = current_render_state.red;
			vtx.p_pre_gl_buffer[(n_verts * vtx_offsets.total) + vtx_offsets.clr + 1] = current_render_state.green;
			vtx.p_pre_gl_buffer[(n_verts * vtx_offsets.total) + vtx_offsets.clr + 2] = current_render_state.blue;
			vtx.p_pre_gl_buffer[(n_verts * vtx_offsets.total) + vtx_offsets.clr + 3] = current_render_state.alpha;
		}
		//don't set the others
		if(vtx_offsets.position >= 0){
			//vtx.p_pre_gl_buffer[(n_verts * vtx_offsets.total) + vtx_offsets.position + 0] = 0;
			//vtx.p_pre_gl_buffer[(n_verts * vtx_offsets.total) + vtx_offsets.position + 1] = 0;
			//vtx.p_pre_gl_buffer[(n_verts * vtx_offsets.total) + vtx_offsets.position + 2] = 0;		
		}
		if(vtx_offsets.uv >= 0){
			//vtx.p_pre_gl_buffer[(n_verts * vtx_offsets.total) + vtx_offsets.uv + 0] = 0;
			//vtx.p_pre_gl_buffer[(n_verts * vtx_offsets.total) + vtx_offsets.uv + 1] = 0;
		}		
	}	
	return vtx.n_vertices;
}
static void PostAddVertexCheck(const VtxDataType type){
	if(type == VTX_POSITION){
		vtx.n_vertices++;
		vtx.vbo_num_floats = vtx_offsets.total * vtx.n_vertices;
		vtx.vbo_size = vtx.vbo_num_floats * float_size;
	}
	if(type == VTX_TEXTURE){
		vtx.has_texture = 1;
	}
	if(type == VTX_COLOUR){
		vtx.has_colour = 1;
	}
}

//buffer overflow fix
//render to the last safe point and then 
//copy the new data back
static void TransferAndDrawFromLastSafePoint(){	
	if(vtx.n_vertices != vtx_fallback.n_vertices){	
		//printf("drawing from last safe point\n");
		VtxData tmp = vtx;
		vtx = vtx_fallback;
		TransferAndDraw();

		vtx.n_vertices = tmp.n_vertices - vtx_fallback.n_vertices;
		vtx.vbo_num_floats = tmp.vbo_num_floats - vtx_fallback.vbo_num_floats;
		vtx.vbo_size = tmp.vbo_size - vtx_fallback.vbo_size;
		vtx.has_colour = vtx_fallback.has_colour;

		//copy from the end of the buffer to the start
		//there should be no worry of overlaps
		//memcpy(vtx.p_pre_gl_buffer, vtx_fallback.p_buffer_loc, vtx.vbo_size);
		memcpy(vtx.p_pre_gl_buffer, 
			vtx_fallback.p_pre_gl_buffer+ vtx_fallback.vbo_num_floats
			, vtx.vbo_size);		
	}
	else{
		//printf("drawing from last safe point\n");
		TransferAndDraw();
	}
}
void AddVertex2D(const VtxDataType type, const float x, const float y){
	int vtx_idx = 0;//
	vtx_idx = PreAddVertexCheck(type);
	if(type == VTX_COLOUR)
		return;

	if(vtx.vbo_size + float_size * 2 >= vtx.vbo_capacity){
		TransferAndDrawFromLastSafePoint();
	}
	
	if(type == VTX_POSITION && vtx_offsets.position >= 0){
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.position + 0] = x;
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.position + 1] = y;
	}
	if(type == VTX_COLOUR && vtx_offsets.clr >= 0){
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.clr + 0] = x;
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.clr + 1] = y;
	}
	if(type == VTX_TEXTURE && vtx_offsets.uv >= 0){
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.uv + 0] = x;
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.uv + 1] = y;
	}
	//vtx.vbo_num_floats = (vtx.p_buffer_loc - vtx.p_pre_gl_buffer);
		
	PostAddVertexCheck(type);
}

void AddVertex3D(const VtxDataType type, const float x, const float y, const float z){
	int vtx_idx = 0;//
	vtx_idx = PreAddVertexCheck(type);
	if(type == VTX_COLOUR)
		return;
	if(vtx.vbo_size + float_size * 3 >= vtx.vbo_capacity){
		TransferAndDrawFromLastSafePoint();
	}

	if(type == VTX_POSITION && vtx_offsets.position >= 0){
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.position + 0] = x;
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.position + 1] = y;
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.position + 2] = z;
	}
	if(type == VTX_COLOUR && vtx_offsets.clr >= 0){
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.clr + 0] = x;
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.clr + 1] = y;
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.clr + 2] = z;
	}
	if(type == VTX_TEXTURE && vtx_offsets.uv >= 0){
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.uv + 0] = x;
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.uv + 1] = y;
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.uv + 2] = z;
	}

	PostAddVertexCheck(type);
}

static int IsPerVertexColour(){
	if(vtx.vertex_state == VAS_VTX_CLR 
		|| vtx.vertex_state == VAS_VTX_CLR_TEX
		|| vtx.vertex_state == VAS_VTX_CLR8_TEX
		){
		return 1;
	}
	return 0;
}

void AddVertex4D(const VtxDataType type, const float x, const float y, const float z, const float w){
	unsigned int same_colour = 0;
	int vtx_idx = 0;//
	int per_vertex_colour = IsPerVertexColour();
	vtx_idx = PreAddVertexCheck(type);

	if(vtx.vbo_size + float_size * 4 >= vtx.vbo_capacity){
		TransferAndDrawFromLastSafePoint();
	}

	if(type == VTX_POSITION && vtx_offsets.position >= 0){
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.position + 0] = x;
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.position + 1] = y;
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.position + 2] = z;
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.position + 3] = w;
	}
	if(type == VTX_COLOUR && vtx_offsets.clr >= 0){
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.clr + 0] = x;
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.clr + 1] = y;
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.clr + 2] = z;
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.clr + 3] = w;

		current_render_state.red	= x;
		current_render_state.green	= y;
		current_render_state.blue	= z;
		current_render_state.alpha	= w;			
	}
	if(type == VTX_TEXTURE && vtx_offsets.uv >= 0){
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.uv + 0] = x;
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.uv + 1] = y;
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.uv + 2] = z;
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.uv + 3] = w;
	}

	PostAddVertexCheck(type);
}


void AddVertex4Db(const VtxDataType type, const unsigned char r, const unsigned char g, const unsigned char b, const unsigned char a){
	union
	{
	  unsigned char clr[4];
	  #if USE_HALF_FLOATS
	  float_type f[2];
	  #else
	  float f;
	  #endif
	} uClr;

	int vtx_idx = 0;//
	unsigned int same_colour = 0;
	int per_vertex_colour = IsPerVertexColour();
	vtx_idx = PreAddVertexCheck(type);

	if(vtx.vbo_size + float_size * 4 >= vtx.vbo_capacity){
		TransferAndDrawFromLastSafePoint();
	}

	uClr.clr[0] = r;
	uClr.clr[1] = g;
	uClr.clr[2] = b;
	uClr.clr[3] = a;

	#if USE_HALF_FLOATS
	if(type == VTX_POSITION && vtx_offsets.position >= 0){
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.position + 0] = uClr.f[0];
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.position + 1] = uClr.f[1];
	}
	if(type == VTX_COLOUR && vtx_offsets.clr >= 0){
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.clr + 0] = uClr.f[0];
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.clr + 1] = uClr.f[1];
	}
	if(type == VTX_TEXTURE && vtx_offsets.uv >= 0){
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.uv + 0] = uClr.f[0];
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.uv + 1] = uClr.f[1];
	}
	#else
	if(type == VTX_POSITION && vtx_offsets.position >= 0){
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.position + 0] = uClr.f;
	}
	if(type == VTX_COLOUR && vtx_offsets.clr >= 0){
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.clr + 0] = uClr.f;
	}
	if(type == VTX_TEXTURE && vtx_offsets.uv >= 0){
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.uv + 0] = uClr.f;
	}
	#endif

	PostAddVertexCheck(type);
}

static void SetGLRenderState(void){
	if(current_render_state.enable_blend){
		glEnable(GL_BLEND);
		glBlendFunc( current_render_state.src, current_render_state.dst );
	}
	else
		glDisable(GL_BLEND);

	if(current_render_state.enable_depth)
		glEnable (GL_DEPTH_TEST);
	else
		glDisable (GL_DEPTH_TEST);

	glDepthMask(current_render_state.depth_mask);
	
	glDepthRangef(current_render_state.depth_min, current_render_state.depth_max);

	if(current_render_state.enable_cull)
		glEnable ( GL_CULL_FACE );
	else
		glDisable ( GL_CULL_FACE);

	if(current_render_state.cull_mode == FRONT)
		glCullFace(GL_FRONT);
	else
		glCullFace(GL_BACK);

	if(current_render_state.lightmap_mode){
		Start(&light_map_shader);		
		if(current_shader_idx!= 2)
			transform_dirty = 1;
		current_shader_idx = 2;
	}
	else if(!vtx.has_texture){		
		Start(&colour_shader);
		if(current_shader_idx!= 1)
			transform_dirty = 1;
		current_shader_idx = 1;
	}
	else{
		Start(&texture_shader);
		SetFloatByLocation(vtx.gammaTexture, &transforms.gamma);
		if(current_shader_idx!= 0)
			transform_dirty = 1;
		current_shader_idx = 0;
	}
	/*
	if(current_render_state.enable_texture)
		glEnable (GL_TEXTURE_2D);
	else
		glDisable (GL_TEXTURE_2D); 
		*/
}

static void ResetVtxData(void){
	vtx.has_texture = 0;
	vtx.n_vertices = 0;
	vtx.has_colour = 0;
	//vtx.p_buffer_loc = vtx.p_pre_gl_buffer+4;
	//vtx.p_buffer_loc = vtx.p_pre_gl_buffer;
	vtx.vbo_size = 0;	
}

static GLenum render_modes[] = {GL_POINTS, GL_TRIANGLES, GL_TRIANGLE_STRIP, GL_TRIANGLE_FAN};
void TransferAndDraw(void){

	// - - - - - - - - - - - - - - - - - -
	// check that we are in good condition to 
	// render
	if (//vtx.p_buffer_loc - vtx.p_pre_gl_buffer <= 4 || 
		(current_render_state.render_mode == RNDR_TRIANGLES && vtx.n_vertices%3 != 0) || 
		vtx.n_vertices == 0 		 
		){
		ResetVtxData();
		if(HasRenderStateChanged()){
			current_render_state = next_render_state;
		}
		return;
	}
	// - - - - - - - - - - - - - - - - - -
	// set state
	SetGLRenderState();

	// - - - - - - - - - - - - - - - - - -
	//Send Data
	if(transform_dirty)
		UpdateTransformUBOs();

	glBufferSubData(GL_ARRAY_BUFFER, 0, vtx.vbo_size, vtx.p_pre_gl_buffer);
	
	size_data_transfer += vtx.vbo_size;

	// - - - - - - - - - - - - - - - - - -
	//draw it 

	BindVAO();
	glDrawArrays(render_modes[current_render_state.render_mode], 0, vtx.n_vertices);
	glBufferData(GL_ARRAY_BUFFER, GRANULARITY, 0, GL_DYNAMIC_DRAW);

	// - - - - - - - - - - - - - - - - - -
	//reset temp draw stuff
	ResetVtxData();

	// - - - - - - - - - - - - - - - - - -
	//if the state has changed not is the time to update
	if(HasRenderStateChanged()){
		current_render_state = next_render_state;
	}	

	num_draw_calls++;
}

void FlushDraw(void){
	TransferAndDraw();
}

void SetRenderSize(const unsigned int w, const unsigned int h)
{
	renderWidth = w;
	renderHeight = h;
	//update FBO!
	SetupFBO(renderWidth, renderHeight);
}

void SetViewport(const unsigned int x, const unsigned int y, const unsigned int w, const unsigned int h){
	TransferAndDraw();
	glViewport(x,y,w,h);
}

void SetOrthoMatrix(const float l, const float r, const float b, const float t, const float n, const float f){
	float mat[16] = {
		//col 1
		2/(r-l), 0, 0, 0,
		//col 2
		0, 	2/(t-b), 0, 0,
		//col 3
		0, 0, -2/(f-n), 0,
		//col 4
		-(r+l)/(r-l), -(t+b)/(t-b), -(f+n)/(f-n), 1
	};	
	memcpy(transforms.proj.a, mat, sizeof(float)*16);	
}

void SetProjectionMatrix(const float l, const float r, const float b, const float t, const float n, const float f){
	float mat[16] = {
		//col 1
		(2*n)/(r-l), 0, 0, 0,
		//col 2
		0, (2*n)/(t-b), 0, 0,
		//col 3
		(r+l)/(r-l), (t+b)/(t-b), -(f+n)/(f-n), -1,
		//col 4
		0, 0, -(2*f*n)/(f-n), 0
	};
	memcpy(transforms.proj.a, mat, sizeof(float)*16);
}

void Push(void){
	TransferAndDraw();
	transform_dirty = 1;
	StackPush();
}

void Pop(void){
	TransferAndDraw();
	transform_dirty = 1;
	StackPop();
}

void Identity(void){
	TransferAndDraw();
	transform_dirty = 1;
	StackIdentity();
}

void Translate(const float x, const float y, const float z){
	TransferAndDraw();
	transform_dirty = 1;
	StackTranslate(x, y, z);
}

void Scale(const float x, const float y, const float z){
	TransferAndDraw();
	transform_dirty = 1;
	StackScale(x, y, z);
}

void SetMatrix(const float mtx[16]){
	TransferAndDraw();
	transform_dirty = 1;
	StackSetMatrix(mtx);
}

void TransformMatrix(const float mtx[16]){
	TransferAndDraw();
	transform_dirty = 1;
	StackTransformMatrix(mtx);
}

void BlitFBO(const int w, const inth )
{
	static int fbo_idx = 0;
#if USE_FBO
	//Matrix44 mtxX,mtxZ;
	//
	//glBindFramebuffer(GL_FRAMEBUFFER, 0);
	//
	//GL_DisableMultitexture();
	//DisableAlphaTest();
	//EnableBlending();
	//DisableDepth();
	//
	//Identity();
	//matRotateX44(DEG_TO_RAD(-90), &mtxX);
	//matRotateZ44(DEG_TO_RAD(90), &mtxZ);
	//TransformMatrix(mtxX.a);
	//TransformMatrix(mtxZ.a);	
	//
	//GL_Bind (fbo_colour_texture[fbo_idx]);
	//
	//SetVertexMode(VAS_CLR_TEX);
	//
	//BeginDrawing(RNDR_TRIANGLE_STRIP);
	//AddVertex4D (VTX_COLOUR, 1, 1, 1, 1);
	//int width = -(vid.width), height = vid.height;
	//AddVertex2D (VTX_TEXTURE, 0.0f, 0.0f);
	//AddVertex3D (VTX_POSITION, 10, 0,		height);
	//
	////AddVertex2D (VTX_TEXTURE, 0.16666666666666666666666666666667f,  0.0f);
	//AddVertex2D (VTX_TEXTURE, 0.625f,  0.0f);
	//AddVertex3D (VTX_POSITION, 10, width,	height);	
	//
	////AddVertex2D (VTX_TEXTURE, 0.0f,  0.22222222222222222222222222222222f);
	//AddVertex2D (VTX_TEXTURE, 0.0f,  0.46875);
	//AddVertex3D (VTX_POSITION, 10, 0,		0);
	//
	////AddVertex2D (VTX_TEXTURE, 0.16666666666666666666666666666667f,  0.22222222222222222222222222222222f);
	//AddVertex2D (VTX_TEXTURE, 0.625f,  0.46875);
	//AddVertex3D (VTX_POSITION, 10, width,	0);
	//
	//EndDrawing();
	//FlushDraw();
	//GL_Bind (0);
	//
	//EnableDepth();
	//fbo_idx = fbo_idx+1;
	//if(fbo_idx > 1)fbo_idx = 0;
	//glBindFramebuffer(GL_FRAMEBUFFER, fbo_handle[fbo_idx]);

#endif
}

void CreatAliasBuffers(int* pVboOffset, int numVerts, void * pData)
{
	if(sizeof(glAliasData)*alias_vert_offset + sizeof(glAliasData)*numVerts > ALIAS_BUFFER_SIZE)
	{
		printf("out of alias memory....this will be an issue just now but not in the future\n");
	}
	printf("Memory Used %d\n", sizeof(glAliasData)*alias_vert_offset + sizeof(glAliasData)*numVerts);

	if(alias_vbo < 0)
	{
		char* tmp = (char*)malloc(ALIAS_BUFFER_SIZE);
		memset(tmp,0xCD,ALIAS_BUFFER_SIZE);
		glGenBuffers(1, &alias_vbo);

		//send data to GPU
		glBindBuffer(GL_ARRAY_BUFFER, alias_vbo);
		glBufferData(GL_ARRAY_BUFFER, ALIAS_BUFFER_SIZE, tmp, GL_STATIC_DRAW);

		alias_vert_offset = 0;

		free(tmp);
	}

	//send data to GPU
	glBindBuffer(GL_ARRAY_BUFFER, alias_vbo);
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(glAliasData)*alias_vert_offset, sizeof(glAliasData)*numVerts, pData);

	*pVboOffset = alias_vert_offset;
	//update offset
	alias_vert_offset += numVerts;
	//revert binding
	glBindBuffer(GL_ARRAY_BUFFER, vtx.vbo_handle);
	//*/
}

int temp = 0;
void StartAliasBatch(float depthmin, float depthmax)
{
	int i = 0;
	FlushDraw();
	//
	glDepthMask(1);
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	glEnable(GL_TEXTURE_2D);
	//
	temp = current_shader_idx;
	current_shader_idx = 3;
	//
	float waterwarp = transforms.waterwarp * 0.2;
	Start(&alias_shader);
	SetFloatByLocation(vtx.aliasRealtime, &transforms.realtime);
	SetFloatByLocation(vtx.aliasWaterwarp, &waterwarp);
	SetFloatByLocation(vtx.gammaAlias, &transforms.gamma);
	glDepthRangef(depthmin, depthmax);

	//
	glBindBuffer(GL_ARRAY_BUFFER, alias_vbo);
	//
	for (i = 0; i<10; i++)glDisableVertexAttribArray(i);
	//
	glEnableVertexAttribArray(UV_LOCATION0);
	glVertexAttribPointer(UV_LOCATION0, 2, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(glAliasData), (char *)0);
	
	glEnableVertexAttribArray(POSITION_LOCATION);
	glVertexAttribPointer(POSITION_LOCATION, 3, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(glAliasData), (char *)(0 + sizeof(char) * 2));
	
	glEnableVertexAttribArray(SHADE_LOCATION);
	glVertexAttribPointer(SHADE_LOCATION, 1, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(glAliasData), (char *)(0 + sizeof(char) * 2 + sizeof(char) * 3));
}

void RenderAlias(const int vbo_offset,  const int posenum, const int numTris, int shadeDotIndex, float shadeLight, float ambientLight)
{
	transforms.shadeIndex = ((float)shadeDotIndex / 15.0f);
	transforms.shadeLight = shadeLight;
	transforms.ambientLight = ambientLight;

	SetFloatByLocation(vtx.normalMin,	&transforms.normalMin);
	SetFloatByLocation(vtx.normalRange, &transforms.normalRange);
	SetFloatByLocation(vtx.shadeIndex,	&transforms.shadeIndex);
	SetFloatByLocation(vtx.shadeLight, &transforms.shadeLight);
	SetFloatByLocation(vtx.ambientLight, &transforms.ambientLight);

	if (transform_dirty)
		UpdateTransformUBOs();
	
	glDrawArrays(GL_TRIANGLES, vbo_offset + ((numTris*3)*posenum), (numTris*3));
}

void EndAliasBatch()
{
	glBindBuffer(GL_ARRAY_BUFFER, vtx.vbo_handle);
	vtx.vertex_state = -1;
	current_shader_idx = temp;
	transform_dirty = 1;
	SetGLRenderState();
}

void CreateBrushBuffers(int numVerts)
{
	unsigned int currentVBO = GetCurrentBuffer(GL_ARRAY_BUFFER_BINDING);

	if(brush_vbo >= 0)
	{
		glDeleteBuffers(1, &brush_vbo);
	}

	glGenBuffers(1, &brush_vbo);

	//send data to GPU
	glBindBuffer(GL_ARRAY_BUFFER, brush_vbo);
	glBufferData(GL_ARRAY_BUFFER, numVerts * sizeof(glBrushData), 0, GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, currentVBO);
}

void AddBrushData(int vertexOffset, int numVerts, void * pData)
{
	unsigned int currentVBO = GetCurrentBuffer(GL_ARRAY_BUFFER_BINDING);
	glBindBuffer(GL_ARRAY_BUFFER, brush_vbo);
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(glBrushData)*vertexOffset, sizeof(glBrushData)*numVerts, pData);
	glBindBuffer(GL_ARRAY_BUFFER, currentVBO);
}

extern int lightmap_active_index;
void StartBrushBatch(float depthmin, float depthmax)
{
	int i = 0;
	static int loc = -1;
	force_render_state_change = 1;
	FlushDraw();

	glDepthMask(1);
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	glEnable(GL_TEXTURE_2D);
	glDepthRangef(depthmin, depthmax);

	temp = current_shader_idx;
	current_shader_idx = 4;

	float waterwarp = transforms.waterwarp * 3.0f;
	Start(&brush_shader);
	SetFloatByLocation(vtx.brushRealtime, &transforms.realtime);
	SetFloatByLocation(vtx.brushWaterwarp, &waterwarp);
	SetFloatByLocation(vtx.gammaBrush, &transforms.gamma);
	UpdateTransformUBOs();

	if (loc == -1){ loc = glGetUniformLocation(brush_shader.handle, "texLightMap"); }

	if (lightmap_active_index == 0)
		glUniform1i(loc, TEX_SLOT_LIGHT_2);
	else if (lightmap_active_index == 1)
		glUniform1i(loc, TEX_SLOT_LIGHT_3);
	else if (lightmap_active_index == 2)
		glUniform1i(loc, TEX_SLOT_LIGHT_0);
	else
		glUniform1i(loc, TEX_SLOT_LIGHT_1);

	glBindBuffer(GL_ARRAY_BUFFER, brush_vbo);

	for (i = 0; i<10; i++)glDisableVertexAttribArray(i);

	//describe data for when we draw
	glEnableVertexAttribArray(POSITION_LOCATION);	
	glVertexAttribPointer(POSITION_LOCATION,	3,	QURP_FLOAT, GL_FALSE, sizeof(glBrushData),	(char *)(0));

	glEnableVertexAttribArray(UV_LOCATION0);
	glVertexAttribPointer(UV_LOCATION0,			2,	QURP_FLOAT, GL_FALSE, sizeof(glBrushData),	(char *)(0 + sizeof(float_type)*3));

	glEnableVertexAttribArray(UV_LOCATION1);
	glVertexAttribPointer(UV_LOCATION1,			2,	QURP_FLOAT, GL_FALSE, sizeof(glBrushData),	(char *)(0 + sizeof(float_type)*5));
}

void SetupWarpBatch(){
	float waterwarp = transforms.waterwarp * 3.0f;
	Start(&warp_shader);
	SetFloatByLocation(vtx.warpRealtime, &transforms.realtime);
	SetFloatByLocation(vtx.gammaWarp, &transforms.gamma);
	SetFloatByLocation(vtx.warpWaterwarp, &waterwarp);
	UpdateTransformUBOs();
}

void SetupSkyBatch(){
	Start(&sky_shader);
	SetFloatByLocation(vtx.skyRealtime, &transforms.realtime);
	SetFloatByLocation(vtx.gammaSky, &transforms.gamma);
	SetVec3ByLocation(vtx.r_origin_shade, transforms.r_origin_shade);
	UpdateTransformUBOs();
}

void SetupColourPass()
{
	glDisable(GL_BLEND);
	glDepthMask(1);
	Start(&brush_shader);
}

void SetRenderOrigin(float x, float y, float z)
{
	transforms.r_origin_shade[0] = x;
	transforms.r_origin_shade[1] = y;
	transforms.r_origin_shade[2] = z;
}

void SetRealTime(float time)
{
	transforms.realtime = time;
}

void SetWaterWarp(float water)
{
	transforms.waterwarp = water;
}

void SetGamma(float gamma)
{
	transforms.gamma = gamma;
}

void SetupLightMapPass()
{
	current_shader_idx = 2;
	force_render_state_change = 1;
	glEnable(GL_BLEND);

	glBlendFunc( GL_ZERO, GL_ONE_MINUS_SRC_COLOR );
	glDepthMask(0);
	Start(&light_map_shader);
	UpdateTransformUBOs();
}

void RenderBrushData(int vertexOffset, int numTris)
{
	glDrawArrays(GL_TRIANGLES, vertexOffset, (numTris*3));
}

void RenderBrushDataElements(unsigned short *pIndices, int numElements)
{
	glDrawElements(GL_TRIANGLES, numElements, GL_UNSIGNED_SHORT, pIndices);
}

void EndBrushBatch()
{
	int i;
	force_render_state_change = 1;
	glDisable(GL_BLEND);
	glDepthMask(1);
	glEnable(GL_DEPTH_TEST);
	SetLightmapMode(0);
	current_shader_idx = temp;

	glBindBuffer(GL_ARRAY_BUFFER, vtx.vbo_handle);
	vtx.vertex_state = -1;
	transform_dirty = 1;
	for (i = 0; i<10; i++)glDisableVertexAttribArray(i);
}

extern void GL_DestroyLightmaps(void);
void ShutdownModernGLPatch(){
	if(1){//vtx.vao_handle){
		//glDeleteVertexArrays(1, &vtx.vao_handle);		
		glDeleteBuffers(1,&vtx.vbo_handle);		
	}
	if(vtx.p_pre_gl_buffer){
		free(vtx.p_pre_gl_buffer);
		vtx.p_pre_gl_buffer = 0;
	}

	glDeleteBuffers(1, &alias_vbo);
	glDeleteBuffers(1, &brush_vbo);

	glDeleteTextures(1, &normal_texture);	

	DeleteShaderProgram(&texture_shader);
	DeleteShaderProgram(&colour_shader);
	DeleteShaderProgram(&light_map_shader);
	DeleteShaderProgram(&alias_shader);
	DeleteShaderProgram(&brush_shader);
	DeleteShaderProgram(&warp_shader);
	DeleteShaderProgram(&sky_shader);
	DestroyStack();

	GL_DestroyLightmaps();
}

#endif
