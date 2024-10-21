#pragma once

#define TIKI_ON 				1-
#define TIKI_OFF				0-

#define IMAPP_ENABLED( value )	((2 - (value 0)) == 1)
#define IMAPP_DISABLED( value )	((2 - (value 0)) == 2)

#define IMAPP_IF( expr )		((expr) ? TIKI_ON : TIKI_OFF)

#if defined( _WIN32 )
#	define IMAPP_PLATFORM_WINDOWS		TIKI_ON
#	if defined( _WIN64 )
#		define IMAPP_POINTER_64			TIKI_ON
#	else
#		define IMAPP_POINTER_32			TIKI_ON
#	endif
#else
#	define IMAPP_PLATFORM_WINDOWS		TIKI_OFF
#endif

#if defined( __ANDROID__ )
#	define IMAPP_PLATFORM_ANDROID		TIKI_ON
#	define IMAPP_PLATFORM_POSIX			TIKI_ON
#else
#	define IMAPP_PLATFORM_ANDROID		TIKI_OFF
#endif

#if defined( __linux__ ) && !defined( __ANDROID__ )
#	define IMAPP_PLATFORM_LINUX			TIKI_ON
#	define IMAPP_PLATFORM_POSIX			TIKI_ON
#else
#	define IMAPP_PLATFORM_LINUX			TIKI_OFF
#endif

#if defined( __EMSCRIPTEN__ )
#	define IMAPP_PLATFORM_WEB			TIKI_ON
#	define IMAPP_PLATFORM_SDL           TIKI_ON
#else
#	define IMAPP_PLATFORM_WEB			TIKI_OFF
#endif

#if defined( _MSC_VER )
#	define IMAPP_COMPILER_MSVC			TIKI_ON
#else
#	define IMAPP_COMPILER_MSVC			TIKI_OFF
#endif

#if defined( __clang__ )
#	define IMAPP_COMPILER_CLANG			TIKI_ON
#else
#	define IMAPP_COMPILER_CLANG			TIKI_OFF
#endif

#if defined( __GNUC__ )
#	define IMAPP_COMPILER_GCC			TIKI_ON
#else
#	define IMAPP_COMPILER_GCC			TIKI_OFF
#endif

#if IMAPP_ENABLED( IMAPP_COMPILER_MSVC )
#	if defined( _M_IX86 )
#		define IMAPP_ARCH_X86			TIKI_ON
#	elif defined( _M_AMD64 )
#		define IMAPP_ARCH_X64			TIKI_ON
#	elif defined( _M_ARM )
#		define IMAPP_ARCH_ARM32			TIKI_ON
#	elif defined( _M_ARM64 )
#		define IMAPP_ARCH_ARM64			TIKI_ON
#	endif
#endif

#if IMAPP_ENABLED( IMAPP_COMPILER_GCC ) || IMAPP_ENABLED( IMAPP_COMPILER_CLANG )
#	if defined( __aarch64__ ) || defined( __amd64__ )
#		define IMAPP_POINTER_64			TIKI_ON
#	elif defined( __arm__ ) || defined( __i386__ )
#		define IMAPP_POINTER_32			TIKI_ON
#	endif
#	if defined( __i386__ )
#		define IMAPP_ARCH_X86			TIKI_ON
#	elif defined( __amd64__ )
#		define IMAPP_ARCH_X64			TIKI_ON
#	elif defined( __arm__ )
#		define IMAPP_ARCH_ARM32			TIKI_ON
#	elif defined( __aarch64__ )
#		define IMAPP_ARCH_ARM64			TIKI_ON
#	endif
#endif

#if !defined( IMAPP_ARCH_X86 )
#	define IMAPP_ARCH_X86				TIKI_OFF
#endif

#if !defined( IMAPP_ARCH_X64 )
#	define IMAPP_ARCH_X64				TIKI_OFF
#endif

#if !defined( IMAPP_ARCH_ARM32 )
#	define IMAPP_ARCH_ARM32				TIKI_OFF
#endif

#if !defined( IMAPP_ARCH_ARM64 )
#	define IMAPP_ARCH_ARM64				TIKI_OFF
#endif

