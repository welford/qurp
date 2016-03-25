-- DebugFont.Vertex

out unsigned int character;
out int position;

void main()
{
    character = inCharacter;
    position = gl_VertexID;
    gl_Position = vec4(0, 0, 0, 0);
}

-- DebugFont.Geometry

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

in unsigned int character[1];
in int position[1];
out vec2 tex_coord;
uniform sampler2D Sampler;

uniform vec2 TextureGlyphSize = vec2(1.0/16.0, 1.0/8.0);
uniform vec2 CharacterScreenSize = vec2(0.05, 0.05);
uniform vec2 ScreenSpaceOrigin = vec2(0.0, 0); // 0,0 bottom eft to 1,1 top right

void main()
{
    // Determine the final quad's position and size:
    float x = (ScreenSpaceOrigin.x*2.0-1.0) + float(position[0]) * CharacterScreenSize.x * 2;
    float y = (ScreenSpaceOrigin.y*2.0-1.0);
    vec4 P = vec4(x, y, 0, 1);
    vec4 U = vec4(1.0, 0, 0, 0) * CharacterScreenSize.x;
    vec4 V = vec4(0, 1.0, 0, 0) * CharacterScreenSize.y;

    // Determine the texture coordinates:
    unsigned int letter = character[0] - 32;
    unsigned int row = letter / 16;
    unsigned int col = letter % 16;

    float S0 = TextureGlyphSize.x * col;
    float T0 = TextureGlyphSize.y * row;
    float S1 = S0 + TextureGlyphSize.x;
    float T1 = T0 + TextureGlyphSize.y;

    // Output the quad's vertices:
    tex_coord = vec2(S0, T1); gl_Position = P-U-V; EmitVertex();
    tex_coord = vec2(S1, T1); gl_Position = P+U-V; EmitVertex();
    tex_coord = vec2(S0, T0); gl_Position = P-U+V; EmitVertex();
    tex_coord = vec2(S1, T0); gl_Position = P+U+V; EmitVertex();	
	
    EndPrimitive();
}

-- DebugFont.Fragment

out vec4 fragColour;
in vec2 tex_coord;

layout(binding=TEXT_TEX_UNIT) uniform sampler2D fontmap;
uniform vec3 CharacterColour = vec3(1,0,0);

void main()
{
    fragColour = vec4(CharacterColour, texture(fontmap, tex_coord).r);
}

-- TESTVERTEX
#version 420 core
layout(location = 0) in int inByte;
out vec3 colour;
void main()
{
	if(inByte==0){
		colour = vec3(1,0,0);
		gl_Position = vec4(-0.5, -0.5, 0 ,1.0);
	}else if(inByte==1){
		colour = vec3(0,1,0);
		gl_Position = vec4(0.5, -0.5, 0 ,1.0);
	}else{
		colour = vec3(0,0,1);
		gl_Position = vec4(0.0, 0.5, 0 ,1.0);
	}
}

-- TESTFRAGMENT
#version 420 core
in vec3 colour;
out vec4 fragColour;
void main()
{
    fragColour = vec4(colour, 1);
}
