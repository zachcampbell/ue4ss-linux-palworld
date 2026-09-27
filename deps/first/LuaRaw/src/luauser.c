#ifdef _WIN32
#include <windows.h>
#include "lua.h"

static struct {
    CRITICAL_SECTION LockSct;
    BOOL Init;
} Gl;

void LuaLockInitial(lua_State* L)
{
    if (!Gl.Init)
    {
        /* Create a mutex */
        InitializeCriticalSection(&Gl.LockSct);
        Gl.Init = TRUE;
    }
}

void LuaLockFinal(lua_State* L)
{
    /* Destroy a mutex. */
    if (Gl.Init)
    {
        DeleteCriticalSection(&Gl.LockSct);
        Gl.Init = FALSE;
    }
}

void LuaLock(lua_State* L)
{
    LuaLockInitial(L);
    /* Wait for control of mutex */
    EnterCriticalSection(&Gl.LockSct);
}

void LuaUnlock(lua_State* L)
{
    /* Release control of mutex */
    LeaveCriticalSection(&Gl.LockSct);
}
#else
#include <pthread.h>
#include "lua.h"

static struct {
    pthread_mutex_t LockSct;
    int Init;
} Gl;

void LuaLockInitial(lua_State* L)
{
    if (!Gl.Init)
    {
        /* Recursive, as the Windows branch's critical section is: a lock count left behind by an error that escaped
           Lua must not freeze the thread that holds it on its next Lua call. */
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&Gl.LockSct, &attr);
        pthread_mutexattr_destroy(&attr);
        Gl.Init = 1;
    }
}

void LuaLockFinal(lua_State* L)
{
    if (Gl.Init)
    {
        pthread_mutex_destroy(&Gl.LockSct);
        Gl.Init = 0;
    }
}

void LuaLock(lua_State* L)
{
    LuaLockInitial(L);
    pthread_mutex_lock(&Gl.LockSct);
}

void LuaUnlock(lua_State* L)
{
    pthread_mutex_unlock(&Gl.LockSct);
}
#endif