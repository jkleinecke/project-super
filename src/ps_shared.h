
#define STB_SPRINTF_DECORATE(name) ps_##name
#define STB_SPRINTF_IMPLEMENTATION
#include "stb_sprintf.h"

// NOTE(james): This copy does NOT work if the src and dst pointers
// will overlap!  If you need that, write another copy method that
// supports that case.

#define CopyString(src, dest) CopyBuffer(src, dest)
#define CopyBuffer(src, dest) Copy(src.size, src.data, dest.data)
#define CopyArray(count, src, dest) Copy((count)*sizeof(*(src)), (src), (dest))
internal void*
Copy(mem_size size, const void* src, void* dst)
{
    CompileAssert(sizeof(umm)==sizeof(src));   // verify pointer size
    mem_size chunks = size / sizeof(src);  // Copy by CPU bit width
    mem_size slice = size % sizeof(src);   // remaining bytes < CPU bit width

    umm* s = (umm*)src;
    umm* d = (umm*)dst;

    while(chunks--) {
        *d++ = *s++;
    }

    u8* d8 = (u8*)d;
    u8* s8 = (u8*)s;

    while(slice--) {
        *d8++ = *s8++;
    }

    ASSERT3(s8 == ((u8*)src + size));
    ASSERT3(d8 == ((u8*)dst + size));
    return dst;
}

#define ZeroStruct(instance) ZeroSize(sizeof(instance), &(instance))
#define ZeroArray(count, array_ptr) ZeroSize((count)*sizeof((array_ptr)[0]), (array_ptr))
#define ZeroBuffer(buffer) ZeroSize(buffer.size, buffer.data)
internal void
ZeroSize(mem_size size, void* ptr)
{
    CompileAssert(sizeof(umm)==sizeof(ptr));   // verify pointer size
    mem_size chunks = size/sizeof(ptr);
    mem_size slice = size%sizeof(ptr);

    umm* p = (umm*)ptr;

    while(chunks--) {
        *p++ = 0;
    }

    u8* s = (u8*)p;

    while(slice--) {
        *s++ = 0;
    }

    ASSERT3(s == ((u8*)ptr + size));
}

inline internal b32x
IsValid(const buffer& buff)
{
    b32x result = (buff.size > 0);
    return result;
}


inline internal 
int StringLength(const char* s)
{
    char* ch = (char*)s;
    for(; *ch; ++ch) {}
    return (int)(ch - s);
}

inline internal
string MakeString(const char* s)
{
    return { (umm)StringLength(s), (u8*)s };
}

inline internal
string MakeString(umm size, const char* szBase)
{
    return { size, (u8*)szBase };
}

inline internal
string MakeString(const char* szBase, const char* szEnd)
{
    ASSERT((umm)(szEnd - szBase) < U16MAX);
    return { (umm)(szEnd - szBase), (u8*)szBase };
}

#define INDEX_NOT_FOUND ((umm)-1)
internal umm
IndexOf(string src, char val, umm start_pos = 0)
{
    ASSERT(start_pos <= src.size);
    for(umm index = start_pos; index < src.size; ++index)
    {
        if(src.data[index] == val)
        {
            return index;
        }
    }
    return INDEX_NOT_FOUND;
}

internal umm
ReverseIndexOf(string src, char val, umm start_pos = 0)
{
    ASSERT( src.size - start_pos >= 0 );
    for(umm index = start_pos + src.size; index >= 0; --index)
    {
        if(src.data[index] == val)
        {
            return index;
        }
    }
    return INDEX_NOT_FOUND;
}

internal string
RemoveExtension(string filenameFull)
{
    string filename = filenameFull;
    
    umm newSize = filename.size;
    for(umm index = 0;
        index < filename.size;
        ++index)
    {
        char C = filename.data[index];
        if(C == '.')
        {
            newSize = index;
        }
        else if((C == '/') || (C == '\\'))
        {
            newSize = filename.size;
        }
    }
    
    filename.size = newSize;
    
    return(filename);
}

