#include "modern_gl_port.h"

#ifdef GLQUAKE

#include "transforms.h"
#include <GL/glew.h>
#include <GL/glew.h>
#include "glsw.h"
#include "shader_gl.h"
#include "vector.h"

#include <string.h>
#include <stdlib.h>

#define STRINGIFY_NAME(s) #s
#define STRINGIFY_VALUE(x) STRINGIFY_NAME(x)
#define HASH_DEFINE_VALUE(x) "#define "#x" "STRINGIFY_VALUE(x)
#define HASH_DEFINE(x) "#define "#x

#define TRANSFORM_UBO_BINDING 0
#define LIGHT_UBO_BINDING 1

#define MAX_DIRECTION_LIGHTS	4
#define MAX_POINT_LIGHTS		5
#define MAX_SPOT_LIGHTS			5

typedef struct {
	int	num_attr;
	VertexAttribute* p_a_vtx_attr;
}_VAS;

//VAS_CLR
VertexAttribute vas_clr[] = {
	{0, COLOUR_LOCATION,	4, STREAM_FLOAT, 0, sizeof(float)*7,			   0,	0, 0},
	{0, POSITION_LOCATION,	3, STREAM_FLOAT, 0, sizeof(float)*7, sizeof(float)*4,	0, 0},
};
//VAS_CLR_TEX
VertexAttribute vas_clr_tx[] = {
	{0, COLOUR_LOCATION,	4, STREAM_FLOAT, 0, sizeof(float)*9,			   0,	0, 0},
	{0, UV_LOCATION0,		2, STREAM_FLOAT, 0, sizeof(float)*9, sizeof(float)*4,	0, 0},
	{0, POSITION_LOCATION,	3, STREAM_FLOAT, 0, sizeof(float)*9, sizeof(float)*6,	0, 0}	
};
//VAS_VTX_CLR
VertexAttribute vas_vtx_clr[] = {
	{0, COLOUR_LOCATION,	4, STREAM_FLOAT, 0, sizeof(float)*7,			   0,	0, 0},
	{0, POSITION_LOCATION,	3, STREAM_FLOAT, 0, sizeof(float)*7, sizeof(float)*4,	0, 0}
};
//VAS_VTX_CLR_TEX
VertexAttribute vas_vtx_clr_tx[] = {
	{0, COLOUR_LOCATION,	4, STREAM_FLOAT, 0, sizeof(float)*9,			   0,	0, 0},
	{0, UV_LOCATION0,		2, STREAM_FLOAT, 0, sizeof(float)*9, sizeof(float)*4,	0, 0},
	{0, POSITION_LOCATION,	3, STREAM_FLOAT, 0, sizeof(float)*9, sizeof(float)*6,	0, 0}
};
//VAS_VTX_CLR8_TEX
VertexAttribute vas_vtx_clr8_tx[] = {
	{0, COLOUR_LOCATION,	4, STREAM_UCHAR, 1, sizeof(float)*6,			   0,	0, 0},
	{0, UV_LOCATION0,		2, STREAM_FLOAT, 0, sizeof(float)*6, sizeof(float)*1,	0, 0},
	{0, POSITION_LOCATION,	3, STREAM_FLOAT, 0, sizeof(float)*6, sizeof(float)*3,	0, 0}
};

//VAS_VTX_TEX_ALIAS
VertexAttribute vas_vtx_tx_alias[] = {
	//NEEDS WORK
	{0, UV_LOCATION0,		2, STREAM_CHAR, 0, sizeof(char)*6,	 0,					0, 0},
	{0, POSITION_LOCATION,	3, STREAM_CHAR, 0, sizeof(char)*6,	 sizeof(char)*3,	0, 0},
	{0, SHADE_LOCATION,	4, STREAM_UCHAR, 1, sizeof(float)*6,			   0,	0, 0},
};

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

//for models....
typedef struct {
	float scale[3];
	float scaleOrigin[3];
	float skinWidth, skinHeight;
}AliasSettings;

