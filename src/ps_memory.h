
/*******************************************************************************

    Take token and Reset to token can be used to allocate temporary
    scratch memory area and then quickly reset back once you are
    done with it

********************************************************************************/

// TODO(james): Move the memory arena + PushXXX definition in here and account for memory alignment

struct memory_token
{
    void* freePointer;

    // TODO(james): Store debug info?? file and lineno..
};

internal inline
memory_token TakeMemoryToken(MemoryArena& arena)
{
    memory_token token = { arena.freePointer };
    return token;
}

internal inline
void ResetArenaToMemoryToken(MemoryArena& arena, memory_token& token)
{
    arena.freePointer = token.freePointer;
}