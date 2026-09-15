//
// Created by Jake Rieger on 9/11/2026.
//

#include "GLRenderDevice.hpp"
#include "GLTranslate.hpp"
#include "../../Common/Log.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ranges>

namespace Xen::RHI::GL {
    namespace {
        /// FNV-1a over raw bytes. Every descriptor hashed here has explicitly
        /// initialised padding, so the result is deterministic.
        u64 HashBytes(const void* Data, const size_t Size) {
            const auto* Bytes = CAST<const u8*>(Data);
            u64 Hash          = 14695981039346656037ULL;
            for (size_t i = 0; i < Size; ++i) {
                Hash ^= Bytes[i];
                Hash *= 1099511628211ULL;
            }
            return Hash;
        }

        u32 FullMipChain(const u32 Width, const u32 Height) {
            u32 Size   = Width > Height ? Width : Height;
            u32 Levels = 1;
            while (Size > 1) {
                Size >>= 1;
                ++Levels;
            }
            return Levels;
        }

        const char* DebugSourceName(const GLenum Source) {
            switch (Source) {
                case GL_DEBUG_SOURCE_API:
                    return "API";
                case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
                    return "WindowSystem";
                case GL_DEBUG_SOURCE_SHADER_COMPILER:
                    return "ShaderCompiler";
                case GL_DEBUG_SOURCE_THIRD_PARTY:
                    return "ThirdParty";
                case GL_DEBUG_SOURCE_APPLICATION:
                    return "Application";
                default:
                    return "Other";
            }
        }

        const char* DebugTypeName(const GLenum Type) {
            switch (Type) {
                case GL_DEBUG_TYPE_ERROR:
                    return "Error";
                case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
                    return "Deprecated";
                case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
                    return "UndefinedBehavior";
                case GL_DEBUG_TYPE_PORTABILITY:
                    return "Portability";
                case GL_DEBUG_TYPE_PERFORMANCE:
                    return "Performance";
                case GL_DEBUG_TYPE_MARKER:
                    return "Marker";
                default:
                    return "Other";
            }
        }
    }  // namespace

    // =======================================================================
    // TransientRing
    // =======================================================================
    bool TransientRing::Initialize(const u32 BytesPerFrame,
                                   const u32 FramesInFlight,
                                   const u32 UboAlign,
                                   const u32 SsboAlign) {
        _Capacity  = BytesPerFrame;
        _UboAlign  = UboAlign ? UboAlign : 256;
        _SsboAlign = SsboAlign ? SsboAlign : 256;
        _Arenas.resize(FramesInFlight);
        _Handles.resize(FramesInFlight);

        constexpr GLbitfield Flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;

        for (Arena& A : _Arenas) {
            glCreateBuffers(1, &A.ID);
            glNamedBufferStorage(A.ID, BytesPerFrame, nullptr, Flags);
            A.Mapped = CAST<u8*>(glMapNamedBufferRange(A.ID, 0, BytesPerFrame, Flags));
            if (!A.Mapped) return false;
        }

        return true;
    }

    void TransientRing::Shutdown() {
        for (auto& [ID, Mapped, Fence] : _Arenas) {
            if (Fence) glDeleteSync(Fence);
            if (Mapped) glUnmapNamedBuffer(ID);
            if (ID) glDeleteBuffers(1, &ID);
        }
        _Arenas.clear();
        _Handles.clear();
    }

    void TransientRing::BeginFrame(const u32 FrameIndex) {
        _FrameIndex = FrameIndex;
        _Head       = 0;

        Arena& A = _Arenas[FrameIndex];
        if (!A.Fence) return;

        // Block until the GPU is done with the arena about to be overwritten.
        GLenum Result = glClientWaitSync(A.Fence, GL_SYNC_FLUSH_COMMANDS_BIT, 0);
        while (Result == GL_TIMEOUT_EXPIRED) {
            Result = glClientWaitSync(A.Fence, GL_SYNC_FLUSH_COMMANDS_BIT, 1000000);
        }

        glDeleteSync(A.Fence);
        A.Fence = nullptr;
    }

    void TransientRing::EndFrame(const u32 FrameIndex) {
        Arena& A = _Arenas[FrameIndex];
        if (A.Fence) glDeleteSync(A.Fence);
        A.Fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    }

    TransientAllocation
    TransientRing::Allocate(const u32 Size, const BufferUsage Usage, const BufferHandle ArenaHandle) {
        u32 Alignment = 16;
        if (Any(Usage & BufferUsage::Uniform)) Alignment = _UboAlign;
        else if (Any(Usage & BufferUsage::Storage)) Alignment = _SsboAlign;

        const u32 Aligned = (_Head + Alignment - 1) & ~(Alignment - 1);
        if (Aligned + Size > _Capacity) return {};  // caller reports the overflow

        _Head = Aligned + Size;

        TransientAllocation Out;
        Out.Buffer = ArenaHandle;
        Out.Offset = Aligned;
        Out.Size   = Size;
        Out.Data   = _Arenas[_FrameIndex].Mapped + Aligned;
        return Out;
    }

    // =======================================================================
    // Lifetime
    // =======================================================================
    GLRenderDevice::GLRenderDevice() = default;

    GLRenderDevice::~GLRenderDevice() {
        if (_Initialized) Shutdown();
    }

    void APIENTRY GLRenderDevice::DebugCallback(const GLenum Source,
                                                const GLenum Type,
                                                const GLuint ID,
                                                const GLenum Severity,
                                                GLsizei,
                                                const GLchar* Message,
                                                const void* UserParam) {
        if (Severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;

        const auto* Device = CAST<const GLRenderDevice*>(UserParam);

        char Buffer[1024];
        std::snprintf(Buffer,
                      sizeof(Buffer),
                      "[GL %s/%s #%u] %s",
                      DebugSourceName(Source),
                      DebugTypeName(Type),
                      ID,
                      Message);

        if (Device && Device->_Desc.ErrorCallback) Device->_Desc.ErrorCallback(Buffer, Device->_Desc.ErrorUserData);
        else std::fprintf(stderr, "%s\n", Buffer);
    }

    bool GLRenderDevice::Initialize(const DeviceDescriptor& Desc) {
        _Desc = Desc;

        if (!glGetString(GL_VERSION)) {
            Log(true, "Initialize called with no current GL context - construct Window first");
            return false;
        }

        GLint Major = 0, Minor = 0;
        glGetIntegerv(GL_MAJOR_VERSION, &Major);
        glGetIntegerv(GL_MINOR_VERSION, &Minor);
        if (Major < 4 || (Major == 4 && Minor < 6)) {
            Log(true, "OpenGL 4.6 required, got %d.%d", Major, Minor);
            return false;
        }

        _Caps.Vendor     = RCAST<const char*>(glGetString(GL_VENDOR));
        _Caps.Renderer   = RCAST<const char*>(glGetString(GL_RENDERER));
        _Caps.ApiVersion = RCAST<const char*>(glGetString(GL_VERSION));

        auto GetInt = [](const GLenum E) {
            GLint V = 0;
            glGetIntegerv(E, &V);
            return CAST<u32>(V);
        };

        _Caps.MaxTextureSize2D             = GetInt(GL_MAX_TEXTURE_SIZE);
        _Caps.MaxTextureArrayLayers        = GetInt(GL_MAX_ARRAY_TEXTURE_LAYERS);
        _Caps.MaxColorAttachments          = GetInt(GL_MAX_COLOR_ATTACHMENTS);
        _Caps.MaxSamples                   = GetInt(GL_MAX_SAMPLES);
        _Caps.MaxTextureUnits              = GetInt(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS);
        _Caps.UniformBufferOffsetAlignment = GetInt(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT);
        _Caps.StorageBufferOffsetAlignment = GetInt(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT);

        _Caps.SupportsSpirv             = true;
        _Caps.SupportsMultiDrawIndirect = true;
        _Caps.SupportsDebugMarkers      = true;

        // GL_MAX_TEXTURE_MAX_ANISOTROPY is core in 4.6, but a glad1 header
        // generated for an older profile may only carry the EXT spelling.
#if defined(GL_MAX_TEXTURE_MAX_ANISOTROPY)
        GLfloat MaxAniso = 1.0f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &MaxAniso);
        _Caps.MaxAnisotropy      = CAST<u32>(MaxAniso);
        _Caps.SupportsAnisotropy = MaxAniso > 1.0f;
#elif defined(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT)
        GLfloat MaxAniso = 1.0f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &MaxAniso);
        _Caps.MaxAnisotropy      = CAST<u32>(MaxAniso);
        _Caps.SupportsAnisotropy = MaxAniso > 1.0f;
#endif

