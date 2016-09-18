
#include "quakedef.h"

static const char* header_vertex = "\
attribute vec4 inVertex;\
attribute vec4 inColour;\
attribute vec3 inNormal;\
attribute vec2 inUV;\
attribute vec2 inUVLightmap;\
attribute float inShadeIndex;\
\n";

static const char* header_fragment = "\n\
uniform sampler2D tex0;\n\
uniform sampler2D texLightMap;\n\
uniform sampler2D sky;\n\
uniform sampler2D skyAlpha;\n\
\n";

static const char* header_shared = "\n\
struct Transforms\n\
{\n\
	mat4		mvp;\n\
	mat4		proj;\n\
	mat4		mv;\n\
	float		normalMin;\n\
	float		normalRange;\n\
	float		shadeIndex;\n\
	float		shadeLight;\n\
	float		ambientLight;\n\
	float		realtime;\n\
	float		gamma;\n\
	vec3		r_origin_shade;\n\
};\n\
uniform  Transforms trans;\n\
uniform sampler2D anorm;\n\
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
	gl_FragColor = pow(texture2D(tex0, uv) * colour,vec4(trans.gamma));\n\
	//gl_FragColor = texture2D(tex0, uv) * colour * trans.gamma;\n\
}\n\
\n";

static const char* brush_clr_vertex = "\n\
varying vec2 uv;\n\
varying vec2 uvLightmap;\n\
void main()\n\
{\n\
	uv = inUV;\n\
	uvLightmap = inUVLightmap;\n\
	gl_Position = trans.mvp * inVertex;\n\
}\n\
\n";

static const char* brush_clr_frag = "\n\
varying vec2 uv;\n\
varying vec2 uvLightmap;\n\
\n\
void main()\n\
{\n\
	vec4 base = texture2D(tex0, uv);\n\
	float fullbright = (1.0 - base.a);\n\
	gl_FragColor = pow(base.rgba * clamp(texture2D(texLightMap, uvLightmap).aaaa*2.0+fullbright, 0.0, 2.0),vec4(trans.gamma));\n\
	//gl_FragColor = base.rgba * clamp(texture2D(texLightMap, uvLightmap).aaaa*2.0+fullbright, 0.0, 2.0) * trans.gamma;\n\
}\n\
\n";

static const char* warp_clr_frag = "\n\
varying vec2 uv;\n\
varying vec2 uvLightmap;\n\
\n\
void main()\n\
{\n\
	vec2 warpUV;\n\
	warpUV.x = uv.x + sin((uv.y + trans.realtime)) * (1.0 / 5.0);\n\
	warpUV.y = uv.y + cos((uv.t + trans.realtime)) * (1.0 / 5.0);\n\
	gl_FragColor = pow(texture2D(tex0, warpUV),vec4(trans.gamma));\n\
	//gl_FragColor = texture2D(tex0, warpUV) * trans.gamma;\n\
}\n\
\n";

static const char* sky_vertex = "\n\
varying vec2 uvslow;\n\
varying vec2 uvfast;\n\
void main()\n\
{\n\
	vec3 eyeSpacePosition =  inVertex.xyz - trans.r_origin_shade;\n\
	eyeSpacePosition.z *= 3.0;\n\
	float len = length(eyeSpacePosition.xyz);\n\
	len = 6.0 * 63.0 / len; \n\
	\n\
	eyeSpacePosition.x *= len; \n\
	eyeSpacePosition.y *= len; \n\
	\n\
	uvslow = vec2(trans.realtime * 8.0 + eyeSpacePosition.x, trans.realtime * 8.0 + eyeSpacePosition.y)*(1.0 / 128.0); \n\
	uvfast = vec2(trans.realtime * 16.0 + eyeSpacePosition.x, trans.realtime * 16.0 + eyeSpacePosition.y)*(1.0 / 128.0); \n\
	//uvslow = inUV; \n\
	//uvfast = inUV; \n\
	gl_Position = trans.mvp * inVertex;\n\
}\n\
\n";

static const char* sky_frag = "\n\
varying vec2 uvslow;\n\
varying vec2 uvfast;\n\
\n\
void main()\n\
{\n\
	vec4 alpha = texture2D(skyAlpha, uvfast);\n\
	gl_FragColor = pow((texture2D(sky, uvslow) * (1.0 - alpha.a)) + (alpha * alpha.a),vec4(trans.gamma));\n\
	//gl_FragColor = (texture2D(sky, uvslow) * (1.0 - alpha.a)) + (alpha * alpha.a) * trans.gamma;\n\
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
	uv = inUVLightmap;\n\
	gl_Position = trans.mvp * inVertex;\n\
}\n\
\n";

static const char* lgt_frag = "\
varying vec2 uv;\n\
void main()\n\
{\n\
	gl_FragColor = texture2D(texLightMap, uv).aaaa;	\n\
}\n\
";

static const char* alias_vertex = "\n\
varying float shade;\n\
varying vec2 uv;\n\
void main()\n\
{\n\
	float texAnorm = texture2D(anorm, vec2(inShadeIndex / 255.0, trans.shadeIndex)).r;\n\
	float scaledTexAnorm = trans.normalMin + (texAnorm * trans.normalRange);\n\
	\n\
	shade = (scaledTexAnorm * trans.shadeLight) + trans.ambientLight;\n\
	uv = inUV;\n\
	gl_Position = trans.mvp * inVertex;\n\
}\n\
\n";

static const char* alias_frag = "\n\
varying float shade;\n\
varying vec2 uv;\n\
\n\
void main()\n\
{	\n\
	vec4 base = texture2D(tex0, uv);\n\
	float fullbright = (1.0 - base.a);\n\
	gl_FragColor = pow(base * clamp(shade+fullbright,0.0,4.0),vec4(trans.gamma));\n\
	//gl_FragColor = base * clamp(shade+fullbright,0.0,4.0) * trans.gamma;\n\
}\n\
\n";
