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

#if IMAPP_ENABLED( IMAPP_DEBUG )
	ImUiHash					shaderHash;
#endif

	GLuint						vertexShader;
	ImAppRendererShader			shaderTexture;
	ImAppRendererShader			shaderColor;
	ImAppRendererShader			shaderFont;
	ImAppRendererShader			shaderFontSdf;
	GLint						programUniformProjection;
	GLint						programUniformTexture;
};

struct ImAppRendererTexture
{
	GLuint						handle;

	uint32						width;
	uint32						height;

	uint8						flags;
};

#if IMAPP_ENABLED( IMAPP_PLATFORM_LINUX ) || IMAPP_ENABLED( IMAPP_PLATFORM_ANDROID ) || IMAPP_ENABLED( IMAPP_PLATFORM_WEB )
#	define IMAPP_RENDERER_GLSL_VERSION "#version 300 es\n#line " IMAPP_STRINGIZE( __LINE__ ) "\n"
#else
#	define IMAPP_RENDERER_GLSL_VERSION "#version 330\n#line " IMAPP_STRINGIZE( (__LINE__ + 1) ) "\n"
#endif

static const char s_vertexShader[] =
	IMAPP_RENDERER_GLSL_VERSION
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

static const char s_fragmentShaderTexture[] =
	IMAPP_RENDERER_GLSL_VERSION
	"precision mediump float;\n"
	"uniform sampler2D Texture;\n"
	"in vec2 vtfUV;\n"
	"in vec4 vtfColor;\n"
	"out vec4 fbColor;\n"
	"void main() {\n"
	"	vec4 texColor = texture(Texture, vtfUV.xy);\n"
	"	fbColor = vtfColor * texColor;\n"
	"}\n";

static const char s_fragmentShaderColor[] =
	IMAPP_RENDERER_GLSL_VERSION
	"precision mediump float;\n"
	"in vec2 vtfUV;\n"
	"in vec4 vtfColor;\n"
	"out vec4 fbColor;\n"
	"void main() {\n"
	"	fbColor = vtfColor;\n"
	"}\n";

static const char s_fragmentShaderFont[] =
	IMAPP_RENDERER_GLSL_VERSION
	"precision mediump float;\n"
	"uniform sampler2D Texture;\n"
	"in vec2 vtfUV;\n"
	"in vec4 vtfColor;\n"
	"out vec4 fbColor;\n"
	"void main() {\n"
	"	float charAlpha = texture(Texture, vtfUV.xy).a;\n"
	"	fbColor = vec4(vtfColor.rgb, vtfColor.a * charAlpha);\n"
	"}\n";

static const char s_fragmentShaderFontSdf[] =
	IMAPP_RENDERER_GLSL_VERSION
	"precision mediump float;\n"
	"uniform sampler2D Texture;\n"
	"in vec2 vtfUV;\n"
	"in vec4 vtfColor;\n"
	"out vec4 fbColor;\n"
	"float median(vec3 v) {\n"
	"	return max(min(v.r, v.g), min(max(v.r, v.g), v.b));\n"
	"}\n"
	"float screenPixelRange() {\n"
	"	vec2 unitRange = vec2(2.0) / vec2(textureSize(Texture, 0));\n"
	"	vec2 screenTexSize = vec2(1.0) / fwidth(vtfUV);\n"
	"	return max( 0.5 * dot(unitRange, screenTexSize), 1.0 );\n"
	"}\n"
	"void main() {\n"
	"	vec3 charDistances = texture(Texture, vtfUV.xy).rgb;\n"
	"	float charDistanceMedian = median( charDistances );\n"
	"	float charDistance = screenPixelRange() * (charDistanceMedian - 0.5);\n"
	"	float charAlpha = clamp(charDistance + 0.5, 0.0, 1.0);\n"
	"	fbColor = vec4(vtfColor.rgb, vtfColor.a * charAlpha);\n"
	"}\n";

