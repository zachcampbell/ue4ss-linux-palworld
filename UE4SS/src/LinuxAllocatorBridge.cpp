// Linux: libUE4SS links libstdc++ statically and hides it (UE4SS/CMakeLists.txt), which would also turn its
// global operator new/delete into the malloc-backed libstdc++ ones. With the dynamic libstdc++ those operators
// resolved through the LD_PRELOAD global scope to the game executable's replacements (Unreal's FMemory-backed
// operator new/delete, which the shipping binary exports), so every UE4SS heap object lived in the engine
// allocator and could be handed to FMemory::Realloc/Free by the Unreal containers. Keep exactly that behaviour:
// forward every global allocation operator to the executable's definition. The first boot without this bridge
// died in FMallocBinned2::Realloc ("Attempt to realloc an unrecognized block") on the first C++ mod start.
#ifdef __linux__
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <new>

namespace
{
    // The library is also preloaded into the wrapper processes on the way to the game (setarch, sh), which export
    // no allocation operators; there the bridge falls back to libc so those processes can exec normally.
    void* self_base()
    {
        Dl_info info{};
        return dladdr(reinterpret_cast<void*>(&self_base), &info) ? info.dli_fbase : nullptr;
    }
    // RTLD_DEFAULT walks the global scope from the executable, so in the game it finds the binary's operator.
    // GCC keeps replaceable allocation operators at default visibility, so in a process whose executable does
    // not define them the walk lands back on this library's own copy; treat that as absent.
    template <typename Fn>
    Fn resolve(const char* name)
    {
        void* p = dlsym(RTLD_DEFAULT, name);
        if (!p) return nullptr;
        Dl_info info{};
        if (dladdr(p, &info) && info.dli_fbase == self_base()) return nullptr;
        return reinterpret_cast<Fn>(p);
    }
    void* fallback_alloc(std::size_t n, std::size_t align = 0)
    {
        if (n == 0) n = 1;
        if (align > alignof(std::max_align_t))
        {
            void* p = nullptr;
            return posix_memalign(&p, align, n) == 0 ? p : nullptr;
        }
        return std::malloc(n);
    }
} // namespace

void* operator new(std::size_t n)
{
    static auto fn = resolve<void* (*)(std::size_t)>("_Znwm");
    if (fn) return fn(n);
    void* p = fallback_alloc(n, 0);
    if (!p) throw std::bad_alloc{}; return p;
}
void* operator new[](std::size_t n)
{
    static auto fn = resolve<void* (*)(std::size_t)>("_Znam");
    if (fn) return fn(n);
    void* p = fallback_alloc(n, 0);
    if (!p) throw std::bad_alloc{}; return p;
}
void* operator new(std::size_t n, const std::nothrow_t& t) noexcept
{
    static auto fn = resolve<void* (*)(std::size_t, const std::nothrow_t&)>("_ZnwmRKSt9nothrow_t");
    if (fn) return fn(n, t);
    void* p = fallback_alloc(n, 0);
    return p;
}
void* operator new[](std::size_t n, const std::nothrow_t& t) noexcept
{
    static auto fn = resolve<void* (*)(std::size_t, const std::nothrow_t&)>("_ZnamRKSt9nothrow_t");
    if (fn) return fn(n, t);
    void* p = fallback_alloc(n, 0);
    return p;
}
void* operator new(std::size_t n, std::align_val_t a)
{
    static auto fn = resolve<void* (*)(std::size_t, std::align_val_t)>("_ZnwmSt11align_val_t");
    if (fn) return fn(n, a);
    void* p = fallback_alloc(n, static_cast<std::size_t>(a));
    if (!p) throw std::bad_alloc{}; return p;
}
void* operator new[](std::size_t n, std::align_val_t a)
{
    static auto fn = resolve<void* (*)(std::size_t, std::align_val_t)>("_ZnamSt11align_val_t");
    if (fn) return fn(n, a);
    void* p = fallback_alloc(n, static_cast<std::size_t>(a));
    if (!p) throw std::bad_alloc{}; return p;
}
void* operator new(std::size_t n, std::align_val_t a, const std::nothrow_t& t) noexcept
{
    static auto fn = resolve<void* (*)(std::size_t, std::align_val_t, const std::nothrow_t&)>("_ZnwmSt11align_val_tRKSt9nothrow_t");
    if (fn) return fn(n, a, t);
    void* p = fallback_alloc(n, static_cast<std::size_t>(a));
    return p;
}
void* operator new[](std::size_t n, std::align_val_t a, const std::nothrow_t& t) noexcept
{
    static auto fn = resolve<void* (*)(std::size_t, std::align_val_t, const std::nothrow_t&)>("_ZnamSt11align_val_tRKSt9nothrow_t");
    if (fn) return fn(n, a, t);
    void* p = fallback_alloc(n, static_cast<std::size_t>(a));
    return p;
}

