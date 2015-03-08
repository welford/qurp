#include "modern_gl_port.h"
#ifdef USE_GLES
#include "transforms.h"

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

#include <string.h>
#include <stdlib.h>

#define STRINGIFY_NAME(s) #s
#define STRINGIFY_VALUE(x) STRINGIFY_NAME(x)
#define HASH_DEFINE_VALUE(x) "#define "#x" "STRINGIFY_VALUE(x)
#define HASH_DEFINE(x) "#define "#x

#define MAX_DIRECTION_LIGHTS	4
#define MAX_POINT_LIGHTS		5
#define MAX_SPOT_LIGHTS			5

typedef struct {
	int	num_attr;
	VertexAttribute* p_a_vtx_attr;
}_VAS;


#if USE_HALF_FLOATS
#define float_type __fp16
#define float_size 2
#else
#define float_type float
#define float_size sizeof(float)
#endif

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
VertexAttribute vas_vtx_clr8_tx[] = {
	{0, COLOUR_LOCATION,	4, STREAM_UCHAR, 1, float_size*6,			 0,	0, 0},
	{0, UV_LOCATION0,		2, STREAM_FLOAT, 0, float_size*6, float_size*1,	1, 0},
	{0, POSITION_LOCATION,	3, STREAM_FLOAT, 0, float_size*6, float_size*3,	3, 0}
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

typedef struct {
	Matrix44 mvp;
	Matrix44 proj; 
	Matrix44 mv;
	float nrm[12];//3x3 matrix is stored as 3 column vectors of length 4, so 3,7 and  11 are unused padding
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

#if USE_HALF_FLOATS
static const GLenum enumToGLAttribType[] = {GL_BYTE, GL_UNSIGNED_BYTE, GL_SHORT,GL_INT,GL_HALF_FLOAT_OES};
#else
static const GLenum enumToGLAttribType[] = { GL_BYTE, GL_UNSIGNED_BYTE, GL_SHORT, GL_INT, GL_FLOAT };
#endif

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
	
}RenderState;

static unsigned int current_shader_idx = 0;

typedef struct _VtxData{
	VertexAttributeState vertex_state;  
	int transform_uniform[3];

	//unsigned int vao_handle;
	unsigned int vbo_handle;

	unsigned int vbo_capacity;		//bytes, constant

	unsigned int vbo_num_floats;	//number of floats currently in the buffer
	unsigned int vbo_size;			//suze in bytes of the number of floats in buffer
	
	unsigned int n_vertices;		//number of vertices
	unsigned int has_colour;
	unsigned int has_texture;

#if USE_HALF_FLOATS
	__fp16 *	p_pre_gl_buffer;
#else
	float *		p_pre_gl_buffer;
#endif
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
	0};
RenderState next_render_state = {
	RNDR_TRIANGLES, 
	0,0,0,0, 
	GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, 
	1, 0.0f, 1.0f, 
	BACK, 
	0,0,0,0, 
	0};
static UBOTransforms transforms;
static UBOLights lights;

#if USE_HALF_FLOATS
static const int GRANULARITY = 16384*4*float_size; //probably too small (half floats)
#else
static const int GRANULARITY = 16384*4*float_size; //probably too small
#endif
static const int transform_stack_size = 10;

SShaderProgram texture_shader;
SShaderProgram colour_shader;
SShaderProgram light_map_shader;

void TransferAndDraw(void);

#define EPSILON 0.0001f//complete hack
static int HasRenderStateChanged(){
	int i=0;
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

	/*
	if(current_render_state.red > next_render_state.red + EPSILON 
		|| current_render_state.red < next_render_state.red - EPSILON)
		return 1;
	printf("%d\n",i++);
	if(current_render_state.green > next_render_state.green + EPSILON 
		|| current_render_state.green < next_render_state.green - EPSILON)
		return 1;
	printf("%d\n",i++);
	if(current_render_state.blue > next_render_state.blue + EPSILON 
		|| current_render_state.blue < next_render_state.blue - EPSILON)
		return 1;
	printf("%d\n",i++);
	if(current_render_state.alpha > next_render_state.alpha + EPSILON 
		|| current_render_state.alpha < next_render_state.alpha - EPSILON)
		return 1;
	printf("%d\n",i++);
	*/
	if(current_render_state.lightmap_mode != next_render_state.lightmap_mode)
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

unsigned int debug_texture = 0;

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


static void SetupShaders(void){
	int got = 0;
	SShader vtxTxShdr, frgTxShdr;
	SShader vtxClrShdr, frgClrShdr;
	SShader vtxLightShdr, frgLightShdr;
	const char *pDVertStr[3] = {0,0,0}, *pDFragStr[3] = {0,0,0};

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
	SetTextureUnitByName(&texture_shader,"tex0", 0);
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
	SetTextureUnitByName(&colour_shader, "tex0", 0);
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
	SetAttributeLocation(&light_map_shader, COLOUR_LOCATION, "inColour");
	SetAttributeLocation(&light_map_shader, UV_LOCATION0, "inUV");	
	LinkShaderProgram(&light_map_shader);	
	Start(&light_map_shader);
	SetTextureUnitByName(&light_map_shader, "tex0", 0);
	Stop();

	//this will delete them after we have deleted the program associated with them
	DeleteShader(&vtxTxShdr);
	DeleteShader(&frgTxShdr);	
	DeleteShader(&vtxClrShdr);
	DeleteShader(&frgClrShdr);	
	DeleteShader(&vtxLightShdr);
	DeleteShader(&frgLightShdr);
}

static void SetupUBOs(){
	current_shader_idx = 0;
	vtx.transform_uniform[0] = glGetUniformLocation(texture_shader.handle, "trans.mvp");
	vtx.transform_uniform[1] = glGetUniformLocation(colour_shader.handle, "trans.mvp");
	vtx.transform_uniform[2] = glGetUniformLocation(light_map_shader.handle, "trans.mvp");

	//transforms
	//glGenBuffers(1, &vtx.transform_ubo_handle);
	//glBindBuffer(GL_UNIFORM_BUFFER, vtx.transform_ubo_handle);
	//glBufferData( GL_UNIFORM_BUFFER, sizeof(UBOTransforms), 0, GL_DYNAMIC_DRAW);
	//glBindBufferBase(GL_UNIFORM_BUFFER, TRANSFORM_UBO_BINDING, vtx.transform_ubo_handle);
}

static void UpdateTransformUBOs(){	
	float *tmp = StackGetTop();
	int i=0;
	for(i=0;i<16;i++){
		transforms.mv.a[i] = tmp[i];
	}
	matMultiply44(&transforms.mv, &transforms.proj, &transforms.mvp);
	/*
	glBindBuffer(GL_UNIFORM_BUFFER, vtx.transform_ubo_handle);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(UBOTransforms), &transforms);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	*/
	SetMtx44ByLocation(vtx.transform_uniform[current_shader_idx], transforms.mvp.a);
	transform_dirty = 0;
}

void StartupModernGLPatch(){
	float * tmp;
	glActiveTexture(GL_TEXTURE0);

	//setup a global VAO/VBO for rendering everything
	//hopefully this will be good enough
	//if(vtx.vao_handle == 0){
	if( 1 ){
		//glGenVertexArrays(1,&vtx.vao_handle);		
		glGenBuffers(1, &vtx.vbo_handle);
		glBindBuffer(GL_ARRAY_BUFFER, vtx.vbo_handle);
		glBufferData(GL_ARRAY_BUFFER, GRANULARITY, 0, GL_DYNAMIC_DRAW);

		//printf("buffer location : %p \n", tmp);
		tmp = (float*)glMapBufferOES(GL_ARRAY_BUFFER, GL_WRITE_ONLY_OES);
		//printf("buffer location : %p \n", tmp);
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

	CreateDebugTextures();
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

void EnableAlpha(){

}

void DisableAlpha(){

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

	if(vtx.vbo_size + float_size * 4 >= vtx.vbo_capacity){
		TransferAndDrawFromLastSafePoint();
	}

	// - - - - - - - - - - - - - - - - - - - - -
	// - - - - - - - - - - - - - - - - - - - - -
	//*vtx.p_buffer_loc++ = x;
	//*vtx.p_buffer_loc++ = y;
	//*vtx.p_buffer_loc++ = z;
	//*vtx.p_buffer_loc++ = w;

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

	if(vtx.vbo_size + float_size * 4 >= vtx.vbo_capacity){
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

	//glBindTexture(GL_TEXTURE_2D, debug_texture);

	// - - - - - - - - - - - - - - - - - -
	//Send Data
	if(transform_dirty)
		UpdateTransformUBOs();

	//BindBuffer(GL_ARRAY_BUFFER, vtx.vbo_handle);
	//transfer buffer to openGL
	//printf("buffer size : %d" , vtx.vbo_size);
	glBufferSubData(GL_ARRAY_BUFFER, 0, vtx.vbo_size, vtx.p_pre_gl_buffer);
	
	size_data_transfer += vtx.vbo_size;

	//glFlush();
	// - - - - - - - - - - - - - - - - - -
	//draw it 

	BindVAO();	
	//printf("vertex size : %d" , vtx.n_vertices);
	
	glDrawArrays(render_modes[current_render_state.render_mode], 0, vtx.n_vertices);
	glBufferData(GL_ARRAY_BUFFER, GRANULARITY, 0, GL_DYNAMIC_DRAW);

	//Stop();
	
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

void ShutdownModernGLPatch(){
	if(1){//vtx.vao_handle){
		//glDeleteVertexArrays(1, &vtx.vao_handle);		
		glDeleteBuffers(1,&vtx.vbo_handle);		
	}
	if(vtx.p_pre_gl_buffer){
		free(vtx.p_pre_gl_buffer);
		vtx.p_pre_gl_buffer = 0;
	}
	//vtx.p_buffer_loc = 0;

	DeleteShaderProgram(&texture_shader);
	DeleteShaderProgram(&colour_shader);
	DeleteShaderProgram(&light_map_shader);
	//glswShutdown();
	DestroyStack();
}

#endif