static const struct ImUiVertexElement s_vertexLayout[] = {
	{ 1u,	ImUiVertexElementType_Float2,	ImUiVertexElementSemantic_PositionScreenSpace },
	{ 1u,	ImUiVertexElementType_Float2,	ImUiVertexElementSemantic_TextureCoordinate },
	{ 1u,	ImUiVertexElementType_UInt,		ImUiVertexElementSemantic_ColorABGR },
};

#if IMAPP_ENABLED( IMAPP_DEBUG )
static ImUiHash	imappRendererGetShaderHash();
#endif

static bool		imappRendererCompileShader( GLuint shader, const char* shaderCode );
static bool		imappRendererCreateShaderProgram( ImAppRenderer* renderer, ImAppRendererShader* shader, const char* shaderCode );
static void		imappRendererDestroyShaderProgram( ImAppRenderer* renderer, ImAppRendererShader* shader );

static void		imappRendererDrawCommands( ImAppRenderer* renderer, ImAppRendererWindow* window, ImUiSurface* surface, int width, int height );

ImUiVertexFormat imappRendererGetVertexFormat()
{
	const ImUiVertexFormat result = { s_vertexLayout, IMAPP_ARRAY_COUNT( s_vertexLayout ) };
	return result;
}

ImAppRenderer* imappRendererCreate( ImUiAllocator* allocator, ImAppPlatform* platform )
{
	IMAPP_ASSERT( platform != NULL );

	ImAppRenderer* renderer = IMUI_MEMORY_NEW_ZERO( allocator, ImAppRenderer );
	if( renderer == NULL )
	{
		return NULL;
	}

	renderer->allocator = allocator;

#if IMAPP_ENABLED( IMAPP_PLATFORM_LINUX ) || IMAPP_ENABLED( IMAPP_PLATFORM_WEB ) || IMAPP_ENABLED( IMAPP_PLATFORM_WINDOWS )
	if( glewInit() != GLEW_OK )
	{
		imappPlatformShowError( platform, "Failed to initialize GLEW.\n" );
		imappRendererDestroy( renderer );
		return NULL;
	}
#endif

#if IMAPP_ENABLED( IMAPP_DEBUG )
	renderer->shaderHash = imappRendererGetShaderHash();
#endif
	if( !imappRendererCreateResources( renderer ) )
	{
		imappRendererDestroy( renderer );
		return NULL;
	}

	return renderer;
}

void imappRendererDestroy( ImAppRenderer* renderer )
{
	imappRendererDestroyResources( renderer );

	ImUiMemoryFree( renderer->allocator, renderer );
}

#if IMAPP_ENABLED( IMAPP_DEBUG )
static ImUiHash imappRendererGetShaderHash()
{
	ImUiHash shaderHash = 0;
	shaderHash = ImUiHashCreateSeed( s_vertexShader, sizeof( s_vertexShader ), shaderHash );
	shaderHash = ImUiHashCreateSeed( s_fragmentShaderTexture, sizeof( s_fragmentShaderTexture ), shaderHash );
	shaderHash = ImUiHashCreateSeed( s_fragmentShaderColor, sizeof( s_fragmentShaderColor ), shaderHash );
	shaderHash = ImUiHashCreateSeed( s_fragmentShaderFont, sizeof( s_fragmentShaderFont ), shaderHash );
	shaderHash = ImUiHashCreateSeed( s_fragmentShaderFontSdf, sizeof( s_fragmentShaderFontSdf ), shaderHash );

	return shaderHash;
}
#endif

void imappRendererUpdate( ImAppRenderer* renderer )
{
	IMAPP_USE( renderer );

#if IMAPP_ENABLED( IMAPP_DEBUG )
	const ImUiHash shaderHash = imappRendererGetShaderHash();
	if( shaderHash != renderer->shaderHash )
	{
		imappRendererDestroyResources( renderer );
		imappRendererCreateResources( renderer );

		renderer->shaderHash = shaderHash;
	}
#endif
}

