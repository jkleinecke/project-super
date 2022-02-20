
/*******************************************************************************

    Take token and Reset to token can be used to allocate temporary
    scratch memory area and then quickly reset back once you are
    done with it

********************************************************************************/

struct memory_arena
{
    platform_memory_block* currentBlock;
    umm minimumBlockSize;

    PlatformMemoryFlags allocationFlags;
    s32 tempCount;
};

struct temporary_memory
{
    memory_arena* arena;
    platform_memory_block* block;
    umm used;
};

inline void
SetMinimumBlockSize(memory_arena& arena, umm minimumSize)
{
    arena.minimumBlockSize = minimumSize;
}

inline memory_index
GetAlignmentOffset(const memory_arena& arena, memory_index alignment)
{
    memory_index alignment_offset = 0;

    memory_index ptr = (memory_index)arena.currentBlock->base + arena.currentBlock->used;
    memory_index alignment_mask = alignment - 1;
    if(ptr & alignment_mask)
    {
        alignment_offset = alignment - (ptr & alignment_mask);
    }

    return alignment_offset;
}

enum class ArenaPushFlags
{
    None    = 0,
    Clear   = 0x01,
    NoClear = ~Clear,
    All     = S32MAX
};
MAKE_ENUM_FLAG(u32, ArenaPushFlags);

struct arena_push_params
{
    ArenaPushFlags flags;
    u32 alignment;
};

inline arena_push_params
DefaultArenaParams()
{
    arena_push_params params = {};
    params.flags = ArenaPushFlags::Clear;
    params.alignment = 4;
    return params;
}

inline arena_push_params
AlignNoClear(u32 alignment)
{
    arena_push_params params = {};
    params.flags = ArenaPushFlags::NoClear;
    params.alignment = alignment;
    return params;
}

inline arena_push_params
Align(u32 alignment, b32 clear)
{
    arena_push_params params = {};
    params.flags = clear ? ArenaPushFlags::Clear : ArenaPushFlags::NoClear;
    params.alignment = alignment;
    return params;
}

inline arena_push_params
NoClear()
{
    arena_push_params params = {};
    params.flags = ArenaPushFlags::NoClear;
    return params;
}

struct arena_bootstrap_params
{
    PlatformMemoryFlags allocationFlags;
    umm initialAllocatedSize;
    umm minimumBlockSize;
};

inline arena_bootstrap_params
DefaultBootstrapParams(void)
{
    arena_bootstrap_params Params = {};
    return(Params);
}

inline arena_bootstrap_params
NonRestoredArena(umm initialSize = 0, umm minBlockSize = 0)
{
    arena_bootstrap_params Params = DefaultBootstrapParams();
    Params.allocationFlags = PlatformMemoryFlags::NotRestored;
    Params.initialAllocatedSize = initialSize;
    Params.minimumBlockSize = minBlockSize;
    return(Params);
}


// TODO(james): Implement memory allocation debug hooks
#define DEBUG_RECORD_ALLOCATION(...)
#define DEBUG_RECORD_BLOCK_ALLOCATION(...)
#define DEBUG_RECORD_BLOCK_FREE(...)
#define DEBUG_RECORD_BLOCK_TRUNCATE(...)

#if 0 //defined(PROJECTSUPER_INTERNAL)
#define DEBUG_MEMORY_NAME(Name) DEBUG_NAME_(__FILE__, __LINE__, __COUNTER__),
#define INTERNAL_MEMORY_PARAM char *GUID,
#define INTERNAL_MEMORY_PASS GUID,
#else
#define DEBUG_MEMORY_NAME(Name)
#define INTERNAL_MEMORY_PARAM 
#define INTERNAL_MEMORY_PASS 
#endif

