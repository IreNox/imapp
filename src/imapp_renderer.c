#include "imapp_renderer.h"

#include "imapp_debug.h"
#include "imapp_internal.h"
#include "imapp_platform.h"
#include "imapp_res_pak.h"

#include <string.h>

#if IMAPP_ENABLED( IMAPP_PLATFORM_WINDOWS )
#	include <GL/glew.h>
#elif IMAPP_ENABLED( IMAPP_PLATFORM_ANDROID )
#	include <GLES3/gl3.h>
#elif IMAPP_ENABLED( IMAPP_PLATFORM_WEB )
#	include <GL/glew.h>
#	include <GLES/gl.h>
#elif IMAPP_ENABLED( IMAPP_PLATFORM_LINUX )
#	include <GL/glew.h>
#	include <GL/gl.h>
#else
#	error "Platform not supported"
#endif

typedef struct ImAppRendererShader
{
	GLuint						fragmentShader;
	GLuint						program;
} ImAppRendererShader;

struct ImAppRenderer
{
	ImUiAllocator*				allocator;
	ImAppWindow*				window;

	float						clearColor[ 4u ];

	GLuint						vertexShader;
	ImAppRendererShader			shaderTexture;
	ImAppRendererShader			shaderColor;
	ImAppRendererShader			shaderFont;
	ImAppRendererShader			shaderFontSdf;
	GLint						programUniformProjection;
	GLint						programUniformTexture;

	GLuint						vertexArray;
	GLuint						vertexBuffer;
	uintsize					vertexBufferSize;
	void*						vertexBufferData;
	GLuint						elementBuffer;
	uintsize					elementBufferSize;
	void*						elementBufferData;
};

struct ImAppRendererTexture
{
	GLuint						handle;

	int							width;
	int							height;

	uint8						flags;
};

static const char s_vertexShader[] =
	"#version 100\n"
	"uniform mat4 ProjectionMatrix;\n"
	"attribute vec2 Position;\n"
	"attribute vec2 TexCoord;\n"
	"attribute vec4 Color;\n"
	"varying vec2 vtfUV;\n"
	"varying vec4 vtfColor;\n"
	"void main() {\n"
	"	vtfUV		= TexCoord;\n"
	"	vtfColor	= Color;\n"
	"	gl_Position	= ProjectionMatrix * vec4(Position.xy, 0, 1);\n"
	"}\n";

static const char s_fragmentShaderTexture[] =
	"#version 100\n"
	"precision mediump float;\n"
	"uniform sampler2D Texture;\n"
	"varying vec2 vtfUV;\n"
	"varying vec4 vtfColor;\n"
	"void main() {\n"
	"	vec4 texColor = texture2D(Texture, vtfUV.xy);\n"
	"	gl_FragColor = vtfColor * texColor;\n"
	"}\n";

static const char s_fragmentShaderColor[] =
	"#version 100\n"
	"precision mediump float;\n"
	"varying vec2 vtfUV;\n"
	"varying vec4 vtfColor;\n"
	"void main() {\n"
	"	gl_FragColor = vtfColor;\n"
	"}\n";

static const char s_fragmentShaderFont[] =
	"#version 100\n"
	"precision mediump float;\n"
	"uniform sampler2D Texture;\n"
	"varying vec2 vtfUV;\n"
	"varying vec4 vtfColor;\n"
	"void main() {\n"
	"	float charAlpha = texture2D(Texture, vtfUV.xy).a;\n"
	"	gl_FragColor = vec4( vtfColor.rgb, vtfColor.a * charAlpha );\n"
	"}\n";

static const char s_fragmentShaderFontSdf[] =
	"#version 100\n"
	"precision mediump float;\n"
	"uniform sampler2D Texture;\n"
	"varying vec2 vtfUV;\n"
	"varying vec4 vtfColor;\n"
	"float median( vec3 v ) {\n"
	"	return max( min( v.r, v.g ), min( max( v.r, v.g ), v.b ) );\n"
	"}\n"
	"void main() {\n"
	"	vec3 charDistances = texture2D(Texture, vtfUV.xy).rgb;\n"
	"	float charDistanceMedian = median( charDistances );\n"
	"	float charDistance = 2.0 * (charDistanceMedian - 0.5);\n"
	"	float charAlpha = clamp( charDistance + 0.5, 0.0, 1.0 );\n"
	"	gl_FragColor = vec4( vtfColor.rgb, vtfColor.a * charAlpha );\n"
	"}\n";