static bool imappRendererCompileShader( GLuint shader, const char* pShaderCode )
{
	glShaderSource( shader, 1, &pShaderCode, 0 );
	glObjectLabel( GL_SHADER, shader, sizeof( __FILE__ ), __FILE__ );
	glCompileShader( shader );

	GLint shaderStatus;
	glGetShaderiv( shader, GL_COMPILE_STATUS, &shaderStatus );

	if( shaderStatus != GL_TRUE )
	{
		char buffer[ 2048u ];
		GLsizei infoLength;
		glGetShaderInfoLog( shader, 2048, &infoLength, buffer );

		ImAppTrace( "[renderer] Failed to compile Shader.\n" );
		ImAppTrace( "Code:\n" );

		const char* errorLine = buffer;
		const char* errorLineEnd = errorLine;
		while( *errorLine )
		{
			errorLineEnd = strchr( errorLine, '\n' );
			if( !errorLineEnd )
			{
				errorLineEnd = buffer + infoLength;
			}

			const uintsize errorLineLength = errorLineEnd - errorLine;
			if( *errorLine != '0' )
			{
				ImAppTrace( "%.*s\n", errorLineLength, errorLine );
			}
			else if( errorLineLength > 0 )
			{
				ImAppTrace( "%s%.*s\n", __FILE__, errorLineLength - 1, errorLine + 1 );
			}

			errorLine = errorLineEnd + 1;
		}

		return false;
	}

	return true;
}

static bool imappRendererCreateShaderProgram( ImAppRenderer* renderer, ImAppRendererShader* shader, const char* shaderCode )
{
	shader->fragmentShader = glCreateShader( GL_FRAGMENT_SHADER );
	if( !shader->fragmentShader )
	{
		ImAppTrace( "[renderer] Failed to create GL Shader.\n" );
		return false;
	}

	if( !imappRendererCompileShader( shader->fragmentShader, shaderCode ) )
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

static void imappRendererDestroyShaderProgram( ImAppRenderer* renderer, ImAppRendererShader* shader )
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

bool imappRendererCreateResources( ImAppRenderer* renderer )
{
	// Shader
	renderer->vertexShader = glCreateShader( GL_VERTEX_SHADER );
	if( !renderer->vertexShader ||
		!imappRendererCompileShader( renderer->vertexShader, s_vertexShader ) )
	{
		ImAppTrace( "[renderer] Failed to create Vertex Shader.\n" );
		return false;
	}

	if( !imappRendererCreateShaderProgram( renderer, &renderer->shaderTexture, s_fragmentShaderTexture ) ||
		!imappRendererCreateShaderProgram( renderer, &renderer->shaderColor, s_fragmentShaderColor ) ||
		!imappRendererCreateShaderProgram( renderer, &renderer->shaderFont, s_fragmentShaderFont ) ||
		!imappRendererCreateShaderProgram( renderer, &renderer->shaderFontSdf, s_fragmentShaderFontSdf ) )
	{
		ImAppTrace( "[renderer] Failed to compile programs.\n" );
		return false;
	}

	renderer->programUniformProjection	= glGetUniformLocation( renderer->shaderTexture.program, "ProjectionMatrix" );
	renderer->programUniformTexture		= glGetUniformLocation( renderer->shaderTexture.program, "Texture" );

	return true;
}

void imappRendererDestroyResources( ImAppRenderer* renderer )
{
	imappRendererDestroyShaderProgram( renderer, &renderer->shaderTexture );
	imappRendererDestroyShaderProgram( renderer, &renderer->shaderColor );
	imappRendererDestroyShaderProgram( renderer, &renderer->shaderFont );
	imappRendererDestroyShaderProgram( renderer, &renderer->shaderFontSdf );

	if( renderer->vertexShader != 0u )
	{
		glDeleteShader( renderer->vertexShader );
		renderer->vertexShader = 0u;
	}
}

void imappRendererConstructWindow( ImAppRenderer* renderer, ImAppRendererWindow* window )
{
	glGenBuffers( 1, &window->vertexBuffer );
	glGenBuffers( 1, &window->elementBuffer );
	glGenVertexArrays( 1, &window->vertexArray );

	glBindVertexArray( window->vertexArray );
	glBindBuffer( GL_ARRAY_BUFFER, window->vertexBuffer );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, window->elementBuffer );

	const GLuint attributePosition	= (GLuint)glGetAttribLocation( renderer->shaderTexture.program, "Position" );
	const GLuint attributeTexCoord	= (GLuint)glGetAttribLocation( renderer->shaderTexture.program, "TexCoord" );
	const GLuint attributeColor		= (GLuint)glGetAttribLocation( renderer->shaderTexture.program, "Color" );
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
}

