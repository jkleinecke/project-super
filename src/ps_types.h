
#ifdef PROJECTSUPER_SLOW
// define NDEBUG so that assert.h will compile out assert(..)
#define NDEBUG 1
#endif

#if !defined(COMPILER_MSVC)
#define COMPILER_MSVC 0
#endif

#if !defined(COMPILER_LLVM)
#define COMPILER_LLVM 0
#endif

#if !COMPILER_MSVC && !COMPILER_LLVM
#if _MSC_VER
#undef COMPILER_MSVC
#define COMPILER_MSVC 1
#else
#undef COMPILER_LLVM
#define COMPILER_LLVM 1
#endif
#endif

#if COMPILER_MSVC
#include <intrin.h>
#elif COMPILER_LLVM
#include <x86intrin.h>
#else
#error SEE/NEON optimizations are not available for this compiler yet!!!!
#endif

#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <float.h>
#include <assert.h>

//===================== Standard Types =============================

typedef int8_t  int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef intptr_t intptr;
typedef uintptr_t uintptr;

typedef intptr_t iptr;
typedef uintptr_t uptr;

typedef size_t mem_size;

typedef float   real32;
typedef double  real64;

typedef int32   bool32;

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float   f32;
typedef float   r32;
typedef float   nf32;   // normalized float 0..1

typedef double  f64;
typedef double  r64;
typedef double  nf64;   // normalized double 0..1

typedef i32   b32;
typedef b32   b32x;

typedef uintptr_t umm;
typedef intptr_t imm;

//===================== Standard Defines =============================

#define flag8(type) u8
#define flag16(type) u16
#define flag32(type) u32
#define flag64(type) u64

#define enum8(type) u8
#define enum16(type) u16
#define enum32(type) u32
#define enum64(type) u64

#define U8MAX 255
#define U16MAX 65535
#define S32MIN ((s32)0x80000000)
#define S32MAX ((s32)0x7fffffff)
#define U32MIN 0
#define U32MAX ((u32)-1)
#define U64MAX ((u64)-1)
#define UMMMAX ((umm)-1)
#define F32MAX FLT_MAX
#define F32MIN -FLT_MAX

#define UMMToPtr(type, value) (type*)(value)
#define PtrToUMM(pointer) ((umm)(pointer))

#define U32ToPtr(type, value) (type*)((mem_size)value)
#define PtrToU32(pointer) ((u32)(mem_size)(pointer))

#define OffsetOf(type, member) (uintptr)&(((type*)0)->member)
#define Pi32 3.14159265359f

// Clarify usage of static keyword with these defines
#define internal static
#define local_persist static
#define global static
#define global_variable static

#ifdef ASSERT
    #undef ASSERT
#endif
#ifdef PROJECTSUPER_SLOW
// #ifdef COMPILER_MSVC
//     _ACRTIMP void __cdecl _wassert(
//         _In_z_ wchar_t const* _Message,
//         _In_z_ wchar_t const* _File,
//         _In_   unsigned       _Line
//         );

//     #define ASSERT(expression) (void)(                                                       \
//             (!!(expression)) ||                                                              \
//             (_wassert(_CRT_WIDE(#expression), _CRT_WIDE(__FILE__), (unsigned)(__LINE__)), 0) \
//         )
// #else
    #define ASSERT(cond) if(!(cond)) {*(volatile int *)0 = 0;}
    #if PROJECTSUPER_SLOW > 1
        #define ASSERT2(cond) ASSERT(cond)
        #if PROJECTSUPER_SLOW > 2
            #define ASSERT3 ASSERT(cond)
        #else
            #define ASSERT3
        #endif
    #else
        #define ASSERT2
        #define ASSERT3
    #endif
#else
    #define ASSERT
    #define ASSERT2
    #define ASSERT3
#endif

#define Kilobytes(n) ((n) * 1024LL)
#define Megabytes(n) (Kilobytes(n) * 1024LL)
#define Gigabytes(n) (Megabytes(n) * 1024LL)
#define Terabytes(n) (Gigabytes(n) * 1024LL)

#ifndef ARRAY_COUNT
#define ARRAY_COUNT(array) (sizeof(array)/sizeof(array[0]))
#endif

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif 

#ifndef IFSET
#define IFSET(cond, set) if(cond) (set) 
#endif

#ifndef CompileAssert
#define CompileAssert(Expr) static_assert(Expr, "Assertion failed: " #Expr)
#endif

#if PROJECTSUPER_INTERNAL
#define NotImplemented ASSERT(!"NotImplemented")
#else
#define NotImplemented CompileAssert(!"NotImplemented!!!!!!!!!!!!")
#endif

#define AlignPow2(Value, Alignment) (((Value) + ((Alignment) - 1)) & ~(((Value) - (Value)) + (Alignment) - 1))
#define Align4(Value) (((Value) + 3) & ~3)
#define Align8(Value) (((Value) + 7) & ~7)
#define Align16(Value) (((Value) + 15) & ~15)

struct buffer
{
    umm size;
    u8* data;
};
typedef buffer string;