static const struct ImUiVertexElement s_vertexLayout[] = {
	{ 1u,	ImUiVertexElementType_Float2,	ImUiVertexElementSemantic_PositionScreenSpace },
	{ 1u,	ImUiVertexElementType_Float2,	ImUiVertexElementSemantic_TextureCoordinate },
	{ 1u,	ImUiVertexElementType_UInt,		ImUiVertexElementSemantic_ColorABGR },
};

static bool		ImAppRendererCompileShader( GLuint shader, const char* shaderCode );
static bool		ImAppRendererCreateShaderProgram( ImAppRenderer* renderer, ImAppRendererShader* shader, const char* shaderCode );
static void		ImAppRendererDestroyShaderProgram( ImAppRenderer* renderer, ImAppRendererShader* shader );

static bool		ImAppRendererCreateResources( ImAppRenderer* renderer );
static void		ImAppRendererDestroyResources( ImAppRenderer* renderer );

static void		ImAppRendererDrawCommands( ImAppRenderer* renderer, ImUiSurface* surface, int width, int height );

ImUiVertexFormat ImAppRendererGetVertexFormat()
{
	const ImUiVertexFormat result = { s_vertexLayout, IMAPP_ARRAY_COUNT( s_vertexLayout ) };
	return result;
}

ImAppRenderer* ImAppRendererCreate( ImUiAllocator* allocator, ImAppPlatform* platform, ImAppWindow* window, ImUiColor clearColor )
{
	IMAPP_ASSERT( platform != NULL );

	ImAppRenderer* renderer = IMUI_MEMORY_NEW_ZERO( allocator, ImAppRenderer );
	if( renderer == NULL )
	{
		return NULL;
	}

	renderer->allocator			= allocator;
	renderer->window			= window;
	renderer->clearColor[ 0u ]	= (float)clearColor.red / 255.0f;
	renderer->clearColor[ 1u ]	= (float)clearColor.green / 255.0f;
	renderer->clearColor[ 2u ]	= (float)clearColor.blue / 255.0f;
	renderer->clearColor[ 3u ]	= (float)clearColor.alpha / 255.0f;

	if( !ImAppPlatformWindowCreateGlContext( window ) )
	{
		ImAppRendererDestroy( renderer );
		return NULL;
	}

#if IMAPP_ENABLED( IMAPP_PLATFORM_LINUX ) || IMAPP_ENABLED( IMAPP_PLATFORM_WEB ) || IMAPP_ENABLED( IMAPP_PLATFORM_WINDOWS )
	if( glewInit() != GLEW_OK )
	{
		ImAppPlatformShowError( platform, "Failed to initialize GLEW.\n" );
		return NULL;
	}
#endif

	if( !ImAppRendererCreateResources( renderer ) )
	{
		ImAppRendererDestroy( renderer );
		return NULL;
	}

	return renderer;
}

void ImAppRendererDestroy( ImAppRenderer* renderer )
{
	ImAppRendererDestroyResources( renderer );
	ImAppPlatformWindowDestroyGlContext( renderer->window );

	ImUiMemoryFree( renderer->allocator, renderer->vertexBufferData );
	ImUiMemoryFree( renderer->allocator, renderer->elementBufferData );

	ImUiMemoryFree( renderer->allocator, renderer );
}

static bool ImAppRendererCompileShader( GLuint shader, const char* pShaderCode )
{
	glShaderSource( shader, 1, &pShaderCode, 0 );
	glCompileShader( shader );

	GLint shaderStatus;
	glGetShaderiv( shader, GL_COMPILE_STATUS, &shaderStatus );

	if( shaderStatus != GL_TRUE )
	{
		char buffer[ 2048u ];
		GLsizei infoLength;
		glGetShaderInfoLog( shader, 2048, &infoLength, buffer );

		ImAppTrace( "[renderer] Failed to compile Shader. Error: %s\n", buffer );
		return false;
	}

	return true;
}