typedef struct {
	Matrix44 mvp;
	Matrix44 proj; 
	Matrix44 mv;
	float r_origin_shade[3];
	float normalMin;
	float normalRange;
	float shadeIndex;
	float shadeLight;
	float ambientLight;
	float realtime;
	float gamma;
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

static const GLenum enumToGLAttribType[] = {GL_BYTE, GL_UNSIGNED_BYTE, GL_SHORT,GL_INT,QURP_FLOAT};

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

typedef struct _VtxData{
	VertexAttributeState vertex_state;  
	unsigned int transform_ubo_handle;
	unsigned int lighting_ubo_handle;

	unsigned int vao_handle;
	unsigned int vbo_handle;

	unsigned int vbo_capacity;		//bytes, constant

	unsigned int vbo_num_floats;	//number of floats currently in the buffer
	unsigned int vbo_size;			//suze in bytes of the number of floats in buffer
	
	unsigned int n_vertices;		//number of vertices
	unsigned int has_colour;
	unsigned int has_texture;

	float *	p_pre_gl_buffer;
	//float *	p_buffer_loc;
}VtxData;

static VtxData vtx = {	VAS_CLR,0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0};
static VtxData vtx_fallback = {	VAS_CLR,0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0};

typedef struct _VtxOffsets{
	int clr;
	int position;
	int uv;
	unsigned int total;
}VtxOffsets;
static VtxOffsets vtx_offsets = {0, 4, -1, 0};


int inbetween_start_end				= 0;
int transform_dirty = 0;
RenderState current_render_state = {
	RNDR_TRIANGLES, 
	0,0,0,0, 
	GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, 
	1, 0.0f, 1.0f, 
	BACK, 
	0,0,0,0, 
	0,
	1};
RenderState next_render_state = {
	RNDR_TRIANGLES, 
	0,0,0,0, 
	GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, 
	1, 0.0f, 1.0f, 
	BACK, 
	0,0,0,0, 
	0,
	1};
static int force_render_state_change = 0;
static UBOTransforms transforms;
static UBOLights lights;
static const int GRANULARITY	 = 16384*4*4; //probably too smalll
static const int transform_stack_size = 10;

static int alias_vbo = -1;				//
static int alias_vao = -1;				//
static int alias_vert_offset = 0;			//
static const int ALIAS_BUFFER_SIZE  = (1024 * 1024 * 12);

static int brush_vbo = -1;				//
static int brush_vao = -1;				//
static int brush_vbo_size = -1;			//

static SShaderProgram texture_shader;
static SShaderProgram colour_shader;
static SShaderProgram light_map_shader;
static SShaderProgram alias_shader;
static SShaderProgram brush_shader;
static SShaderProgram warp_shader;
static SShaderProgram sky_shader;

//we render to an internal buffer, then blit it to screen
#define USE_FBO 1
static unsigned int fbo_handle;
static unsigned int fbo_colour_texture;
static unsigned int fbo_depth_texture;

static unsigned int renderWidth = 0; renderHeight = 0;

void TransferAndDraw(void);

#define EPSILON 0.0001f//complete hack
static int HasRenderStateChanged(){
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

	if(current_render_state.cull_mode != next_render_state.cull_mode)
		return 1;
	/*
	if(current_render_state.red > next_render_state.red + EPSILON 
		|| current_render_state.red < next_render_state.red - EPSILON)
		return 1;
	if(current_render_state.green > next_render_state.green + EPSILON 
		|| current_render_state.green < next_render_state.green - EPSILON)
		return 1;
	if(current_render_state.blue > next_render_state.blue + EPSILON 
		|| current_render_state.blue < next_render_state.blue - EPSILON)
		return 1;
	if(current_render_state.alpha > next_render_state.alpha + EPSILON 
		|| current_render_state.alpha < next_render_state.alpha - EPSILON)
		return 1;
	*/
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
void BindVAO(void){	glBindVertexArray( vtx.vao_handle ); }
void UnbindVAO(void){	glBindVertexArray( 0 ); }

#define BUFFER_OFFSET(i) ((char *)0 + (i))
static void SetAttributeFormat( const VertexAttribute* pAttr, unsigned int numAttr, unsigned int v_offset)
{
	GLenum type = QURP_FLOAT;		
	unsigned int currentVBO = GetCurrentBuffer(GL_ARRAY_BUFFER_BINDING);
	unsigned int i;

	TransferAndDraw();

	if(vtx.n_vertices != 0){
		printf("ermergerd");
	}

	BindVAO();

	for(i=0;i<10;i++)glDisableVertexAttribArray(i);

	for(i=0; i<numAttr; i++)
	{
		if(pAttr[i].vbo != 0)
			BindBuffer(GL_ARRAY_BUFFER, pAttr[i].vbo);

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
		}
		if(pAttr[i].vbo != 0)
			BindBuffer(GL_ARRAY_BUFFER, currentVBO);
	}
	UnbindVAO();
}

void SetVertexMode(const VertexAttributeState state){
	int i=0;
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
				vtx_offsets.clr = vas[state].p_a_vtx_attr[i].offset / sizeof(float);				
			}
			if(vas[state].p_a_vtx_attr[i].idx ==POSITION_LOCATION){
				vtx_offsets.position = vas[state].p_a_vtx_attr[i].offset / sizeof(float);

				vtx_offsets.total = vas[state].p_a_vtx_attr[i].stride / sizeof(float);
			}
			if(vas[state].p_a_vtx_attr[i].idx ==UV_LOCATION0){
				vtx_offsets.uv = vas[state].p_a_vtx_attr[i].offset / sizeof(float);
			}
		}
	}
}


#define DIM 16
#define WIDTH DIM
#define HEIGHT DIM