void imappRendererDestructWindow( ImAppRenderer* renderer, ImAppRendererWindow* window )
{
	if( window->vertexArray != 0u )
	{
		glDeleteVertexArrays( 1, &window->vertexArray );
		window->vertexArray = 0u;
	}

	if( window->elementBuffer != 0u )
	{
		glDeleteBuffers( 1, &window->elementBuffer );
		window->elementBuffer = 0u;
	}

	if( window->vertexBuffer != 0u )
	{
		glDeleteBuffers( 1, &window->vertexBuffer );
		window->vertexBuffer = 0u;
	}

	ImUiMemoryFree( renderer->allocator, window->vertexBufferData );
	ImUiMemoryFree( renderer->allocator, window->elementBufferData );
}

ImAppRendererTexture* imappRendererTextureCreate( ImAppRenderer* renderer )
{
	return IMUI_MEMORY_NEW_ZERO( renderer->allocator, ImAppRendererTexture );
}

ImAppRendererTexture* imappRendererTextureCreateFromMemory( ImAppRenderer* renderer, const void* data, uint32_t width, uint32_t height, ImAppRendererFormat format, uint8_t flags )
{
	ImAppRendererTexture* texture = imappRendererTextureCreate( renderer );
	if( !texture )
	{
		return NULL;
	}

	if( !imappRendererTextureInitializeDataFromMemory( renderer, texture, data, width, height, format, flags ) )
	{
		imappRendererTextureDestroy( renderer, texture );
		return NULL;
	}

	return texture;
}

bool imappRendererTextureInitializeDataFromMemory( ImAppRenderer* renderer, ImAppRendererTexture* texture, const void* data, uint32_t width, uint32_t height, ImAppRendererFormat format, uint8_t flags )
{
	IMAPP_USE( renderer );

	if( texture == NULL )
	{
		return false;
	}

	texture->width		= width;
	texture->height		= height;
	texture->flags		= flags;

	GLenum sourceFormat = GL_RGBA;
	GLint targetFormat = GL_RGBA8;
	switch( format )
	{
	case ImAppRendererFormat_R8:
		sourceFormat	= GL_ALPHA;
		targetFormat	= GL_ALPHA8;
		break;

	case ImAppRendererFormat_RGB8:
		sourceFormat	= GL_RGB;
		targetFormat	= GL_RGB8;
		break;

	case ImAppRendererFormat_RGBA8:
		sourceFormat	= GL_RGBA;
		targetFormat	= GL_RGBA8;
		break;
	}

	glGenTextures( 1, &texture->handle );
	glBindTexture( GL_TEXTURE_2D, texture->handle );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

	const GLint wrapMode = flags & ImAppResPakTextureFlags_Repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapMode );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapMode );
	glTexImage2D( GL_TEXTURE_2D, 0, targetFormat, (GLsizei)width, (GLsizei)height, 0, sourceFormat, GL_UNSIGNED_BYTE, data );
	glBindTexture( GL_TEXTURE_2D, 0 );

	return true;
}

void imappRendererTextureDestroyData( ImAppRenderer* renderer, ImAppRendererTexture* texture )
{
	IMAPP_USE( renderer );

	if( texture->handle != 0u )
	{
		glDeleteTextures( 1u, &texture->handle );
		texture->handle = 0u;
	}
}

