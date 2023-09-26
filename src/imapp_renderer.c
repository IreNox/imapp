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

	ImAppRendererTexture*		pFirstTexture;
};

struct ImAppRendererTexture
{
	ImAppRendererTexture*		pPrev;
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
	float						color[ 4u ];
};
typedef struct ImAppRendererVertex ImAppRendererVertex;

//#if IMAPP_ENABLED( IMAPP_PLATFORM_ANDROID )
//#	define IMAPP_GLSL_VERSION "#version 300 es\n"
//#else
//#	define IMAPP_GLSL_VERSION "#version 150\n"
//#endif

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
	"	float charColor = texture2D(Texture, vtfUV.xy).r;"
	"	gl_FragColor = vec4( vtfColor.rgb, vtfColor.a * charColor );\n"
	"}\n";

//static const struct nk_draw_vertex_layout_element s_aVertexLayout[] = {
//	{ NK_VERTEX_POSITION,	NK_FORMAT_FLOAT,	IMAPP_OFFSETOF( ImAppRendererVertex, position ) },
//	{ NK_VERTEX_TEXCOORD,	NK_FORMAT_FLOAT,	IMAPP_OFFSETOF( ImAppRendererVertex, uv ) },
//	{ NK_VERTEX_COLOR,		NK_FORMAT_R8G8B8A8,	IMAPP_OFFSETOF( ImAppRendererVertex, color ) },
//	{ NK_VERTEX_LAYOUT_END }
//};

#define IMAPP_MAX_VERTEX_COUNT	8u * 1024u
#define IMAPP_MAX_ELEMENT_COUNT	16u * 1024u
#define IMAPP_MAX_VERTEX_SIZE	sizeof( ImAppRendererVertex ) * IMAPP_MAX_VERTEX_COUNT
#define IMAPP_MAX_ELEMENT_SIZE	sizeof( nk_draw_index ) * IMAPP_MAX_ELEMENT_COUNT

static bool		ImAppRendererCompileShader( GLuint shader, const char* pShaderCode );
static GLuint	ImAppRendererCreateTexture( const void* pData, int width, int height );

static bool		ImAppRendererCreateResources( ImAppRenderer* renderer );
static void		ImAppRendererDestroyResources( ImAppRenderer* renderer );

static void		ImAppRendererDrawCommands( ImAppRenderer* renderer, const ImUiDrawData* drawData, int width, int height );

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

	//struct nk_allocator allocator = ImAppAllocatorGetNuklear( renderer->allocator );
	//nk_font_atlas_init( &renderer->nkFontAtlas, &allocator );
	//nk_buffer_init( &renderer->nkCommands, &allocator, 4u * 1024u );

	//renderer->nkConvertConfig.vertex_layout		= s_aVertexLayout;
	//renderer->nkConvertConfig.vertex_size			= sizeof( ImAppRendererVertex );
	//renderer->nkConvertConfig.vertex_alignment		= NK_ALIGNOF( ImAppRendererVertex );
	//renderer->nkConvertConfig.circle_segment_count	= 22;
	//renderer->nkConvertConfig.curve_segment_count	= 22;
	//renderer->nkConvertConfig.arc_segment_count	= 22;
	//renderer->nkConvertConfig.global_alpha			= 1.0f;
	//renderer->nkConvertConfig.shape_AA				= NK_ANTI_ALIASING_ON;
	//renderer->nkConvertConfig.line_AA				= NK_ANTI_ALIASING_ON;

	//renderer->nkCreated							= true;

	return renderer;
}

