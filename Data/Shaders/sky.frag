#version 330 core

in vec2 fragUv;

out vec4 color;

uniform sampler2D skyTexture;
uniform float timer; // time of day in [0, 1), shifted so the strip's columns line up

void main()
{
	// The sky strip's horizontal axis is the time of day; the vertical axis is the altitude.
	vec2 uv = vec2(timer, fragUv.t);
	color = texture(skyTexture, uv);
}