void imappRendererTextureDestroy( ImAppRenderer* renderer, ImAppRendererTexture* texture )
{
	if( texture == NULL )
	{
		return;
	}

	imappRendererTextureDestroyData( renderer, texture );

	ImUiMemoryFree( renderer->allocator, texture );
}

void imappRendererDraw( ImAppRenderer* renderer, ImAppRendererWindow* window, ImUiSurface* surface, int width, int height, float clearColor[ 4 ] )
{
	glViewport( 0, 0, width, height );

	glClearColor( clearColor[ 0 ], clearColor[ 1 ], clearColor[ 2 ], clearColor[ 3 ] );
	glClear( GL_COLOR_BUFFER_BIT );

	glEnable( GL_BLEND );
	glBlendEquation( GL_FUNC_ADD );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	glDisable( GL_CULL_FACE );
	glDisable( GL_DEPTH_TEST );
	glEnable( GL_SCISSOR_TEST );

	glUseProgram( renderer->shaderTexture.program );
	glUniform1i( renderer->programUniformTexture, 0 );

	imappRendererDrawCommands( renderer, window, surface, width, height );

	// reset OpenGL state
	glUseProgram( 0 );
	glBindBuffer( GL_ARRAY_BUFFER, 0 );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
	glBindVertexArray( 0 );
	glDisable( GL_BLEND );
	glDisable( GL_SCISSOR_TEST );
}

static void imappRendererDrawCommands( ImAppRenderer* renderer, ImAppRendererWindow* window, ImUiSurface* surface, int width, int height )
{
	width = width <= 0 ? 1 : width;
	height = height <= 0 ? 1 : height;

	// bind buffers
	glBindVertexArray( window->vertexArray );
	glBindBuffer( GL_ARRAY_BUFFER, window->vertexBuffer );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, window->elementBuffer );

	uintsize vertexDataSize = 0u;
	uintsize indexDataSize = 0u;
	ImUiSurfaceGetMaxBufferSizes( surface, &vertexDataSize, &indexDataSize );

	if( vertexDataSize > window->vertexBufferSize ||
		window->vertexBufferSize > vertexDataSize * 2 )
	{
		vertexDataSize = IMUI_NEXT_POWER_OF_TWO( vertexDataSize );
		window->vertexBufferData = ImUiMemoryRealloc( renderer->allocator, window->vertexBufferData, window->vertexBufferSize, vertexDataSize );
		window->vertexBufferSize = vertexDataSize;
	}

	if( indexDataSize > window->elementBufferSize ||
		window->elementBufferSize > indexDataSize * 2 )
	{
		indexDataSize = IMUI_NEXT_POWER_OF_TWO( indexDataSize );
		window->elementBufferData = ImUiMemoryRealloc( renderer->allocator, window->elementBufferData, window->elementBufferSize, indexDataSize );
		window->elementBufferSize = indexDataSize;
	}

	const ImUiDrawData* drawData = ImUiSurfaceGenerateDrawData( surface, window->vertexBufferData, &vertexDataSize, window->elementBufferData, &indexDataSize );
	glBufferData( GL_ARRAY_BUFFER, (GLsizeiptr)vertexDataSize, window->vertexBufferData, GL_DYNAMIC_DRAW );
	glBufferData( GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)indexDataSize, window->elementBufferData, GL_DYNAMIC_DRAW );

	const GLfloat projectionMatrix[ 4 ][ 4 ] = {
		{  2.0f / (float)width,	0.0f,					 0.0f,	0.0f },
		{  0.0f,				-2.0f / (float)height,	 0.0f,	0.0f },
		{  0.0f,				0.0f,					-1.0f,	0.0f },
		{ -1.0f,				1.0f,					 0.0f,	1.0f }
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

		ImAppRendererTexture* texture = (ImAppRendererTexture*)command->textureHandle;
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
