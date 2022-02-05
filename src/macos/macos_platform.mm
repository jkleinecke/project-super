
#include <Cocoa/Cocoa.h>

#include "ps_platform.h"
#include "ps_shared.h"
#include "ps_intrinsics.h"
#include "ps_math.h"
#include "ps_memory.h"
#include "ps_collections.h"
// #include "ps_graphics.h"

#include "macos_platform.h"


#if PROJECTSUPER_INTERNAL
#define LOG(level, msg, ...) Platform.DEBUG_Log(level, __FILE__, __LINE__, msg, ## __VA_ARGS__)
#define LOG_DEBUG(msg, ...) LOG(LogLevel::Debug, msg, ## __VA_ARGS__)
#define LOG_INFO(msg, ...) LOG(LogLevel::Info, msg, ## __VA_ARGS__)
#define LOG_ERROR(msg, ...) LOG(LogLevel::Error, msg, ## __VA_ARGS__)
#else
#define LOG(level, msg, ...) Platform.Log(level, msg, ## __VA_ARGS__)
#define LOG_DEBUG(msg, ...) 
#define LOG_INFO(msg, ...) LOG(LogLevel::Info, msg, ## __VA_ARGS__)
#define LOG_ERROR(msg, ...) LOG(LogLevel::Error, msg, ##  __VA_ARGS__)
#endif

#include "../vulkan/vk_platform.cpp"   

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <copyfile.h>
#include <libproc.h>
#include <time.h>
#include <dlfcn.h>

global_variable bool32 GlobalRunning = true;
global_variable macos_state GlobalMacosState;
platform_api Platform;

//------------------------
//---- LOGGING
//------------------------

internal void 
MacosLog(LogLevel level, const char* format, ...)
{
	va_list args;
    va_start(args, format);

	char szMessage[4096];
    int n = FormatStringV(szMessage, format, args);
    va_end(args);
    
	char *szLevel = 0;
	switch(level)
    {
    case LogLevel::Debug:
		szLevel = "DBG";
        break;
    case LogLevel::Info:
		szLevel = "INF";
        break;
    case LogLevel::Error:
		szLevel = "ERR";
        break;
    }
	NSLog(@"%s | %s", szLevel, szMessage);
}

internal void
MacosDebugLog(LogLevel level, const char* file, int lineno, const char* format, ...)
{
	va_list args;
    va_start(args, format);

	char szMessage[4096];
    int n = FormatStringV(szMessage, format, args);
    va_end(args);

	char *szLevel = 0;
	switch(level)
    {
    case LogLevel::Debug:
		szLevel = "DBG";
        break;
    case LogLevel::Info:
		szLevel = "INF";
        break;
    case LogLevel::Error:
		szLevel = "ERR";
        break;
    }
	NSLog(@"%s | %s(%d) | %s", szLevel, file, lineno, szMessage);
}


//------------------------
//---- MEMORY
//------------------------

#if PROJECTSUPER_INTERNAL
internal debug_platform_memory_stats
MacosGetMemoryStats()
{
    debug_platform_memory_stats stats = {};

    BeginTicketMutex(&GlobalMacosState.memoryMutex);
    macos_memory_block* sentinal = &GlobalMacosState.memorySentinal;
    for(macos_memory_block* block = sentinal->next; block != sentinal; block = block->next)
    {
        // make sure we don't have any obviously bad allocations
        ASSERT(block->block.size <= U32MAX);

        stats.totalSize += block->block.size;
        stats.totalUsed += block->block.used;
    } 
    EndTicketMutex(&GlobalMacosState.memoryMutex);

    return stats;
}
#endif

inline bool32
MacosIsInLoop(macos_state& state)
{
    //return state.runMode == RunLoopMode::Playback;
	return false;
}