#define PushStruct(Arena, type, ...) (type *)PushSize_(DEBUG_MEMORY_NAME("PushStruct") Arena, sizeof(type), ## __VA_ARGS__)
#define PushArray(Arena, Count, type, ...) (type *)PushSize_(DEBUG_MEMORY_NAME("PushArray") Arena, (Count)*sizeof(type), ## __VA_ARGS__)
#define PushSize(Arena, Size, ...) PushSize_(DEBUG_MEMORY_NAME("PushSize") Arena, Size, ## __VA_ARGS__)
#define PushCopy(...) PushCopy_(DEBUG_MEMORY_NAME("PushCopy") __VA_ARGS__)
#define PushStringZ(...) PushStringZ_(DEBUG_MEMORY_NAME("PushStringZ") __VA_ARGS__)
#define PushString(...) PushString_(DEBUG_MEMORY_NAME("PushString") __VA_ARGS__)
#define PushBuffer(...) PushBuffer_(DEBUG_MEMORY_NAME("PushBuffer") __VA_ARGS__)
#define PushAndNullTerminate(...) PushAndNullTerminate_(DEBUG_MEMORY_NAME("PushAndNullTerminate") __VA_ARGS__)
#define BootstrapPushStructMember(type, Member, ...) (type *)BootstrapPushSize_(DEBUG_MEMORY_NAME("BootstrapPushStructMember") sizeof(type), OffsetOf(type, Member), ## __VA_ARGS__)
#define BootstrapScratchArena(memName, ...) (memory_arena *)BootstrapPushSize_(DEBUG_MEMORY_NAME(memName) sizeof(memory_arena), 0, ## __VA_ARGS__)

inline memory_index
GetEffectiveSizeFor(const memory_arena& arena, memory_index sizeInit, arena_push_params params = DefaultArenaParams())
{
    memory_index size = sizeInit;
    
    memory_index alignmentOffset = GetAlignmentOffset(arena, params.alignment);
    size += alignmentOffset;
    
    return(size);
}

inline void *
PushSize_(INTERNAL_MEMORY_PARAM
          memory_arena &arena, memory_index sizeInit, arena_push_params params = DefaultArenaParams())
{
    void *result = 0;
    
    ASSERT(params.alignment <= 128);
    ASSERT(IsPow2(params.alignment));
    
    memory_index size = 0;
    if(arena.currentBlock)
    {
        size = GetEffectiveSizeFor(arena, sizeInit, params);
    }
    
    if(!arena.currentBlock ||
       ((arena.currentBlock->used + size) > arena.currentBlock->size))
    {
        size = sizeInit; // NOTE(james): The base will automatically be aligned now!
        if(IS_ANY_FLAG_SET(arena.allocationFlags, PlatformMemoryFlags::OverflowCheck|PlatformMemoryFlags::UnderflowCheck))
        {
            arena.minimumBlockSize = 0;
            size = AlignPow2(size, params.alignment);
        }
        else if(!arena.minimumBlockSize)
        {
            // TODO(james): Tune default block size eventually?
            arena.minimumBlockSize = Kilobytes(64);
        }
        
        memory_index blockSize = Maximum(size, arena.minimumBlockSize);
        
        platform_memory_block *newBlock = Platform.AllocateMemoryBlock(blockSize, arena.allocationFlags);
        newBlock->prev_block = arena.currentBlock;
        arena.currentBlock = newBlock;
        DEBUG_RECORD_BLOCK_ALLOCATION(arena.currentBlock); 
    }    
    
    ASSERT((arena.currentBlock->used + size) <= arena.currentBlock->size);
    
    memory_index alignmentOffset = GetAlignmentOffset(arena, params.alignment);
    umm offsetInBlock = arena.currentBlock->used + alignmentOffset;
    result = arena.currentBlock->base + offsetInBlock;
    arena.currentBlock->used += size;
    
    ASSERT(size >= sizeInit);
    
    // NOTE(james): This is just to guarantee that nobody passed in an alignment
    // on their first allocation that was _greater_ that than the page alignment
    ASSERT(arena.currentBlock->used <= arena.currentBlock->size);
    
    if(IS_FLAG_BIT_SET(params.flags, ArenaPushFlags::Clear))
    {
        ZeroSize(sizeInit, result);
    }
    
    DEBUG_RECORD_ALLOCATION(arena.currentBlock, GUID, size, sizeInit, offsetInBlock);
    
    return result;
}

internal void *
PushCopy_(INTERNAL_MEMORY_PARAM
          memory_arena& arena, umm size, const void *source, arena_push_params params = DefaultArenaParams())
{
    void *result = PushSize_(INTERNAL_MEMORY_PASS arena, size, params);
    Copy(size, source, result);
    return result;
}

// NOTE(james): This is generally not for production use, this is probably
// only really something we need during testing, but who knows
inline char *
PushStringZ_(INTERNAL_MEMORY_PARAM
             memory_arena& arena, const char *source)
{
    u32 size = 1;
    // NOTE(james): figure out how big the source string is...
    for(const char *at = source;
        *at;
        ++at)
    {
        ++size;
    }
    
    char *dest = (char *)PushSize_(INTERNAL_MEMORY_PASS arena, size, NoClear());
    for(u32 charIndex = 0;
        charIndex < size;
        ++charIndex)
    {
        dest[charIndex] = source[charIndex];
    }
    
    return dest;
}

