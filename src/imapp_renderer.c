#include "imapp_renderer.h"

#include "imapp_debug.h"
#include "imapp_internal.h"
#include "imapp_platform.h"

#include <string.h>

#if IMAPP_ENABLED( IMAPP_PLATFORM_WINDOWS )
#	include <GL/glew.h>
#elif IMAPP_ENABLED( IMAPP_PLATFORM_ANDROID )
#	include <GLES3/gl3.h>
#endif

struct ImAppRenderer
{
	ImUiAllocator*				allocator;
	ImAppWindow*				window;

	GLuint						vertexShader;
	GLuint						fragmentShader;
	GLuint						fragmentShaderColor;
	GLuint						fragmentShaderFont;

	GLuint						program;
	GLuint						programColor;
	GLuint						programFont;
	GLint						programUniformProjection;
	GLint						programUniformTexture;

	GLuint						vertexArray;
	GLuint						vertexBuffer;
	GLuint						elementBuffer;
};

struct ImAppRendererTexture
{
	GLuint						handle;

	int							width;
	int							height;

	ImAppRendererShading		shading;
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

static const char s_fragmentShader[] =
	"#version 100\n"
	"precision mediump float;\n"
	"uniform sampler2D Texture;\n"
	"varying vec2 vtfUV;\n"
	"varying vec4 vtfColor;\n"
	"void main(){\n"
	"	vec4 texColor = texture2D(Texture, vtfUV.xy);"
	"	gl_FragColor = vtfColor * texColor;\n"
	"}\n";

static const char s_fragmentShaderColor[] =
	"#version 100\n"
	"precision mediump float;\n"
	"varying vec2 vtfUV;\n"
	"varying vec4 vtfColor;\n"
	"void main(){\n"
	"	gl_FragColor = vtfColor;\n"
	"}\n";

static const char s_fragmentShaderFont[] =
	"#version 100\n"
	"precision mediump float;\n"
	"uniform sampler2D Texture;\n"
	"varying vec2 vtfUV;\n"
	"varying vec4 vtfColor;\n"
	"void main(){\n"
	"	float charColor = texture2D(Texture, vtfUV.xy).a;"
	"	gl_FragColor = vec4( vtfColor.rgb, vtfColor.a * charColor );\n"
	"}\n";

static const struct ImUiVertexElement s_vertexLayout[] = {
	{ 1u,	ImUiVertexElementType_Float2,	ImUiVertexElementSemantic_PositionScreenSpace },
	{ 1u,	ImUiVertexElementType_Float2,	ImUiVertexElementSemantic_TextureCoordinate },
	{ 1u,	ImUiVertexElementType_UInt,		ImUiVertexElementSemantic_ColorABGR },
};

static bool		ImAppRendererCompileShader( GLuint shader, const char* pShaderCode );

static bool		ImAppRendererCreateResources( ImAppRenderer* renderer );
static void		ImAppRendererDestroyResources( ImAppRenderer* renderer );

static void		ImAppRendererDrawCommands( ImAppRenderer* renderer, const ImUiDrawData* drawData, int width, int height );

ImUiVertexFormat ImAppRendererGetVertexFormat()
{
	const ImUiVertexFormat result = { s_vertexLayout, IMAPP_ARRAY_COUNT( s_vertexLayout ) };
	return result;
}

ImAppRenderer* ImAppRendererCreate( ImUiAllocator* allocator, ImAppPlatform* platform, ImAppWindow* window )
{
	IMAPP_ASSERT( platform != NULL );

	ImAppRenderer* renderer = IMUI_MEMORY_NEW_ZERO( allocator, ImAppRenderer );
	if( renderer == NULL )
	{
		return NULL;
	}

	renderer->allocator	= allocator;
	renderer->window	= window;

	if( !ImAppPlatformWindowCreateGlContext( window ) )
	{
		ImAppRendererDestroy( renderer );
		return NULL;
	}

#if IMAPP_ENABLED( IMAPP_PLATFORM_WINDOWS )
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

static bool ImAppRendererCreateResources( ImAppRenderer* renderer )
{
	// Shader
	renderer->vertexShader			= glCreateShader( GL_VERTEX_SHADER );
	renderer->fragmentShader		= glCreateShader( GL_FRAGMENT_SHADER );
	renderer->fragmentShaderColor	= glCreateShader( GL_FRAGMENT_SHADER );
	renderer->fragmentShaderFont	= glCreateShader( GL_FRAGMENT_SHADER );
	if( renderer->vertexShader == 0u ||
		renderer->fragmentShader == 0u ||
		renderer->fragmentShaderColor == 0u ||
		renderer->fragmentShaderFont == 0u )
	{
		ImAppTrace( "[renderer] Failed to create GL Shader.\n" );
		return false;
	}

	if( !ImAppRendererCompileShader( renderer->vertexShader, s_vertexShader ) ||
		!ImAppRendererCompileShader( renderer->fragmentShader, s_fragmentShader ) ||
		!ImAppRendererCompileShader( renderer->fragmentShaderColor, s_fragmentShaderColor ) ||
		!ImAppRendererCompileShader( renderer->fragmentShaderFont, s_fragmentShaderFont ) )
	{
		ImAppTrace( "[renderer] Failed to compile GL Shader.\n" );
		return false;
	}

	renderer->program = glCreateProgram();
	renderer->programColor = glCreateProgram();
	renderer->programFont = glCreateProgram();
	if( renderer->program == 0u ||
		renderer->programColor == 0u ||
		renderer->programFont == 0u )
	{
		ImAppTrace( "[renderer] Failed to create GL Program.\n" );
		return false;
	}

	glAttachShader( renderer->program, renderer->vertexShader );
	glAttachShader( renderer->program, renderer->fragmentShader );
	glLinkProgram( renderer->program );

	glAttachShader( renderer->programColor, renderer->vertexShader );
	glAttachShader( renderer->programColor, renderer->fragmentShaderColor );
	glLinkProgram( renderer->programColor );

	glAttachShader( renderer->programFont, renderer->vertexShader );
	glAttachShader( renderer->programFont, renderer->fragmentShaderFont );
	glLinkProgram( renderer->programFont );

	bool ok = true;
	GLint programStatus;
	glGetProgramiv( renderer->program, GL_LINK_STATUS, &programStatus );
	ok &= (programStatus == GL_TRUE);
	glGetProgramiv( renderer->programColor, GL_LINK_STATUS, &programStatus );
	ok &= (programStatus == GL_TRUE);
	glGetProgramiv( renderer->programFont, GL_LINK_STATUS, &programStatus );
	ok &= (programStatus == GL_TRUE);

	if( !ok )
	{
		ImAppTrace( "[renderer] Failed to link GL Program.\n" );
		return false;
	}

	renderer->programUniformProjection	= glGetUniformLocation( renderer->program, "ProjectionMatrix" );
	renderer->programUniformTexture		= glGetUniformLocation( renderer->program, "Texture" );

	// Buffer
	const GLuint attributePosition	= (GLuint)glGetAttribLocation( renderer->program, "Position" );
	const GLuint attributeTexCoord	= (GLuint)glGetAttribLocation( renderer->program, "TexCoord" );
	const GLuint attributeColor		= (GLuint)glGetAttribLocation( renderer->program, "Color" );

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

	if( renderer->program != 0u )
	{
		glDetachShader( renderer->program, renderer->fragmentShader );
		glDetachShader( renderer->program, renderer->vertexShader );
		glDeleteProgram( renderer->program );

		renderer->program = 0u;
	}

	if( renderer->vertexShader != 0u )
	{
		glDeleteShader( renderer->vertexShader );
		renderer->vertexShader = 0u;
	}

	if( renderer->fragmentShader != 0u )
	{
		glDeleteShader( renderer->fragmentShader );
		renderer->fragmentShader = 0u;
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

ImAppRendererTexture* ImAppRendererTextureCreateFromMemory( ImAppRenderer* renderer, const void* pData, int width, int height, ImAppRendererFormat format, ImAppRendererShading shading )
{
	ImAppRendererTexture* texture = IMUI_MEMORY_NEW_ZERO( renderer->allocator, ImAppRendererTexture );
	if( texture == NULL )
	{
		return NULL;
	}

	texture->width		= width;
	texture->height		= height;
	texture->shading	= shading;

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
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
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

void ImAppRendererDraw( ImAppRenderer* renderer, ImAppWindow* window, const ImUiDrawData* drawData )
{
	int width;
	int height;
	ImAppPlatformWindowGetSize( &width, &height, window );

	glViewport( 0, 0, width, height );

	const float color[ 4u ] = { 0.01f, 0.2f, 0.7f, 1.0f };
	glClearColor( color[ 0u ], color[ 1u ], color[ 2u ], color[ 3u ] );
	glClear( GL_COLOR_BUFFER_BIT );

	glEnable( GL_BLEND );
	glBlendEquation( GL_FUNC_ADD );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	glDisable( GL_CULL_FACE );
	glDisable( GL_DEPTH_TEST );
	glEnable( GL_SCISSOR_TEST );

	glUseProgram( renderer->program );
	glUniform1i( renderer->programUniformTexture, 0 );

	ImAppRendererDrawCommands( renderer, drawData, width, height );

	// reset OpenGL state
	glUseProgram( 0 );
	glBindBuffer( GL_ARRAY_BUFFER, 0 );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
	glBindVertexArray( 0 );
	glDisable( GL_BLEND );
	glDisable( GL_SCISSOR_TEST );
}

static void ImAppRendererDrawCommands( ImAppRenderer* renderer, const ImUiDrawData* drawData, int width, int height )
{
	// bind buffers
	glBindVertexArray( renderer->vertexArray );
	glBindBuffer( GL_ARRAY_BUFFER, renderer->vertexBuffer );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, renderer->elementBuffer );

	glBufferData( GL_ARRAY_BUFFER, drawData->vertexDataSize, NULL, GL_STREAM_DRAW );
	glBufferData( GL_ELEMENT_ARRAY_BUFFER, drawData->indexDataSize, NULL, GL_STREAM_DRAW);

	// upload
	{
		void* pVertexData = glMapBufferRange( GL_ARRAY_BUFFER, 0, drawData->vertexDataSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT ); // GL_MAP_UNSYNCHRONIZED_BIT
		void* pElementData = glMapBufferRange( GL_ELEMENT_ARRAY_BUFFER, 0, drawData->indexDataSize, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT );

		memcpy( pVertexData, drawData->vertexData, drawData->vertexDataSize );
		memcpy( pElementData, drawData->indexData, drawData->indexDataSize );

		glUnmapBuffer( GL_ARRAY_BUFFER );
		glUnmapBuffer( GL_ELEMENT_ARRAY_BUFFER );
	}

	const GLfloat projectionMatrix[ 4 ][ 4 ] ={
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
			programHandle	= renderer->programColor;
			textureHandle	= 0u;
		}
		else if( texture->shading == ImAppRendererShading_Opaque )
		{
			alphaBlend		= false;
			programHandle	= renderer->program;
			textureHandle	= texture->handle;
		}
		else if( texture->shading == ImAppRendererShading_Translucent )
		{
			alphaBlend		= true;
			programHandle	= renderer->program;
			textureHandle	= texture->handle;
		}
		else if( texture->shading == ImAppRendererShading_Font )
		{
			alphaBlend		= true;
			programHandle	= renderer->programFont;
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