internal platform_memory_block*
MacosAllocateMemoryBlock(memory_index size, PlatformMemoryFlags flags)
{
	// NOTE(james): We require memory block headers not to change the cache
    // line alignment of an allocation
    CompileAssert(sizeof(macos_memory_block) == 128);

	const umm pageSize = 4096; // TODO(james): Query from system?
    umm totalSize = size + sizeof(macos_memory_block);
    umm baseOffset = sizeof(macos_memory_block);
    umm protectOffset = 0;

	if(IS_FLAG_BIT_SET(flags, PlatformMemoryFlags::UnderflowCheck))
	{
		totalSize = size + 2*pageSize;
		baseOffset = 2*pageSize;
		protectOffset = pageSize;
	}
	else if(IS_FLAG_BIT_SET(flags, PlatformMemoryFlags::OverflowCheck))
	{
		umm sizeRoundedUp = AlignPow2(size, pageSize);
		totalSize = sizeRoundedUp + 2*pageSize;
		baseOffset = pageSize + sizeRoundedUp - size;
		protectOffset = pageSize + sizeRoundedUp;
	}

	macos_memory_block *block = (macos_memory_block*)mmap(0, totalSize,
									PROT_READ | PROT_WRITE,
									MAP_PRIVATE | MAP_ANON,
									-1, 0);
	if(block == MAP_FAILED)
	{
		LOG_ERROR("OSXAllocateMemory: mmap error: %d  %s", errno, strerror(errno));
		ASSERT(false);
	}
	block->totalAllocatedSize = totalSize;
	block->block.base = (u8 *)block + baseOffset;
	ASSERT(block->block.used == 0);
	ASSERT(block->block.prev_block == 0);

	if(IS_FLAG_BIT_SET(flags, PlatformMemoryFlags::OverflowCheck) || IS_FLAG_BIT_SET(flags,PlatformMemoryFlags::UnderflowCheck))
	{
		int result = mprotect((u8*)block + protectOffset, pageSize, PROT_NONE);
		if (result != 0)
		{
		}
		else
		{
			LOG_ERROR("OSXAllocateMemory: Underflow mprotect error: %d  %s", errno, strerror(errno));
			ASSERT(false);
		}
	}

	macos_memory_block *sentinal = &GlobalMacosState.memorySentinal;
	block->next = sentinal;
	block->block.size = size;
	block->block.flags = flags;
	block->loopingFlags = MemoryLoopingFlags::None;
	if(MacosIsInLoop(GlobalMacosState) && IS_FLAG_BIT_NOT_SET(flags, PlatformMemoryFlags::NotRestored))
	{
		block->loopingFlags = MemoryLoopingFlags::Allocated;
	}

	BeginTicketMutex(&GlobalMacosState.memoryMutex);
	block->prev = sentinal->prev;
	block->prev->next = block;
	block->next->prev = block;
	EndTicketMutex(&GlobalMacosState.memoryMutex);
	
	platform_memory_block* platformBlock = &block->block;
	return platformBlock;
}

internal void
MacosFreeMemoryBlock(macos_memory_block *block)
{
	BeginTicketMutex(&GlobalMacosState.memoryMutex);
	block->prev->next = block->next;
	block->next->prev = block->prev;
	EndTicketMutex(&GlobalMacosState.memoryMutex);

	// NOTE(james): For porting to other platforms that need the size to unmap
    // pages, you can get it from Block->Block.Size!
	if (munmap(block, block->totalAllocatedSize) != 0)
	{
		LOG_ERROR("OSXFreeMemoryBlock: munmap error: %d  %s", errno, strerror(errno));
		ASSERT(false);
	}
}

internal void
MacosDeallocatedMemoryBlock(platform_memory_block* block)
{
	if(block)
	{
		macos_memory_block *macBlock ((macos_memory_block*)block);
		if(MacosIsInLoop(GlobalMacosState) && IS_FLAG_BIT_NOT_SET(macBlock->block.flags, PlatformMemoryFlags::NotRestored))
		{
			macBlock->loopingFlags = MemoryLoopingFlags::Deallocated;
		}
		else
		{
			MacosFreeMemoryBlock(macBlock);
		}
	}
}

internal void
MacosClearMemoryBlocksByMask(macos_state& state, MemoryLoopingFlags mask)
{
	BeginTicketMutex(&state.memoryMutex);
	macos_memory_block* sentinal = &state.memorySentinal;
	for(macos_memory_block* block_iter = sentinal->next; block_iter != sentinal; )
	{
		macos_memory_block* block = block_iter;
		block_iter = block->next;

		if((block->loopingFlags & mask) == mask)
		{
			MacosFreeMemoryBlock(block);
		}
		else
		{
			block->loopingFlags = MemoryLoopingFlags::None;
		}
	}
	EndTicketMutex(&state.memoryMutex);
}