unsigned int debug_texture = 0, normal_texture = 0, light_texture;

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
	int black=0;
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
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 256, 16, 0, GL_RED, GL_UNSIGNED_BYTE, pTexture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glActiveTexture(GL_TEXTURE0 + TEX_SLOT_CLR);
	free(pTexture);
}

static void CreateLightTextures(){
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
	//debug_texture = 13;
	glActiveTexture(GL_TEXTURE0 + TEX_SLOT_LIGHT_RENDER);
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
	switch(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER)) 
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
		case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
			printf("GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER\n");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
			printf("GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER\n");
			break;
		case GL_FRAMEBUFFER_UNSUPPORTED:
			printf("GL_FRAMEBUFFER_UNSUPPORTED\n");
			break;
		default:
			printf("FBO Status Error!\n");
			break;
	}
}

static void SetupFBO(const int width, const int height){
#if USE_FBO

	glGenTextures(1, &fbo_colour_texture);	
	glBindTexture(GL_TEXTURE_2D, fbo_colour_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBindTexture(GL_TEXTURE_2D, 0);

	glGenTextures(1, &fbo_depth_texture);	
	glBindTexture(GL_TEXTURE_2D, fbo_depth_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_STENCIL, width, height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, 0);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBindTexture(GL_TEXTURE_2D, 0);

	glGenFramebuffers(1, &fbo_handle);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_handle);
	glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo_colour_texture, 0);
	glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, fbo_depth_texture, 0);

	checkfbo();	

	GLenum fba[1] = {GL_COLOR_ATTACHMENT0};
	glDrawBuffers(1, fba);
#endif
}