#if !defined( IMAPP_PLATFORM_POSIX )
#	define IMAPP_PLATFORM_POSIX			TIKI_OFF
#endif

#if !defined( IMAPP_PLATFORM_SDL )
#	define IMAPP_PLATFORM_SDL			TIKI_OFF
#endif

#if !defined( IMAPP_POINTER_32 )
#	define IMAPP_POINTER_32				TIKI_OFF
#endif
#if !defined( IMAPP_POINTER_64 )
#	define IMAPP_POINTER_64				TIKI_OFF
#endif

#if defined( DEBUG ) || defined( _DEBUG ) || defined( __DEBUG__ )
#	define IMAPP_DEBUG					TIKI_ON
#else
#	define IMAPP_DEBUG					TIKI_OFF
#endif

#if IMAPP_ENABLED( IMAPP_COMPILER_MSVC )
#	define IMAPP_HAS_BREAK				TIKI_ON
#	define IMAPP_BREAK __debugbreak()
#elif IMAPP_ENABLED( IMAPP_COMPILER_GCC ) || IMAPP_ENABLED( IMAPP_COMPILER_CLANG )
#	define IMAPP_HAS_BREAK				TIKI_ON
#	define IMAPP_BREAK __builtin_trap()
#else
#	define IMAPP_HAS_BREAK				TIKI_OFF
#endif

#if IMAPP_ENABLED( IMAPP_DEBUG ) && IMAPP_ENABLED( IMAPP_HAS_BREAK )
#	define IMAPP_ASSERT( expr ) if( !(expr) ) IMAPP_BREAK
#	define IMAPP_VERIFY( expr ) if( !(expr) ) IMAPP_BREAK
#else
#	define IMAPP_ASSERT( expr )
#	define IMAPP_VERIFY( expr ) expr
#endif

#if IMAPP_ENABLED( IMAPP_COMPILER_MSVC )
#	define IMAPP_ALIGN_PREFIX( var )		__declspec( align( var ) )
#	define IMAPP_ALIGN_POSTFIX( var )
#	define IMAPP_ALIGNOF( type )			__alignof( type )
#elif IMAPP_ENABLED( IMAPP_COMPILER_GCC ) || IMAPP_ENABLED( IMAPP_COMPILER_CLANG )
#	define IMAPP_ALIGN_PREFIX( var )
#	define IMAPP_ALIGN_POSTFIX( var )		__attribute__( ( aligned( var ) ) )
#	define IMAPP_ALIGNOF( type )			__alignof__( type )
#else
#	error Platform not supported
#endif

#if IMAPP_ENABLED( IMAPP_COMPILER_GCC ) || IMAPP_ENABLED( IMAPP_COMPILER_CLANG )
#	define IMAPP_OFFSETOF( type, member )	__builtin_offsetof( type, member )
#else
#	define IMAPP_OFFSETOF( type, member )	((size_t)(&((type*)0)->member))
#endif

#define IMAPP_USE_INLINE				TIKI_ON

#if IMAPP_ENABLED( IMAPP_USE_INLINE )
#	if IMAPP_ENABLED( IMAPP_COMPILER_MSVC )
#		define IMAPP_INLINE					inline
#		define IMAPP_FORCE_INLINE			__forceinline
#		define IMAPP_NO_INLINE				__declspec(noinline)
#	elif IMAPP_ENABLED( IMAPP_COMPILER_GCC ) || IMAPP_ENABLED( IMAPP_COMPILER_CLANG )
#		define IMAPP_INLINE					inline
#		define IMAPP_FORCE_INLINE			inline __attribute__((always_inline))
#		define IMAPP_NO_INLINE				__attribute__((noinline))
#	else
#		error Platform not supported
#	endif
#else
#	define IMAPP_INLINE
#	define IMAPP_FORCE_INLINE
#	define IMAPP_NO_INLINE
#endif

#define IMAPP_STATIC_ASSERT( expr ) static_assert( ( expr ), #expr )

#define IMAPP_ARRAY_COUNT( arr ) ( sizeof( arr ) / sizeof( *arr ) )

#define IMAPP_USE( var ) (void)var
