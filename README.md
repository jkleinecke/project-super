# Project Super Source

This codebase draws initial inspiration from the Handmade Hero project.  Many thanks to Casey Muratori and the members of the Handmade community for their hardwork.

A few differences from the original Handmade version:

* Vulkan based renderer
* Some libraries have been included, but not without being able to fully understand where they fit and what they are doing under the hood
* Generally templates and struct methods aren't used, but that isn't a hard and fast rule.  Using them for things like array<> and hashtable<> generally makes for a more readable and understandable codebase

Goals for this codebase:

* While performance isn't a primary concern, it will be a by-product of not hiding where work is performed.  This means no RTTI, which eliminates a lot of hidden execution on the codebase.
  
  * **EXAMPLE** If you want to clean up a resource, free it yourself.  Don't rely on a desctructor and the object going out of scope. This increases effort on the programmer in the short term, but infinitely increases the ability to tell what is being executed and when.
  
* When in doubt, keep it simple and iterate
* Understand everything that the code is doing, including when pulling in libraries
* Predictable memory usage, allocate and free in waves

A list of ongoing needs is being kept in the [TODO](TODO.txt) file.  It will be updated over time.

Many thanks to the following libraries used for reference and inspiration:

* [The Forge](https://github.com/ConfettiFX/The-Forge)
* [gfx](https://github.com/gboisse/gfx)
