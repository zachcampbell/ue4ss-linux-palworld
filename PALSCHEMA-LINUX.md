# Branch linux-palschema: running PalSchema on the native Linux Palworld server

Twenty commits on top of the v1.0.2-palworld-linux release (4bf136e), made while porting PalSchema to
this UE4SS build on PalServer-Linux-Shipping v1.0.5.102999 (September 2026). Each commit message says
what broke and how it was found; in short:

- Real GMalloc (Zydis walk from the exported operator new into FMemory::Malloc) and FMemory routed
  through it, so engine memory handed to UE4SS containers is freed by the right allocator.
- The MSVC-derived vtable layout maps are off by one slot on Linux after the destructor (Itanium
  double-destructor slot); every map is checked against a generated baseline and corrected.
- 5.1 layouts: FSoftObjectPath (TopLevelAssetPath + SubPathString), FStaticConstructObjectParameters
  padded to the size the engine reads.
- Initialization refusals return normally through StaticStorage::bInitRefused instead of throwing
  (a C++ throw inside the preload faults in libsteam_api's unwinder on this binary).
- StaticConstructObject AOB scan rejects mid-function candidates; ForEachUObject no longer skips
  live objects with zero internal flags and no longer stops after four chunks (a Palworld world has
  more than 262144 objects at boot, so everything created afterwards was invisible).
- Exit path: the event loop is stopped from the engine's UObject-array shutdown notification and a
  late atexit handler, and the program object is never torn down from the library destructor (the
  game heap is gone by then). Signal handler prints a stack dump.

Build (Ubuntu, gcc-13, ninja):

    cmake -S . -B build_linux -G Ninja -DCMAKE_BUILD_TYPE=Game__Dev__Linux64 \
          -DUE4SS_GUI_ENABLED=OFF -DUE4SS_INPUT_ENABLED=OFF
    ninja -C build_linux UE4SS

C++ mods must be built with the same GUI/input settings (CppUserModBase layout differs otherwise).
Server-side requirements: run the binary with ASLR off (`setarch x86_64 -R`), delete the empty
MajorVersion/MinorVersion/DebugBuild lines from the shipped UE4SS-settings.ini, disable the stock Lua
mods, set DefaultExecuteInGameThreadMethod = ProcessEvent. The matching PalSchema branch lives in the
companion repository (PalSchema, branch linux).