        if (Desc.EnableValidation) {
            glEnable(GL_DEBUG_OUTPUT);
            // Synchronous so the callback fires on the offending call and the
            // stack trace is meaningful.
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(&GLRenderDevice::DebugCallback, this);
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
        }

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        // Only meaningful when the window was created GLFW_SRGB_CAPABLE and
        // textures were uploaded with an sRGB format, hence opt-in.
        if (Desc.EnableSrgbFramebuffer) glEnable(GL_FRAMEBUFFER_SRGB);

        const u32 Frames = Desc.FramesInFlight ? Desc.FramesInFlight : 3;
        if (!_Transient.Initialize(Desc.TransientBufferSize,
                                   Frames,
                                   _Caps.UniformBufferOffsetAlignment,
                                   _Caps.StorageBufferOffsetAlignment)) {
            Log(true, "failed to create the transient ring buffer");
            return false;
        }

        // Register each arena in the pool, so a transient allocation hands back
        // a real handle the command stream binds like any other buffer.
        for (u32 i = 0; i < Frames; ++i) {
            GLBuffer Arena;
            Arena.ID    = _Transient.GetBufferID(i);
            Arena.Size  = Desc.TransientBufferSize;
            Arena.Usage = BufferUsage::Uniform | BufferUsage::Storage | BufferUsage::Vertex | BufferUsage::Index |
                          BufferUsage::Indirect;
            Arena.Memory    = MemoryUsage::CpuToGpu;
            Arena.Transient = true;
            _Transient.SetHandle(i, _Buffers.Allocate(Arena));
        }

        _State.Invalidate();
        _ForceStateApply = true;
        _Initialized     = true;

