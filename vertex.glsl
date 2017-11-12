#version 120

uniform vec2 screen_size;

void main()
{
	// Scale screenspace coords to [-1.0, 1.0] transform.
	vec4 scale = vec4(screen_size, 1.0, 1.0);
	gl_Position = (gl_Vertex * scale * 2.0) - 1.0;
}
