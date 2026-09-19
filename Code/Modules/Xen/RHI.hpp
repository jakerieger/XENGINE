//
// Created by Jake Rieger on 9/11/2026.
//

#pragma once

#include <Common/XenCommon.hpp>

#include <cassert>
#include <utility>
#include <vector>

namespace Xen::RHI {
    // These bound the fixed-size arrays in descriptors, so every descriptor stays trivially copyable and cheap to hash.
    inline constexpr u32 MAX_COLOR_ATTACHMENTS = 8;
    inline constexpr u32 MAX_VERTEX_ATTRIBUTES = 16;
    inline constexpr u32 MAX_VERTEX_BUFFERS    = 8;

    // Deliberately shaped like Xen::TextureHandle and Xen::ActorHandle: a
    // single u32 named ID, with 0 reserved for "unset". The generation starts
    // at 1 and skips 0 on wrap, exactly as Scene::ActorSlot does, which is
    // what makes ID == 0 impossible for a live handle.
    inline constexpr u32 HANDLE_INDEX_BITS = 20;
    inline constexpr u32 HANDLE_INDEX_MASK = (1U << HANDLE_INDEX_BITS) - 1;
    inline constexpr u32 HANDLE_GEN_MASK   = 0xFFFU;

    template<typename Tag>
    struct alignas(4) Handle {
        u32 ID {0};

        constexpr Handle() = default;
        constexpr Handle(const u32 Index, const u32 Generation)
            : ID((Index & HANDLE_INDEX_MASK) | ((Generation & HANDLE_GEN_MASK) << HANDLE_INDEX_BITS)) {}

        NODISCARD constexpr u32 Index() const { return ID & HANDLE_INDEX_MASK; }
        NODISCARD constexpr u32 Generation() const { return (ID >> HANDLE_INDEX_BITS) & HANDLE_GEN_MASK; }
        NODISCARD constexpr bool IsValid() const { return ID != 0; }

        constexpr bool operator==(const Handle& R) const { return ID == R.ID; }
        constexpr bool operator!=(const Handle& R) const { return ID != R.ID; }
    };

    struct BufferTag;
    struct TextureTag;
    struct SamplerTag;
    struct ShaderTag;
    struct PipelineTag;
    struct LayoutTag;

    using BufferHandle   = Handle<BufferTag>;
    using TextureHandle  = Handle<TextureTag>;
    using SamplerHandle  = Handle<SamplerTag>;
    using ShaderHandle   = Handle<ShaderTag>;
    using PipelineHandle = Handle<PipelineTag>;
    using LayoutHandle   = Handle<LayoutTag>;

    static_assert(sizeof(BufferHandle) == 4, "handles must stay 4 bytes");

    /// @brief Backends store their real objects here, addressed by handle.
    ///
    /// Freed slots are recycled FIFO rather than LIFO, so an index is not
    /// immediately reissued. Stale-handle bugs then surface promptly instead
    /// of intermittently.
    template<typename T, typename H>
    class Pool {
    public:
        explicit Pool(const size_t Reserve = 256) {
            _Items.reserve(Reserve);
            _Generations.reserve(Reserve);
        }

        H Allocate(T Value = T {}) {
            u32 Index;

            if (!_FreeSlots.empty()) {
                Index = _FreeSlots.front();
                _FreeSlots.erase(_FreeSlots.begin());
                _Items[Index] = std::move(Value);
                _Alive[Index] = true;
            } else {
                Index = CAST<u32>(_Items.size());
                assert(Index < HANDLE_INDEX_MASK && "RHI resource pool exhausted");
                _Items.emplace_back(std::move(Value));
                _Generations.push_back(1);
                _Alive.push_back(true);
            }

            ++_LiveCount;

            return H {Index, _Generations[Index]};
        }

        void Free(const H Handle) {
            if (!IsValid(Handle)) return;

            const u32 Index = Handle.Index();

            _Generations[Index] = (_Generations[Index] + 1) & HANDLE_GEN_MASK;
            if (_Generations[Index] == 0) _Generations[Index] = 1;

            _Alive[Index] = false;
            _Items[Index] = T {};
            _FreeSlots.push_back(Index);
            --_LiveCount;
        }

