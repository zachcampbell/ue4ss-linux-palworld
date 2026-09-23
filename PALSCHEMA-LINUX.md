# Branch linux-palschema: running PalSchema on the native Linux Palworld server

Twenty-nine commits on top of the v1.0.2-palworld-linux release (4bf136e), made while porting PalSchema to
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
- C++ runtime kept private (commits 21-24, found with the first batch of Lua mods): libstdc++ is
  linked statically and hidden, because an LD_PRELOAD resolves its imports through the executable
  (which exports libc++abi's __cxa_throw and std typeinfos) and libsteam_api.so (which exports its own
  __gxx_personality_v0) before libstdc++.so.6, so a dynamically linked runtime throws with one library,
  unwinds with another and faults. The global allocation operators still forward to the game binary's
  FMemory-backed ones (UE4SS/src/LinuxAllocatorBridge.cpp) so UE4SS heap objects stay in the engine
  allocator. With the runtime private, Lua errors are C++ exceptions again instead of longjmp (which
  skipped every lock_guard between lua_error and luaD_rawrunprotected and leaked the Lua mutex on each
  refused RegisterHook), and LuaMod::update_async no longer sleeps while holding that mutex (three Lua
  mods starved the game thread until the engine's hang detector fired).
- Manual addresses win (commit 25): UE4SS_Addresses.ini values for GNatives and ProcessInternal are no
  longer overwritten by the later heuristic scans. On PalServer-Linux-Shipping the GNatives heuristic picks
  the wrong pointer table (0xbcc4448; the interpreter uses 0xc1534a0), and with it every hooked native
  called from Blueprint script stepped its parameters through the wrong functions and desynced the caller
  (an "undefined opcode" fatal, or a null call). Ship an ini with `GNatives=0xc1534a0` next to the library.
- Blueprint function hooks (commit 26): the manual ProcessLocalScriptFunction address wins over the byte-pattern
  scan as well. That scan matched a register spill inside a static-init routine (0x4782ea5) and funchook detoured
  the Lua script hook onto it; ProcessInternal's own scan picked 0x43f8c30, so every RegisterHook on a Blueprint
  function was refused (UFunction::Func 0x7b7f590 never equalled it). With `ProcessInternal=0x7b7f590` and
  `ProcessLocalScriptFunction=0x7b7f6b0` (the tail-jump target at the end of ProcessInternal) in the ini, script
  hooks register and fire; the callback runs after the function, as on Windows, and only for calls that enter
  through ProcessEvent (script-to-script calls have the function inlined into UObject::CallFunction).
- Script hook self (commit 27): ProcessInternal tail-jumps into ProcessLocalScriptFunction after two virtual calls
  without restoring rdi, since the real function reads Stack.Object and ignores Context. The Linux detour now takes
  the object from the frame; before that every script-hook callback that called self:get() dereferenced a stale
  register (0x11a) and the server died two seconds after a player joined a base with hungry pals.
- Startup wait (commits 28 and 29): an attempt to start UE4SS as soon as the server's UDP port was bound (~7 s
  after launch) is reverted. At that point the engine is still constructing objects: one boot crashed inside
  store_all_object_types at 6 s, and PalSchema's blocking asset loads landing on the startup streaming left every
  later client join stuck at "connected". The fixed 30 s wait stays; PalSchema's Linux core init also holds until
  30 s of uptime on its own.

Build (Ubuntu, gcc-13, ninja):

    cmake -S . -B build_linux -G Ninja -DCMAKE_BUILD_TYPE=Game__Dev__Linux64 \
          -DUE4SS_GUI_ENABLED=OFF -DUE4SS_INPUT_ENABLED=OFF
    ninja -C build_linux UE4SS

C++ mods must be built with the same GUI/input settings (CppUserModBase layout differs otherwise) and
may carry their own static libstdc++ (the companion PalSchema build does).
Server-side requirements: run the binary with ASLR off (`setarch x86_64 -R`), delete the empty
MajorVersion/MinorVersion/DebugBuild lines from the shipped UE4SS-settings.ini, disable the stock Lua
mods, set DefaultExecuteInGameThreadMethod = ProcessEvent. The matching PalSchema branch lives in the
companion repository (PalSchema, branch linux).