        Log(false, "OpenGL %s | %s | %s", _Caps.ApiVersion, _Caps.Renderer, _Caps.Vendor);
        return true;
    }

    void GLRenderDevice::Shutdown() {
        if (!_Initialized) return;

        // Flush everything queued regardless of frame age.
        for (const PendingDelete& P : _PendingDeletes) {
            switch (P.Type) {
                case GLObjectType::Buffer:
                    glDeleteBuffers(1, &P.ID);
                    break;
                case GLObjectType::Texture:
                    glDeleteTextures(1, &P.ID);
                    break;
                case GLObjectType::Sampler:
                    glDeleteSamplers(1, &P.ID);
                    break;
                case GLObjectType::Shader:
                    glDeleteShader(P.ID);
                    break;
                case GLObjectType::Program:
                    glDeleteProgram(P.ID);
                    break;
                case GLObjectType::VAO:
                    glDeleteVertexArrays(1, &P.ID);
                    break;
                case GLObjectType::FBO:
                    glDeleteFramebuffers(1, &P.ID);
                    break;
            }
        }
        _PendingDeletes.clear();

        _Buffers.ForEachLive([](GLBuffer& B) {
            if (B.ID && !B.Transient) glDeleteBuffers(1, &B.ID);
        });
        _Textures.ForEachLive([](GLTexture& T) {
            if (T.ID) glDeleteTextures(1, &T.ID);
        });
        _Samplers.ForEachLive([](GLSampler& S) {
            if (S.ID) glDeleteSamplers(1, &S.ID);
        });
        _Shaders.ForEachLive([](GLShader& S) {
            if (S.ID) glDeleteShader(S.ID);
        });
        _Pipelines.ForEachLive([](GLPipeline& P) {
            if (P.Program) glDeleteProgram(P.Program);
        });

        for (auto& [Key, VAO] : _VAOCache)
            glDeleteVertexArrays(1, &VAO);
        for (auto& [Key, FBO] : _FBOCache)
            glDeleteFramebuffers(1, &FBO);
        _VAOCache.clear();
        _FBOCache.clear();

        _Transient.Shutdown();
        _Initialized = false;
    }

    void GLRenderDevice::Log(const bool Error, const char* Fmt, ...) const {
        char Buffer[1024];
        va_list Args;
        va_start(Args, Fmt);
        std::vsnprintf(Buffer, sizeof(Buffer), Fmt, Args);
        va_end(Args);

        if (Error) GetLogger().Log(Logger::Severity::Error, Buffer);
        else GetLogger().Log(Logger::Severity::Debug, Buffer);
    }

    // =======================================================================
    // Resource creation
    // =======================================================================
    BufferHandle GLRenderDevice::CreateBuffer(const BufferDesc& Desc) {
        GLBuffer Buffer;
        Buffer.Size   = Desc.Size;
        Buffer.Usage  = Desc.Usage;
        Buffer.Memory = Desc.Memory;

        glCreateBuffers(1, &Buffer.ID);

        GLbitfield Flags = 0;
        switch (Desc.Memory) {
            case MemoryUsage::GpuOnly:
                // DYNAMIC_STORAGE grants glNamedBufferSubData. The cost is
                // negligible and it keeps small writes possible with no
                // staging path.
                Flags = GL_DYNAMIC_STORAGE_BIT;
                break;
            case MemoryUsage::CpuToGpu:
                Flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_DYNAMIC_STORAGE_BIT;
                break;
            case MemoryUsage::GpuToCpu:
                Flags = GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
                break;
        }

        glNamedBufferStorage(Buffer.ID, CAST<GLsizeiptr>(Desc.Size), Desc.InitialData, Flags);

        if (Desc.Memory != MemoryUsage::GpuOnly) {
            const GLbitfield MapFlags = (Desc.Memory == MemoryUsage::CpuToGpu ? GL_MAP_WRITE_BIT : GL_MAP_READ_BIT) |
                                        GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
            Buffer.Mapped = glMapNamedBufferRange(Buffer.ID, 0, CAST<GLsizeiptr>(Desc.Size), MapFlags);
        }

        if (Desc.DebugName && _Desc.EnableDebugMarkers) glObjectLabel(GL_BUFFER, Buffer.ID, -1, Desc.DebugName);

        return _Buffers.Allocate(Buffer);
    }

    TextureHandle GLRenderDevice::CreateTexture(const TextureDesc& Desc) {
        GLTexture Texture;
        Texture.Fmt         = Desc.Fmt;
        Texture.Width       = Desc.Width;
        Texture.Height      = Desc.Height;
        Texture.ArrayLayers = Desc.ArrayLayers;
        Texture.SampleCount = Desc.SampleCount;
        Texture.Usage       = Desc.Usage;
        Texture.Target      = ToGLTextureTarget(Desc.Type, Desc.SampleCount);
        Texture.MipLevels   = Desc.MipLevels ? Desc.MipLevels : FullMipChain(Desc.Width, Desc.Height);

        const GLFormat Fmt = ToGLFormat(Desc.Fmt);
        glCreateTextures(Texture.Target, 1, &Texture.ID);

        const bool Multisample = Desc.SampleCount > 1;
        switch (Desc.Type) {
            case TextureType::Texture2D:
            case TextureType::TextureCube:
                // Cube maps use the 2D entry point; GL derives the six faces.
                if (Multisample) {
                    glTextureStorage2DMultisample(Texture.ID,
                                                  CAST<GLsizei>(Desc.SampleCount),
                                                  Fmt.InternalFormat,
                                                  CAST<GLsizei>(Desc.Width),
                                                  CAST<GLsizei>(Desc.Height),
                                                  GL_TRUE);
                } else {
                    glTextureStorage2D(Texture.ID,
                                       CAST<GLsizei>(Texture.MipLevels),
                                       Fmt.InternalFormat,
                                       CAST<GLsizei>(Desc.Width),
                                       CAST<GLsizei>(Desc.Height));
                }
                break;

            case TextureType::Texture2DArray:
                if (Multisample) {
                    glTextureStorage3DMultisample(Texture.ID,
                                                  CAST<GLsizei>(Desc.SampleCount),
                                                  Fmt.InternalFormat,
                                                  CAST<GLsizei>(Desc.Width),
                                                  CAST<GLsizei>(Desc.Height),
                                                  CAST<GLsizei>(Desc.ArrayLayers),
                                                  GL_TRUE);
                } else {
                    glTextureStorage3D(Texture.ID,
                                       CAST<GLsizei>(Texture.MipLevels),
                                       Fmt.InternalFormat,
                                       CAST<GLsizei>(Desc.Width),
                                       CAST<GLsizei>(Desc.Height),
                                       CAST<GLsizei>(Desc.ArrayLayers));
                }
                break;
        }

        if (Desc.DebugName && _Desc.EnableDebugMarkers) glObjectLabel(GL_TEXTURE, Texture.ID, -1, Desc.DebugName);

        return _Textures.Allocate(Texture);
    }

    SamplerHandle GLRenderDevice::CreateSampler(const SamplerDesc& Desc) {
        GLSampler Sampler;
        glCreateSamplers(1, &Sampler.ID);

        glSamplerParameteri(Sampler.ID, GL_TEXTURE_MIN_FILTER, ToGLMinFilter(Desc.MinFilter, Desc.MipFilter));
        glSamplerParameteri(Sampler.ID,
                            GL_TEXTURE_MAG_FILTER,
                            Desc.MagFilter == FilterMode::Linear ? GL_LINEAR : GL_NEAREST);
        glSamplerParameteri(Sampler.ID, GL_TEXTURE_WRAP_S, ToGLAddressMode(Desc.AddressU));
        glSamplerParameteri(Sampler.ID, GL_TEXTURE_WRAP_T, ToGLAddressMode(Desc.AddressV));
        glSamplerParameterf(Sampler.ID, GL_TEXTURE_LOD_BIAS, Desc.MipLodBias);
        glSamplerParameterf(Sampler.ID, GL_TEXTURE_MIN_LOD, Desc.MinLod);
        glSamplerParameterf(Sampler.ID, GL_TEXTURE_MAX_LOD, Desc.MaxLod);

#if defined(GL_TEXTURE_MAX_ANISOTROPY)
        if (Desc.MaxAnisotropy > 1 && _Caps.SupportsAnisotropy) {
            const f32 Aniso =
              CAST<f32>(Desc.MaxAnisotropy < _Caps.MaxAnisotropy ? Desc.MaxAnisotropy : _Caps.MaxAnisotropy);
            glSamplerParameterf(Sampler.ID, GL_TEXTURE_MAX_ANISOTROPY, Aniso);
        }
#endif

        if (Desc.AddressU == AddressMode::ClampToBorder || Desc.AddressV == AddressMode::ClampToBorder) {
            f32 Border[4] {0.0f, 0.0f, 0.0f, 0.0f};
            if (Desc.Border == BorderColor::OpaqueBlack) Border[3] = 1.0f;
            if (Desc.Border == BorderColor::OpaqueWhite) Border[0] = Border[1] = Border[2] = Border[3] = 1.0f;
            glSamplerParameterfv(Sampler.ID, GL_TEXTURE_BORDER_COLOR, Border);
        }

        if (Desc.DebugName && _Desc.EnableDebugMarkers) glObjectLabel(GL_SAMPLER, Sampler.ID, -1, Desc.DebugName);

        return _Samplers.Allocate(Sampler);
    }

    ShaderHandle GLRenderDevice::CreateShader(const ShaderDesc& Desc) {
        GLShader Shader;
        Shader.Stage = Desc.Stage;
        Shader.ID    = glCreateShader(ToGLShaderStage(Desc.Stage));

        if (Desc.SourceType == ShaderSourceType::SPIRV) {
            glShaderBinary(1, &Shader.ID, GL_SHADER_BINARY_FORMAT_SPIR_V, Desc.Code, CAST<GLsizei>(Desc.CodeSize));
            glSpecializeShader(Shader.ID, Desc.EntryPoint ? Desc.EntryPoint : "main", 0, nullptr, nullptr);
        } else {
            const auto* Source = CAST<const GLchar*>(Desc.Code);
            const GLint Length = CAST<GLint>(Desc.CodeSize);
            glShaderSource(Shader.ID, 1, &Source, Desc.CodeSize ? &Length : nullptr);
            glCompileShader(Shader.ID);
        }

        GLint Compiled = GL_FALSE;
        glGetShaderiv(Shader.ID, GL_COMPILE_STATUS, &Compiled);
        if (!Compiled) {
            GLint LogLength = 0;
            glGetShaderiv(Shader.ID, GL_INFO_LOG_LENGTH, &LogLength);
            std::vector<char> InfoLog(LogLength > 1 ? LogLength : 1, '\0');
            glGetShaderInfoLog(Shader.ID, LogLength, nullptr, InfoLog.data());
            Log(true, "shader compile failed (%s):\n%s", Desc.DebugName ? Desc.DebugName : "unnamed", InfoLog.data());
            glDeleteShader(Shader.ID);
            return {};
        }

        if (Desc.DebugName && _Desc.EnableDebugMarkers) glObjectLabel(GL_SHADER, Shader.ID, -1, Desc.DebugName);

        return _Shaders.Allocate(Shader);
    }

    PipelineHandle GLRenderDevice::CreateGraphicsPipeline(const GraphicsPipelineDesc& Desc) {
        GLPipeline Pipeline;
        Pipeline.Program      = glCreateProgram();
        Pipeline.Layout       = Desc.Layout;
        Pipeline.Topology     = ToGLTopology(Desc.Topology);
        Pipeline.Rasterizer   = Desc.Rasterizer;
        Pipeline.DepthStencil = Desc.DepthStencil;
        Pipeline.Blend        = Desc.Blend;

        const ShaderHandle Stages[] = {Desc.VertexShader, Desc.FragmentShader, Desc.GeometryShader};
        for (const ShaderHandle H : Stages) {
            if (const GLShader* S = _Shaders.Get(H)) glAttachShader(Pipeline.Program, S->ID);
        }

        glLinkProgram(Pipeline.Program);

        GLint Linked = GL_FALSE;
        glGetProgramiv(Pipeline.Program, GL_LINK_STATUS, &Linked);
        if (!Linked) {
            GLint LogLength = 0;
            glGetProgramiv(Pipeline.Program, GL_INFO_LOG_LENGTH, &LogLength);
            std::vector<char> InfoLog(LogLength > 1 ? LogLength : 1, '\0');
            glGetProgramInfoLog(Pipeline.Program, LogLength, nullptr, InfoLog.data());
            Log(true, "program link failed (%s):\n%s", Desc.DebugName ? Desc.DebugName : "unnamed", InfoLog.data());
            glDeleteProgram(Pipeline.Program);
            return {};
        }

        // Detach so the shader objects can be destroyed independently of the
        // pipeline that linked them.
        for (const ShaderHandle H : Stages) {
            if (const GLShader* S = _Shaders.Get(H)) glDetachShader(Pipeline.Program, S->ID);
        }

        Pipeline.VAO = GetOrCreateVAO(Desc.Layout);
        for (u32 i = 0; i < Desc.Layout.BindingCount; ++i) {
            if (const VertexBufferBinding& B = Desc.Layout.Bindings[i]; B.Binding < MAX_VERTEX_BUFFERS)
                Pipeline.Strides[B.Binding] = B.Stride;
        }

        if (Desc.DebugName && _Desc.EnableDebugMarkers) glObjectLabel(GL_PROGRAM, Pipeline.Program, -1, Desc.DebugName);

        return _Pipelines.Allocate(Pipeline);
    }

    PipelineHandle GLRenderDevice::CreateComputePipeline(const ComputePipelineDesc& Desc) {
        const GLShader* Shader = _Shaders.Get(Desc.ComputeShader);
        if (!Shader) {
            Log(true, "CreateComputePipeline: invalid compute shader handle");
            return {};
        }

        GLPipeline Pipeline;
        Pipeline.IsCompute = true;
        Pipeline.Program   = glCreateProgram();
        glAttachShader(Pipeline.Program, Shader->ID);
        glLinkProgram(Pipeline.Program);

        GLint Linked = GL_FALSE;
        glGetProgramiv(Pipeline.Program, GL_LINK_STATUS, &Linked);
        if (!Linked) {
            GLint LogLength = 0;
            glGetProgramiv(Pipeline.Program, GL_INFO_LOG_LENGTH, &LogLength);
            std::vector InfoLog(LogLength > 1 ? LogLength : 1, '\0');
            glGetProgramInfoLog(Pipeline.Program, LogLength, nullptr, InfoLog.data());
            Log(true, "compute link failed (%s):\n%s", Desc.DebugName ? Desc.DebugName : "unnamed", InfoLog.data());
            glDeleteProgram(Pipeline.Program);
            return {};
        }

        glDetachShader(Pipeline.Program, Shader->ID);
        return _Pipelines.Allocate(Pipeline);
    }

    // =======================================================================
    // Destruction, deferred so a resource drawn with this frame can be
    // destroyed now
    // =======================================================================
    void GLRenderDevice::EnqueueDelete(const GLObjectType Type, const GLuint ID) {
        if (ID) _PendingDeletes.push_back({.Type = Type, .ID = ID, .Frame = _FrameCounter});
    }

    void GLRenderDevice::ProcessDeletions() {
        const u32 Frames = _Desc.FramesInFlight ? _Desc.FramesInFlight : 3;
        if (_FrameCounter < Frames) return;

        const u32 Threshold = _FrameCounter - Frames;

        size_t Write = 0;
        for (size_t Read = 0; Read < _PendingDeletes.size(); ++Read) {
            const PendingDelete& P = _PendingDeletes[Read];
            if (P.Frame > Threshold) {
                _PendingDeletes[Write++] = P;
                continue;
            }

            switch (P.Type) {
                case GLObjectType::Buffer:
                    glDeleteBuffers(1, &P.ID);
                    break;
                case GLObjectType::Texture:
                    glDeleteTextures(1, &P.ID);
                    break;
                case GLObjectType::Sampler:
                    glDeleteSamplers(1, &P.ID);
                    break;
                case GLObjectType::Shader:
                    glDeleteShader(P.ID);
                    break;
                case GLObjectType::Program:
                    glDeleteProgram(P.ID);
                    break;
                case GLObjectType::VAO:
                    glDeleteVertexArrays(1, &P.ID);
                    break;
                case GLObjectType::FBO:
                    glDeleteFramebuffers(1, &P.ID);
                    break;
            }
        }

        _PendingDeletes.resize(Write);
    }

    void GLRenderDevice::DestroyBuffer(const BufferHandle Handle) {
        if (const GLBuffer* Buffer = _Buffers.Get(Handle)) {
            if (Buffer->Transient) return;  // owned by the ring
            if (Buffer->Mapped) glUnmapNamedBuffer(Buffer->ID);
            EnqueueDelete(GLObjectType::Buffer, Buffer->ID);
        }
        _Buffers.Free(Handle);
    }

    void GLRenderDevice::DestroyTexture(const TextureHandle Handle) {
        if (const GLTexture* Texture = _Textures.Get(Handle)) {
            EnqueueDelete(GLObjectType::Texture, Texture->ID);

            // Any cached FBO referencing this texture is now stale. Dropping
            // the whole cache is cheap - it refills within a frame or two.
            for (const auto& FBO : _FBOCache | std::views::values)
                EnqueueDelete(GLObjectType::FBO, FBO);
            _FBOCache.clear();
        }
        _Textures.Free(Handle);
    }

    void GLRenderDevice::DestroySampler(const SamplerHandle Handle) {
        if (const GLSampler* Sampler = _Samplers.Get(Handle)) EnqueueDelete(GLObjectType::Sampler, Sampler->ID);
        _Samplers.Free(Handle);
    }

    void GLRenderDevice::DestroyShader(const ShaderHandle Handle) {
        if (const GLShader* Shader = _Shaders.Get(Handle)) EnqueueDelete(GLObjectType::Shader, Shader->ID);
        _Shaders.Free(Handle);
    }

    void GLRenderDevice::DestroyPipeline(const PipelineHandle Handle) {
        if (const GLPipeline* Pipeline = _Pipelines.Get(Handle))
            EnqueueDelete(GLObjectType::Program, Pipeline->Program);
        // The VAO is shared through the layout cache and outlives the pipeline.
        _Pipelines.Free(Handle);
    }

    // =======================================================================
    // Immediate operations
    // =======================================================================
    void GLRenderDevice::UploadTexture(const TextureHandle Handle, const TextureUploadDesc& Upload) {
        const GLTexture* Texture = _Textures.Get(Handle);
        if (!Texture) {
            Log(true, "UploadTexture: stale or unset texture handle");
            return;
        }

        const auto [InternalFormat, BaseFormat, DataType, Compressed] = ToGLFormat(Texture->Fmt);
        const u32 Mip                                                 = Upload.MipLevel;
        const u32 Width  = Upload.Width ? Upload.Width : (Texture->Width >> Mip ? Texture->Width >> Mip : 1);
        const u32 Height = Upload.Height ? Upload.Height : (Texture->Height >> Mip ? Texture->Height >> Mip : 1);

        const bool Layered = Texture->Target == GL_TEXTURE_2D_ARRAY || Texture->Target == GL_TEXTURE_CUBE_MAP;

        if (Compressed) {
            if (Layered) {
                glCompressedTextureSubImage3D(Texture->ID,
                                              CAST<GLint>(Mip),
                                              CAST<GLint>(Upload.X),
                                              CAST<GLint>(Upload.Y),
                                              CAST<GLint>(Upload.ArrayLayer),
                                              CAST<GLsizei>(Width),
                                              CAST<GLsizei>(Height),
                                              1,
                                              InternalFormat,
                                              CAST<GLsizei>(Upload.DataSize),
                                              Upload.Data);
            } else {
                glCompressedTextureSubImage2D(Texture->ID,
                                              CAST<GLint>(Mip),
                                              CAST<GLint>(Upload.X),
                                              CAST<GLint>(Upload.Y),
                                              CAST<GLsizei>(Width),
                                              CAST<GLsizei>(Height),
                                              InternalFormat,
                                              CAST<GLsizei>(Upload.DataSize),
                                              Upload.Data);
            }
        } else {
            if (Layered) {
                glTextureSubImage3D(Texture->ID,
                                    CAST<GLint>(Mip),
                                    CAST<GLint>(Upload.X),
                                    CAST<GLint>(Upload.Y),
                                    CAST<GLint>(Upload.ArrayLayer),
                                    CAST<GLsizei>(Width),
                                    CAST<GLsizei>(Height),
                                    1,
                                    BaseFormat,
                                    DataType,
                                    Upload.Data);
            } else {
                glTextureSubImage2D(Texture->ID,
                                    CAST<GLint>(Mip),
                                    CAST<GLint>(Upload.X),
                                    CAST<GLint>(Upload.Y),
                                    CAST<GLsizei>(Width),
                                    CAST<GLsizei>(Height),
                                    BaseFormat,
                                    DataType,
                                    Upload.Data);
            }
        }
    }

    void GLRenderDevice::UpdateBuffer(const BufferHandle Handle, const u64 Offset, const void* Data, const u64 Size) {
        const GLBuffer* Buffer = _Buffers.Get(Handle);
        if (!Buffer || !Data || !Size) return;

        if (Buffer->Mapped) std::memcpy(CAST<u8*>(Buffer->Mapped) + Offset, Data, Size);
        else glNamedBufferSubData(Buffer->ID, CAST<GLintptr>(Offset), CAST<GLsizeiptr>(Size), Data);
    }

    void* GLRenderDevice::MapBuffer(const BufferHandle Handle, const u64 Offset, const u64 Size) {
        const GLBuffer* Buffer = _Buffers.Get(Handle);
        if (!Buffer) return nullptr;
        if (Buffer->Mapped) return CAST<u8*>(Buffer->Mapped) + Offset;
        return glMapNamedBufferRange(Buffer->ID,
                                     CAST<GLintptr>(Offset),
                                     CAST<GLsizeiptr>(Size),
                                     GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT);
    }

    void GLRenderDevice::UnmapBuffer(const BufferHandle Handle) {
        if (const GLBuffer* Buffer = _Buffers.Get(Handle); Buffer && !Buffer->Mapped) glUnmapNamedBuffer(Buffer->ID);
    }

    TransientAllocation GLRenderDevice::AllocateTransient(const u32 Size, const BufferUsage Usage) {
        const TransientAllocation Alloc = _Transient.Allocate(Size, Usage, _Transient.GetHandle(_FrameIndex));
        if (!Alloc.IsValid())
            Log(true,
                "transient ring exhausted (%u bytes requested); raise DeviceDescriptor::TransientBufferSize",
                Size);
        return Alloc;
    }

    // =======================================================================
    // Caches
    // =======================================================================
    GLuint GLRenderDevice::GetOrCreateVAO(const VertexLayout& Layout) {
        const u64 Key = HashBytes(&Layout, sizeof(Layout));
        if (const auto It = _VAOCache.find(Key); It != _VAOCache.end()) return It->second;

        GLuint VAO = 0;
        glCreateVertexArrays(1, &VAO);

        // Format and buffer binding are declared separately, which is why this
        // VAO is reusable by every mesh sharing the layout rather than being
        // bound to one buffer.
        for (u32 i = 0; i < Layout.AttributeCount; ++i) {
            const VertexAttribute& Attr                  = Layout.Attributes[i];
            const auto [Size, Type, Normalized, Integer] = ToGLVertexFormat(Attr.Fmt);

            glEnableVertexArrayAttrib(VAO, Attr.Location);
            if (Integer) {
                glVertexArrayAttribIFormat(VAO, Attr.Location, Size, Type, Attr.Offset);
            } else {
                glVertexArrayAttribFormat(VAO, Attr.Location, Size, Type, Normalized, Attr.Offset);
            }
            glVertexArrayAttribBinding(VAO, Attr.Location, Attr.Binding);
        }

        for (u32 i = 0; i < Layout.BindingCount; ++i) {
            const VertexBufferBinding& B = Layout.Bindings[i];
            glVertexArrayBindingDivisor(VAO, B.Binding, B.InputRate == VertexInputRate::Instance ? B.Divisor : 0);
        }

        _VAOCache.emplace(Key, VAO);
        return VAO;
    }

    GLuint GLRenderDevice::GetOrCreateFBO(const RenderPassDesc& Desc) {
        // Keyed on what actually determines FBO identity: the attached images.
        struct Key {
            GLuint Color[MAX_COLOR_ATTACHMENTS] {};
            u32 ColorMip[MAX_COLOR_ATTACHMENTS] {};
            u32 ColorLayer[MAX_COLOR_ATTACHMENTS] {};
            GLuint Depth {0};
            u32 DepthMip {0};
            u32 DepthLayer {0};
            u32 Count {0};
            u32 HasDepth {0};
        } K {};

        K.Count    = Desc.ColorAttachmentCount;
        K.HasDepth = Desc.HasDepthStencil ? 1u : 0u;

        for (u32 i = 0; i < Desc.ColorAttachmentCount; ++i) {
            if (const GLTexture* Tex = _Textures.Get(Desc.ColorAttachments[i].Texture)) {
                K.Color[i]      = Tex->ID;
                K.ColorMip[i]   = Desc.ColorAttachments[i].MipLevel;
                K.ColorLayer[i] = Desc.ColorAttachments[i].ArrayLayer;
            }
        }
        if (Desc.HasDepthStencil) {
            if (const GLTexture* Tex = _Textures.Get(Desc.DepthStencil.Texture)) {
                K.Depth      = Tex->ID;
                K.DepthMip   = Desc.DepthStencil.MipLevel;
                K.DepthLayer = Desc.DepthStencil.ArrayLayer;
            }
        }

        const u64 Hash = HashBytes(&K, sizeof(K));
        if (const auto It = _FBOCache.find(Hash); It != _FBOCache.end()) return It->second;

        GLuint FBO = 0;
        glCreateFramebuffers(1, &FBO);

        GLenum DrawBuffers[MAX_COLOR_ATTACHMENTS] {};
        for (u32 i = 0; i < Desc.ColorAttachmentCount; ++i) {
            const ColorAttachment& A = Desc.ColorAttachments[i];
            const GLTexture* Tex     = _Textures.Get(A.Texture);
            if (!Tex) {
                DrawBuffers[i] = GL_NONE;
                continue;
            }

            if (const bool Layered = Tex->Target == GL_TEXTURE_2D_ARRAY || Tex->Target == GL_TEXTURE_CUBE_MAP) {
                glNamedFramebufferTextureLayer(FBO,
                                               GL_COLOR_ATTACHMENT0 + i,
                                               Tex->ID,
                                               CAST<GLint>(A.MipLevel),
                                               CAST<GLint>(A.ArrayLayer));
            } else {
                glNamedFramebufferTexture(FBO, GL_COLOR_ATTACHMENT0 + i, Tex->ID, CAST<GLint>(A.MipLevel));
            }

            DrawBuffers[i] = GL_COLOR_ATTACHMENT0 + i;
        }

        if (Desc.ColorAttachmentCount == 0) {
            glNamedFramebufferDrawBuffer(FBO, GL_NONE);
            glNamedFramebufferReadBuffer(FBO, GL_NONE);
        } else {
            glNamedFramebufferDrawBuffers(FBO, CAST<GLsizei>(Desc.ColorAttachmentCount), DrawBuffers);
        }

        if (Desc.HasDepthStencil) {
            if (const GLTexture* Tex = _Textures.Get(Desc.DepthStencil.Texture)) {
                const GLenum Point = IsDepthStencilFormat(Tex->Fmt) ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
                if (const bool Layered = Tex->Target == GL_TEXTURE_2D_ARRAY || Tex->Target == GL_TEXTURE_CUBE_MAP) {
                    glNamedFramebufferTextureLayer(FBO,
                                                   Point,
                                                   Tex->ID,
                                                   CAST<GLint>(Desc.DepthStencil.MipLevel),
                                                   CAST<GLint>(Desc.DepthStencil.ArrayLayer));
                } else {
                    glNamedFramebufferTexture(FBO, Point, Tex->ID, CAST<GLint>(Desc.DepthStencil.MipLevel));
                }
            }
        }

        if (const GLenum Status = glCheckNamedFramebufferStatus(FBO, GL_FRAMEBUFFER);
            Status != GL_FRAMEBUFFER_COMPLETE) {
            Log(true, "incomplete framebuffer (%s): 0x%X", Desc.DebugName ? Desc.DebugName : "unnamed", Status);
        }

        if (Desc.DebugName && _Desc.EnableDebugMarkers) glObjectLabel(GL_FRAMEBUFFER, FBO, -1, Desc.DebugName);

        _FBOCache.emplace(Hash, FBO);
        return FBO;
    }

    // =======================================================================
    // State application
    // =======================================================================
    void GLRenderDevice::ApplyRasterizer(const RasterizerState& State) {
        const bool Force = _ForceStateApply;

        const bool Cull = State.Cull != CullMode::None;
        if (Force || Cull != _State.CullEnabled) {
            Cull ? glEnable(GL_CULL_FACE) : glDisable(GL_CULL_FACE);
            _State.CullEnabled = Cull;
        }
        if (Cull) {
            if (const GLenum Face = State.Cull == CullMode::Front ? GL_FRONT : GL_BACK;
                Force || Face != _State.CullFace) {
                glCullFace(Face);
                _State.CullFace = Face;
            }
        }

        if (const GLenum Front = State.Front == FrontFace::Clockwise ? GL_CW : GL_CCW;
            Force || Front != _State.FrontFace) {
            glFrontFace(Front);
            _State.FrontFace = Front;
        }

        if (const GLenum Poly = State.Fill == FillMode::Wireframe ? GL_LINE : GL_FILL;
            Force || Poly != _State.PolygonMode) {
            glPolygonMode(GL_FRONT_AND_BACK, Poly);
            _State.PolygonMode = Poly;
        }

        if (Force || State.LineWidth != _State.LineWidth) {
            glLineWidth(State.LineWidth);
            _State.LineWidth = State.LineWidth;
        }
    }

    void GLRenderDevice::ApplyDepthStencil(const DepthStencilState& State) {
        const bool Force = _ForceStateApply;

        if (Force || State.DepthTestEnable != _State.DepthTest) {
            State.DepthTestEnable ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
            _State.DepthTest = State.DepthTestEnable;
        }
        if (Force || State.DepthWriteEnable != _State.DepthWrite) {
            glDepthMask(State.DepthWriteEnable ? GL_TRUE : GL_FALSE);
            _State.DepthWrite = State.DepthWriteEnable;
        }

        if (const GLenum Func = ToGLCompareOp(State.DepthCompare); Force || Func != _State.DepthFunc) {
            glDepthFunc(Func);
            _State.DepthFunc = Func;
        }

        if (Force || State.StencilEnable != _State.StencilTest) {
            State.StencilEnable ? glEnable(GL_STENCIL_TEST) : glDisable(GL_STENCIL_TEST);
            _State.StencilTest = State.StencilEnable;
        }

        if (State.StencilEnable) {
            glStencilFuncSeparate(GL_FRONT,
                                  ToGLCompareOp(State.Front.Compare),
                                  CAST<GLint>(_StencilRef),
                                  State.StencilReadMask);
            glStencilFuncSeparate(GL_BACK,
                                  ToGLCompareOp(State.Back.Compare),
                                  CAST<GLint>(_StencilRef),
                                  State.StencilReadMask);
            glStencilOpSeparate(GL_FRONT,
                                ToGLStencilOp(State.Front.FailOp),
                                ToGLStencilOp(State.Front.DepthFailOp),
                                ToGLStencilOp(State.Front.PassOp));
            glStencilOpSeparate(GL_BACK,
                                ToGLStencilOp(State.Back.FailOp),
                                ToGLStencilOp(State.Back.DepthFailOp),
                                ToGLStencilOp(State.Back.PassOp));
            glStencilMask(State.StencilWriteMask);
        }
    }

    void GLRenderDevice::ApplyBlend(const BlendState& State) {
        const bool Force = _ForceStateApply;
        const u32 Count  = State.IndependentBlend ? MAX_COLOR_ATTACHMENTS : 1;

        for (u32 i = 0; i < Count; ++i) {
            const BlendAttachmentState& Want = State.Attachments[i];
            const BlendAttachmentState& Have = _State.BlendState_[i];

            if (Force || Want.BlendEnable != _State.BlendEnabled[i]) {
                if (State.IndependentBlend) {
                    Want.BlendEnable ? glEnablei(GL_BLEND, i) : glDisablei(GL_BLEND, i);
                } else {
                    Want.BlendEnable ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
                    for (bool& Enabled : _State.BlendEnabled)
                        Enabled = Want.BlendEnable;
                }
                _State.BlendEnabled[i] = Want.BlendEnable;
            }

            if (Want.BlendEnable && (Force || Want.SrcColor != Have.SrcColor || Want.DstColor != Have.DstColor ||
                                     Want.SrcAlpha != Have.SrcAlpha || Want.DstAlpha != Have.DstAlpha ||
                                     Want.ColorOp != Have.ColorOp || Want.AlphaOp != Have.AlphaOp)) {
                if (State.IndependentBlend) {
                    glBlendFuncSeparatei(i,
                                         ToGLBlendFactor(Want.SrcColor),
                                         ToGLBlendFactor(Want.DstColor),
                                         ToGLBlendFactor(Want.SrcAlpha),
                                         ToGLBlendFactor(Want.DstAlpha));
                    glBlendEquationSeparatei(i, ToGLBlendOp(Want.ColorOp), ToGLBlendOp(Want.AlphaOp));
                } else {
                    glBlendFuncSeparate(ToGLBlendFactor(Want.SrcColor),
                                        ToGLBlendFactor(Want.DstColor),
                                        ToGLBlendFactor(Want.SrcAlpha),
                                        ToGLBlendFactor(Want.DstAlpha));
                    glBlendEquationSeparate(ToGLBlendOp(Want.ColorOp), ToGLBlendOp(Want.AlphaOp));
                }
            }

            if (Force || Want.WriteMask != Have.WriteMask) {
                const GLboolean R = Any(Want.WriteMask, ColorMask::R) ? GL_TRUE : GL_FALSE;
                const GLboolean G = Any(Want.WriteMask, ColorMask::G) ? GL_TRUE : GL_FALSE;
                const GLboolean B = Any(Want.WriteMask, ColorMask::B) ? GL_TRUE : GL_FALSE;
                const GLboolean A = Any(Want.WriteMask, ColorMask::A) ? GL_TRUE : GL_FALSE;
                if (State.IndependentBlend) glColorMaski(i, R, G, B, A);
                else glColorMask(R, G, B, A);
            }

            _State.BlendState_[i] = Want;
            if (!State.IndependentBlend) {
                for (BlendAttachmentState& S : _State.BlendState_)
                    S = Want;
            }
        }

        _State.IndependentBlend = State.IndependentBlend;
    }

    void GLRenderDevice::ApplyPipeline(const GLPipeline& Pipeline) {
        if (Pipeline.Program != _State.Program) {
            glUseProgram(Pipeline.Program);
            _State.Program = Pipeline.Program;
            ++_Stats.PipelineBinds;
        } else {
            ++_Stats.RedundantBindsSkipped;
        }

        if (Pipeline.IsCompute) {
            _CurrentPipeline = &Pipeline;
            return;
        }

        if (Pipeline.VAO != _State.VAO) {
            glBindVertexArray(Pipeline.VAO);
            _State.VAO = Pipeline.VAO;
            // A new VAO carries its own vertex and element bindings, so
            // everything bound previously must be re-applied.
            _VertexBufferDirtyMask = 0xFFFFFFFF;
            _IndexBufferDirty      = true;
        }

        ApplyRasterizer(Pipeline.Rasterizer);
        ApplyDepthStencil(Pipeline.DepthStencil);
        ApplyBlend(Pipeline.Blend);

        _ForceStateApply = false;
        _CurrentPipeline = &Pipeline;
    }

    void GLRenderDevice::FlushVertexState() {
        if (!_CurrentPipeline) return;

        if (_VertexBufferDirtyMask) {
            for (u32 Slot = 0; Slot < MAX_VERTEX_BUFFERS; ++Slot) {
                if (!(_VertexBufferDirtyMask & (1u << Slot))) continue;
                const auto& [Buffer, Offset] = _PendingVB[Slot];
                if (!Buffer) continue;
                glVertexArrayVertexBuffer(_State.VAO,
                                          Slot,
                                          Buffer,
                                          Offset,
                                          CAST<GLsizei>(_CurrentPipeline->Strides[Slot]));
            }
            _VertexBufferDirtyMask = 0;
        }

        if (_IndexBufferDirty) {
            glVertexArrayElementBuffer(_State.VAO, _PendingIndexBuffer);
            _IndexBufferDirty = false;
        }
    }

    // =======================================================================
    // Render passes
    // =======================================================================
    void GLRenderDevice::ExecuteBeginRenderPass(const RenderPassDesc& Desc) {
        const GLuint FBO = Desc.IsSwapChainTarget ? 0 : GetOrCreateFBO(Desc);
        if (FBO != _State.Framebuffer) {
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FBO);
            _State.Framebuffer = FBO;
        }

        // Clears respect the scissor test, the depth mask and the colour
        // masks, so force them open first and record the change in the cache.
        // The next pipeline bind fixes whatever it cares about.
        if (_State.ScissorTest) {
            glDisable(GL_SCISSOR_TEST);
            _State.ScissorTest = false;
        }

        bool NeedsDepthWrite = false;
        if (Desc.HasDepthStencil && Desc.DepthStencil.DepthLoad == LoadOp::Clear) NeedsDepthWrite = true;
        if (NeedsDepthWrite && !_State.DepthWrite) {
            glDepthMask(GL_TRUE);
            _State.DepthWrite = true;
        }

        bool NeedsColorMask = false;
        for (u32 i = 0; i < Desc.ColorAttachmentCount; ++i) {
            if (Desc.ColorAttachments[i].Load == LoadOp::Clear) NeedsColorMask = true;
        }
        if (NeedsColorMask) {
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            for (BlendAttachmentState& S : _State.BlendState_)
                S.WriteMask = ColorMask::All;
        }

        for (u32 i = 0; i < Desc.ColorAttachmentCount; ++i) {
            const ColorAttachment& A = Desc.ColorAttachments[i];
            if (A.Load == LoadOp::Clear) glClearNamedFramebufferfv(FBO, GL_COLOR, CAST<GLint>(i), A.Clear.Color);
        }

        if (Desc.HasDepthStencil) {
            const DepthStencilAttachment& DS = Desc.DepthStencil;
            const bool ClearDepth            = DS.DepthLoad == LoadOp::Clear;
            const bool ClearStencil          = DS.StencilLoad == LoadOp::Clear;

            if (ClearDepth && ClearStencil) {
                glClearNamedFramebufferfi(FBO, GL_DEPTH_STENCIL, 0, DS.Clear.Depth, CAST<GLint>(DS.Clear.Stencil));
            } else if (ClearDepth) {
                glClearNamedFramebufferfv(FBO, GL_DEPTH, 0, &DS.Clear.Depth);
            } else if (ClearStencil) {
                const GLint Stencil = CAST<GLint>(DS.Clear.Stencil);
                glClearNamedFramebufferiv(FBO, GL_STENCIL, 0, &Stencil);
            }
        }

        // Default viewport covers the whole target. An explicit SetViewport
        // after BeginRenderPass overrides it.
        u32 Width  = _SwapWidth;
        u32 Height = _SwapHeight;
        if (!Desc.IsSwapChainTarget) {
            const TextureHandle First =
              Desc.ColorAttachmentCount ? Desc.ColorAttachments[0].Texture : Desc.DepthStencil.Texture;
            if (const GLTexture* Tex = _Textures.Get(First)) {
                const u32 Mip =
                  Desc.ColorAttachmentCount ? Desc.ColorAttachments[0].MipLevel : Desc.DepthStencil.MipLevel;
                Width  = Tex->Width >> Mip ? Tex->Width >> Mip : 1;
                Height = Tex->Height >> Mip ? Tex->Height >> Mip : 1;
            }
        }

        if (Desc.IsSwapChainTarget && (Width == 0 || Height == 0)) {
            Log(true,
                "swap chain pass with a %ux%u viewport - SetSwapChainSize was never called, "
                "so the clear will land but nothing will rasterize",
                Width,
                Height);
        }

        glViewport(0, 0, CAST<GLsizei>(Width), CAST<GLsizei>(Height));
        _State.View = Viewport {
          .X        = 0.0f,
          .Y        = 0.0f,
          .Width    = CAST<f32>(Width),
          .Height   = CAST<f32>(Height),
          .MinDepth = 0.0f,
          .MaxDepth = 1.0f,
        };

        _CurrentPass    = Desc;
        _CurrentPassFBO = FBO;
        _InRenderPass   = true;
        ++_Stats.RenderPasses;
    }

    void GLRenderDevice::ExecuteEndRenderPass() {
        // StoreOp::DontCare tells the driver the contents are dead. Free on
        // desktop, a real bandwidth win on tiled GPUs, and it lets the driver
        // skip resolving or writing the attachment back.
        GLenum Discard[MAX_COLOR_ATTACHMENTS + 2];
        GLsizei Count        = 0;
        const bool DefaultFB = _CurrentPassFBO == 0;

        for (u32 i = 0; i < _CurrentPass.ColorAttachmentCount; ++i) {
            if (_CurrentPass.ColorAttachments[i].Store != StoreOp::DontCare) continue;
            Discard[Count++] = DefaultFB ? GL_COLOR : CAST<GLenum>(GL_COLOR_ATTACHMENT0 + i);
        }

        if (_CurrentPass.HasDepthStencil) {
            if (_CurrentPass.DepthStencil.DepthStore == StoreOp::DontCare)
                Discard[Count++] = DefaultFB ? GL_DEPTH : GL_DEPTH_ATTACHMENT;
            if (_CurrentPass.DepthStencil.StencilStore == StoreOp::DontCare)
                Discard[Count++] = DefaultFB ? GL_STENCIL : GL_STENCIL_ATTACHMENT;
        }

        if (Count > 0) glInvalidateNamedFramebufferData(_CurrentPassFBO, Count, Discard);

        _InRenderPass = false;
    }

    // =======================================================================
    // Frame
    // =======================================================================
    void GLRenderDevice::BeginFrame() {
        const u32 Frames = _Desc.FramesInFlight ? _Desc.FramesInFlight : 3;
        _FrameIndex      = _FrameCounter % Frames;

        _Transient.BeginFrame(_FrameIndex);
        ProcessDeletions();

        _Stats           = FrameStats {};
        _CurrentPipeline = nullptr;

        // ImGui, the window system, or any other GL consumer may have touched
        // state between frames. One invalidation per frame is cheap insurance.
        _State.Invalidate();
        _ForceStateApply = true;
    }

    void GLRenderDevice::EndFrame() {
        _Stats.TransientBytesUsed = _Transient.GetBytesUsed();
        _LastStats                = _Stats;
        _Transient.EndFrame(_FrameIndex);
        ++_FrameCounter;
    }

    void GLRenderDevice::SetSwapChainSize(const u32 Width, const u32 Height) {
        _SwapWidth  = Width;
        _SwapHeight = Height;
    }

    // =======================================================================
    // The dispatch loop
    // =======================================================================
    void GLRenderDevice::Submit(const CommandBuffer& Commands) {
        CommandIterator It(Commands);

        while (It.HasNext()) {
            const CmdHeader& Header = It.Next();
            ++_Stats.CommandsExecuted;

            switch (Header.Type) {
                case CmdType::BeginRenderPass:
                    ExecuteBeginRenderPass(It.Payload<Cmd::BeginRenderPass>().Desc);
                    break;

                case CmdType::EndRenderPass:
                    ExecuteEndRenderPass();
                    break;

                case CmdType::SetViewport: {
                    const Viewport& V = It.Payload<Cmd::SetViewport>().View;
                    glViewport(CAST<GLint>(V.X), CAST<GLint>(V.Y), CAST<GLsizei>(V.Width), CAST<GLsizei>(V.Height));
                    glDepthRangef(V.MinDepth, V.MaxDepth);
                    _State.View = V;
                    break;
                }

                case CmdType::SetScissor: {
                    const auto& P = It.Payload<Cmd::SetScissor>();
                    if (P.Enabled != _State.ScissorTest) {
                        P.Enabled ? glEnable(GL_SCISSOR_TEST) : glDisable(GL_SCISSOR_TEST);
                        _State.ScissorTest = P.Enabled;
                    }
                    if (P.Enabled) {
                        glScissor(P.Rect.X, P.Rect.Y, CAST<GLsizei>(P.Rect.Width), CAST<GLsizei>(P.Rect.Height));
                        _State.Scissor = P.Rect;
                    }
                    break;
                }

                case CmdType::SetBlendConstants: {
                    const auto& C = It.Payload<Cmd::SetBlendConstants>().Constants;
                    glBlendColor(C[0], C[1], C[2], C[3]);
                    break;
                }

                case CmdType::SetStencilReference:
                    _StencilRef = It.Payload<Cmd::SetStencilReference>().Reference;
                    if (_CurrentPipeline && _CurrentPipeline->DepthStencil.StencilEnable)
                        ApplyDepthStencil(_CurrentPipeline->DepthStencil);
                    break;

                case CmdType::BindPipeline: {
                    const PipelineHandle H = It.Payload<Cmd::BindPipeline>().Pipeline;
                    if (const GLPipeline* P = _Pipelines.Get(H)) ApplyPipeline(*P);
                    else Log(true, "BindPipeline: stale or unset pipeline handle");
                    break;
                }

                case CmdType::BindVertexBuffer: {
                    const auto& P = It.Payload<Cmd::BindVertexBuffer>();
                    if (P.Slot >= MAX_VERTEX_BUFFERS) break;
                    const GLBuffer* Buffer = _Buffers.Get(P.Buffer);
                    _PendingVB[P.Slot]     = {Buffer ? Buffer->ID : 0, CAST<GLintptr>(P.Offset)};
                    _VertexBufferDirtyMask |= (1u << P.Slot);
                    break;
                }

                case CmdType::BindIndexBuffer: {
                    const auto& P          = It.Payload<Cmd::BindIndexBuffer>();
                    const GLBuffer* Buffer = _Buffers.Get(P.Buffer);
                    _PendingIndexBuffer    = Buffer ? Buffer->ID : 0;
                    _IndexBufferDirty      = true;
                    _IndexType             = ToGLIndexType(P.Type);
                    _IndexSize             = GetIndexTypeSize(P.Type);
                    _IndexOffset           = P.Offset;
                    break;
                }

                case CmdType::BindUniformBuffer: {
                    const auto& P = It.Payload<Cmd::BindUniformBuffer>();
                    if (const GLBuffer* Buffer = _Buffers.Get(P.Buffer)) {
                        const GLsizeiptr Size =
                          P.Size ? CAST<GLsizeiptr>(P.Size) : CAST<GLsizeiptr>(Buffer->Size - P.Offset);
                        glBindBufferRange(GL_UNIFORM_BUFFER, P.Slot, Buffer->ID, CAST<GLintptr>(P.Offset), Size);
                    }
                    break;
                }

                case CmdType::BindStorageBuffer: {
                    const auto& P = It.Payload<Cmd::BindStorageBuffer>();
                    if (const GLBuffer* Buffer = _Buffers.Get(P.Buffer)) {
                        const GLsizeiptr Size =
                          P.Size ? CAST<GLsizeiptr>(P.Size) : CAST<GLsizeiptr>(Buffer->Size - P.Offset);
                        glBindBufferRange(GL_SHADER_STORAGE_BUFFER, P.Slot, Buffer->ID, CAST<GLintptr>(P.Offset), Size);
                    }
                    break;
                }

                case CmdType::BindTexture: {
                    const auto& P            = It.Payload<Cmd::BindTexture>();
                    const GLTexture* Texture = _Textures.Get(P.Texture);
                    const GLuint TexID       = Texture ? Texture->ID : 0;

                    if (P.Slot < _State.TextureUnits.size()) {
                        if (_State.TextureUnits[P.Slot] != TexID) {
                            glBindTextureUnit(P.Slot, TexID);
                            _State.TextureUnits[P.Slot] = TexID;
                        } else {
                            ++_Stats.RedundantBindsSkipped;
                        }

                        const GLSampler* Sampler = _Samplers.Get(P.Sampler);
                        const GLuint SamplerID   = Sampler ? Sampler->ID : 0;
                        if (_State.SamplerUnits[P.Slot] != SamplerID) {
                            glBindSampler(P.Slot, SamplerID);
                            _State.SamplerUnits[P.Slot] = SamplerID;
                        }
                    } else {
                        glBindTextureUnit(P.Slot, TexID);
                    }
                    break;
                }

                case CmdType::Draw: {
                    const auto& P = It.Payload<Cmd::Draw>();
                    FlushVertexState();
                    if (!_CurrentPipeline) break;
                    glDrawArraysInstancedBaseInstance(_CurrentPipeline->Topology,
                                                      CAST<GLint>(P.FirstVertex),
                                                      CAST<GLsizei>(P.VertexCount),
                                                      CAST<GLsizei>(P.InstanceCount),
                                                      P.FirstInstance);
                    ++_Stats.DrawCalls;
                    break;
                }

                case CmdType::DrawIndexed: {
                    const auto& P = It.Payload<Cmd::DrawIndexed>();
                    FlushVertexState();
                    if (!_CurrentPipeline) break;
                    const auto Offset =
                      RCAST<const void*>(CAST<uptr>(_IndexOffset + CAST<u64>(P.FirstIndex) * CAST<u64>(_IndexSize)));
                    glDrawElementsInstancedBaseVertexBaseInstance(_CurrentPipeline->Topology,
                                                                  CAST<GLsizei>(P.IndexCount),
                                                                  _IndexType,
                                                                  Offset,
                                                                  CAST<GLsizei>(P.InstanceCount),
                                                                  P.VertexOffset,
                                                                  P.FirstInstance);
                    ++_Stats.DrawCalls;
                    break;
                }

                case CmdType::DrawIndexedIndirect: {
                    const auto& P = It.Payload<Cmd::DrawIndexedIndirect>();
                    FlushVertexState();
                    if (!_CurrentPipeline) break;
                    if (const GLBuffer* Buffer = _Buffers.Get(P.Buffer)) {
                        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, Buffer->ID);
                        glDrawElementsIndirect(_CurrentPipeline->Topology,
                                               _IndexType,
                                               RCAST<const void*>(CAST<uptr>(P.Offset)));
                        ++_Stats.DrawCalls;
                    }
                    break;
                }

                case CmdType::MultiDrawIndexedIndirect: {
                    const auto& P = It.Payload<Cmd::DrawIndexedIndirect>();
                    FlushVertexState();
                    if (!_CurrentPipeline) break;
                    if (const GLBuffer* Buffer = _Buffers.Get(P.Buffer)) {
                        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, Buffer->ID);
                        glMultiDrawElementsIndirect(_CurrentPipeline->Topology,
                                                    _IndexType,
                                                    RCAST<const void*>(CAST<uptr>(P.Offset)),
                                                    CAST<GLsizei>(P.DrawCount),
                                                    CAST<GLsizei>(P.Stride));
                        _Stats.DrawCalls += P.DrawCount;
                    }
                    break;
                }

                case CmdType::Dispatch: {
                    const auto& P = It.Payload<Cmd::Dispatch>();
                    glDispatchCompute(P.GroupsX, P.GroupsY, P.GroupsZ);
                    break;
                }

                case CmdType::UpdateBuffer: {
                    const auto& P = It.Payload<Cmd::UpdateBuffer>();
                    UpdateBuffer(P.Buffer, P.Offset, It.Trailing<Cmd::UpdateBuffer>(), P.Size);
                    break;
                }

                case CmdType::CopyBuffer: {
                    const auto& P       = It.Payload<Cmd::CopyBuffer>();
                    const GLBuffer* Src = _Buffers.Get(P.Src);
                    const GLBuffer* Dst = _Buffers.Get(P.Dst);
                    if (Src && Dst) {
                        glCopyNamedBufferSubData(Src->ID,
                                                 Dst->ID,
                                                 CAST<GLintptr>(P.SrcOffset),
                                                 CAST<GLintptr>(P.DstOffset),
                                                 CAST<GLsizeiptr>(P.Size));
                    }
                    break;
                }

                case CmdType::GenerateMips: {
                    const auto& P = It.Payload<Cmd::GenerateMips>();
                    if (const GLTexture* Texture = _Textures.Get(P.Texture)) glGenerateTextureMipmap(Texture->ID);
                    break;
                }

                case CmdType::MemoryBarrier: {
                    const Cmd::BarrierBits Bits = It.Payload<Cmd::MemoryBarrier>().Bits;
                    using B                     = Cmd::BarrierBits;
                    GLbitfield Flags            = 0;
                    if (Any(Bits, B::VertexBuffer)) Flags |= GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT;
                    if (Any(Bits, B::IndexBuffer)) Flags |= GL_ELEMENT_ARRAY_BARRIER_BIT;
                    if (Any(Bits, B::UniformBuffer)) Flags |= GL_UNIFORM_BARRIER_BIT;
                    if (Any(Bits, B::StorageBuffer)) Flags |= GL_SHADER_STORAGE_BARRIER_BIT;
                    if (Any(Bits, B::TextureFetch)) Flags |= GL_TEXTURE_FETCH_BARRIER_BIT;
                    if (Any(Bits, B::IndirectBuffer)) Flags |= GL_COMMAND_BARRIER_BIT;
                    if (Any(Bits, B::Framebuffer)) Flags |= GL_FRAMEBUFFER_BARRIER_BIT;
                    glMemoryBarrier(Flags ? Flags : GL_ALL_BARRIER_BITS);
                    break;
                }

                case CmdType::PushDebugGroup: {
                    if (!_Desc.EnableDebugMarkers) break;
                    const auto& P = It.Payload<Cmd::DebugLabel>();
                    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION,
                                     0,
                                     CAST<GLsizei>(P.Length),
                                     RCAST<const GLchar*>(It.Trailing<Cmd::DebugLabel>()));
                    break;
                }

                case CmdType::PopDebugGroup:
                    if (_Desc.EnableDebugMarkers) glPopDebugGroup();
                    break;

                case CmdType::InsertDebugMarker: {
                    if (!_Desc.EnableDebugMarkers) break;
                    const auto& P = It.Payload<Cmd::DebugLabel>();
                    glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION,
                                         GL_DEBUG_TYPE_MARKER,
                                         0,
                                         GL_DEBUG_SEVERITY_NOTIFICATION,
                                         CAST<GLsizei>(P.Length),
                                         RCAST<const GLchar*>(It.Trailing<Cmd::DebugLabel>()));
                    break;
                }

                default:
                    // Unknown opcode. TotalSize is in the header, so skipping
                    // is safe rather than a desync.
                    break;
            }
        }
    }
}  // namespace Xen::RHI::GL

namespace Xen::RHI {
    std::unique_ptr<IRenderDevice> CreateRenderDevice(const Backend API) {
        switch (API) {
            case Backend::OpenGL:
                return std::make_unique<GL::GLRenderDevice>();
            // case Backend::Vulkan: return std::make_unique<VK::VulkanRenderDevice>();
            // case Backend::D3D12:  return std::make_unique<D3D12::D3D12RenderDevice>();
            default:
                return nullptr;
        }
    }
}  // namespace Xen::RHI