//------------------------
//---- FILE I/O
//------------------------

internal void
MacosSetupFileLocationsTable(macos_state &state)
{
	// TODO(james): Put user folder somewhere in the users home folder..

	state.fileLocationsTable[(u32)FileLocation::Content].location = FileLocation::Content;
	FormatString(state.fileLocationsTable[(u32)FileLocation::Content].folder, FILENAME_MAX, "%s../data/", state.appFolder);

	state.fileLocationsTable[(u32)FileLocation::User].location = FileLocation::User;
	FormatString(state.fileLocationsTable[(u32)FileLocation::User].folder, FILENAME_MAX, "%s", state.appFolder);

	state.fileLocationsTable[(u32)FileLocation::Diagnostic].location = FileLocation::Diagnostic;
	FormatString(state.fileLocationsTable[(u32)FileLocation::Diagnostic].folder, FILENAME_MAX, "%s", state.appFolder);
}

internal void
MacosGetAppFilename(macos_state &state)
{
	pid_t PID = getpid();
	int r = proc_pidpath(PID, state.appFilename, sizeof(state.appFilename));

	if (r <= 0)
	{
		LOG_ERROR("Error getting process path: pid %d: %s\n", (int)PID, strerror(errno));
	}
	else
	{
		LOG_INFO("process pid: %d   path: %s\n", PID, state.appFilename);
	}

    char* onePastLastAppFilenameSlash = state.appFilename;
    for(char *scan = state.appFilename;
        *scan;
        ++scan)
    {
        if(*scan == '/')
        {
            onePastLastAppFilenameSlash = scan + 1;
        }
    }

	Copy(PtrToUMM(onePastLastAppFilenameSlash) - PtrToUMM(state.appFilename), state.appFilename, state.appFolder);
}

internal platform_file
MacosOpenFile(FileLocation location, const char* filename, FileUsage usage)
{
	platform_file result{};
	int flags = 0;

	if ((usage & FileUsage::ReadWrite) == FileUsage::ReadWrite)
	{
		flags |= O_RDWR;
	}

	if((usage & FileUsage::Write) == FileUsage::Write)
	{
		// will be true for read/write as well
		flags |= O_CREAT;
	}
	else if(usage == FileUsage::Read)
	{
		// it's read-only here
		flags |= O_RDONLY;
	}

	char filepath[FILENAME_MAX];
	FormatString(filepath, FILENAME_MAX, "%s%s", GlobalMacosState.fileLocationsTable[(u32)location].folder, filename);

	struct stat fileStats;
	if(stat(filepath, &fileStats) == 0)
	{
		result.size = fileStats.st_size;
	}

	int fd = open(filepath, flags, 0600);
	result.error = (fd == -1);
	*(int*)&result.platform = fd;

	if(fd == -1)
	{
		LOG_ERROR("Error opening file |%s|: %d\n", filepath, errno);
		perror(NULL);
	}

	return result;
}

internal u64
MacosReadFile(platform_file& file, void* buffer, u64 size)
{
	if(file.error)
	{
		ASSERT(false);
		return 0;
	}

	int handle = *(int*)&file.platform;

	// TODO(james): consider using mmap for an overlapped i/o like file read
	u64 bytes = read(handle, buffer, size);

	return bytes;
}

internal u64
MacosWriteFile(platform_file& file, const void* buffer, u64 size)
{
	if(file.error)
	{
		ASSERT(false);
		return 0;
	}

	int handle = *(int*)&file.platform;

	u64 bytes = write(handle, buffer, size);

	return bytes;
}

internal void
MacosCloseFile(platform_file& file)
{
	int handle = *(int*)&file.platform;
	if(handle != -1)
	{
		close(handle);
	}
}

internal time_t
MacosGetLastWriteTime(const char* filename)
{
	time_t lastWriteTime = 0;

	struct stat fileTime;
	if(stat(filename, &fileTime) == 0)
	{
		lastWriteTime = fileTime.st_mtimespec.tv_sec;
	}

	return lastWriteTime;
}

//------------------------
//---- CODE LOADING
//------------------------

