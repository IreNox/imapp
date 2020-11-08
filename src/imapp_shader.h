#pragma once

static const char s_vertexShader[] =
	"#version 150\n"
	"uniform mat4 ProjectionMatrix;\n"
	"in vec2 Position;\n"
	"in vec2 TexCoord;\n"
	"in vec4 Color;\n"
	"out vec2 vtfUV;\n"
	"out vec4 vtfColor;\n"
	"void main() {\n"
	"	vtfUV		= TexCoord;\n"
	"	vtfColor	= Color;\n"
	"	gl_Position	= ProjectionMatrix * vec4(Position.xy, 0, 1);\n"
	"}\n";

static const char s_fragmentShader[] =
	"#version 150\n"
	"precision mediump float;\n"
	"uniform sampler2D Texture;\n"
	"in vec2 vtfUV;\n"
	"in vec4 vtfColor;\n"
	"out vec4 OutColor;\n"
	"void main(){\n"
	"	vec4 sample = texture(Texture, vtfUV.xy);"
	"	OutColor = vtfColor * sample;\n"
	"}\n";
