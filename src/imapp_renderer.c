#include "imapp_renderer.h"

#include "imapp_memory.h"

#include <stdint.h>

#if IMAPP_ENABLED( IMAPP_PLATFORM_WINDOWS )
#	include <GL/glew.h>
#elif IMAPP_ENABLED( IMAPP_PLATFORM_ANDROID )
#	include <GLES3/gl3.h>
#endif

struct ImAppRenderer
{
	ImAppAllocator*				pAllocator;

	GLuint						vertexShader;
	GLuint						fragmentShader;

	GLuint						program;
	GLint						programUniformProjection;
	GLint						programUniformTexture;

	GLuint						vertexArray;
	GLuint						vertexBuffer;
	GLuint						elementBuffer;

	bool						nkCreated;
	struct nk_font_atlas		nkFontAtlas;
	struct nk_convert_config	nkConvertConfig;
	struct nk_buffer			nkCommands;

	ImAppRendererTexture*		pFirstTexture;
};

struct ImAppRendererTexture
{
	ImAppRendererTexture*		pNext;

	GLuint						texture;

	int							width;
	int							height;

	void*						pData;
	size_t						size;
};

struct ImAppRendererVertex
{
	float						position[ 2u ];
	float						uv[ 2u ];
	uint8_t						color[ 4u ];
};
typedef struct ImAppRendererVertex ImAppRendererVertex;

#if IMAPP_ENABLED( IMAPP_PLATFORM_ANDROID )
#	define IMAPP_GLSL_VERSION "#version 300 es\n"
#else
#	define IMAPP_GLSL_VERSION "#version 150\n"
#endif

static const char s_vertexShader[] =
	IMAPP_GLSL_VERSION
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
	IMAPP_GLSL_VERSION
	"precision mediump float;\n"
	"uniform sampler2D Texture;\n"
	"in vec2 vtfUV;\n"
	"in vec4 vtfColor;\n"
	"out vec4 OutColor;\n"
	"void main(){\n"
	"	vec4 texColor = texture(Texture, vtfUV.xy);"
	"	OutColor = vtfColor * texColor;\n"
	"}\n";

static const struct nk_draw_vertex_layout_element s_aVertexLayout[] = {
	{ NK_VERTEX_POSITION,	NK_FORMAT_FLOAT,	IMAPP_OFFSETOF( ImAppRendererVertex, position ) },
	{ NK_VERTEX_TEXCOORD,	NK_FORMAT_FLOAT,	IMAPP_OFFSETOF( ImAppRendererVertex, uv ) },
	{ NK_VERTEX_COLOR,		NK_FORMAT_R8G8B8A8,	IMAPP_OFFSETOF( ImAppRendererVertex, color ) },
	{ NK_VERTEX_LAYOUT_END }
};

#define IMAPP_MAX_VERTEX_COUNT	8u * 1024u
#define IMAPP_MAX_ELEMENT_COUNT	16u * 1024u
#define IMAPP_MAX_VERTEX_SIZE	sizeof( ImAppRendererVertex ) * IMAPP_MAX_VERTEX_COUNT
#define IMAPP_MAX_ELEMENT_SIZE	sizeof( nk_draw_index ) * IMAPP_MAX_ELEMENT_COUNT

static bool		ImAppRendererCompileShader( GLuint shader, const char* pShaderCode );
static GLuint	ImAppRendererCreateTexture( const void* pData, int width, int height );

static bool		ImAppRendererCreateResources( ImAppRenderer* pRenderer );
static void		ImAppRendererDestroyResources( ImAppRenderer* pRenderer );

static void		ImAppRendererDrawNuklear( ImAppRenderer* pRenderer, struct nk_context* pNkContext, int height );

