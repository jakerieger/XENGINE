//
// Created by Jake Rieger on 9/23/2026.
//

#include "ShaderHotReload.hpp"

#if XEN_WITH_SHADER_HOT_RELOAD

    #include <Common/Log.hpp>

    #include <algorithm>
    #include <cstdlib>
    #include <fstream>
    #include <sstream>
    #include <unordered_map>

namespace Xen {
    namespace {
        namespace fs = std::filesystem;

        // Same entry points Scripts/compile_engine_shaders.py looks for -
        // only a stage whose entry point actually appears in the source gets
        // compiled, so a depth-only shader (a vertex stage alone) doesn't
        // spuriously fail a pixel-stage compile with no PSMain to find.
        struct StageSpec {
            const char* EntryPoint;
            const char* Profile;
            const char* Suffix;
        };
        constexpr StageSpec Stages[] = {
          {"VSMain", "vs_6_0", ".vs"},
          {"PSMain", "ps_6_0", ".ps"},
          {"CSMain", "cs_6_0", ".cs"},
        };

        std::string ReadFileText(const fs::path& Path) {
            std::ifstream Stream(Path, std::ios::binary);
            if (!Stream) return {};
            std::stringstream Buffer;
            Buffer << Stream.rdbuf();
            return Buffer.str();
        }

        std::string ToLower(std::string S) {
            std::ranges::transform(S, S.begin(), [](const unsigned char C) { return CAST<char>(std::tolower(C)); });
            return S;
        }

        // Shells out to dxc.exe with the same -T/-E/-Fo shape
        // compile_engine_shaders.py already invokes at build time. DxcPath
        // is an absolute path resolved at CMake configure time (see
        // XenGame.cmake) - falls back to a bare "dxc.exe" PATH lookup if
        // that resolution failed, which will likely fail here too (this
        // process's own PATH almost never has the VS dev tools on it).
        //
        // Routed through a scratch .bat file rather than a direct
        // std::system(command) call: cmd.exe's own /C argument parsing only
        // special-cases "exactly two quote characters" (its documented rule
        // for preserving a quoted, space-containing program path) - with
        // more than two quoted segments (the exe path, -Fo's target, the
        // source file, the redirect target - four pairs here), it falls
        // back to blindly stripping the first and last quote characters
        // anywhere in the whole string, silently corrupting an unrelated
        // pair and breaking path parsing ("The directory name is invalid",
        // confirmed by hand against dxc.exe's own real install path, which
        // has a space in "Program Files (x86)"). A .bat file's own lines
        // don't go through that /C heuristic - only the single call to
        // launch the batch file does, and quoting just that one path is
        // exactly the two-quote case cmd.exe already handles correctly.
        // stdout/stderr are captured to a scratch log file next to
        // OutputFile so a compile error can be surfaced through LOG_WARN
        // instead of vanishing into the game's own hidden console.
        bool InvokeDxc(const fs::path& DxcPath, const fs::path& HlslFile, const StageSpec& Stage,
                       const fs::path& OutputFile) {
            const fs::path LogFile = OutputFile.string() + ".hotreload.log";
            const fs::path BatFile = OutputFile.string() + ".hotreload.bat";

            {
                std::ofstream Bat(BatFile);
                Bat << "@\"" << (DxcPath.empty() ? fs::path("dxc.exe") : DxcPath).string() << "\" -T "
                    << Stage.Profile << " -E " << Stage.EntryPoint << " -Fo \"" << OutputFile.string() << "\" \""
                    << HlslFile.string() << "\" > \"" << LogFile.string() << "\" 2>&1\n";
            }

            const std::string Cmd = "\"" + BatFile.string() + "\"";
            const int Result      = std::system(Cmd.c_str());
            if (Result != 0) {
                const std::string Log = ReadFileText(LogFile);
                LOG_WARN("Shader hot-reload: failed to compile %s (%s) - %s",
                        HlslFile.filename().string().c_str(),
                        Stage.EntryPoint,
                        Log.empty() ? "dxc.exe not found on PATH?" : Log.c_str());
            }

            std::error_code Ec;
            fs::remove(LogFile, Ec);
            fs::remove(BatFile, Ec);
            return Result == 0;
        }
    }  // namespace

    struct ShaderHotReload::Impl {
        fs::path SourceDir;
        fs::path OutputDir;
        fs::path DxcPath;
        std::unordered_map<std::string, fs::file_time_type> ShaderWriteTimes;
        fs::file_time_type IncludeWriteTime {};
        f32 PollAccumulator {0.0f};
        bool Ready {false};

        static constexpr f32 PollIntervalSeconds = 0.75f;

        bool RecompileOne(const fs::path& HlslFile) const {
            const std::string Source    = ReadFileText(HlslFile);
            const std::string StemLower = ToLower(HlslFile.stem().string());

            bool AnyStage = false;
            bool Ok       = true;
            for (const StageSpec& Stage : Stages) {
                if (Source.find(Stage.EntryPoint) == std::string::npos) continue;
                AnyStage = true;
                Ok &= InvokeDxc(DxcPath, HlslFile, Stage, OutputDir / ("xen.shader." + StemLower + Stage.Suffix));
            }

            if (!AnyStage) {
                LOG_WARN("Shader hot-reload: %s has no VSMain/PSMain/CSMain - nothing to compile",
                        HlslFile.filename().string().c_str());
                return false;
            }
            if (Ok) LOG_INFO("Shader hot-reload: recompiled %s", HlslFile.filename().string().c_str());
            return Ok;
        }
    };

