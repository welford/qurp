/*
//shader (Texture)
got = glswGetShadersAlt("shaders.Header.Vertex.ES+shaders.Shared.ES+shaders.SimpleVertexTextured", pDVertStr, 3);	
got = glswGetShadersAlt("shaders.Header.Fragment.ES+shaders.Shared.ES+shaders.SimpleFragmentTextured", pDFragStr, 3);

//shader (COLOUR)
got = glswGetShadersAlt("shaders.Header.Vertex.ES+shaders.Shared.ES+shaders.SimpleVertexColoured", pDVertStr, 3);	
got = glswGetShadersAlt("shaders.Header.Fragment.ES+shaders.Shared.ES+shaders.SimpleFragmentColoured", pDFragStr, 3);

//shader (LIGHTMAP)
got = glswGetShadersAlt("shaders.Header.Vertex.ES+shaders.Shared.ES+shaders.SimpleVertexLightmap", pDVertStr, 3);	
got = glswGetShadersAlt("shaders.Header.Fragment.ES+shaders.Shared.ES+shaders.SimpleFragmentLightmap", pDFragStr, 3);
*/

static const char* header_vertex = "\
attribute vec4 inVertex;\
attribute vec4 inColour;\
attribute vec3 inNormal;\
attribute vec2 inUV;\
\n";

static const char* header_fragment = "\n\
uniform sampler2D tex0;\n\
\n";

static const char* header_shared = "\n\
struct Transforms\n\
{\n\
	mat4		mvp;\n\
	mat4		proj;\n\
	mat4		mv;\n\
	mat3		nrm;\n\
};\n\
uniform  Transforms trans;\n\
\n";

static const char* txt_clr_vertex = "\n\
varying vec4 colour;\n\
varying vec2 uv;\n\
void main()\n\
{\n\
	colour = inColour;	\n\
	uv = inUV;\n\
	gl_Position = trans.mvp * inVertex;\n\
}\n\
\n";

static const char* txt_clr_frag = "\n\
varying vec4 colour;\n\
varying vec2 uv;\n\
\n\
void main()\n\
{	\n\
	gl_FragColor = texture2D(tex0, uv) * colour;\n\
}\n\
\n";

static const char* clr_vertex = "\
varying vec4 colour;\n\
void main()\n\
{\n\
	colour = inColour;	\n\
	gl_Position = trans.mvp * inVertex;\n\
}\n\
\n";

static const char* clr_frag = "\
varying vec4 colour;\n\
\n\
void main()\n\
{	\n\
	gl_FragColor = colour; \n\
}\n\
\n";

static const char* lgt_vertex = "\n\
varying vec2 uv;\n\
void main()\n\
{\n\
	uv = inUV;\n\
	gl_Position = trans.mvp * inVertex;\n\
}\n\
\n";

static const char* lgt_frag = "\
varying vec2 uv;\n\
void main()\n\
{\n\
	gl_FragColor = texture2D(tex0, uv).aaaa;	\n\
}\n\
";