static void SetupShaders(void){
	int got = 0;
	SShader vtxTxShdr, frgTxShdr;
	SShader vtxAlShdr, frgAlShdr;
	SShader vtxClrShdr, frgClrShdr;
	SShader vtxLightShdr, frgLightShdr;
	SShader vtxAliasShdr;//, vtxAliasShdr;
	const char *pDVertStr[4] = {0, 0,0,0}, *pDFragStr[4] = {0,0,0,0};

	glswInit();
	glswSetPath("./id1/shaders/", ".glsl");

	glswAddDirectiveToken("Version", "#version 420 core");

	glswAddDirectiveToken("Shared", HASH_DEFINE_VALUE(TRANSFORM_UBO_BINDING));
	glswAddDirectiveToken("Shared", HASH_DEFINE_VALUE(LIGHT_UBO_BINDING));
	glswAddDirectiveToken("Shared", HASH_DEFINE_VALUE(MAX_DIRECTION_LIGHTS));
	glswAddDirectiveToken("Shared", HASH_DEFINE_VALUE(MAX_POINT_LIGHTS));
	glswAddDirectiveToken("Shared", HASH_DEFINE_VALUE(MAX_SPOT_LIGHTS));

	glswAddDirectiveToken("Vertex", HASH_DEFINE_VALUE(POSITION_LOCATION));
	glswAddDirectiveToken("Vertex", HASH_DEFINE_VALUE(COLOUR_LOCATION));
	glswAddDirectiveToken("Vertex", HASH_DEFINE_VALUE(SHADE_LOCATION));
	glswAddDirectiveToken("Vertex", HASH_DEFINE_VALUE(NORMAL_LOCATION));	
	glswAddDirectiveToken("Vertex", HASH_DEFINE_VALUE(UV_LOCATION0));
	glswAddDirectiveToken("Vertex", HASH_DEFINE_VALUE(UV_LOCATION1));
	glswAddDirectiveToken("Vertex", HASH_DEFINE_VALUE(TEXT_LOCATION));
	glswAddDirectiveToken("Vertex", HASH_DEFINE_VALUE(TEX_SLOT_ANORM));

	glswAddDirectiveToken("Fragment", HASH_DEFINE_VALUE(TEX_SLOT_CLR));	
	glswAddDirectiveToken("Fragment", HASH_DEFINE_VALUE(TEX_SLOT_LIGHT_RENDER));
	glswAddDirectiveToken("Fragment", HASH_DEFINE_VALUE(TEX_SLOT_SKY));
	glswAddDirectiveToken("Fragment", HASH_DEFINE_VALUE(TEX_SLOT_SKY_ALPHA));

	//shader (TEXTURED)
	got = glswGetShadersAlt("shaders.Version+shaders.Header.Vertex+shaders.Shared+shaders.SimpleVertexTextured", pDVertStr, sizeof(pDVertStr)/sizeof(pDVertStr[0]));	
	CreateShader(&vtxTxShdr, VERT, pDVertStr, got);
	got = glswGetShadersAlt("shaders.Version+shaders.Header.Fragment+shaders.Shared+shaders.SimpleFragmentTextured", pDFragStr, sizeof(pDFragStr)/sizeof(pDFragStr[0]));
	CreateShader(&frgTxShdr, FRAG, pDFragStr, got);
	CreateShaderProgram(&texture_shader);
	AddShaderToProgram(&texture_shader,&vtxTxShdr);
	AddShaderToProgram(&texture_shader,&frgTxShdr);
	LinkShaderProgram(&texture_shader);	

	//shader (BRUSH)
	got = glswGetShadersAlt("shaders.Version+shaders.Header.Vertex+shaders.Shared+shaders.BrushVertex", pDVertStr, sizeof(pDVertStr)/sizeof(pDVertStr[0]));	
	CreateShader(&vtxTxShdr, VERT, pDVertStr, got);
	got = glswGetShadersAlt("shaders.Version+shaders.Header.Fragment+shaders.Shared+shaders.BrushFragment", pDFragStr, sizeof(pDFragStr)/sizeof(pDFragStr[0]));
	CreateShader(&frgTxShdr, FRAG, pDFragStr, got);
	CreateShaderProgram(&brush_shader);
	AddShaderToProgram(&brush_shader,&vtxTxShdr);
	AddShaderToProgram(&brush_shader,&frgTxShdr);
	LinkShaderProgram(&brush_shader);

	//shader (WARP)
	got = glswGetShadersAlt("shaders.Version+shaders.Header.Fragment+shaders.Shared+shaders.WarpFragment", pDFragStr, sizeof(pDFragStr) / sizeof(pDFragStr[0]));
	CreateShader(&frgTxShdr, FRAG, pDFragStr, got);
	CreateShaderProgram(&warp_shader);
	AddShaderToProgram(&warp_shader, &vtxTxShdr);
	AddShaderToProgram(&warp_shader, &frgTxShdr);
	LinkShaderProgram(&warp_shader);
	transforms.realtime = 0.0f;
	transforms.gamma = 1.0f;

	//shader (SKY)
	got = glswGetShadersAlt("shaders.Version+shaders.Header.Vertex+shaders.Shared+shaders.SkyVertex", pDVertStr, sizeof(pDVertStr) / sizeof(pDVertStr[0]));
	CreateShader(&vtxTxShdr, VERT, pDVertStr, got);
	got = glswGetShadersAlt("shaders.Version+shaders.Header.Fragment+shaders.Shared+shaders.SkyFragment", pDFragStr, sizeof(pDFragStr) / sizeof(pDFragStr[0]));
	CreateShader(&frgTxShdr, FRAG, pDFragStr, got);
	CreateShaderProgram(&sky_shader);
	AddShaderToProgram(&sky_shader, &vtxTxShdr);
	AddShaderToProgram(&sky_shader, &frgTxShdr);
	LinkShaderProgram(&sky_shader);

	//shader (ALIAS)
	got = glswGetShadersAlt("shaders.Version+shaders.Header.Vertex+shaders.Shared+shaders.SimpleVertexAlias", pDVertStr, sizeof(pDVertStr)/sizeof(pDVertStr[0]));	
	CreateShader(&vtxAlShdr, VERT, pDVertStr, got);
	got = glswGetShadersAlt("shaders.Version+shaders.Header.Fragment+shaders.Shared+shaders.SimpleFragmentAlias", pDFragStr, sizeof(pDFragStr)/sizeof(pDFragStr[0]));
	CreateShader(&frgAlShdr, FRAG, pDFragStr, got);
	CreateShaderProgram(&alias_shader);
	AddShaderToProgram(&alias_shader,&vtxAlShdr);
	AddShaderToProgram(&alias_shader,&frgAlShdr);
	LinkShaderProgram(&alias_shader);	

	//shader (COLOUR)
	got = glswGetShadersAlt("shaders.Version+shaders.Header.Vertex+shaders.Shared+shaders.SimpleVertexColoured", pDVertStr, sizeof(pDVertStr)/sizeof(pDVertStr[0]));	
	CreateShader(&vtxClrShdr, VERT, pDVertStr, got);
	got = glswGetShadersAlt("shaders.Version+shaders.Header.Fragment+shaders.Shared+shaders.SimpleFragmentColoured", pDFragStr, sizeof(pDFragStr)/sizeof(pDFragStr[0]));
	CreateShader(&frgClrShdr, FRAG, pDFragStr, got);
	CreateShaderProgram(&colour_shader);
	AddShaderToProgram(&colour_shader,&vtxClrShdr);
	AddShaderToProgram(&colour_shader,&frgClrShdr);
	LinkShaderProgram(&colour_shader);	

	//shader (LIGHTMAP)
	got = glswGetShadersAlt("shaders.Version+shaders.Header.Vertex+shaders.Shared+shaders.SimpleVertexLightmap", pDVertStr, sizeof(pDVertStr)/sizeof(pDVertStr[0]));	
	CreateShader(&vtxLightShdr, VERT, pDVertStr, got);
	got = glswGetShadersAlt("shaders.Version+shaders.Header.Fragment+shaders.Shared+shaders.SimpleFragmentLightmap", pDFragStr, sizeof(pDFragStr)/sizeof(pDFragStr[0]));
	CreateShader(&frgLightShdr, FRAG, pDFragStr, got);
	CreateShaderProgram(&light_map_shader);
	AddShaderToProgram(&light_map_shader, &vtxLightShdr);
	AddShaderToProgram(&light_map_shader, &frgLightShdr);
	LinkShaderProgram(&light_map_shader);	

	//this will delete them after we have deleted the program associated with them
	DeleteShader(&vtxTxShdr);
	DeleteShader(&frgTxShdr);	
	DeleteShader(&vtxAlShdr);
	DeleteShader(&frgAlShdr);
	DeleteShader(&vtxClrShdr);
	DeleteShader(&frgClrShdr);	
	DeleteShader(&vtxLightShdr);
	DeleteShader(&frgLightShdr);
}

