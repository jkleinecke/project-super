

struct macos_loaded_code
{
    b32x isValid;
	u32 nTempDLNumber;

	const char* pszTransientDLName;
	const char* pszDLFullPath;
	// const char* LockFullPath;

	void* DL;
	time_t DLLastWriteTime;

	u32 nFunctionCount;
	char** ppszFunctionNames;
	void** ppFunctions;
};


struct macos_game_function_table
{
	game_update_and_render* UpdateAndRender;
};


global char* MacosGameFunctionTableNames[] =
{
	"GameUpdateAndRender"
};

enum class MemoryLoopingFlags
{
    None,
    Allocated   = 0x1,
    Deallocated = 0x2,
};
MAKE_ENUM_FLAG(u32, MemoryLoopingFlags);

struct macos_saved_memory_block
{
    u64 base_pointer;
    u64 size;
};

struct macos_memory_block
{
    platform_memory_block block;
    macos_memory_block* next;
    macos_memory_block* prev;
    MemoryLoopingFlags loopingFlags;

    u64 totalAllocatedSize;
    u64 pad[7];
};

struct macos_state
{
    ticket_mutex memoryMutex;
    macos_memory_block memorySentinal;

    // TODO(james): setup app delegate
    NSWindow* window;
    NSString* appName;
    NSString* workingDirectory;
};