void operator delete(void* p) noexcept
{
    static auto fn = resolve<void (*)(void*)>("_ZdlPv");
    if (fn) fn(p); else std::free(p);
}
void operator delete[](void* p) noexcept
{
    static auto fn = resolve<void (*)(void*)>("_ZdaPv");
    if (fn) fn(p); else std::free(p);
}
void operator delete(void* p, std::size_t n) noexcept
{
    static auto fn = resolve<void (*)(void*, std::size_t)>("_ZdlPvm");
    if (fn) fn(p, n); else std::free(p);
}
void operator delete[](void* p, std::size_t n) noexcept
{
    static auto fn = resolve<void (*)(void*, std::size_t)>("_ZdaPvm");
    if (fn) fn(p, n); else std::free(p);
}
void operator delete(void* p, const std::nothrow_t& t) noexcept
{
    static auto fn = resolve<void (*)(void*, const std::nothrow_t&)>("_ZdlPvRKSt9nothrow_t");
    if (fn) fn(p, t); else std::free(p);
}
void operator delete[](void* p, const std::nothrow_t& t) noexcept
{
    static auto fn = resolve<void (*)(void*, const std::nothrow_t&)>("_ZdaPvRKSt9nothrow_t");
    if (fn) fn(p, t); else std::free(p);
}
void operator delete(void* p, std::align_val_t a) noexcept
{
    static auto fn = resolve<void (*)(void*, std::align_val_t)>("_ZdlPvSt11align_val_t");
    if (fn) fn(p, a); else std::free(p);
}
void operator delete[](void* p, std::align_val_t a) noexcept
{
    static auto fn = resolve<void (*)(void*, std::align_val_t)>("_ZdaPvSt11align_val_t");
    if (fn) fn(p, a); else std::free(p);
}
void operator delete(void* p, std::size_t n, std::align_val_t a) noexcept
{
    static auto fn = resolve<void (*)(void*, std::size_t, std::align_val_t)>("_ZdlPvmSt11align_val_t");
    if (fn) fn(p, n, a); else std::free(p);
}
void operator delete[](void* p, std::size_t n, std::align_val_t a) noexcept
{
    static auto fn = resolve<void (*)(void*, std::size_t, std::align_val_t)>("_ZdaPvmSt11align_val_t");
    if (fn) fn(p, n, a); else std::free(p);
}
void operator delete(void* p, std::align_val_t a, const std::nothrow_t& t) noexcept
{
    static auto fn = resolve<void (*)(void*, std::align_val_t, const std::nothrow_t&)>("_ZdlPvSt11align_val_tRKSt9nothrow_t");
    if (fn) fn(p, a, t); else std::free(p);
}
void operator delete[](void* p, std::align_val_t a, const std::nothrow_t& t) noexcept
{
    static auto fn = resolve<void (*)(void*, std::align_val_t, const std::nothrow_t&)>("_ZdaPvSt11align_val_tRKSt9nothrow_t");
    if (fn) fn(p, a, t); else std::free(p);
}

#endif