        NODISCARD bool IsValid(const H Handle) const {
            if (!Handle.IsValid()) return false;
            const u32 Index = Handle.Index();
            return Index < _Items.size() && _Alive[Index] && _Generations[Index] == Handle.Generation();
        }

        T* Get(const H Handle) { return IsValid(Handle) ? &_Items[Handle.Index()] : nullptr; }
        const T* Get(const H Handle) const { return IsValid(Handle) ? &_Items[Handle.Index()] : nullptr; }

        NODISCARD size_t GetLiveCount() const { return _LiveCount; }

        template<typename Fn>
        void ForEachLive(Fn&& Visit) {
            for (size_t i = 0; i < _Items.size(); ++i) {
                if (_Alive[i]) Visit(_Items[i]);
            }
        }

    private:
        std::vector<T> _Items;
        std::vector<u16> _Generations;
        std::vector<bool> _Alive;
        std::vector<u32> _FreeSlots;
        size_t _LiveCount {0};
    };

    enum class Format : u8 {
        Unknown = 0,

        R8_UNORM,
        RG8_UNORM,
        RGBA8_UNORM,
        RGBA8_SRGB,
        BGRA8_UNORM,
        R8_UINT,
        RGBA8_UINT,

        R16_FLOAT,
        RG16_FLOAT,
        RGBA16_FLOAT,
        R16_UINT,

        R32_FLOAT,
        RG32_FLOAT,
        RGB32_FLOAT,
        RGBA32_FLOAT,
        R32_UINT,
        RG32_UINT,
        RGBA32_UINT,

        RGB10A2_UNORM,
        R11G11B10_FLOAT,

        BC1_UNORM,
        BC3_UNORM,
        BC7_UNORM,
        BC7_SRGB,

        D16_UNORM,
        D24_UNORM_S8_UINT,
        D32_FLOAT,

        Count
    };

    struct FormatInfo {
        const char* Name;
        u8 ComponentCount;
        u8 BytesPerBlock;  // bytes per pixel when uncompressed
        u8 BlockWidth;     // 1 when uncompressed
        u8 BlockHeight;
        bool IsInteger;  // fetched as int rather than float
        bool IsNormalized;
        bool IsSRGB;
        bool IsDepth;
        bool IsStencil;
        bool IsCompressed;
    };

    const FormatInfo& GetFormatInfo(Format Fmt);

    inline bool IsDepthFormat(const Format Fmt) {
        return GetFormatInfo(Fmt).IsDepth;
    }

    inline bool IsDepthStencilFormat(const Format Fmt) {
        const FormatInfo& Info = GetFormatInfo(Fmt);
        return Info.IsDepth && Info.IsStencil;
    }

    enum class BufferUsage : u16 {
        None     = 0,
        Vertex   = 1 << 0,
        Index    = 1 << 1,
        Uniform  = 1 << 2,
        Storage  = 1 << 3,
        Indirect = 1 << 4,
        CopySrc  = 1 << 5,
        CopyDst  = 1 << 6,
    };

    constexpr BufferUsage operator|(const BufferUsage A, const BufferUsage B) {
        return CAST<BufferUsage>(CAST<u16>(A) | CAST<u16>(B));
    }

    constexpr BufferUsage operator&(const BufferUsage A, const BufferUsage B) {
        return CAST<BufferUsage>(CAST<u16>(A) & CAST<u16>(B));
    }

    constexpr bool Any(const BufferUsage A) {
        return CAST<u16>(A) != 0;
    }

    /// @brief How the CPU expects to touch a buffer over its lifetime.
    enum class MemoryUsage : u8 {
        /// Uploaded at creation, rarely written after. GL: immutable storage.
        /// Vulkan: DEVICE_LOCAL.
        GpuOnly,
        /// Written most frames. GL: persistent coherent mapping.
        /// Vulkan: HOST_VISIBLE | HOST_COHERENT.
        CpuToGpu,
        /// Readback target.
        GpuToCpu,
    };

