
#include <Cocoa/Cocoa.h>

#include <stdio.h>

int main(int argc, const char* argv[])
{
    @autoreleasepool
    {
        NSString* appName = @"Project Super";
        // Cocoa boilerplate startup code
        NSApplication* app = [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        // Set working directory
        NSString *dir = [[NSFileManager defaultManager] currentDirectoryPath];

        NSFileManager* FileManager = [NSFileManager defaultManager];
        Context.WorkingDirectory = [NSString stringWithFormat:@"%@/Contents/Resources",
            [[NSBundle mainBundle] bundlePath]];
        if ([FileManager changeCurrentDirectoryPath:Context.WorkingDirectory] == NO)
        {
            Assert(0);
        }
        NSLog(@"working directory: %@", Context.WorkingDirectory);

        Context.AppDelegate = [[HandmadeAppDelegate alloc] init];
        [app setDelegate:Context.AppDelegate];

        [NSApp finishLaunching];

        // Create the main application window
        NSRect ScreenRect = [[NSScreen mainScreen] frame];

        NSRect InitialFrame = NSMakeRect((ScreenRect.size.width - WindowWidth) * 0.5,
                                        (ScreenRect.size.height - WindowHeight) * 0.5,
                                        WindowWidth,
                                        WindowHeight);

        NSWindow* Window = [[NSWindow alloc] initWithContentRect:InitialFrame
                                        styleMask:NSWindowStyleMaskTitled
                                                    | NSWindowStyleMaskClosable
                                                    | NSWindowStyleMaskMiniaturizable
                                                    | NSWindowStyleMaskResizable
                                        backing:NSBackingStoreBuffered
                                        defer:NO];

        [Window setBackgroundColor: NSColor.redColor];
        [Window setDelegate:Context.AppDelegate];

        NSView* CV = [Window contentView];
        [CV setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
        [CV setAutoresizesSubviews:YES];

        [Window setMinSize:NSMakeSize(160, 90)];
        [Window setTitle:appName];
        [Window makeKeyAndOrderFront:nil];
    }

    return 0;
}