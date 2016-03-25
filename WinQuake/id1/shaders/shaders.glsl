
-- Version

//blank on purpose


-- Header.Vertex

layout(location = POSITION_LOCATION)	in vec4 inVertex;
layout(location = COLOUR_LOCATION)		in vec4 inColour;
layout(location = NORMAL_LOCATION)		in vec3 inNormal;
layout(location = UV_LOCATION0)			in vec2 inUV;
layout(location = TEXT_LOCATION)		in int inCharacter;


-- Header.Fragment
layout(binding=0) uniform sampler2D tex0;


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
	mat3		nrm;
}trans;

struct UBODirectionLight
{
	vec4 direction;		//eye space
	vec4 diffuse;
};//32 bytes

struct UBOPointLight
{
	vec4	position;	//eye space
	vec4	diffuse;
	float	intensity, half_intensity_distance, linear_attenuation, quadratic_attenuation;
};//48 bytes

struct UBOSpotLight
{
	vec4	position;	//eye space
	vec4	direction;	//eye space
	vec4	diffuse;
	float	intensity, half_intensity_distance, linear_attenuation, quadratic_attenuation;
	float	cosine_cutoff, dummy0, dummy1, dummy2;
};//48 bytes

layout(std140, binding=LIGHT_UBO_BINDING) uniform Lights
{
	int		number_of_direction_lights;
	int		number_of_point_lights;
	int		number_of_spot_lights;
	int		dummy;

	UBODirectionLight direction_light[MAX_DIRECTION_LIGHTS];
	UBOPointLight point_light[MAX_POINT_LIGHTS];
	UBOSpotLight spot_light[MAX_SPOT_LIGHTS];

}lights;

//can be done in the vertex or fragment shader so we'll plop it here for nwo
void DirectionalLight(const in UBODirectionLight light, /*const in vec3 esHalfVector, */ const in vec3 normal, inout vec4 diffuse, inout vec4 specular)
{
	float nDotVP;
	float nDotHV;
	float pf;

	nDotVP = max(0.0, dot(normal, light.direction.xyz ));	
	//nDotHV = max(0.0, dot(normal, esHalfVector ));
	if(nDotVP == 0.0)	
		pf = 0.0;
	else
		pf = pow(nDotHV, 16);	

	diffuse += light.diffuse  * nDotVP;	
	//specular += light.specular * pf * min(1.0, nDotVP*5.0);	
}

void PointLight(const in UBOPointLight light, in vec3 esVtx, /*const in vec3 esHalfVector, */  in vec3 normal, inout vec4 diffuse, inout vec4 specular)
{
	vec3 surfaceToLight;			// direction from surface to light position
	float distanceSurfaceToLight;	//
	float dotNormalLightDir;		// normal . light direction	
	float attenuation = 0;			// computed attenuation factor

	surfaceToLight = light.position.xyz - esVtx;
	distanceSurfaceToLight = length(surfaceToLight);
	surfaceToLight = normalize(surfaceToLight);


	// Compute attenuation;
	float D2 = pow(light.half_intensity_distance,2);
	attenuation = max(0, D2 / (D2 + light.quadratic_attenuation * pow(distanceSurfaceToLight,2)));
	//halfVector = normalize (VP + eye);
	dotNormalLightDir = max (0.0, dot(normal, surfaceToLight));

	//nDotHV = max (0.0, dot (normal, halfVector));
	//if (nDotVP == 0.0)
	//	pf = 0.0;
	//else
	//	pf = pow (nDotHV, uniMaterial.shininess);
	//ambient  += light.ambient;
	diffuse  += light.diffuse * dotNormalLightDir * attenuation;
	//specular += light.specular * pf * attenuation * min(1.0, nDotVP*2.0);
}

void SpotLight(const in UBOSpotLight light, in vec3 esVtx, /*const in vec3 esHalfVector, */  in vec3 normal, inout vec4 diffuse, inout vec4 specular)
{
	vec3 surfaceToLight;			// direction from surface to light position
	float distanceSurfaceToLight;	//
	float dotNormalLightDir;		// normal . light direction
	float dotSpotDirLightDir;		// normal . spot direction
	float attenuation = 0;			// computed attenuation factor

	surfaceToLight = light.position.xyz - esVtx;
	distanceSurfaceToLight = length(surfaceToLight);
	surfaceToLight = normalize(surfaceToLight);

	dotSpotDirLightDir = dot(surfaceToLight, light.direction.xyz);

	// Compute attenuation;
	float D2 = pow(light.half_intensity_distance,2);
	attenuation = max(0, D2 / (D2 + light.quadratic_attenuation * pow(distanceSurfaceToLight,2)));

	if(dotSpotDirLightDir <	 light.cosine_cutoff)
		attenuation = 0;

	//halfVector = normalize (VP + eye);
	dotNormalLightDir = max (0.0, dot(normal, surfaceToLight));

	//nDotHV = max (0.0, dot (normal, halfVector));
	//if (nDotVP == 0.0)
	//	pf = 0.0;
	//else
	//	pf = pow (nDotHV, uniMaterial.shininess);
	//ambient  += light.ambient;
	diffuse  += light.diffuse * dotNormalLightDir * attenuation;
	//specular += light.specular * pf * attenuation * min(1.0, nDotVP*2.0);
}

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
	//fragColour = vec4(1,0,0,0) + texture(tex0, uv);
	//fragColour = vec4(uv.x,uv.y,0,0);

	fragColour = texture(tex0, uv) * colour;
	//fragColour.a *= colour.a;	
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
	uv = inUV;
	gl_Position = trans.mvp * inVertex;
}

-- SimpleFragmentLightmap

in vec2 uv;
out vec4 fragColour;
void main()
{
	fragColour = texture(tex0, uv).rrra;

}