    enum class IndexType : u8 { U16, U32 };

    struct BufferDesc {
        u64 Size {0};
        BufferUsage Usage {BufferUsage::None};
        MemoryUsage Memory {MemoryUsage::GpuOnly};
        const void* InitialData {nullptr};
        const char* DebugName {nullptr};
    };

    enum class TextureType : u8 { Texture2D, Texture2DArray, TextureCube };

    enum class TextureUsage : u16 {
        None        = 0,
        Sampled     = 1 << 0,
        ColorTarget = 1 << 1,
        DepthTarget = 1 << 2,
        CopySrc     = 1 << 3,
        CopyDst     = 1 << 4,
    };

    constexpr TextureUsage operator|(const TextureUsage A, const TextureUsage B) {
        return CAST<TextureUsage>(CAST<u16>(A) | CAST<u16>(B));
    }

    constexpr TextureUsage operator&(const TextureUsage A, const TextureUsage B) {
        return CAST<TextureUsage>(CAST<u16>(A) & CAST<u16>(B));
    }

    constexpr bool Any(const TextureUsage A) {
        return CAST<u16>(A) != 0;
    }

    enum class FilterMode : u8 { Nearest, Linear };
    enum class MipMode : u8 { None, Nearest, Linear };
    enum class AddressMode : u8 { Repeat, MirrorRepeat, ClampToEdge, ClampToBorder };
    enum class BorderColor : u8 { TransparentBlack, OpaqueBlack, OpaqueWhite };

    struct TextureDesc {
        TextureType Type {TextureType::Texture2D};
        Format Fmt {Format::RGBA8_UNORM};
        u32 Width {1};
        u32 Height {1};
        u32 ArrayLayers {1};
        u32 MipLevels {1};  // 0 means the full chain
        u32 SampleCount {1};
        TextureUsage Usage {TextureUsage::Sampled};
        const char* DebugName {nullptr};
    };

    struct TextureUploadDesc {
        const void* Data {nullptr};
        size_t DataSize {0};
        u32 MipLevel {0};
        u32 ArrayLayer {0};
        u32 X {0}, Y {0};
        u32 Width {0}, Height {0};  // 0 means the full mip extent
    };

    struct SamplerDesc {
        FilterMode MinFilter {FilterMode::Linear};
        FilterMode MagFilter {FilterMode::Linear};
        MipMode MipFilter {MipMode::Linear};
        AddressMode AddressU {AddressMode::Repeat};
        AddressMode AddressV {AddressMode::Repeat};
        BorderColor Border {BorderColor::OpaqueBlack};
        u8 MaxAnisotropy {1};  // 1 disables
        f32 MipLodBias {0.0f};
        f32 MinLod {0.0f};
        f32 MaxLod {1000.0f};
        const char* DebugName {nullptr};
    };

    // --- Shaders ----------------------------------------------------------
    enum class ShaderStage : u8 { Vertex, Fragment, Geometry, Compute };

    /// @brief GLSL/SPIRV are retained from the pre-D3D12 GL backend for
    /// reference and unused by any current backend. HLSL is source text,
    /// compiled at shader-creation time via the legacy D3DCompile (SM 5.x) -
    /// no offline shader build step, matching the old GLSL runtime-compile
    /// workflow (still what SpriteRenderer uses). DXIL is already-compiled
    /// bytecode (Shader Model 6.0, produced offline by dxc.exe via
    /// Scripts/compile_engine_shaders.py and loaded from a pak, never from a
    /// game's own Content directory) - CreateShader wraps it directly with
    /// no compilation step, and EntryPoint is unused since it's baked into
    /// the compiled container.
    enum class ShaderSourceType : u8 { GLSL, SPIRV, HLSL, DXIL };