ImAppRenderer* ImAppRendererCreate( ImAppAllocator* pAllocator, ImAppPlatform* pPlatform )
{
	IMAPP_ASSERT( pPlatform != NULL );

	ImAppRenderer* pRenderer = IMAPP_NEW_ZERO( pAllocator, ImAppRenderer );
	if( pRenderer == NULL )
	{
		return NULL;
	}

	pRenderer->pAllocator = pAllocator;

#if IMAPP_ENABLED( IMAPP_PLATFORM_WINDOWS )
	if( glewInit() != GLEW_OK )
	{
		ImAppShowError( pPlatform, "Failed to initialize GLEW.\n" );
		return NULL;
	}
#endif

	if( !ImAppRendererCreateResources( pRenderer ) )
	{
		ImAppRendererDestroy( pRenderer );
		return NULL;
	}

	nk_font_atlas_init_default( &pRenderer->nkFontAtlas );
	nk_buffer_init_default( &pRenderer->nkCommands );

	pRenderer->nkConvertConfig.vertex_layout		= s_aVertexLayout;
	pRenderer->nkConvertConfig.vertex_size			= sizeof( ImAppRendererVertex );
	pRenderer->nkConvertConfig.vertex_alignment		= NK_ALIGNOF( ImAppRendererVertex );
	pRenderer->nkConvertConfig.circle_segment_count	= 22;
	pRenderer->nkConvertConfig.curve_segment_count	= 22;
	pRenderer->nkConvertConfig.arc_segment_count	= 22;
	pRenderer->nkConvertConfig.global_alpha			= 1.0f;
	pRenderer->nkConvertConfig.shape_AA				= NK_ANTI_ALIASING_ON;
	pRenderer->nkConvertConfig.line_AA				= NK_ANTI_ALIASING_ON;

	pRenderer->nkCreated							= true;

	return pRenderer;
}

void ImAppRendererDestroy( ImAppRenderer* pRenderer )
{
	if( pRenderer->nkCreated )
	{
		nk_font_atlas_cleanup( &pRenderer->nkFontAtlas );
		nk_buffer_free( &pRenderer->nkCommands );
	}

	while( pRenderer->pFirstTexture != NULL )
	{
		ImAppRendererTexture* pTexture = pRenderer->pFirstTexture;
		pRenderer->pFirstTexture = pTexture->pNext;

		ImAppRendererTextureDestroy( pRenderer, pTexture );
	}

	ImAppRendererDestroyResources( pRenderer );

	ImAppFree( pRenderer->pAllocator, pRenderer );
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

static bool ImAppRendererCreateResources( ImAppRenderer* pRenderer )
{
	// Shader
	pRenderer->vertexShader		= glCreateShader( GL_VERTEX_SHADER );
	pRenderer->fragmentShader	= glCreateShader( GL_FRAGMENT_SHADER );
	if( pRenderer->vertexShader == 0u ||
		pRenderer->fragmentShader == 0u )
	{
		ImAppTrace( "[renderer] Failed to create GL Shader.\n" );
		return false;
	}

	if( !ImAppRendererCompileShader( pRenderer->vertexShader, s_vertexShader ) ||
		!ImAppRendererCompileShader( pRenderer->fragmentShader, s_fragmentShader ) )
	{
		ImAppTrace( "[renderer] Failed to compile GL Shader.\n" );
		return false;
	}

	pRenderer->program = glCreateProgram();
	if( pRenderer->program == 0u )
	{
		ImAppTrace( "[renderer] Failed to create GL Program.\n" );
		return false;
	}

	GLint programStatus;
	glAttachShader( pRenderer->program, pRenderer->vertexShader );
	glAttachShader( pRenderer->program, pRenderer->fragmentShader );
	glLinkProgram( pRenderer->program );
	glGetProgramiv( pRenderer->program, GL_LINK_STATUS, &programStatus );

	if( programStatus != GL_TRUE )
	{
		ImAppTrace( "[renderer] Failed to link GL Program.\n" );
		return false;
	}

	pRenderer->programUniformProjection	= glGetUniformLocation( pRenderer->program, "ProjectionMatrix" );
	pRenderer->programUniformTexture	= glGetUniformLocation( pRenderer->program, "Texture" );

	// Buffer
	const GLuint attributePosition	= (GLuint)glGetAttribLocation( pRenderer->program, "Position" );
	const GLuint attributeTexCoord	= (GLuint)glGetAttribLocation( pRenderer->program, "TexCoord" );
	const GLuint attributeColor		= (GLuint)glGetAttribLocation( pRenderer->program, "Color" );

	glGenBuffers( 1, &pRenderer->vertexBuffer );
	glGenBuffers( 1, &pRenderer->elementBuffer );
	glGenVertexArrays( 1, &pRenderer->vertexArray );

	glBindVertexArray( pRenderer->vertexArray );
	glBindBuffer( GL_ARRAY_BUFFER, pRenderer->vertexBuffer );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, pRenderer->elementBuffer );

	glEnableVertexAttribArray( attributePosition );
	glEnableVertexAttribArray( attributeTexCoord );
	glEnableVertexAttribArray( attributeColor );

	const GLsizei vertexSize	= sizeof( ImAppRendererVertex );
	size_t vertexPositionOffset	= IMAPP_OFFSETOF( ImAppRendererVertex, position );
	size_t vertexUvOffset		= IMAPP_OFFSETOF( ImAppRendererVertex, uv );
	size_t vertexColorOffset	= IMAPP_OFFSETOF( ImAppRendererVertex, color );
	glVertexAttribPointer( attributePosition, 2, GL_FLOAT, GL_FALSE, vertexSize, (void*)vertexPositionOffset );
	glVertexAttribPointer( attributeTexCoord, 2, GL_FLOAT, GL_FALSE, vertexSize, (void*)vertexUvOffset );
	glVertexAttribPointer( attributeColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, vertexSize, (void*)vertexColorOffset );

	return true;
}