    ShaderHotReload::ShaderHotReload() : _Impl(std::make_unique<Impl>()) {}
    ShaderHotReload::~ShaderHotReload() = default;

    bool ShaderHotReload::Initialize(const std::filesystem::path& SourceDir,
                                     const std::filesystem::path& OutputDir,
                                     const std::filesystem::path& DxcPath) {
        if (SourceDir.empty() || OutputDir.empty() || !fs::is_directory(SourceDir) || !fs::is_directory(OutputDir)) {
            return false;
        }
        if (DxcPath.empty()) {
            LOG_WARN("Shader hot-reload: dxc.exe location unknown (not found at CMake configure time) - "
                     "falling back to a bare PATH lookup, which will likely fail");
        }

        _Impl->SourceDir = SourceDir;
        _Impl->DxcPath   = DxcPath;
        _Impl->OutputDir = OutputDir;

        // Baseline every existing file's write time - nothing here has
        // "changed" yet, the offline build already compiled whatever's on
        // disk right now.
        std::error_code Ec;
        for (const auto& Entry : fs::directory_iterator(SourceDir, Ec)) {
            if (!Entry.is_regular_file() || Entry.path().extension() != ".hlsl") continue;
            _Impl->ShaderWriteTimes[Entry.path().filename().string()] = Entry.last_write_time();
        }

        const fs::path IncludeDir = SourceDir / "Include";
        if (fs::is_directory(IncludeDir)) {
            for (const auto& Entry : fs::directory_iterator(IncludeDir, Ec)) {
                if (!Entry.is_regular_file() || Entry.path().extension() != ".hlsli") continue;
                _Impl->IncludeWriteTime = std::max(_Impl->IncludeWriteTime, Entry.last_write_time());
            }
        }

        _Impl->Ready = true;
        LOG_INFO("Shader hot-reload: watching %s", SourceDir.string().c_str());
        return true;
    }

    bool ShaderHotReload::Poll(const f32 DeltaTime) {
        if (!_Impl->Ready) return false;

        _Impl->PollAccumulator += DeltaTime;
        if (_Impl->PollAccumulator < Impl::PollIntervalSeconds) return false;
        _Impl->PollAccumulator = 0.0f;

        std::error_code Ec;

        // An Include/*.hlsli edit can affect every top-level .hlsl that
        // includes it, and this doesn't track per-shader include
        // dependencies - so treat any change here as "recompile everything",
        // the safe (if coarse) fallback.
        bool IncludeChanged        = false;
        const fs::path IncludeDir  = _Impl->SourceDir / "Include";
        if (fs::is_directory(IncludeDir)) {
            for (const auto& Entry : fs::directory_iterator(IncludeDir, Ec)) {
                if (!Entry.is_regular_file() || Entry.path().extension() != ".hlsli") continue;
                const auto Time = Entry.last_write_time();
                if (Time > _Impl->IncludeWriteTime) IncludeChanged = true;
                _Impl->IncludeWriteTime = std::max(_Impl->IncludeWriteTime, Time);
            }
        }

        bool AnyRecompiled = false;

        if (IncludeChanged) {
            LOG_INFO("Shader hot-reload: a shared Include/*.hlsli changed - recompiling every engine shader");
            for (const auto& Entry : fs::directory_iterator(_Impl->SourceDir, Ec)) {
                if (!Entry.is_regular_file() || Entry.path().extension() != ".hlsl") continue;
                if (_Impl->RecompileOne(Entry.path())) AnyRecompiled = true;
                _Impl->ShaderWriteTimes[Entry.path().filename().string()] = Entry.last_write_time();
            }
        } else {
            for (const auto& Entry : fs::directory_iterator(_Impl->SourceDir, Ec)) {
                if (!Entry.is_regular_file() || Entry.path().extension() != ".hlsl") continue;

                const std::string Name = Entry.path().filename().string();
                const auto Time        = Entry.last_write_time();
                const auto It          = _Impl->ShaderWriteTimes.find(Name);

                // A brand-new file appearing mid-session (not present at
                // Initialize) is baselined, not compiled - nothing in the
                // running game references its AssetID yet regardless.
                const bool KnownUnchanged = It != _Impl->ShaderWriteTimes.end() && It->second == Time;
                const bool NeverSeen      = It == _Impl->ShaderWriteTimes.end();
                _Impl->ShaderWriteTimes[Name] = Time;
                if (KnownUnchanged || NeverSeen) continue;

                if (_Impl->RecompileOne(Entry.path())) AnyRecompiled = true;
            }
        }

        return AnyRecompiled;
    }
}  // namespace Xen

#else

namespace Xen {
    struct ShaderHotReload::Impl {};

    ShaderHotReload::ShaderHotReload()  = default;
    ShaderHotReload::~ShaderHotReload() = default;

    bool ShaderHotReload::Initialize(const std::filesystem::path&, const std::filesystem::path&,
                                     const std::filesystem::path&) {
        return false;
    }

    bool ShaderHotReload::Poll(f32) {
        return false;
    }
}  // namespace Xen

#endif