    struct ShaderDesc {
        ShaderStage Stage {ShaderStage::Vertex};
        ShaderSourceType SourceType {ShaderSourceType::GLSL};
        const void* Code {nullptr};
        size_t CodeSize {0};  // bytes; for GLSL this excludes the terminator
        const char* EntryPoint {"main"};  // ignored for DXIL
        const char* DebugName {nullptr};
    };

    // --- Pipeline layout ----------------------------------------------------
    //
    // What a pipeline binds, declared independently of any one pipeline so a
    // material system can build its own binding shape instead of every
    // pipeline sharing one hardcoded layout. A D3D12 backend turns this into
    // a root signature; a future Vulkan backend would turn it into a
    // descriptor set layout - callers only ever see slots and types.
    enum class BindingType : u8 {
        UniformBuffer,
        StorageBuffer,
        SampledTexture,
        StorageTexture,
        Sampler,
    };

    enum class ShaderVisibility : u8 { Vertex, Fragment, Compute, All };

    struct BindingSlot {
        u32 Slot {0};  // shader register: b# for UniformBuffer, t# for
                        // SampledTexture, u# for Storage*, s# for Sampler
        BindingType Type {BindingType::UniformBuffer};
        ShaderVisibility Visibility {ShaderVisibility::All};
        u32 Count {1};  // array size; 1 means a single descriptor, not a table
    };

    struct PipelineLayoutDesc {
        // MeshRenderer's PBR layout alone is 19 (3 cbuffers + 8 texture/
        // sampler pairs, see MaterialBindings.hpp) - Binding() has no bounds
        // check, so this must stay comfortably above the largest layout any
        // pipeline builds.
        static constexpr u32 MAX_BINDINGS = 32;

        BindingSlot Bindings[MAX_BINDINGS] {};
        u8 BindingCount {0};
        const char* DebugName {nullptr};

        PipelineLayoutDesc& Binding(const u32 Slot,
                                    const BindingType Type,
                                    const ShaderVisibility Visibility = ShaderVisibility::All,
                                    const u32 Count                  = 1) {
            Bindings[BindingCount++] = BindingSlot {Slot, Type, Visibility, Count};
            return *this;
        }
    };

    // --- Pipeline state ---------------------------------------------------
    enum class PrimitiveTopology : u8 { PointList, LineList, LineStrip, TriangleList, TriangleStrip };

    enum class VertexInputRate : u8 { Vertex, Instance };

    enum class CompareOp : u8 { Never, Less, Equal, LessEqual, Greater, NotEqual, GreaterEqual, Always };

    enum class CullMode : u8 { None, Front, Back };
    enum class FrontFace : u8 { CounterClockwise, Clockwise };
    enum class FillMode : u8 { Solid, Wireframe };

    enum class BlendFactor : u8 {
        Zero,
        One,
        SrcColor,
        OneMinusSrcColor,
        DstColor,
        OneMinusDstColor,
        SrcAlpha,
        OneMinusSrcAlpha,
        DstAlpha,
        OneMinusDstAlpha,
        ConstantColor,
        OneMinusConstantColor,
    };

    enum class BlendOp : u8 { Add, Subtract, ReverseSubtract, Min, Max };

    enum class ColorMask : u8 {
        None = 0,
        R    = 1 << 0,
        G    = 1 << 1,
        B    = 1 << 2,
        A    = 1 << 3,
        RGB  = R | G | B,
        All  = R | G | B | A
    };

    constexpr ColorMask operator|(const ColorMask A, const ColorMask B) {
        return CAST<ColorMask>(CAST<u8>(A) | CAST<u8>(B));
    }
    constexpr bool Any(const ColorMask A, const ColorMask B) {
        return (CAST<u8>(A) & CAST<u8>(B)) != 0;
    }

    enum class StencilOp : u8 {
        Keep,
        Zero,
        Replace,
        IncrementClamp,
        DecrementClamp,
        Invert,
        IncrementWrap,
        DecrementWrap
    };

    // Every state block below is POD and comparable by memcmp, so the whole
    // pipeline descriptor hashes in one pass.