static void ImAppRendererDestroyResources( ImAppRenderer* pRenderer )
{
	if( pRenderer->vertexArray != 0u )
	{
		glDeleteVertexArrays( 1, &pRenderer->vertexArray );
		pRenderer->vertexArray = 0u;
	}

	if( pRenderer->elementBuffer != 0u )
	{
		glDeleteBuffers( 1, &pRenderer->elementBuffer );
		pRenderer->elementBuffer = 0u;
	}

	if( pRenderer->vertexBuffer != 0u )
	{
		glDeleteBuffers( 1, &pRenderer->vertexBuffer );
		pRenderer->vertexBuffer = 0u;
	}

	if( pRenderer->program != 0u )
	{
		glDetachShader( pRenderer->program, pRenderer->fragmentShader );
		glDetachShader( pRenderer->program, pRenderer->vertexShader );
		glDeleteProgram( pRenderer->program );

		pRenderer->program = 0u;
	}

	if( pRenderer->vertexShader != 0u )
	{
		glDeleteShader( pRenderer->vertexShader );
		pRenderer->vertexShader = 0u;
	}

	if( pRenderer->fragmentShader != 0u )
	{
		glDeleteShader( pRenderer->fragmentShader );
		pRenderer->fragmentShader = 0u;
	}
}

bool ImAppRendererRecreateResources( ImAppRenderer* pRenderer )
{
	ImAppRendererDestroyResources( pRenderer );

	if( !ImAppRendererCreateResources( pRenderer ) )
	{
		return false;
	}

	for( ImAppRendererTexture* pTexture = pRenderer->pFirstTexture; pTexture != NULL; pTexture = pTexture->pNext )
	{
		glDeleteTextures( 1u, &pTexture->texture );
		pTexture->texture = ImAppRendererCreateTexture( pTexture->pData, pTexture->width, pTexture->height );
	}

	return true;
}

struct nk_font* ImAppRendererCreateDefaultFont( ImAppRenderer* pRenderer, struct nk_context* pNkContext )
{
	nk_font_atlas_begin( &pRenderer->nkFontAtlas );