internal void
MacosUnloadCode(macos_loaded_code& code)
{
	if(code.DL)
	{
		// NOTE(james): may start having trouble unloading
		// code that we have pointers into, (like strings?)
		// in that case just never unload the code.  Will
		// only be an issue during development...
		dlclose(code.DL);

		code.DL = 0;
	}

	code.isValid = false;
	ZeroArray(code.nFunctionCount, code.ppFunctions);
}

internal void
MacosLoadCode(macos_state& state, macos_loaded_code& code)
{
	// TODO(james): generate a truly unique temp path name here...
	char szTempPath[FILENAME_MAX];
	FormatString(szTempPath, FILENAME_MAX, "%s%s", state.appFolder, code.pszTransientDLName);

	code.DLLastWriteTime = MacosGetLastWriteTime(code.szDLFullPath);
	int result = copyfile(code.szDLFullPath, szTempPath, 0, COPYFILE_ALL);
	ASSERT(result == 0);

	code.DL = dlopen(code.szDLFullPath, RTLD_LAZY|RTLD_GLOBAL);
	// code.DL = dlopen(szTempPath, RTLD_LAZY|RTLD_GLOBAL);

	if(code.DL)
	{
		LOG_INFO("Successful load of %s", code.szDLFullPath);

		code.isValid = true;

		for(u32 functionIndex = 0; functionIndex < code.nFunctionCount; functionIndex++)
		{
			void* function = dlsym(code.DL, code.ppszFunctionNames[functionIndex]);
			if(function)
			{
				code.ppFunctions[functionIndex] = function;
			}
			else
			{
				code.isValid = false;
			}
		}
	}
	else
	{
		code.isValid = false;
		LOG_ERROR("Error loading of %s", code.szDLFullPath);
	}

	if(!code.isValid)
	{
		MacosUnloadCode(code);
	}
}

internal b32x
MacosCheckForCodeChange(macos_loaded_code& code)
{
	time_t lastWriteTime = MacosGetLastWriteTime(code.szDLFullPath);
	return lastWriteTime != code.DLLastWriteTime;
}

internal void
MacosReloadCode(macos_state& state, macos_loaded_code& code)
{
	// TODO(james): make this work
	// MacosUnloadCode(code);

	// MacosLoadCode(state, code);
}

//------------------------
//---- WORK QUEUE
//------------------------



//------------------------
//---- AUDIO
//------------------------



//------------------------
//---- OS MESSAGES
//------------------------

