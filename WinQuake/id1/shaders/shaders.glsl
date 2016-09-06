
-- Version

//blank on purpose


-- Header.Vertex

layout(location = POSITION_LOCATION)	in vec4 inVertex;
layout(location = COLOUR_LOCATION)		in vec4 inColour;
layout(location = NORMAL_LOCATION)		in vec3 inNormal;
layout(location = UV_LOCATION0)			in vec2 inUV;
layout(location = UV_LOCATION1)			in vec2 inUV1;
layout(location = TEXT_LOCATION)		in int inCharacter;
layout(location = SHADE_LOCATION)		in float inShadeIndex;
layout(binding=TEX_SLOT_ANORM)			uniform sampler2D anorm;

-- Header.Fragment
layout(binding=TEX_SLOT_CLR)			uniform sampler2D tex0;
layout(binding=TEX_SLOT_LIGHT_RENDER)	uniform sampler2D texLightmap;
layout(binding=TEX_SLOT_SKY)			uniform	sampler2D sky;
layout(binding=TEX_SLOT_SKY_ALPHA)		uniform sampler2D skyAlpha;


-- Header.Vertex.ES
#version 330 core
layout(location = POSITION_LOCATION)	in vec4 inVertex;
layout(location = COLOUR_LOCATION)		in vec4 inColour;
layout(location = NORMAL_LOCATION)		in vec3 inNormal;
layout(location = UV_LOCATION0)			in vec2 inUV;


-- Header.Fragment.ES
#version 330 core
uniform sampler2D tex0;


-- Shared
#ifdef TEXTURE_BUFFER_SKINNING
layout(binding=SKINNING_TEXTURE_BINDING) uniform samplerBuffer sb_skinning_matrices;
#endif

#ifdef TRANSFORM_UBO_BINDING
layout(std140, binding=TRANSFORM_UBO_BINDING) uniform Transforms
#else
layout(std140) uniform Transforms
#endif
{
	mat4		mvp;
	mat4		proj;
	mat4		mv;
	vec3		r_origin_shade; //for the sky shader it's the origin, for alias it's the light direction
	float		normalMin;
	float		normalRange;
	float		shadeIndex;
	float		shadeLight;
	float		ambientLight;
	float		realtime;
	float		gamma;
}trans;


-- Shared.ES

struct Transforms
{
	mat4		mvp;
	mat4		proj;
	mat4		mv;
	mat3		nrm;
};

uniform  Transforms trans;


-- SimpleVertexTextured
out vec4 colour;
out vec2 uv;
void main()
{
	colour = inColour;	
	uv = inUV;
	gl_Position = trans.mvp * inVertex;
}

-- SimpleFragmentTextured

in vec4 colour;
in vec2 uv;

out vec4 fragColour;

void main()
{
	//fragColour = texture(tex0, uv) * colour;
	fragColour = pow(texture(tex0, uv),vec4(trans.gamma)) * colour;
}

-- BrushVertex

out vec2 uv;
out vec2 uvLightmap;
void main()
{
	uv = inUV;
	uvLightmap = inUV1;
	gl_Position = trans.mvp * inVertex;
}

-- BrushFragment

in vec2 uv;
in vec2 uvLightmap;

out vec4 fragColour;

void main()
{
	//vec4 base = texture(tex0, uv);
	//float fullbright = (1.0 - base.a);
	//fragColour = pow(base * clamp(texture(texLightmap, uvLightmap).r*2.0+fullbright,0.0,2.0), vec4(trans.gamma));
	fragColour = pow(texture(tex0, uv), vec4(trans.gamma));
}

-- WarpFragment

in vec2 uv;
in vec2 uvLightmap;

out vec4 fragColour;

void main()
{
	vec2 warpUV;
	warpUV.x = uv.x + sin( (uv.y + trans.realtime)) * (1.0/5.0);
	warpUV.y = uv.y + cos( (uv.t + trans.realtime)) * (1.0/5.0);
	fragColour = pow(texture(tex0, warpUV),vec4(trans.gamma));
}

-- SkyVertex

out vec2 uvslow, uvfast;
void main()
{
	vec3 eyeSpacePosition =  inVertex.xyz - trans.r_origin_shade;
	eyeSpacePosition.z *= 3.0;
	float length = length(eyeSpacePosition.xyz);
	length = 6*63.0/length;
	
	eyeSpacePosition.x *= length;
	eyeSpacePosition.y *= length;
	
	uvslow = vec2(trans.realtime*8 + eyeSpacePosition.x,trans.realtime*8 + eyeSpacePosition.y)*(1/128.0);
	uvfast = vec2(trans.realtime*16 + eyeSpacePosition.x,trans.realtime*16 + eyeSpacePosition.y)*(1/128.0);

	gl_Position = trans.mvp * inVertex;
}

-- SkyFragment

in vec2 uvslow,uvfast;
out vec4 fragColour;

void main()
{
	vec4 alpha = texture(skyAlpha, uvfast);
	fragColour = pow(mix(texture(sky, uvslow), alpha, alpha.a),vec4(trans.gamma));
}

-- SimpleVertexAlias
out float shade;
out vec2 uv;

void main()
{
	float texAnorm = texture(anorm, vec2(inShadeIndex / 255.0, trans.shadeIndex)).r;
	float scaledTexAnorm = trans.normalMin + (texAnorm * trans.normalRange);

	shade = (scaledTexAnorm * trans.shadeLight) + trans.ambientLight;

	uv = inUV;

	gl_Position = trans.mvp * inVertex;
}

-- SimpleFragmentAlias

in float shade;
in vec2 uv;

out vec4 fragColour;

void main()
{
	vec4 base = texture(tex0, uv);
	float fullbright = (1.0 - base.a);
	fragColour = pow(base*clamp(shade+fullbright,0.0,4.0),vec4(trans.gamma));
}

-- SimpleVertexColoured
out vec4 colour;
void main()
{
	colour = inColour;	
	gl_Position = trans.mvp * inVertex;
}

-- SimpleFragmentColoured

in vec4 colour;

out vec4 fragColour;

void main()
{	
	fragColour = colour;	
}

-- SimpleVertexLightmap
out vec2 uv;
void main()
{
	uv = inUV1;
	gl_Position = trans.mvp * inVertex;
}

-- SimpleFragmentLightmap

in vec2 uv;
out vec4 fragColour;
void main()
{
	fragColour = pow(texture(texLightmap, uv).rrra,vec4(trans.gamma)); //applying gamme twice?
}