	struct nk_font* pFont = nk_font_atlas_add_default( &pRenderer->nkFontAtlas, 13, 0 );

	int imageWidth;
	int imageHeight;
	const void* pImageData = nk_font_atlas_bake( &pRenderer->nkFontAtlas, &imageWidth, &imageHeight, NK_FONT_ATLAS_RGBA32 );
	ImAppRendererTexture* pTexture = ImAppRendererTextureCreateFromMemory( pRenderer, pImageData, imageWidth, imageHeight );
	nk_font_atlas_end( &pRenderer->nkFontAtlas, nk_handle_ptr( pTexture ), &pRenderer->nkConvertConfig.null );

	return pFont;
}

static GLuint ImAppRendererCreateTexture( const void* pData, int width, int height )
{
	GLuint texture = 0;
	glGenTextures( 1, &texture );
	glBindTexture( GL_TEXTURE_2D, texture );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)width, (GLsizei)height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pData );

	return texture;
}

ImAppRendererTexture* ImAppRendererTextureCreateFromFile( ImAppRenderer* pRenderer, const void* pFilename )
{
	//int x, y, n;
	//GLuint tex;
	//unsigned char *data = stbi_load( filename, &x, &y, &n, 0 );
	//if( !data ) die( "[SDL]: failed to load image: %s", filename );
	//glGenTextures( 1, &tex );
	//glBindTexture( GL_TEXTURE_2D, tex );
	//glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST );
	//glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_NEAREST );
	//glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	//glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	//glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
	//glGenerateMipmap( GL_TEXTURE_2D );
	//stbi_image_free( data );
	//return nk_image_id( (int)tex );

	//SDL_CreateTextureFromSurface
	return NULL;
}

ImAppRendererTexture* ImAppRendererTextureCreateFromMemory( ImAppRenderer* pRenderer, const void* pData, int width, int height )
{
	ImAppRendererTexture* pTexture = IMAPP_NEW_ZERO( pRenderer->pAllocator, ImAppRendererTexture );
	if( pTexture == NULL )
	{
		return NULL;
	}

	pTexture->width		= width;
	pTexture->height	= height;
	pTexture->size		= width * height * 4u;
	pTexture->pData		= ImAppMalloc( pRenderer->pAllocator, pTexture->size );

	if( pTexture->pData == NULL )
	{
		ImAppRendererTextureDestroy( pRenderer, pTexture );
		return NULL;
	}

	memcpy( pTexture->pData, pData, pTexture->size );

	pTexture->texture = ImAppRendererCreateTexture( pData, width, height );

	pTexture->pNext = pRenderer->pFirstTexture;
	pRenderer->pFirstTexture = pTexture;

	return pTexture;
}

void ImAppRendererTextureDestroy( ImAppRenderer* pRenderer, ImAppRendererTexture* pTexture )
{
	if( pTexture == NULL )
	{
		return;
	}

	if( pTexture->texture != 0u )
	{
		glDeleteTextures( 1u, &pTexture->texture );
		pTexture->texture = 0u;
	}

	ImAppFree( pRenderer->pAllocator, pTexture );
}