void ImAppRendererDestroy( ImAppRenderer* renderer )
{
	//if( renderer->nkCreated )
	//{
	//	nk_font_atlas_cleanup( &renderer->nkFontAtlas );
	//	nk_buffer_free( &renderer->nkCommands );
	//}

	while( renderer->pFirstTexture != NULL )
	{
		ImAppRendererTexture* pTexture = renderer->pFirstTexture;
		ImAppRendererTextureDestroy( renderer, pTexture );
	}

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

	const GLsizei vertexSize	= sizeof( ImAppRendererVertex );
	size_t vertexPositionOffset	= IMAPP_OFFSETOF( ImAppRendererVertex, position );
	size_t vertexUvOffset		= IMAPP_OFFSETOF( ImAppRendererVertex, uv );
	size_t vertexColorOffset	= IMAPP_OFFSETOF( ImAppRendererVertex, color );
	glVertexAttribPointer( attributePosition, 2, GL_FLOAT, GL_FALSE, vertexSize, (void*)vertexPositionOffset );
	glVertexAttribPointer( attributeTexCoord, 2, GL_FLOAT, GL_FALSE, vertexSize, (void*)vertexUvOffset );
	glVertexAttribPointer( attributeColor, 4, GL_FLOAT, GL_TRUE, vertexSize, (void*)vertexColorOffset );

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

	for( ImAppRendererTexture* pTexture = renderer->pFirstTexture; pTexture != NULL; pTexture = pTexture->pNext )
	{
		glDeleteTextures( 1u, &pTexture->texture );
		pTexture->texture = ImAppRendererCreateTexture( pTexture->pData, pTexture->width, pTexture->height );
	}

	return true;
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

//ImAppRendererTexture* ImAppRendererTextureCreateFromFile( ImAppRenderer* renderer, const void* pFilename )
//{
//	//int x, y, n;
//	//GLuint tex;
//	//unsigned char *data = stbi_load( filename, &x, &y, &n, 0 );
//	//if( !data ) die( "[SDL]: failed to load image: %s", filename );
//	//glGenTextures( 1, &tex );
//	//glBindTexture( GL_TEXTURE_2D, tex );
//	//glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST );
//	//glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_NEAREST );
//	//glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
//	//glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
//	//glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
//	//glGenerateMipmap( GL_TEXTURE_2D );
//	//stbi_image_free( data );
//	//return nk_image_id( (int)tex );
//
//	//SDL_CreateTextureFromSurface
//	return NULL;
//}

ImAppRendererTexture* ImAppRendererTextureCreateFromMemory( ImAppRenderer* renderer, const void* pData, int width, int height )
{
	ImAppRendererTexture* pTexture = IMUI_MEMORY_NEW_ZERO( renderer->allocator, ImAppRendererTexture );
	if( pTexture == NULL )
	{
		return NULL;
	}

	pTexture->width		= width;
	pTexture->height	= height;
	pTexture->size		= width * height * 4u;
	pTexture->pData		= ImUiMemoryAlloc( renderer->allocator, pTexture->size );

	if( pTexture->pData == NULL )
	{
		ImAppRendererTextureDestroy( renderer, pTexture );
		return NULL;
	}

	memcpy( pTexture->pData, pData, pTexture->size );

	pTexture->texture = ImAppRendererCreateTexture( pData, width, height );

	if( renderer->pFirstTexture != NULL )
	{
		renderer->pFirstTexture->pPrev = pTexture;
	}

	pTexture->pNext = renderer->pFirstTexture;
	renderer->pFirstTexture = pTexture;

	return pTexture;
}

void ImAppRendererTextureDestroy( ImAppRenderer* renderer, ImAppRendererTexture* pTexture )
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

	if( pTexture->pPrev != NULL )
	{
		pTexture->pPrev->pNext = pTexture->pNext;
	}

	if( pTexture->pNext != NULL )
	{
		pTexture->pNext->pPrev = pTexture->pPrev;
	}

	if( pTexture == renderer->pFirstTexture )
	{
		renderer->pFirstTexture = pTexture->pNext;
	}

	ImUiMemoryFree( renderer->allocator, pTexture );
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
	//glEnable( GL_SCISSOR_TEST );

	glUseProgram( renderer->program );
	glUniform1i( renderer->programUniformTexture, 0 );

	const GLfloat projectionMatrix[ 4 ][ 4 ] ={
		{  2.0f / width,	0.0f,			 0.0f,	0.0f },
		{  0.0f,			-2.0f / height,	 0.0f,	0.0f },
		{  0.0f,			0.0f,			-1.0f,	0.0f },
		{ -1.0f,			1.0f,			 0.0f,	1.0f }
	};
	glUniformMatrix4fv( renderer->programUniformProjection, 1, GL_FALSE, &projectionMatrix[ 0u ][ 0u ] );

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
	//glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, renderer->elementBuffer );

	glBufferData( GL_ARRAY_BUFFER, drawData->vertexDataSize, NULL, GL_STREAM_DRAW );
	//glBufferData( GL_ELEMENT_ARRAY_BUFFER, drawData->indexDataSize, NULL, GL_STREAM_DRAW);

	// upload
	{
		void* pVertexData = glMapBufferRange( GL_ARRAY_BUFFER, 0, drawData->vertexDataSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT ); // GL_MAP_UNSYNCHRONIZED_BIT
		//void* pElementData = glMapBufferRange( GL_ELEMENT_ARRAY_BUFFER, 0, drawData->indexDataSize, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT );

		memcpy( pVertexData, drawData->vertexData, drawData->vertexDataSize );
		//memcpy( pElementData, drawData->indexData, drawData->indexDataSize );
		//{
		//	struct nk_buffer vertexBuffer;
		//	struct nk_buffer elementBuffer;
		//	nk_buffer_init_fixed( &vertexBuffer, pVertexData, IMAPP_MAX_VERTEX_SIZE );
		//	nk_buffer_init_fixed( &elementBuffer, pElementData, IMAPP_MAX_ELEMENT_SIZE );
		//	const nk_flags result = nk_convert( pNkContext, &renderer->nkCommands, &vertexBuffer, &elementBuffer, &renderer->nkConvertConfig );
		//	IMAPP_ASSERT( result == NK_CONVERT_SUCCESS );
		//}
		glUnmapBuffer( GL_ARRAY_BUFFER );
		//glUnmapBuffer( GL_ELEMENT_ARRAY_BUFFER );
	}

	GLint offset = 0;
	const uint32_t* elementOffset = NULL;
	for( size_t i = 0u; i < drawData->commandCount; ++i )
	{
		const ImUiDrawCommand* command = &drawData->commands[ i ];

		ImAppRendererTexture* texture = (ImAppRendererTexture*)command->texture;
		if( texture == NULL )
		{
			//glUseProgram( renderer->programColor );

			//const GLfloat projectionMatrix[ 4 ][ 4 ] ={
			//	{  2.0f / width,	0.0f,			 0.0f,	0.0f },
			//	{  0.0f,			-2.0f / height,	 0.0f,	0.0f },
			//	{  0.0f,			0.0f,			-1.0f,	0.0f },
			//	{ -1.0f,			1.0f,			 0.0f,	1.0f }
			//};
			//glUniformMatrix4fv( renderer->programUniformProjection, 1, GL_FALSE, &projectionMatrix[ 0u ][ 0u ] );

			glBindTexture( GL_TEXTURE_2D, 0 );
		}
		else
		{
			glBindTexture( GL_TEXTURE_2D, texture->texture );
		}

		//glScissor(
		//	(GLint)(pCommand->clip_rect.x),
		//	(GLint)((height - (GLint)(pCommand->clip_rect.y + pCommand->clip_rect.h))),
		//	(GLint)(pCommand->clip_rect.w),
		//	(GLint)(pCommand->clip_rect.h)
		//);

		glDrawArrays( GL_TRIANGLES, offset, (GLsizei)command->count );
		//glDrawElements( GL_TRIANGLES, (GLsizei)command->count, GL_UNSIGNED_INT, elementOffset );
		elementOffset += command->count;
		offset += (GLint)command->count;
	}

	//nk_clear( pNkContext );
	//nk_buffer_clear( &renderer->nkCommands );
}