    struct BlendAttachmentState {
        bool BlendEnable {false};
        BlendFactor SrcColor {BlendFactor::One};
        BlendFactor DstColor {BlendFactor::Zero};
        BlendOp ColorOp {BlendOp::Add};
        BlendFactor SrcAlpha {BlendFactor::One};
        BlendFactor DstAlpha {BlendFactor::Zero};
        BlendOp AlphaOp {BlendOp::Add};
        ColorMask WriteMask {ColorMask::All};

        /// @brief Standard non-premultiplied alpha. What sprites want.
        static BlendAttachmentState AlphaBlend() {
            return BlendAttachmentState {.BlendEnable = true,
                                         .SrcColor    = BlendFactor::SrcAlpha,
                                         .DstColor    = BlendFactor::OneMinusSrcAlpha,
                                         .ColorOp     = BlendOp::Add,
                                         .SrcAlpha    = BlendFactor::One,
                                         .DstAlpha    = BlendFactor::OneMinusSrcAlpha,
                                         .AlphaOp     = BlendOp::Add,
                                         .WriteMask   = ColorMask::All};
        }

        static BlendAttachmentState Additive() {
            return BlendAttachmentState {.BlendEnable = true,
                                         .SrcColor    = BlendFactor::SrcAlpha,
                                         .DstColor    = BlendFactor::One,
                                         .ColorOp     = BlendOp::Add,
                                         .SrcAlpha    = BlendFactor::Zero,
                                         .DstAlpha    = BlendFactor::One,
                                         .AlphaOp     = BlendOp::Add,
                                         .WriteMask   = ColorMask::All};
        }
    };

    struct BlendState {
        /// GL 4.6 supports per-target blend via glBlendFuncSeparatei. When
        /// false only Attachments[0] is read and applied to every target.
        bool IndependentBlend {false};
        u8 _Pad[3] {};
        BlendAttachmentState Attachments[MAX_COLOR_ATTACHMENTS] {};
    };

    struct StencilFaceState {
        StencilOp FailOp {StencilOp::Keep};
        StencilOp DepthFailOp {StencilOp::Keep};
        StencilOp PassOp {StencilOp::Keep};
        CompareOp Compare {CompareOp::Always};
    };

    struct DepthStencilState {
        bool DepthTestEnable {false};
        bool DepthWriteEnable {false};
        CompareOp DepthCompare {CompareOp::Less};
        bool StencilEnable {false};
        u8 StencilReadMask {0xFF};
        u8 StencilWriteMask {0xFF};
        u8 _Pad[2] {};
        StencilFaceState Front {};
        StencilFaceState Back {};
    };

    struct RasterizerState {
        FillMode Fill {FillMode::Solid};
        /// None by default: a 2D sprite with a negative scale flips its
        /// winding, so back-face culling would make mirrored sprites vanish.
        CullMode Cull {CullMode::None};
        FrontFace Front {FrontFace::CounterClockwise};
        u8 _Pad {};
        f32 LineWidth {1.0f};
    };

    // --- Vertex layout ----------------------------------------------------
    //
    // Attribute format is declared separately from the buffer binding, which
    // is GL 4.5's model and Vulkan's. The backend can then cache one VAO per
    // *layout* rather than one per mesh.
    struct VertexAttribute {
        u8 Location {0};  // shader input location
        u8 Binding {0};   // which vertex buffer slot supplies it
        Format Fmt {Format::Unknown};
        u8 _Pad {0};
        u16 Offset {0};  // byte offset within the vertex
        u16 _Pad2 {0};
    };

    struct VertexBufferBinding {
        u8 Binding {0};
        VertexInputRate InputRate {VertexInputRate::Vertex};
        u16 Stride {0};
        u32 Divisor {1};  // only read when InputRate is Instance
    };

    struct VertexLayout {
        VertexAttribute Attributes[MAX_VERTEX_ATTRIBUTES] {};
        VertexBufferBinding Bindings[MAX_VERTEX_BUFFERS] {};
        u8 AttributeCount {0};
        u8 BindingCount {0};
        u8 _Pad[2] {};