void ImAppRendererDraw( ImAppRenderer* pRenderer, struct nk_context* pNkContext, int width, int height )
{
	const float color[ 4u ] = { 1.0f, 0.0f, 0.0f, 1.0f };
	glClearColor( color[ 0u ], color[ 1u ], color[ 2u ], color[ 3u ] );
	glClear( GL_COLOR_BUFFER_BIT );

	glEnable( GL_BLEND );
	glBlendEquation( GL_FUNC_ADD );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	glDisable( GL_CULL_FACE );

	glDisable( GL_DEPTH_TEST );
	glEnable( GL_SCISSOR_TEST );

	glUseProgram( pRenderer->program );
	glUniform1i( pRenderer->programUniformTexture, 0 );

	const GLfloat projectionMatrix[ 4 ][ 4 ] ={
		{  2.0f / width,	0.0f,			 0.0f,	0.0f },
		{  0.0f,			-2.0f / height,	 0.0f,	0.0f },
		{  0.0f,			0.0f,			-1.0f,	0.0f },
		{ -1.0f,			1.0f,			 0.0f,	1.0f }
	};
	glUniformMatrix4fv( pRenderer->programUniformProjection, 1, GL_FALSE, &projectionMatrix[ 0u ][ 0u ] );

	ImAppRendererDrawNuklear( pRenderer, pNkContext, height );

	// reset OpenGL state
	glUseProgram( 0 );
	glBindBuffer( GL_ARRAY_BUFFER, 0 );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
	glBindVertexArray( 0 );
	glDisable( GL_BLEND );
	glDisable( GL_SCISSOR_TEST );
}

static void ImAppRendererDrawNuklear( ImAppRenderer* pRenderer, struct nk_context* pNkContext, int height )
{
	// bind buffers
	glBindVertexArray( pRenderer->vertexArray );
	glBindBuffer( GL_ARRAY_BUFFER, pRenderer->vertexBuffer );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, pRenderer->elementBuffer );

	glBufferData( GL_ARRAY_BUFFER, IMAPP_MAX_VERTEX_SIZE, NULL, GL_STREAM_DRAW );
	glBufferData( GL_ELEMENT_ARRAY_BUFFER, IMAPP_MAX_ELEMENT_SIZE, NULL, GL_STREAM_DRAW );

	// upload
	{
		void* pVertexData = glMapBufferRange( GL_ARRAY_BUFFER, 0, IMAPP_MAX_VERTEX_SIZE, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT );
		void* pElementData = glMapBufferRange( GL_ELEMENT_ARRAY_BUFFER, 0, IMAPP_MAX_ELEMENT_SIZE, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT );
		{
			struct nk_buffer vertexBuffer;
			struct nk_buffer elementBuffer;
			nk_buffer_init_fixed( &vertexBuffer, pVertexData, IMAPP_MAX_VERTEX_SIZE );
			nk_buffer_init_fixed( &elementBuffer, pElementData, IMAPP_MAX_ELEMENT_SIZE );
			const nk_flags result = nk_convert( pNkContext, &pRenderer->nkCommands, &vertexBuffer, &elementBuffer, &pRenderer->nkConvertConfig );
			IMAPP_ASSERT( result == NK_CONVERT_SUCCESS );
		}
		glUnmapBuffer( GL_ARRAY_BUFFER );
		glUnmapBuffer( GL_ELEMENT_ARRAY_BUFFER );
	}

	const nk_draw_index* pElementOffset = NULL;
	const struct nk_draw_command* pCommand;
	nk_draw_foreach( pCommand, pNkContext, &pRenderer->nkCommands )
	{
		IMAPP_ASSERT( pCommand->elem_count >= 0u );

		ImAppRendererTexture* pTexture = (ImAppRendererTexture*)pCommand->texture.ptr;
		if( pTexture == NULL )
		{
			glBindTexture( GL_TEXTURE_2D, 0 );
		}
		else
		{
			glBindTexture( GL_TEXTURE_2D, pTexture->texture );
		}

		glScissor(
			(GLint)(pCommand->clip_rect.x),
			(GLint)((height - (GLint)(pCommand->clip_rect.y + pCommand->clip_rect.h))),
			(GLint)(pCommand->clip_rect.w),
			(GLint)(pCommand->clip_rect.h)
		);

		glDrawElements( GL_TRIANGLES, (GLsizei)pCommand->elem_count, GL_UNSIGNED_SHORT, pElementOffset );
		pElementOffset += pCommand->elem_count;
	}

	nk_clear( pNkContext );
	nk_buffer_clear( &pRenderer->nkCommands );
}
