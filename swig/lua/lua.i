%module(directors="1") luaxspcomm

%include <lua_fnptr.i>

%{
#include "xspcomm/xcallback.h"
%}

%typemap(typecheck) xfunction<void, u_int64_t, void*> {
  $1 = lua_isfunction(L, $input);
}

%typemap(typecheck) xfunction<int, bool> {
  $1 = lua_isfunction(L, $input);
}

%typemap(in) xfunction<void, u_int64_t, void*> {
  if (!lua_isfunction(L, $input)) {
    SWIG_Lua_pusherrstring(L, "Expected a Lua function");
    SWIG_fail;
  }
  struct LuaCallback {
    lua_State* L;
    int ref;
    LuaCallback(lua_State* L, int ref) : L(L), ref(ref) {}
    ~LuaCallback() {
      luaL_unref(L, LUA_REGISTRYINDEX, ref);
    }
    void operator()(u_int64_t c) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
      lua_pushnumber(L, c);
      if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        lua_pop(L, 1);
      }
    }
  };
  lua_pushvalue(L, $input);
  int ref = luaL_ref(L, LUA_REGISTRYINDEX);
  auto* cb = new LuaCallback(L, ref);
  $1 = [cb](u_int64_t c, void* p) {
    (*cb)(c);
  };
}

%typemap(in) xfunction<int, bool> {
    if (!lua_isfunction(L, $input)) {
        SWIG_Lua_pusherrstring(L, "Expected a Lua function");
        SWIG_fail;
    }
    struct LuaCallback {
        lua_State* L;
        int ref;
        LuaCallback(lua_State* L, int ref) : L(L), ref(ref) {}
        ~LuaCallback() {
        luaL_unref(L, LUA_REGISTRYINDEX, ref);
        }
        int operator()(bool b) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
        lua_pushboolean(L, b);
        if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
            lua_pop(L, 1);
            return 0;
        }
        int r = lua_tointeger(L, -1);
        lua_pop(L, 1);
        return r;
        }
    };
    lua_pushvalue(L, $input);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    auto* cb = new LuaCallback(L, ref);
    $1 = [cb](bool b) {
        return (*cb)(b);
    };
}

%include "xspcomm/xcallback.h"

%include ../xcomm.i