void MacosProcessMessages(macos_state& state)
{
    NSEvent* Event;

	//ZeroStruct(GameData->NewInput->FKeyPressed);
	//memcpy(GameData->OldKeyboardState, GameData->KeyboardState, sizeof(GameData->KeyboardState));

	do
	{
		Event = [NSApp nextEventMatchingMask:NSUIntegerMax
									untilDate:nil
										inMode:NSDefaultRunLoopMode
									dequeue:YES];

		switch ([Event type])
		{
/*
			case NSKeyDown:
			case NSKeyUp:
			{
				u32 KeyCode = [Event keyCode];
				unichar C = [[Event charactersIgnoringModifiers] characterAtIndex:0];
				u32 ModifierFlags = [Event modifierFlags];
				int CommandKeyFlag = (ModifierFlags & NSCommandKeyMask) > 0;
				int ControlKeyFlag = (ModifierFlags & NSControlKeyMask) > 0;
				int AlternateKeyFlag = (ModifierFlags & NSAlternateKeyMask) > 0;
				int ShiftKeyFlag = (ModifierFlags & NSShiftKeyMask) > 0;

				int KeyDownFlag = ([Event type] == NSKeyDown) ? 1 : 0;
				GameData->KeyboardState[KeyCode] = KeyDownFlag;

				//printf("%s: keyCode: %d  unichar: %c\n", NSKeyDown ? "KeyDown" : "KeyUp", [Event keyCode], C);

				OSXKeyProcessing(KeyDownFlag, KeyCode, C,
								ShiftKeyFlag, CommandKeyFlag, ControlKeyFlag, AlternateKeyFlag,
								GameData->NewInput, GameData);
			} break;

			case NSFlagsChanged:
			{
				u32 KeyCode = 0;
				u32 ModifierFlags = [Event modifierFlags];
				int CommandKeyFlag = (ModifierFlags & NSCommandKeyMask) > 0;
				int ControlKeyFlag = (ModifierFlags & NSControlKeyMask) > 0;
				int AlternateKeyFlag = (ModifierFlags & NSAlternateKeyMask) > 0;
				int ShiftKeyFlag = (ModifierFlags & NSShiftKeyMask) > 0;

				GameData->KeyboardState[kVK_Command] = CommandKeyFlag;
				GameData->KeyboardState[kVK_Control] = ControlKeyFlag;
				GameData->KeyboardState[kVK_Alternate] = AlternateKeyFlag;
				GameData->KeyboardState[kVK_Shift] = ShiftKeyFlag;

				int KeyDownFlag = 0;

				if (CommandKeyFlag != GameData->OldKeyboardState[kVK_Command])
				{
					KeyCode = kVK_Command;

					if (CommandKeyFlag)
					{
						KeyDownFlag = 1;
					}
				}

				if (ControlKeyFlag != GameData->OldKeyboardState[kVK_Control])
				{
					KeyCode = kVK_Control;

					if (ControlKeyFlag)
					{
						KeyDownFlag = 1;
					}
				}

				if (AlternateKeyFlag != GameData->OldKeyboardState[kVK_Alternate])
				{
					KeyCode = kVK_Option;

					if (AlternateKeyFlag)
					{
						KeyDownFlag = 1;
					}
				}

				if (ShiftKeyFlag != GameData->OldKeyboardState[kVK_Shift])
				{
					if (ShiftKeyFlag)
					{
						KeyDownFlag = 1;
					}

					KeyCode = kVK_Shift;
				}

				//printf("Keyboard flags changed: Cmd: %d  Ctrl: %d  Opt: %d  Shift: %d\n",
			//			CommandKeyFlag, ControlKeyFlag, AlternateKeyFlag, ShiftKeyFlag);

				OSXKeyProcessing(-1, KeyCode, KeyCode,
								ShiftKeyFlag, CommandKeyFlag, ControlKeyFlag, AlternateKeyFlag,
								GameData->NewInput, GameData);
			} break;
*/
			default:
				[NSApp sendEvent:Event];
		}
	} while (Event != nil);

}


//------------------------
//---- MAIN LOOP
//------------------------