        VertexLayout& Attribute(const u8 Location, const u8 Binding, const Format Fmt, const u16 Offset) {
            Attributes[AttributeCount++] = VertexAttribute {
              .Location = Location,
              .Binding  = Binding,
              .Fmt      = Fmt,
              ._Pad     = 0,
              .Offset   = Offset,
              ._Pad2    = 0,
            };
            return *this;
        }

        VertexLayout& Binding(const u8 Binding,
                              const u16 Stride,
                              const VertexInputRate Rate = VertexInputRate::Vertex,
                              const u32 Divisor          = 1) {
            Bindings[BindingCount++] = VertexBufferBinding {
              .Binding   = Binding,
              .InputRate = Rate,
              .Stride    = Stride,
              .Divisor   = Divisor,
            };
            return *this;
        }
    };

    /// @brief Immutable once created.
    ///
    /// ColorFormats and DepthFormat describe the render target layout this
    /// pipeline is compatible with. GL ignores them; Vulkan and D3D12 require
    /// them at creation time. Declaring them now costs nothing and avoids a
    /// refactor later.
    struct GraphicsPipelineDesc {
        ShaderHandle VertexShader {};
        ShaderHandle FragmentShader {};
        ShaderHandle GeometryShader {};  // optional

        /// @brief What this pipeline binds - see PipelineLayoutDesc. Required;
        /// create it first via IRenderDevice::CreatePipelineLayout.
        LayoutHandle PipelineLayout {};

        VertexLayout Layout {};
        PrimitiveTopology Topology {PrimitiveTopology::TriangleList};

        RasterizerState Rasterizer {};
        DepthStencilState DepthStencil {};
        BlendState Blend {};

        Format ColorFormats[MAX_COLOR_ATTACHMENTS] {};
        u8 ColorAttachmentCount {1};
        Format DepthFormat {Format::Unknown};
        u32 SampleCount {1};

        const char* DebugName {nullptr};
    };

    struct ComputePipelineDesc {
        ShaderHandle ComputeShader {};

        /// @brief What this pipeline binds - see PipelineLayoutDesc. Required;
        /// create it first via IRenderDevice::CreatePipelineLayout.
        LayoutHandle PipelineLayout {};

        const char* DebugName {nullptr};
    };

    // --- Render passes ----------------------------------------------------
    enum class LoadOp : u8 { Load, Clear, DontCare };
    enum class StoreOp : u8 { Store, DontCare };  // DontCare invalidates the attachment

    struct ClearValue {
        f32 Color[4] {0.0f, 0.0f, 0.0f, 1.0f};
        f32 Depth {1.0f};
        u32 Stencil {0};
    };

    struct ColorAttachment {
        TextureHandle Texture {};
        u32 MipLevel {0};
        u32 ArrayLayer {0};
        LoadOp Load {LoadOp::Clear};
        StoreOp Store {StoreOp::Store};
        ClearValue Clear {};
    };

    struct DepthStencilAttachment {
        TextureHandle Texture {};
        u32 MipLevel {0};
        u32 ArrayLayer {0};
        LoadOp DepthLoad {LoadOp::Clear};
        StoreOp DepthStore {StoreOp::DontCare};
        LoadOp StencilLoad {LoadOp::DontCare};
        StoreOp StencilStore {StoreOp::DontCare};
        ClearValue Clear {};
    };

    /// @brief Recorded into the command stream rather than created as an
    /// object, so targets can change per frame with no cache lookup on the
    /// caller's side.
    struct RenderPassDesc {
        ColorAttachment ColorAttachments[MAX_COLOR_ATTACHMENTS] {};
        u8 ColorAttachmentCount {0};
        bool HasDepthStencil {false};
        DepthStencilAttachment DepthStencil {};

        /// When true the pass targets the window's default framebuffer and
        /// attachment textures are ignored - only the load/store ops apply.
        bool IsSwapChainTarget {false};

        const char* DebugName {nullptr};