static void SetupUBOs(){
	//transforms
	glGenBuffers(1, &vtx.transform_ubo_handle);
	glBindBuffer(GL_UNIFORM_BUFFER, vtx.transform_ubo_handle);
	glBufferData( GL_UNIFORM_BUFFER, sizeof(UBOTransforms), 0, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_UNIFORM_BUFFER, TRANSFORM_UBO_BINDING, vtx.transform_ubo_handle);

	//lights
	glGenBuffers(1, &vtx.lighting_ubo_handle);
	glBindBuffer(GL_UNIFORM_BUFFER, vtx.lighting_ubo_handle);
	glBufferData( GL_UNIFORM_BUFFER, sizeof(UBOLights), 0, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_UNIFORM_BUFFER, LIGHT_UBO_BINDING, vtx.lighting_ubo_handle);
}

/*static*/ void UpdateTransformUBOs(){	
	float *tmp = StackGetTop();
	int i=0;
	for(i=0;i<16;i++){
		transforms.mv.a[i] = tmp[i];
	}
	matMultiply44(&transforms.mv, &transforms.proj, &transforms.mvp);

	glBindBuffer(GL_UNIFORM_BUFFER, vtx.transform_ubo_handle);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(UBOTransforms), &transforms);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	transform_dirty = 0;
}

static void UpdateLightUBOs(){
	glBindBuffer(GL_UNIFORM_BUFFER, vtx.lighting_ubo_handle);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(UBOLights), &lights);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
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

void SetGamma(float gamma)
{
	transforms.gamma = gamma;
}

void StartupModernGLPatch(const int width, const int height){
	//setup a global VAO/VBO for rendering everything
	//hopefully this will be good enough
	if(vtx.vao_handle == 0){
		glGenVertexArrays(1,&vtx.vao_handle);		
		glGenBuffers(1, &vtx.vbo_handle);
		glBindBuffer(GL_ARRAY_BUFFER, vtx.vbo_handle);
		glBufferData(GL_ARRAY_BUFFER, GRANULARITY, 0, GL_DYNAMIC_DRAW);
		vtx.vbo_capacity = GRANULARITY + 4;
		vtx.vbo_size = 0;
	}

	// - - - - - - - - - - - - - - - -
	//setup the UBOS
	// - - - - - - - - - - - - - - - -
	SetupUBOs();
	matSetIdentity44(&transforms.mv);
	matSetIdentity44(&transforms.proj);

	// - - - - - - - - - - - - - - - -
	//setup the shaders library
	// - - - - - - - - - - - - - - - -
	SetupShaders();	

	// - - - - - - - - - - - - - - - -
	//Allocate float data 
	// - - - - - - - - - - - - - - - -
	vtx.p_pre_gl_buffer = (float*)malloc(vtx.vbo_capacity);	

	// - - - - - - - - - - - - - - - -
	//
	// - - - - - - - - - - - - - - - -
	InitialiseStack(transform_stack_size);

	SetRenderSize(width,height);

	//CreateDebugTextures();
	CreateANormTextures();
	CreateLightTextures();
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
		vtx.vbo_size = vtx.vbo_num_floats * sizeof( float );
	}
	if(type == VTX_TEXTURE)
		vtx.has_texture = 1;
	if(type == VTX_COLOUR)
		vtx.has_colour = 1;
}

//buffer overflow fix
//render to the last safe point and then 
//copy the new data back
static void TransferAndDrawFromLastSafePoint(){	
	if(vtx.n_vertices != vtx_fallback.n_vertices){
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
		TransferAndDraw();
	}
}