internal string
RemovePath(string filepath)
{
    string filename = filepath;
    
    umm newStart = 0;
    for(umm index = 0;
        index < filename.size;
        ++index)
    {
        char C = filename.data[index];
        if((C == '/') || (C == '\\'))
        {
            newStart = index + 1;
        }
    }
    
    filename.data += newStart;
    filename.size -= newStart;
    
    return(filename);
}

internal u8 *
Advance(buffer& buff, umm count)
{
    u8 *result = 0;
    
    if(buff.size >= count)
    {
        result = buff.data;
        buff.data += count;
        buff.size -= count;
    }
    else
    {
        buff.data += buff.size;
        buff.size = 0;
    }
    
    return(result);
}

#define FormatString ps_snprintf

#if COMPILER_MSVC
#define CompletePreviousReadsBeforeFutureReads _ReadBarrier()
#define CompletePreviousWritesBeforeFutureWrites _WriteBarrier()
inline uint32 AtomicCompareExchangeUInt32(uint32 volatile *Value, uint32 New, uint32 Expected)
{
    uint32 Result = _InterlockedCompareExchange((long volatile *)Value, New, Expected);
    
    return(Result);
}
inline u64 AtomicExchangeU64(u64 volatile *Value, u64 New)
{
    u64 Result = _InterlockedExchange64((__int64 volatile *)Value, New);
    
    return(Result);
}
inline u64 AtomicAddU64(u64 volatile *Value, u64 Addend)
{
    // NOTE: Returns the original value _prior_ to adding
    u64 Result = _InterlockedExchangeAdd64((__int64 volatile *)Value, Addend);
    
    return(Result);
}
inline u32 GetThreadID(void)
{
    u8 *ThreadLocalStorage = (u8 *)__readgsqword(0x30);
    u32 ThreadID = *(u32 *)(ThreadLocalStorage + 0x48);
    
    return(ThreadID);
}

#elif COMPILER_LLVM
// TODO(james): Does LLVM have real read-specific barriers yet?
#define CompletePreviousReadsBeforeFutureReads asm volatile("" ::: "memory")
#define CompletePreviousWritesBeforeFutureWrites asm volatile("" ::: "memory")
inline uint32 AtomicCompareExchangeUInt32(uint32 volatile *Value, uint32 New, uint32 Expected)
{
    uint32 Result = __sync_val_compare_and_swap(Value, Expected, New);
    
    return(Result);
}
inline u64 AtomicExchangeU64(u64 volatile *Value, u64 New)
{
    u64 Result = __sync_lock_test_and_set(Value, New);
    
    return(Result);
}
inline u64 AtomicAddU64(u64 volatile *Value, u64 Addend)
{
    // NOTE: Returns the original value _prior_ to adding
    u64 Result = __sync_fetch_and_add(Value, Addend);
    
    return(Result);
}
inline u32 GetThreadID(void)
{
    u32 ThreadID;
#if defined(__APPLE__) && defined(__x86_64__)
    asm("mov %%gs:0x00,%0" : "=r"(ThreadID));
#elif defined(__i386__)
    asm("mov %%gs:0x08,%0" : "=r"(ThreadID));
#elif defined(__x86_64__)
    asm("mov %%fs:0x10,%0" : "=r"(ThreadID));
#else
#error Unsupported architecture
#endif
    
    return(ThreadID);
}
#else
// TODO(james): Other compilers/platforms??
#endif

inline u32
SafeTruncateToU32(u64 Value)
{
    ASSERT(Value <= U32MAX);
    uint32 Result = (uint32)Value;
    return(Result);
}

inline u16
SafeTruncateToU16(u32 Value)
{
    ASSERT(Value <= U16MAX);
    u16 Result = (u16)Value;
    return(Result);
}

inline u8
SafeTruncateToU8(u64 Value)
{
    ASSERT(Value <= U8MAX);
    u8 Result = (u8)Value;
    return(Result);
}