static bool ImAppRendererCreateShaderProgram( ImAppRenderer* renderer, ImAppRendererShader* shader, const char* shaderCode )
{
	shader->fragmentShader = glCreateShader( GL_FRAGMENT_SHADER );
	if( !shader->fragmentShader )
	{
		ImAppTrace( "[renderer] Failed to create GL Shader.\n" );
		return false;
	}

	if( !ImAppRendererCompileShader( shader->fragmentShader, shaderCode ) )
	{
		ImAppTrace( "[renderer] Failed to compile GL Shader.\n" );
		return false;
	}

	shader->program = glCreateProgram();
	if( !shader->program )
	{
		ImAppTrace( "[renderer] Failed to create GL Program.\n" );
		return false;
	}

	glAttachShader( shader->program, renderer->vertexShader );
	glAttachShader( shader->program, shader->fragmentShader );
	glLinkProgram( shader->program );

	GLint programStatus;
	glGetProgramiv( shader->program, GL_LINK_STATUS, &programStatus );
	if( programStatus != GL_TRUE )
	{
		ImAppTrace( "[renderer] Failed to link GL Program.\n" );
		return false;
	}

	return true;
}

static void ImAppRendererDestroyShaderProgram( ImAppRenderer* renderer, ImAppRendererShader* shader )
{
	if( shader->program != 0u )
	{
		glDetachShader( shader->program, shader->fragmentShader );
		glDetachShader( shader->program, renderer->vertexShader );
		glDeleteProgram( shader->program );

		shader->program = 0u;
	}

	if( shader->fragmentShader != 0u )
	{
		glDeleteShader( shader->fragmentShader );
		shader->fragmentShader = 0u;
	}
}

static bool ImAppRendererCreateResources( ImAppRenderer* renderer )
{
	// Shader
	renderer->vertexShader = glCreateShader( GL_VERTEX_SHADER );
	if( !renderer->vertexShader ||
		!ImAppRendererCompileShader( renderer->vertexShader, s_vertexShader ) )
	{
		ImAppTrace( "[renderer] Failed to create Vertex Shader.\n" );
		return false;
	}

	if( !ImAppRendererCreateShaderProgram( renderer, &renderer->shaderTexture, s_fragmentShaderTexture ) ||
		!ImAppRendererCreateShaderProgram( renderer, &renderer->shaderColor, s_fragmentShaderColor ) ||
		!ImAppRendererCreateShaderProgram( renderer, &renderer->shaderFont, s_fragmentShaderFont ) ||
		!ImAppRendererCreateShaderProgram( renderer, &renderer->shaderFontSdf, s_fragmentShaderFontSdf ) )
	{
		ImAppTrace( "[renderer] Failed to compile programs.\n" );
		return false;
	}

	renderer->programUniformProjection	= glGetUniformLocation( renderer->shaderTexture.program, "ProjectionMatrix" );
	renderer->programUniformTexture		= glGetUniformLocation( renderer->shaderTexture.program, "Texture" );

	// Buffer
	const GLuint attributePosition	= (GLuint)glGetAttribLocation( renderer->shaderTexture.program, "Position" );
	const GLuint attributeTexCoord	= (GLuint)glGetAttribLocation( renderer->shaderTexture.program, "TexCoord" );
	const GLuint attributeColor		= (GLuint)glGetAttribLocation( renderer->shaderTexture.program, "Color" );

	glGenBuffers( 1, &renderer->vertexBuffer );
	glGenBuffers( 1, &renderer->elementBuffer );
	glGenVertexArrays( 1, &renderer->vertexArray );

	glBindVertexArray( renderer->vertexArray );
	glBindBuffer( GL_ARRAY_BUFFER, renderer->vertexBuffer );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, renderer->elementBuffer );

	glEnableVertexAttribArray( attributePosition );
	glEnableVertexAttribArray( attributeTexCoord );
	glEnableVertexAttribArray( attributeColor );

	const GLsizei vertexSize	= 20u;
	size_t vertexPositionOffset	= 0u;
	size_t vertexUvOffset		= 8u;
	size_t vertexColorOffset	= 16u;
	glVertexAttribPointer( attributePosition, 2, GL_FLOAT, GL_FALSE, vertexSize, (void*)vertexPositionOffset );
	glVertexAttribPointer( attributeTexCoord, 2, GL_FLOAT, GL_FALSE, vertexSize, (void*)vertexUvOffset );
	glVertexAttribPointer( attributeColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, vertexSize, (void*)vertexColorOffset );

	return true;
}