void AddVertex2D(const VtxDataType type, const float x, const float y){
	int vtx_idx = 0;//
	vtx_idx = PreAddVertexCheck(type);
	if(type == VTX_COLOUR)
		return;

	if(vtx.vbo_size + sizeof( float ) * 2 >= vtx.vbo_capacity){
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
	if(vtx.vbo_size + sizeof( float ) * 3 >= vtx.vbo_capacity){
		TransferAndDrawFromLastSafePoint();
	}

	//*vtx.p_buffer_loc++ = x;
	//*vtx.p_buffer_loc++ = y;
	//*vtx.p_buffer_loc++ = z;
	//vtx.vbo_num_floats = (vtx.p_buffer_loc - vtx.p_pre_gl_buffer);
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

	if(vtx.vbo_size + sizeof( float ) * 4 >= vtx.vbo_capacity){
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

	//vtx.vbo_num_floats = (vtx.p_buffer_loc - vtx.p_pre_gl_buffer);
	//vtx.vbo_size = vtx.vbo_num_floats * sizeof( float );

	PostAddVertexCheck(type);
}


void AddVertex4Db(const VtxDataType type, const unsigned char r, const unsigned char g, const unsigned char b, const unsigned char a){
	union
	{
	  unsigned char clr[4];
	  float f;
	} uClr;
	int vtx_idx = 0;//
	unsigned int same_colour = 0;
	int per_vertex_colour = IsPerVertexColour();
	vtx_idx = PreAddVertexCheck(type);

	if(vtx.vbo_size + sizeof( float ) * 4 >= vtx.vbo_capacity){
		TransferAndDrawFromLastSafePoint();
	}
	uClr.clr[0] = r;
	uClr.clr[1] = g;
	uClr.clr[2] = b;
	uClr.clr[3] = a;

	// - - - - - - - - - - - - - - - - - - - - -
	// - - - - - - - - - - - - - - - - - - - - -
	if(type == VTX_POSITION && vtx_offsets.position >= 0){
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.position + 0] = uClr.f;
	}
	if(type == VTX_COLOUR && vtx_offsets.clr >= 0){
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.clr + 0] = uClr.f;
	}
	if(type == VTX_TEXTURE && vtx_offsets.uv >= 0){
		vtx.p_pre_gl_buffer[(vtx_idx * vtx_offsets.total) + vtx_offsets.uv + 0] = uClr.f;
	}
	//*vtx.p_buffer_loc++ = uClr.f;
	//*vtx.p_buffer_loc++ = y;
	//*vtx.p_buffer_loc++ = z;
	//*vtx.p_buffer_loc++ = w;

	//vtx.vbo_num_floats = (vtx.p_buffer_loc - vtx.p_pre_gl_buffer);
	//vtx.vbo_size = vtx.vbo_num_floats * sizeof( float );

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

	if(current_render_state.enable_alpha_test)
		glEnable (GL_ALPHA_TEST);
	else
		glDisable (GL_ALPHA_TEST);

	glDepthMask(current_render_state.depth_mask);
	glDepthRange(current_render_state.depth_min, current_render_state.depth_max);

	if(current_render_state.enable_cull)
		glEnable ( GL_CULL_FACE );
	else
		glDisable ( GL_CULL_FACE);

	if(current_render_state.cull_mode == FRONT)
		glCullFace(GL_FRONT);
	else
		glCullFace(GL_BACK);

	if(current_render_state.lightmap_mode){
		glEnable(GL_TEXTURE_2D);
		Start(&light_map_shader);
	}
	else if(!vtx.has_texture){		
		Start(&colour_shader);
	}
	else{
		glEnable(GL_TEXTURE_2D);
		Start(&texture_shader);
	}
/*	if(current_render_state.enable_texture)
		glEnable (GL_TEXTURE_2D);
	else
		glDisable (GL_TEXTURE_2D); */
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
	
	if(transform_dirty)
		UpdateTransformUBOs();

	
	// - - - - - - - - - - - - - - - - - -
	//Send Data
	BindBuffer(GL_ARRAY_BUFFER, vtx.vbo_handle);
	//transfer buffer to openGL
	glBufferSubData(GL_ARRAY_BUFFER, 0, vtx.vbo_size, vtx.p_pre_gl_buffer);
	size_data_transfer += vtx.vbo_size;

	// - - - - - - - - - - - - - - - - - -
	//draw it 

	// - - - - - - - - - - - - - - - - - -
	// set state
	SetGLRenderState();

	BindVAO();	
	
	glDrawArrays(render_modes[current_render_state.render_mode], 0, vtx.n_vertices);

	//Stop();
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

void BlitFBO(const int windowWidth, const int windowHeight)
{

#if USE_FBO
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_handle);
	glDrawBuffer(GL_BACK);
	glReadBuffer(GL_COLOR_ATTACHMENT0);

	glBlitFramebuffer(0,0,renderWidth,renderHeight, 0,0,windowWidth,windowHeight,GL_COLOR_BUFFER_BIT, GL_NEAREST);
	//glBlitFramebuffer(0,0,renderWidth,renderHeight, 0,0,640,480,GL_COLOR_BUFFER_BIT, GL_NEAREST);

	glReadBuffer(GL_FRONT);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_handle);
	GLenum fba[1] = {GL_COLOR_ATTACHMENT0};
	glDrawBuffers(1, fba);
