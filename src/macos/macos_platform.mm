
#include <Cocoa/Cocoa.h>

#include "ps_platform.h"
#include "ps_shared.h"
#include "ps_intrinsics.h"
#include "ps_math.h"
#include "ps_memory.h"

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

#include <stdio.h>

global_variable bool32 GlobalRunning = true;
platform_api Platform;

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

int main(int argc, const char* argv[])
{
    @autoreleasepool
    {
		Platform.Log = &MacosLog;

#if PROJECTSUPER_INTERNAL
		Platform.DEBUG_Log = &MacosDebugLog;
#endif

        macos_state state = {};
		state.appName = @"Project Super";

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

		render_context gameRender = {};
		gameRender.renderDimensions.Width = windowWidth;
		gameRender.renderDimensions.Height = windowHeight;
		ps_graphics_backend graphicsDriver = platform_load_graphics_backend(layer, windowWidth, windowHeight);
		ps_graphics_backend_api graphicsApi = graphicsDriver.api; 
		gameRender.resourceQueue = &graphicsDriver.resourceQueue;
		gameRender.AddResourceOperation = graphicsApi.AddResourceOperation;
		gameRender.IsResourceOperationComplete = graphicsApi.IsResourceOperationComplete;

        while(GlobalRunning)
        {
            MacosProcessMessages(state);

			graphicsApi.BeginFrame(graphicsDriver.instance, &gameRender.commands);

			// TODO(james): actually invoke the game loop here..

			graphicsApi.EndFrame(graphicsDriver.instance, &gameRender.commands);

        }
    }

    return 0;
}