static void ImAppRendererDestroyResources( ImAppRenderer* renderer )
{
	if( renderer->vertexArray != 0u )
	{
		glDeleteVertexArrays( 1, &renderer->vertexArray );
		renderer->vertexArray = 0u;
	}

	if( renderer->elementBuffer != 0u )
	{
		glDeleteBuffers( 1, &renderer->elementBuffer );
		renderer->elementBuffer = 0u;
	}

	if( renderer->vertexBuffer != 0u )
	{
		glDeleteBuffers( 1, &renderer->vertexBuffer );
		renderer->vertexBuffer = 0u;
	}

	ImAppRendererDestroyShaderProgram( renderer, &renderer->shaderTexture );
	ImAppRendererDestroyShaderProgram( renderer, &renderer->shaderColor );
	ImAppRendererDestroyShaderProgram( renderer, &renderer->shaderFont );
	ImAppRendererDestroyShaderProgram( renderer, &renderer->shaderFontSdf );

	if( renderer->vertexShader != 0u )
	{
		glDeleteShader( renderer->vertexShader );
		renderer->vertexShader = 0u;
	}
}

bool ImAppRendererRecreateResources( ImAppRenderer* renderer )
{
	ImAppRendererDestroyResources( renderer );

	if( !ImAppRendererCreateResources( renderer ) )
	{
		return false;
	}

	return true;
}

ImAppRendererTexture* ImAppRendererTextureCreateFromMemory( ImAppRenderer* renderer, const void* pData, int width, int height, ImAppRendererFormat format, uint8_t flags )
{
	ImAppRendererTexture* texture = IMUI_MEMORY_NEW_ZERO( renderer->allocator, ImAppRendererTexture );
	if( texture == NULL )
	{
		return NULL;
	}

	texture->width		= width;
	texture->height		= height;
	texture->flags		= flags;

	GLint sourceFormat = GL_RGBA;
	GLint targetFormat = GL_RGBA;
	switch( format )
	{
	case ImAppRendererFormat_R8:
		sourceFormat	= GL_ALPHA;
		targetFormat	= GL_ALPHA;
		break;

	case ImAppRendererFormat_RGB8:
		sourceFormat	= GL_RGB;
		targetFormat	= GL_RGB;
		break;

	case ImAppRendererFormat_RGBA8:
		sourceFormat	= GL_RGBA;
		targetFormat	= GL_RGBA;
		break;
	}

	glGenTextures( 1, &texture->handle );
	glBindTexture( GL_TEXTURE_2D, texture->handle );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

	const GLint wrapMode = flags & ImAppResPakTextureFlags_Repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapMode );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapMode );
	glTexImage2D( GL_TEXTURE_2D, 0, targetFormat, (GLsizei)width, (GLsizei)height, 0, sourceFormat, GL_UNSIGNED_BYTE, pData );
	glBindTexture( GL_TEXTURE_2D, 0 );

	return texture;
}

void ImAppRendererTextureDestroy( ImAppRenderer* renderer, ImAppRendererTexture* texture )
{
	if( texture == NULL )
	{
		return;
	}

	if( texture->handle != 0u )
	{
		glDeleteTextures( 1u, &texture->handle );
		texture->handle = 0u;
	}

	ImUiMemoryFree( renderer->allocator, texture );
}

void ImAppRendererDraw( ImAppRenderer* renderer, ImAppWindow* window, ImUiSurface* surface )
{
	int width;
	int height;
	ImAppPlatformWindowGetSize( window, &width, &height );

	glViewport( 0, 0, width, height );

	glClearColor( renderer->clearColor[ 0u ], renderer->clearColor[ 1u ], renderer->clearColor[ 2u ], renderer->clearColor[ 3u ] );
	glClear( GL_COLOR_BUFFER_BIT );

	glEnable( GL_BLEND );
	glBlendEquation( GL_FUNC_ADD );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	glDisable( GL_CULL_FACE );
	glDisable( GL_DEPTH_TEST );
	glEnable( GL_SCISSOR_TEST );

	glUseProgram( renderer->shaderTexture.program );
	glUniform1i( renderer->programUniformTexture, 0 );

	ImAppRendererDrawCommands( renderer, surface, width, height );

	// reset OpenGL state
	glUseProgram( 0 );
	glBindBuffer( GL_ARRAY_BUFFER, 0 );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
	glBindVertexArray( 0 );
	glDisable( GL_BLEND );
	glDisable( GL_SCISSOR_TEST );
}

