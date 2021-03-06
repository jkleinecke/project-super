
#define STB_SPRINTF_DECORATE(name) ps_##name
#define STB_SPRINTF_IMPLEMENTATION
#include "libs/stb/stb_sprintf.h"
#define STB_DS_IMPLEMENTATION
#include "libs/stb/stb_ds.h"



// NOTE(james): This copy does NOT work if the src and dst pointers
// will overlap!  If you need that, write another copy method that
// supports that case.

#define CopyBuffer(src, dest) Copy(src.size, src.data, dest.data)
#define CopyArray(count, src, dest) Copy((count)*sizeof(*(src)), (src), (dest))
internal void*
Copy(umm size, const void* src, void* dst)
{
    CompileAssert(sizeof(umm)==sizeof(src));   // verify pointer size
    memory_index chunks = size / sizeof(src);  // Copy by CPU bit width
    memory_index slice = size % sizeof(src);   // remaining bytes < CPU bit width

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

// NOTE(james): This is a special copy helper specifically to pull a member of struct in an array
//  into an array of single values.
//  To use:
//      count       - The number of elements in each array
//      offset      - The memory offset of the member in the struct
//      elementSize - The size of the member in bytes
//      stride      - The size of the struct
//      src         - pointer to the source array
//      dstSize     - The total size of the dst array in bytes
//      dst         - pointer to the destination array
//
//      returns - pointer to the destination array
internal void*
SparseCopyArray(u32 count, umm offset, umm elementSize, umm stride, const void* src, umm dstSize, void* dst)
{
    ASSERT(elementSize * count <= dstSize);

    u8* s = (u8*)src;
    u8* d = (u8*)dst;
    s += offset;

    for(u32 i = 0; i < count; ++i)
    {
        Copy(elementSize, s, d);
        d += elementSize;
        s += stride;
    }

    return dst;
}

internal b32
MemCompare(umm size, const void* a, const void* b)
{
    CompileAssert(sizeof(umm)==sizeof(a));   // verify pointer size
    memory_index chunks = size / sizeof(a);  // Compare by CPU bit width
    memory_index slice = size % sizeof(a);   // remaining bytes < CPU bit width

    umm* l = (umm*)a;
    umm* r = (umm*)b;

    while(chunks--) {
        if(*l++ != *r++) { return false; }
    }

    u8* l8 = (u8*)l;
    u8* r8 = (u8*)r;

    while(slice--) {
        if(*l8++ != *r8++) { return false; }
    }

    return true;
}

#define ZeroStruct(instance) ZeroSize(sizeof(instance), &(instance))
#define ZeroArray(count, array_ptr) ZeroSize((count)*sizeof((array_ptr)[0]), (array_ptr))
#define ZeroBuffer(buffer) ZeroSize(buffer.size, buffer.data)
internal void
ZeroSize(umm size, void* ptr)
{
    CompileAssert(sizeof(umm)==sizeof(ptr));   // verify pointer size
    memory_index chunks = size/sizeof(ptr);
    memory_index slice = size%sizeof(ptr);

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

internal char*
CopyString(const char* src, char* dst, umm maxdstsize)
{
    umm srclength = (umm)StringLength(src);
    umm sizetocopy = srclength < maxdstsize-1 ? srclength : maxdstsize-1;
    Copy(sizetocopy, src, dst);
    dst[sizetocopy] = 0; // put the null terminator at the end
    return dst;
}

internal b32
CompareStrings(const char* l, const char* r)
{
    int l_size = StringLength(l);
    int r_size = StringLength(r);

    if(l_size != r_size)
        return false;

    return MemCompare((umm)l_size, l, r);
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
#define FormatStringV ps_vsprintf

inline b32x IsPow2(u32 Value)
{
    b32x Result = ((Value & ~(Value - 1)) == Value);
    return(Result);
}

#if COMPILER_MSVC
#define CompletePreviousReadsBeforeFutureReads _ReadBarrier()
#define CompletePreviousWritesBeforeFutureWrites _WriteBarrier()
#define YieldProcessor _mm_pause
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

inline u32 AtomicIncrementU32(u32 volatile *Value)
{
    u32 Result = _InterlockedIncrement((long volatile *)Value);

    return (Result);
}

inline u64 AtomicIncrementU64(u64 volatile *Value)
{
    u64 Result = _InterlockedIncrement64((__int64 volatile *)Value);

    return (Result);
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
// TODO(james): is this right for llvm?
#define YieldProcessor _mm_pause
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

inline u64 AtomicIncrementU64(u64 volatile *Value)
{
    u64 Result = __sync_fetch_and_add(Value, 1);
    
    return(Result);
}

inline u32 AtomicIncrementU32(u32 volatile *Value)
{
    u32 Result = __sync_fetch_and_add(Value, 1);

    return (Result);
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

struct ticket_mutex
{
    u64 volatile ticket;
    u64 volatile serving;
};

inline void
BeginTicketMutex(ticket_mutex *mutex)
{
    u64 Ticket = AtomicAddU64(&mutex->ticket, 1);
    while(Ticket != mutex->serving) {YieldProcessor();}
}


inline void
EndTicketMutex(ticket_mutex *mutex)
{
    AtomicAddU64(&mutex->serving, 1);
}

u64 MurmurHash64(const void * key, u32 len, u64 seed)
{
    const u64 m = 0xc6a4a7935bd1e995ULL;
    const u32 r = 47;

    u64 h = seed ^ (len * m);

    const u64 * data = (const u64 *)key;
    const u64 * end = data + (len/8);

    while(data != end)
    {
        #ifdef PLATFORM_BIG_ENDIAN
            u64 k = *data++;
            i8 *p = (i8 *)&k;
            i8 c;
            c = p[0]; p[0] = p[7]; p[7] = c;
            c = p[1]; p[1] = p[6]; p[6] = c;
            c = p[2]; p[2] = p[5]; p[5] = c;
            c = p[3]; p[3] = p[4]; p[4] = c;
        #else
            u64 k = *data++;
        #endif

        k *= m;
        k ^= k >> r;
        k *= m;
        
        h ^= k;
        h *= m;
    }

    const u8 * data2 = (const u8*)data;

    switch(len & 7)
    {
    case 7: h ^= u64(data2[6]) << 48;
    case 6: h ^= u64(data2[5]) << 40;
    case 5: h ^= u64(data2[4]) << 32;
    case 4: h ^= u64(data2[3]) << 24;
    case 3: h ^= u64(data2[2]) << 16;
    case 2: h ^= u64(data2[1]) << 8;
    case 1: h ^= u64(data2[0]);
            h *= m;
    };
    
    h ^= h >> r;
    h *= m;
    h ^= h >> r;

    return h;
}

internal inline constexpr u32
strhash32(const char *s, umm count)
{
    // fnv1a_32
    return count ? (strhash32(s, count - 1) ^ s[count - 1]) * 16777619u : 2166136261u;
}

internal inline constexpr u64
strhash64(const char* s, umm count)
{
    // fnv1a_64
    return count ? (strhash64(s, count - 1) ^ s[count - 1]) * 1099511628211u : 14695981039346656037u;
}

template< umm N >
constexpr u32 StrHash32_T( const char (&s)[N] )
{   
    // NOTE(james): -1 because we don't want to hash the '/0' 
    return strhash32( s, N - 1);
}

template< umm N >
constexpr u64 StrHash64_T( const char (&s)[N] )
{   
    // NOTE(james): -1 because we don't want to hash the '/0' 
    return strhash64( s, N - 1);
}

template<u32 HASH>
struct _ConstHashString32 { enum : u32 { hash = HASH }; };
template<u64 HASH>
struct _ConstHashString64 { enum : u64 { hash = HASH }; };

#define C_HASH(name) _ConstHashString32< StrHash32_T( #name ) >::hash
#define C_HASH64(name) _ConstHashString64< StrHash64_T( #name ) >::hash

inline
u32 wang_hash32(u32 val)
{
    val += ~(val << 15);
    val ^= val >> 10;
    val += val << 3;
    val ^= val >> 6;
    val += ~(val << 11);
    val ^= val >> 16;
    return val;
}

inline
u64 wang_hash64(u64 val)
{
    val = ~val + (val << 21);
    val = val ^ (val >> 24);
    val = (val + (val << 3)) + (val << 8);
    val = val ^ (val >> 14);
    val = (val + (val << 2)) + (val << 4);
    val = val ^ (val >> 28);
    val = val + (val << 31);
    return val;
}

inline
u64 split_hash64(u64 val)
{
    val ^= val >> 30;
    val *= 0xbf58476d1ce4e5b9;
    val ^= val >> 27;
    val *= 0x94d049bb133111eb;
    val ^= val >> 31;
    return val;
}

#define HASH(val) split_hash64(val)