int main(int argc, const char* argv[])
{
	SetDefaultFPBehavior();

    @autoreleasepool
    {
		Platform.Log = &MacosLog;

		Platform.AllocateMemoryBlock = &MacosAllocateMemoryBlock;
		Platform.DeallocateMemoryBlock = &MacosDeallocatedMemoryBlock;

		Platform.OpenFile = &MacosOpenFile;
		Platform.ReadFile = &MacosReadFile;
		Platform.WriteFile = &MacosWriteFile;
		Platform.CloseFile = &MacosCloseFile;

#if PROJECTSUPER_INTERNAL
		Platform.DEBUG_Log = &MacosDebugLog;
		Platform.DEBUG_GetMemoryStats = &MacosGetMemoryStats;
#endif

        macos_state& state = GlobalMacosState;
		state.appName = @"Project Super";
		// NOTE(james): memory sentinal needs to point back to itself to establish the memory ring
		macos_memory_block* sentinal = &state.memorySentinal;
		sentinal->next = sentinal;
		sentinal->prev = sentinal;

		MacosGetAppFilename(state);
		MacosSetupFileLocationsTable(state);

        f32 windowWidth = 1280.0f;
        f32 windowHeight = 720.0f;

        // Cocoa boilerplate startup code
        NSApplication* app = [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        // Set working directory
        NSString *dir = [[NSFileManager defaultManager] currentDirectoryPath];

        NSFileManager* FileManager = [NSFileManager defaultManager];
        state.workingDirectory = [NSString stringWithFormat:@"%@/Contents/Resources",
            [[NSBundle mainBundle] bundlePath]];
        // if ([FileManager changeCurrentDirectoryPath:state.workingDirectory] == NO)
        // {
        //     ASSERT(0);
        // }
        NSLog(@"working directory: %@", state.workingDirectory);

        //macContext.AppDelegate = [[HandmadeAppDelegate alloc] init];
        //[app setDelegate:Context.AppDelegate];

        [NSApp finishLaunching];

        // Create the main application window
        NSRect ScreenRect = [[NSScreen mainScreen] frame];

        NSRect InitialFrame = NSMakeRect((ScreenRect.size.width - windowWidth) * 0.5,
                                        (ScreenRect.size.height - windowHeight) * 0.5,
                                        windowWidth,
                                        windowHeight);

        NSWindow* Window = [[NSWindow alloc] initWithContentRect:InitialFrame
                                        styleMask:NSWindowStyleMaskTitled
                                                    | NSWindowStyleMaskClosable
                                                    | NSWindowStyleMaskMiniaturizable
                                                    | NSWindowStyleMaskResizable
                                        backing:NSBackingStoreBuffered
                                        defer:NO];

        [Window setBackgroundColor: NSColor.redColor];
        //[Window setDelegate:Context.AppDelegate];

        NSView* CV = [Window contentView];
        [CV setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
        [CV setAutoresizesSubviews:YES];

		// TODO(james): load this more gracefully
		NSBundle* bundle = [NSBundle bundleWithPath:@"/System/Library/Frameworks/QuartzCore.framework"];
        ASSERT(bundle);

		// NOTE: Create the layer here as makeBackingLayer should not return nil
        CAMetalLayer* layer = (CAMetalLayer*)[[bundle classNamed:@"CAMetalLayer"] layer];
		ASSERT(layer)

		// TODO(james): only required for a retina display
		[layer setContentsScale:[Window backingScaleFactor]];

		[CV setLayer: (CALayer*)layer];
		[CV setWantsLayer: YES];

        [Window setMinSize:NSMakeSize(160, 90)];
        [Window setTitle:state.appName];
        [Window makeKeyAndOrderFront:nil];

		game_memory gameMemory = {};
		render_context gameRender = {};
		InputContext input = {};
		AudioContext audio = {};

		gameMemory.platformApi = Platform;

		ps_graphics_backend graphicsDriver = platform_load_graphics_backend(layer, windowWidth, windowHeight);
		gfx_api& gfx = graphicsDriver.gfx; 
		gameRender.renderDimensions.Width = windowWidth;
		gameRender.renderDimensions.Height = windowHeight;
		gameRender.gfx = gfx;

		macos_game_function_table gameFunctions = {};
		macos_loaded_code gameCode = {};
		FormatString(gameCode.szDLFullPath,sizeof(gameCode.szDLFullPath), "%s%s", state.appFolder, "ps_game.dylib");
		gameCode.pszTransientDLName = "temp_ps_game.dylib";
		gameCode.nFunctionCount = ARRAY_COUNT(MacosGameFunctionTableNames);
		gameCode.ppFunctions = (void**)&gameFunctions;
		gameCode.ppszFunctionNames = (char**)&MacosGameFunctionTableNames;

		MacosLoadCode(GlobalMacosState, gameCode);
		ASSERT(gameCode.isValid);

		u64 CurrentTime = clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
		u64 lastFrameStartTime = CurrentTime;

        while(GlobalRunning)
        {
			if(MacosCheckForCodeChange(gameCode))
			{
				MacosReloadCode(GlobalMacosState, gameCode);
			}

            MacosProcessMessages(state);

			if(gameFunctions.UpdateAndRender)
			{
				gameFunctions.UpdateAndRender(gameMemory, gameRender, input, audio);
			}

			u64 gameSimTime = clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
			f32 elapsedFrameTime = (f32)(gameSimTime - lastFrameStartTime) / 1000000000.0f ;
			lastFrameStartTime = gameSimTime;

			input.clock.totalTime += elapsedFrameTime;
			input.clock.elapsedFrameTime = elapsedFrameTime;

			// TODO(james): Sleep if we are running faster than the target framerate??

			LOG_DEBUG("Frame Time: %.2f ms, Total Time: %.2f ms", elapsedFrameTime * 1000.0f, elapsedFrameTime * 1000.0f);

			++input.clock.frameCounter;
        }

		// TODO(james): verify that we can unload the graphics resources properly
		platform_unload_graphics_backend(&graphicsDriver);
    }

    return 0;
}