static void ImAppRendererDrawCommands( ImAppRenderer* renderer, ImUiSurface* surface, int width, int height )
{
	width = width <= 0 ? 1 : width;
	height = height <= 0 ? 1 : height;

	// bind buffers
	glBindVertexArray( renderer->vertexArray );
	glBindBuffer( GL_ARRAY_BUFFER, renderer->vertexBuffer );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, renderer->elementBuffer );

	uintsize vertexDataSize = 0u;
	uintsize indexDataSize = 0u;
	ImUiSurfaceGetMaxBufferSizes( surface, &vertexDataSize, &indexDataSize );

	if( vertexDataSize > renderer->vertexBufferSize )
	{
		renderer->vertexBufferData = ImUiMemoryRealloc( renderer->allocator, renderer->vertexBufferData, renderer->vertexBufferSize, vertexDataSize );
		renderer->vertexBufferSize = vertexDataSize;
	}

	if( indexDataSize > renderer->elementBufferSize )
	{
		renderer->elementBufferData = ImUiMemoryRealloc( renderer->allocator, renderer->elementBufferData, renderer->elementBufferSize, indexDataSize );
		renderer->elementBufferSize = indexDataSize;
	}

	const ImUiDrawData* drawData = ImUiSurfaceGenerateDrawData( surface, renderer->vertexBufferData, &vertexDataSize, renderer->elementBufferData, &indexDataSize );
	glBufferData( GL_ARRAY_BUFFER, vertexDataSize, renderer->vertexBufferData, GL_STREAM_DRAW );
	glBufferData( GL_ELEMENT_ARRAY_BUFFER, indexDataSize, renderer->elementBufferData, GL_STREAM_DRAW );

	const GLfloat projectionMatrix[ 4 ][ 4 ] = {
		{  2.0f / width,	0.0f,			 0.0f,	0.0f },
		{  0.0f,			-2.0f / height,	 0.0f,	0.0f },
		{  0.0f,			0.0f,			-1.0f,	0.0f },
		{ -1.0f,			1.0f,			 0.0f,	1.0f }
	};

	bool alphaBlend = true;
	GLuint programHandle = 0;
	GLuint textureHandle = 0;

	bool lastAlphaBlend = true;
	GLuint lastProgramHandle = 0;

	const uint32_t* elementOffset = NULL;
	for( size_t i = 0u; i < drawData->commandCount; ++i )
	{
		const ImUiDrawCommand* command = &drawData->commands[ i ];

		ImAppRendererTexture* texture = (ImAppRendererTexture*)command->texture;
		if( texture == NULL )
		{
			alphaBlend		= true;
			programHandle	= renderer->shaderColor.program;
			textureHandle	= 0u;
		}
		else if( texture->flags & ImAppResPakTextureFlags_Font )
		{
			alphaBlend		= true;
			programHandle	= renderer->shaderFont.program;
			textureHandle	= texture->handle;
		}
		else if( texture->flags & ImAppResPakTextureFlags_FontSdf )
		{
			alphaBlend		= true;
			programHandle	= renderer->shaderFontSdf.program;
			textureHandle	= texture->handle;
		}
		else if( texture->flags & ImAppResPakTextureFlags_Opaque )
		{
			alphaBlend		= false;
			programHandle	= renderer->shaderTexture.program;
			textureHandle	= texture->handle;
		}
		else
		{
			alphaBlend		= true;
			programHandle	= renderer->shaderTexture.program;
			textureHandle	= texture->handle;
		}

		if( lastProgramHandle != programHandle )
		{
			glUseProgram( programHandle );
			glUniformMatrix4fv( renderer->programUniformProjection, 1, GL_FALSE, &projectionMatrix[ 0u ][ 0u ] );

			lastProgramHandle = programHandle;
		}

		if( lastAlphaBlend != alphaBlend )
		{
			(alphaBlend ? glEnable : glDisable)( GL_BLEND );

			lastAlphaBlend = alphaBlend;
		}

		glBindTexture( GL_TEXTURE_2D, textureHandle );

		glScissor(
			(GLint)(command->clipRect.pos.x),
			(GLint)((height - (GLint)(command->clipRect.pos.y + command->clipRect.size.height))),
			(GLint)(command->clipRect.size.width),
			(GLint)(command->clipRect.size.height)
		);

		const GLenum topology = (command->topology == ImUiDrawTopology_LineList ? GL_LINES : GL_TRIANGLES);
		glDrawElements( topology, (GLsizei)command->count, GL_UNSIGNED_INT, elementOffset );
		elementOffset += command->count;
	}
}
