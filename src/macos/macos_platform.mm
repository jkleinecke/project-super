
#include <Cocoa/Cocoa.h>

#include "ps_platform.h"
#include "ps_shared.h"
#include "ps_intrinsics.h"
#include "ps_math.h"
#include "ps_memory.h"

#include "macos_platform.h"

#define LOG_ERROR
#define LOG_WARN
#define LOG_INFO
#include "macos_vulkan.cpp"

#include <stdio.h>

global_variable bool32 GlobalRunning = true;

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

        [Window setMinSize:NSMakeSize(160, 90)];
        [Window setTitle:state.appName];
        [Window makeKeyAndOrderFront:nil];

		MacosLoadGraphicsBackend();

        while(GlobalRunning)
        {
            MacosProcessMessages(state);
        }
    }

    return 0;
}