internal buffer
PushBuffer_(INTERNAL_MEMORY_PARAM
            memory_arena& arena, umm size, arena_push_params params = DefaultArenaParams())
{
    buffer result;
    result.size = size;
    result.data = (u8 *)PushSize_(INTERNAL_MEMORY_PASS arena, result.size, params);
    
    return result ;
}

internal string
PushString_(INTERNAL_MEMORY_PARAM
            memory_arena &arena, const char *source)
{
    string result;
    result.size = StringLength(source);
    result.data = (u8 *)PushCopy_(INTERNAL_MEMORY_PASS arena, result.size, source);
    
    return result;
}

internal string
PushString_(INTERNAL_MEMORY_PARAM
            memory_arena& arena, const string& source)
{
    string result;
    result.size = source.size;
    result.data = (u8 *)PushCopy_(INTERNAL_MEMORY_PASS arena, result.size, source.data);
    
    return result;
}

inline char *
PushAndNullTerminate_(INTERNAL_MEMORY_PARAM
                      memory_arena& arena, u32 length, const char *source)
{
    char *dest = (char *)PushSize_(INTERNAL_MEMORY_PASS arena, length + 1, NoClear());
    for(u32 charIndex = 0;
        charIndex < length;
        ++charIndex)
    {
        dest[charIndex] = source[charIndex];
    }
    dest[length] = 0;
    
    return dest;
}

inline temporary_memory
BeginTemporaryMemory(memory_arena& arena)
{
    temporary_memory result;
    
    result.arena = &arena;
    result.block = arena.currentBlock;
    result.used = arena.currentBlock ? arena.currentBlock->used : 0;
    
    ++arena.tempCount;
    
    return result;
}

inline void
FreeLastBlock(memory_arena& arena)
{
    platform_memory_block *free = arena.currentBlock;
    DEBUG_RECORD_BLOCK_FREE(free);
    arena.currentBlock = free->prev_block;
    Platform.DeallocateMemoryBlock(free);
}

inline void
EndTemporaryMemory(const temporary_memory& tempMem)
{
    memory_arena& arena = *tempMem.arena;
    while(arena.currentBlock != tempMem.block)
    {
        FreeLastBlock(arena);
    }
    
    if(arena.currentBlock)
    {
        ASSERT(arena.currentBlock->used >= tempMem.used);
        arena.currentBlock->used = tempMem.used;
        DEBUG_RECORD_BLOCK_TRUNCATE(arena.currentBlock);
    }
    
    ASSERT(arena.tempCount > 0);
    --arena.tempCount;
}

inline void
KeepTemporaryMemory(const temporary_memory& tempMem)
{
    memory_arena *arena = tempMem.arena;
    
    ASSERT(arena->tempCount > 0);
    --arena->tempCount;
}

inline void
Clear(memory_arena& arena)
{
    while(arena.currentBlock)
    {
        // NOTE(james): Because the arena itself may be stored in the last block,
        // we must ensure that we don't look at it after freeing.
        b32 ThisIsLastBlock = (arena.currentBlock->prev_block == 0);
        FreeLastBlock(arena);
        if(ThisIsLastBlock)
        {
            break;
        }
    }
}

inline void
CheckArena(const memory_arena& arena)
{
    ASSERT(arena.tempCount == 0);
}

inline void *
BootstrapPushSize_(INTERNAL_MEMORY_PARAM umm structSize, umm offsetToArena,
                   arena_bootstrap_params bootstrapParams = DefaultBootstrapParams(), 
                   arena_push_params params = DefaultArenaParams())
{
    memory_arena bootstrap = {};
    bootstrap.allocationFlags = bootstrapParams.allocationFlags;
    // NOTE(james): can be beneficially to allocate a bigger/smaller block than the 
    //   normal min block size initially
    bootstrap.minimumBlockSize = bootstrapParams.initialAllocatedSize; 

    void *structptr = PushSize_(INTERNAL_MEMORY_PASS bootstrap, structSize, params);
    
    // After the initial allocation, resume normal practices
    bootstrap.minimumBlockSize = bootstrapParams.minimumBlockSize;
    *(memory_arena *)((u8 *)structptr + offsetToArena) = bootstrap;
    
    return structptr;
}