        static RenderPassDesc
        SwapChain(const f32 R = 0.0f, const f32 G = 0.0f, const f32 B = 0.0f, const f32 A = 1.0f) {
            RenderPassDesc D;
            D.IsSwapChainTarget         = true;
            D.ColorAttachmentCount      = 1;
            D.ColorAttachments[0].Load  = LoadOp::Clear;
            D.ColorAttachments[0].Clear = ClearValue {{R, G, B, A}, 1.0f, 0};
            return D;
        }

        /// @brief A single-color-attachment pass targeting an offscreen
        /// texture (a Viewport's color target, a post-process scratch
        /// texture, ...) instead of the swap chain.
        static RenderPassDesc ColorTarget(const TextureHandle Target,
                                          const f32 R = 0.0f,
                                          const f32 G = 0.0f,
                                          const f32 B = 0.0f,
                                          const f32 A = 1.0f) {
            RenderPassDesc D;
            D.ColorAttachmentCount        = 1;
            D.ColorAttachments[0].Texture = Target;
            D.ColorAttachments[0].Load    = LoadOp::Clear;
            D.ColorAttachments[0].Clear   = ClearValue {{R, G, B, A}, 1.0f, 0};
            return D;
        }

        /// @brief Same as ColorTarget, plus a depth attachment cleared to
        /// DepthClear (1.0, the far plane, by convention) - what a
        /// depth-tested 3D pass targeting a Viewport with a depth buffer
        /// uses instead of ColorTarget.
        static RenderPassDesc ColorAndDepthTarget(const TextureHandle Color,
                                                  const TextureHandle Depth,
                                                  const f32 R          = 0.0f,
                                                  const f32 G          = 0.0f,
                                                  const f32 B          = 0.0f,
                                                  const f32 A          = 1.0f,
                                                  const f32 DepthClear = 1.0f) {
            RenderPassDesc D             = ColorTarget(Color, R, G, B, A);
            D.HasDepthStencil            = true;
            D.DepthStencil.Texture       = Depth;
            D.DepthStencil.DepthLoad     = LoadOp::Clear;
            D.DepthStencil.Clear.Depth   = DepthClear;
            return D;
        }
    };

    // --- Misc -------------------------------------------------------------
    struct Viewport {
        f32 X {0.0f}, Y {0.0f}, Width {0.0f}, Height {0.0f};
        f32 MinDepth {0.0f}, MaxDepth {1.0f};
    };

    struct ScissorRect {
        i32 X {0}, Y {0};
        u32 Width {0}, Height {0};
    };

    struct DrawIndirectCommand {
        u32 VertexCount;
        u32 InstanceCount;
        u32 FirstVertex;
        u32 FirstInstance;
    };

    struct DrawIndexedIndirectCommand {
        u32 IndexCount;
        u32 InstanceCount;
        u32 FirstIndex;
        i32 VertexOffset;
        u32 FirstInstance;
    };

    /// @brief Sub-allocation from the per-frame ring buffer.
    ///
    /// Data points into persistently mapped memory. Write to it immediately;
    /// it is not valid past the current frame.
    struct TransientAllocation {
        BufferHandle Buffer {};
        u32 Offset {0};
        u32 Size {0};
        void* Data {nullptr};

        NODISCARD bool IsValid() const { return Data != nullptr; }
    };

    struct DeviceCaps {
        const char* Vendor {""};
        const char* Renderer {""};
        const char* ApiVersion {""};

        u32 MaxTextureSize2D {0};
        u32 MaxTextureArrayLayers {0};
        u32 MaxColorAttachments {0};
        u32 MaxSamples {0};
        u32 MaxAnisotropy {1};
        u32 MaxTextureUnits {0};
        u32 UniformBufferOffsetAlignment {256};
        u32 StorageBufferOffsetAlignment {256};

        bool SupportsSpirv {false};
        bool SupportsMultiDrawIndirect {false};
        bool SupportsAnisotropy {false};
        bool SupportsDebugMarkers {false};
    };
}  // namespace Xen::RHI