#endif

}

void CreatAliasBuffers(int* pVboOffset, int numVerts, void * pData)
{
	unsigned int currentVBO = GetCurrentBuffer(GL_ARRAY_BUFFER_BINDING);
	unsigned int currentVAO = GetCurrentBuffer(GL_VERTEX_ARRAY_BINDING);

	if(sizeof(glAliasData)*alias_vert_offset + sizeof(glAliasData)*numVerts > ALIAS_BUFFER_SIZE)
	{
		printf("out of alias memory....this will be an issue just now but not in the future\n");
	}
	printf("Memory Used %d\n", sizeof(glAliasData)*alias_vert_offset + sizeof(glAliasData)*numVerts);

	if(alias_vbo < 0)
	{
		glGenVertexArrays(1, &alias_vao);
		glGenBuffers(1, &alias_vbo);

		//send data to GPU
		glBindBuffer(GL_ARRAY_BUFFER, alias_vbo);
		glBufferData(GL_ARRAY_BUFFER, ALIAS_BUFFER_SIZE, 0, GL_STATIC_DRAW);

		alias_vert_offset = 0;

		//describe data for when we draw
		glBindVertexArray( alias_vao );

		glEnableVertexAttribArray(UV_LOCATION0);
		glBindBuffer(GL_ARRAY_BUFFER, alias_vbo);
		glVertexAttribPointer(UV_LOCATION0, 2, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(glAliasData), (char *)0);

		glEnableVertexAttribArray(POSITION_LOCATION);
		glBindBuffer(GL_ARRAY_BUFFER, alias_vbo);
		glVertexAttribPointer(POSITION_LOCATION, 3, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(glAliasData), (char *)(0 + sizeof(char) * 2));

		glEnableVertexAttribArray(SHADE_LOCATION);
		glBindBuffer(GL_ARRAY_BUFFER, alias_vbo);
		glVertexAttribPointer(SHADE_LOCATION, 1, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(glAliasData), (char *)(0 + sizeof(char)*2 + sizeof(char)*3));

		glBindVertexArray( currentVAO );
	}

	
	//send data to GPU
	glBindBuffer(GL_ARRAY_BUFFER, alias_vbo);
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(glAliasData)*alias_vert_offset, sizeof(glAliasData)*numVerts, pData);

	*pVboOffset = alias_vert_offset;
	//update offset
	alias_vert_offset += numVerts;
	//revert binding
	glBindBuffer(GL_ARRAY_BUFFER, currentVBO);
}

void StartAliasBatch(float depthmin, float depthmax)
{
	FlushDraw();
	
	glDepthMask(1);
	glDisable(GL_BLEND);
	glEnable (GL_DEPTH_TEST);
	glEnable (GL_CULL_FACE);
	glCullFace(GL_FRONT);
	glEnable(GL_TEXTURE_2D);
	Start(&alias_shader);
	glDepthRange(depthmin, depthmax);

	glBindVertexArray(alias_vao);
}

void RenderAlias(const int vbo_offset,  const int posenum, const int numTris, int shadeDotIndex, float shadeLight, float ambientlight)
{
	//highjack the normal matrix for now...
	transforms.shadeIndex = ((float)shadeDotIndex / 15.0f);
	transforms.shadeLight = shadeLight;
	transforms.ambientLight = ambientlight;

	UpdateTransformUBOs();

	glDrawArrays(GL_TRIANGLES, vbo_offset + ((numTris*3)*posenum), (numTris*3));
}

void EndAliasBatch()
{
	glBindVertexArray( vtx.vao_handle );
}

