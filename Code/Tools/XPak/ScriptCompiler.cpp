// Author: Jake Rieger
// Created: 3/5/2025.
//

#include "ScriptCompiler.hpp"
#include <iostream>

extern "C" {
#include <lualib.h>
#include <lauxlib.h>
#include <luajit.h>
}

namespace x {
    static constexpr int kLuaKeepDebugInfo {0};
    static constexpr int kLuaStripDebugInfo {1};

    vector<u8> ScriptCompiler::Compile(const str& source, const str& chunkName) {
        vector<u8> bytecode;

        lua_State* L = luaL_newstate();
        if (!L) {
            std::cerr << "Failed to create Lua state" << std::endl;
            return bytecode;
        }

        int loadResult = luaL_loadbuffer(L, source.c_str(), source.size(), chunkName.c_str());
        if (loadResult != 0) {
            std::cerr << "Failed to compile Lua script: " << lua_tostring(L, -1) << std::endl;
            lua_close(L);
            return bytecode;
        }

        BytecodeWriterState writerState;
        writerState.bytecode = &bytecode;

        // Visual Studio 2022 uses Lua 5.2 for some reason, this fixes the discrepancy
        // TODO: Enforce the same Lua version between IDE project files
#if LUA_VERSION_NUM >= 502
        int dumpResult = lua_dump(L, BytecodeWriter, &writerState, kLuaKeepDebugInfo);
#else
        int dumpResult = lua_dump(L, BytecodeWriter, &writerState);
#endif

        if (dumpResult != 0) {
            std::cerr << "Failed to dump Lua bytecode: " << dumpResult << std::endl;
            bytecode.clear();
        }

        lua_close(L);
        return bytecode;
    }

    int ScriptCompiler::BytecodeWriter(lua_State* L, const void* p, size_t size, void* ud) {
        BytecodeWriterState* state = CAST<BytecodeWriterState*>(ud);
        const u8* data             = CAST<const u8*>(p);
        state->bytecode->insert(state->bytecode->end(), data, data + size);
        return 0;
    }
}  // namespace x