void CreateBrushBuffers(int numVerts)
{
#if BATCH_BRUSH
	unsigned int currentVBO = GetCurrentBuffer(GL_ARRAY_BUFFER_BINDING);
	unsigned int currentVAO = GetCurrentBuffer(GL_VERTEX_ARRAY_BINDING);

	if(brush_vbo >= 0)
	{
		glDeleteBuffers(1, &brush_vbo);
		glDeleteVertexArrays(1, &brush_vao);
	}

	glGenVertexArrays(1, &brush_vao);
	glGenBuffers(1, &brush_vbo);

	//send data to GPU
	glBindBuffer(GL_ARRAY_BUFFER, brush_vbo);
	glBufferData(GL_ARRAY_BUFFER, numVerts * sizeof(glBrushData), 0, GL_STATIC_DRAW);

	//describe data for when we draw
	glBindVertexArray( brush_vao );

	glEnableVertexAttribArray(POSITION_LOCATION);
	glBindBuffer(GL_ARRAY_BUFFER, brush_vbo);
	glVertexAttribPointer(POSITION_LOCATION, 3, QURP_FLOAT, GL_FALSE, sizeof(glBrushData), (char *)(0));

	glEnableVertexAttribArray(UV_LOCATION0);
	glBindBuffer(GL_ARRAY_BUFFER, brush_vbo);
	glVertexAttribPointer(UV_LOCATION0, 2, QURP_FLOAT, GL_FALSE, sizeof(glBrushData), (char *)(0 + sizeof(float)*3));


	glEnableVertexAttribArray(UV_LOCATION1);
	glBindBuffer(GL_ARRAY_BUFFER, brush_vbo);
	glVertexAttribPointer(UV_LOCATION1, 2, QURP_FLOAT, GL_FALSE, sizeof(glBrushData), (char *)(0 + sizeof(float)*5));

	glBindVertexArray( currentVAO );
	glBindBuffer(GL_ARRAY_BUFFER, currentVBO);
#endif
}

void AddBrushData(int vertexOffset, int numVerts, void * pData)
{
#if BATCH_BRUSH
	unsigned int currentVBO = GetCurrentBuffer(GL_ARRAY_BUFFER_BINDING);
	glBindBuffer(GL_ARRAY_BUFFER, brush_vbo);
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(glBrushData)*vertexOffset, sizeof(glBrushData)*numVerts, pData);
	glBindBuffer(GL_ARRAY_BUFFER, currentVBO);
#endif
}

void StartBrushBatch(float depthmin, float depthmax)
{
#if BATCH_BRUSH
	force_render_state_change = 1;
	FlushDraw();

	glDepthMask(1);
	glDisable(GL_BLEND);
	glEnable (GL_DEPTH_TEST);
	glEnable (GL_CULL_FACE);
	glCullFace(GL_FRONT);
	glEnable(GL_TEXTURE_2D);
	glDepthRange(depthmin, depthmax);

	Start(&brush_shader);
	UpdateTransformUBOs();
	glBindVertexArray(brush_vao);
#endif
}

void SetupWarpBatch()
{
#if BATCH_BRUSH
	Start(&warp_shader);
#endif
}

void SetupSkyBatch()
{
#if BATCH_BRUSH
	Start(&sky_shader);
#endif
}

void SetupColourPass()
{
#if BATCH_BRUSH
	glDisable(GL_BLEND);
	glDepthMask(1);
	Start(&brush_shader);
#endif
}

void SetupLightMapPass()
{
#if BATCH_BRUSH
	force_render_state_change = 1;
	glEnable(GL_BLEND);
	glBlendFunc( GL_ZERO, GL_ONE_MINUS_SRC_COLOR );
	glDepthMask(0);
	Start(&light_map_shader);
#endif
}

void RenderBrushData(int vertexOffset, int numTris)
{
#if BATCH_BRUSH
	unsigned int currentVBO = GetCurrentBuffer(GL_ARRAY_BUFFER_BINDING);
	unsigned int currentVAO = GetCurrentBuffer(GL_VERTEX_ARRAY_BINDING);

	glDrawArrays(GL_TRIANGLES, vertexOffset, (numTris*3));
#endif
}

void RenderBrushDataElements(unsigned short *pIndices, int numElements)
{
#if BATCH_BRUSH
	unsigned int currentVBO = GetCurrentBuffer(GL_ARRAY_BUFFER_BINDING);
	unsigned int currentVAO = GetCurrentBuffer(GL_VERTEX_ARRAY_BINDING);

	glDrawElements(GL_TRIANGLES, numElements, GL_UNSIGNED_SHORT, pIndices);
#endif
}

void EndBrushBatch()
{
	force_render_state_change = 1;
	glDisable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(1);
	SetLightmapMode(0);
	SetDepthMask(1);

#if BATCH_BRUSH
	glBindVertexArray( vtx.vao_handle );
#endif
}

void ShutdownModernGLPatch(){
	if(vtx.vao_handle){

		glDeleteVertexArrays(1, &vtx.vao_handle);		
		glDeleteBuffers(1,&vtx.vbo_handle);
		glDeleteBuffers(1,&vtx.transform_ubo_handle);
		glDeleteBuffers(1,&vtx.lighting_ubo_handle);
	}
	if(vtx.p_pre_gl_buffer){
		free(vtx.p_pre_gl_buffer);
		vtx.p_pre_gl_buffer = 0;
	}
	//vtx.p_buffer_loc = 0;

	DeleteShaderProgram(&texture_shader);
	DeleteShaderProgram(&colour_shader);
	DeleteShaderProgram(&light_map_shader);
	DeleteShaderProgram(&brush_shader);
	DeleteShaderProgram(&alias_shader);	
	glswShutdown();
	DestroyStack();
}

#endif // GLQUAKE