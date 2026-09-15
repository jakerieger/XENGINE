//
// Created by Jake Rieger on 9/11/2026.
//
// Records a backend-agnostic stream of commands.
//
// This class has no virtual functions and knows nothing about any graphics
// API. It packs POD payloads into a byte buffer; the backend walks that buffer
// at submit time. Three consequences worth understanding:
//
//   * No vtable dispatch per draw. The only virtual calls are on
//     IRenderDevice, which runs at resource-creation frequency.
//   * Recording is thread-safe by construction, as long as each thread owns
//     its own CommandBuffer. Nothing shared is touched while recording.
//   * The stream is just bytes, so it can be dumped, diffed between frames, or
//     walked by a validation layer without touching the GPU.
//

#pragma once

#include "RHI.hpp"

#include <cstring>
#include <type_traits>
#include <vector>

namespace Xen::RHI {
    enum class CmdType : u16 {
        Invalid = 0,

        BeginRenderPass,
        EndRenderPass,

        SetViewport,
        SetScissor,
        SetBlendConstants,
        SetStencilReference,

        BindPipeline,
        BindVertexBuffer,
        BindIndexBuffer,
        BindUniformBuffer,
        BindStorageBuffer,
        BindTexture,

        Draw,
        DrawIndexed,
        DrawIndexedIndirect,
        MultiDrawIndexedIndirect,

        Dispatch,

        UpdateBuffer,  // trailing bytes follow the payload
        CopyBuffer,
        GenerateMips,
        MemoryBarrier,

        PushDebugGroup,  // trailing string follows the payload
        PopDebugGroup,
        InsertDebugMarker,

        Count
    };

    /// @brief 8 bytes, which keeps every payload naturally aligned.
    ///
    /// TotalSize lives here so a backend can skip an opcode it does not
    /// implement instead of desynchronizing the walk.
    struct CmdHeader {
        CmdType Type;
        u16 _Pad;
        u32 TotalSize;  // header + payload + trailing data, already aligned
    };
    static_assert(sizeof(CmdHeader) == 8, "CmdHeader must be 8 bytes");

    namespace Cmd {
        struct BeginRenderPass {
            RenderPassDesc Desc;
        };

        struct EndRenderPass {};

        struct SetViewport {
            Viewport View;
        };

        struct SetScissor {
            ScissorRect Rect;
            bool Enabled;
            u8 _Pad[3];
        };

        struct SetBlendConstants {
            f32 Constants[4];
        };

        struct SetStencilReference {
            u32 Reference;
        };

        struct BindPipeline {
            PipelineHandle Pipeline;
        };

        struct BindVertexBuffer {
            BufferHandle Buffer;
            u32 Slot;
            u64 Offset;
        };

        struct BindIndexBuffer {
            BufferHandle Buffer;
            u64 Offset;
            IndexType Type;
            u8 _Pad[3];
        };

        struct BindUniformBuffer {
            BufferHandle Buffer;
            u32 Slot;
            u64 Offset;
            u64 Size;
        };

        struct BindStorageBuffer {
            BufferHandle Buffer;
            u32 Slot;
            u64 Offset;
            u64 Size;
        };

        struct BindTexture {
            TextureHandle Texture;
            SamplerHandle Sampler;
            u32 Slot;
        };

        struct Draw {
            u32 VertexCount;
            u32 InstanceCount;
            u32 FirstVertex;
            u32 FirstInstance;
        };

        struct DrawIndexed {
            u32 IndexCount;
            u32 InstanceCount;
            u32 FirstIndex;
            i32 VertexOffset;
            u32 FirstInstance;
        };

        struct DrawIndexedIndirect {
            BufferHandle Buffer;
            u64 Offset;
            u32 DrawCount;
            u32 Stride;
        };

        struct Dispatch {
            u32 GroupsX, GroupsY, GroupsZ;
        };

        struct UpdateBuffer {
            BufferHandle Buffer;
            u64 Offset;
            u32 Size;
        };

        struct CopyBuffer {
            BufferHandle Src, Dst;
            u64 SrcOffset, DstOffset, Size;
        };

        struct GenerateMips {
            TextureHandle Texture;
        };

        enum class BarrierBits : u32 {
            None           = 0,
            VertexBuffer   = 1 << 0,
            IndexBuffer    = 1 << 1,
            UniformBuffer  = 1 << 2,
            StorageBuffer  = 1 << 3,
            TextureFetch   = 1 << 4,
            IndirectBuffer = 1 << 5,
            Framebuffer    = 1 << 6,
            All            = 0xFFFFFFFF,
        };

        constexpr BarrierBits operator|(BarrierBits A, BarrierBits B) {
            return CAST<BarrierBits>(CAST<u32>(A) | CAST<u32>(B));
        }

        constexpr bool Any(const BarrierBits A, const BarrierBits B) {
            return (CAST<u32>(A) & CAST<u32>(B)) != 0;
        }

        struct MemoryBarrier {
            BarrierBits Bits;
        };

        struct DebugLabel {
            u32 Length;  // string trails, not null-terminated
        };
    }  // namespace Cmd

    class CommandBuffer {
    public:
        explicit CommandBuffer(const size_t ReserveBytes = 64 * 1024) { _Data.reserve(ReserveBytes); }

        CommandBuffer(const CommandBuffer&)            = delete;
        CommandBuffer& operator=(const CommandBuffer&) = delete;
        CommandBuffer(CommandBuffer&&)                 = default;
        CommandBuffer& operator=(CommandBuffer&&)      = default;

        /// @brief Clears the stream but keeps the allocation, so a steady-state
        /// frame records without allocating. Reuse one buffer across frames.
        void Reset() {
            _Data.clear();
            _CommandCount = 0;
            _DrawCount    = 0;
        }

        NODISCARD bool IsEmpty() const { return _Data.empty(); }
        NODISCARD size_t GetCommandCount() const { return _CommandCount; }
        NODISCARD size_t GetDrawCount() const { return _DrawCount; }
        NODISCARD size_t GetSizeInBytes() const { return _Data.size(); }
        NODISCARD const u8* GetData() const { return _Data.data(); }

        // --- Render passes ----------------------------------------------
        void BeginRenderPass(const RenderPassDesc& Desc) {
            Emit<Cmd::BeginRenderPass>(CmdType::BeginRenderPass).Desc = Desc;
        }
        void EndRenderPass() { Emit<Cmd::EndRenderPass>(CmdType::EndRenderPass); }

        // --- Dynamic state ----------------------------------------------
        void SetViewport(const Viewport& View) { Emit<Cmd::SetViewport>(CmdType::SetViewport).View = View; }

        void SetViewport(const f32 X, const f32 Y, const f32 Width, const f32 Height) {
            SetViewport(Viewport {X, Y, Width, Height, 0.0f, 1.0f});
        }

        void SetScissor(const ScissorRect& Rect) {
            auto& P   = Emit<Cmd::SetScissor>(CmdType::SetScissor);
            P.Rect    = Rect;
            P.Enabled = true;
        }
        void DisableScissor() {
            auto& P   = Emit<Cmd::SetScissor>(CmdType::SetScissor);
            P.Rect    = {};
            P.Enabled = false;
        }
        void SetBlendConstants(const f32 R, const f32 G, const f32 B, const f32 A) {
            auto& P        = Emit<Cmd::SetBlendConstants>(CmdType::SetBlendConstants);
            P.Constants[0] = R;
            P.Constants[1] = G;
            P.Constants[2] = B;
            P.Constants[3] = A;
        }
        void SetStencilReference(const u32 Reference) {
            Emit<Cmd::SetStencilReference>(CmdType::SetStencilReference).Reference = Reference;
        }

        // --- Binding ------------------------------------------------------
        void BindPipeline(const PipelineHandle Pipeline) {
            Emit<Cmd::BindPipeline>(CmdType::BindPipeline).Pipeline = Pipeline;
        }

        void BindVertexBuffer(const u32 Slot, const BufferHandle Buffer, const u64 Offset = 0) {
            auto& P  = Emit<Cmd::BindVertexBuffer>(CmdType::BindVertexBuffer);
            P.Slot   = Slot;
            P.Buffer = Buffer;
            P.Offset = Offset;
        }
        void BindVertexBuffer(const u32 Slot, const TransientAllocation& Alloc) {
            BindVertexBuffer(Slot, Alloc.Buffer, Alloc.Offset);
        }

        void BindIndexBuffer(const BufferHandle Buffer, const IndexType Type, const u64 Offset = 0) {
            auto& P  = Emit<Cmd::BindIndexBuffer>(CmdType::BindIndexBuffer);
            P.Buffer = Buffer;
            P.Type   = Type;
            P.Offset = Offset;
        }

        void BindUniformBuffer(const u32 Slot, const BufferHandle Buffer, const u64 Offset = 0, const u64 Size = 0) {
            auto& P  = Emit<Cmd::BindUniformBuffer>(CmdType::BindUniformBuffer);
            P.Slot   = Slot;
            P.Buffer = Buffer;
            P.Offset = Offset;
            P.Size   = Size;
        }
        void BindUniformBuffer(const u32 Slot, const TransientAllocation& Alloc) {
            BindUniformBuffer(Slot, Alloc.Buffer, Alloc.Offset, Alloc.Size);
        }

        void BindStorageBuffer(const u32 Slot, const BufferHandle Buffer, const u64 Offset = 0, const u64 Size = 0) {
            auto& P  = Emit<Cmd::BindStorageBuffer>(CmdType::BindStorageBuffer);
            P.Slot   = Slot;
            P.Buffer = Buffer;
            P.Offset = Offset;
            P.Size   = Size;
        }

        void BindTexture(const u32 Slot, const TextureHandle Texture, const SamplerHandle Sampler = {}) {
            auto& P   = Emit<Cmd::BindTexture>(CmdType::BindTexture);
            P.Slot    = Slot;
            P.Texture = Texture;
            P.Sampler = Sampler;
        }

        // --- Draws --------------------------------------------------------
        void Draw(const u32 VertexCount,
                  const u32 InstanceCount = 1,
                  const u32 FirstVertex   = 0,
                  const u32 FirstInstance = 0) {
            auto& P         = Emit<Cmd::Draw>(CmdType::Draw);
            P.VertexCount   = VertexCount;
            P.InstanceCount = InstanceCount;
            P.FirstVertex   = FirstVertex;
            P.FirstInstance = FirstInstance;
            ++_DrawCount;
        }

        void DrawIndexed(const u32 IndexCount,
                         const u32 InstanceCount = 1,
                         const u32 FirstIndex    = 0,
                         const i32 VertexOffset  = 0,
                         const u32 FirstInstance = 0) {
            auto& P         = Emit<Cmd::DrawIndexed>(CmdType::DrawIndexed);
            P.IndexCount    = IndexCount;
            P.InstanceCount = InstanceCount;
            P.FirstIndex    = FirstIndex;
            P.VertexOffset  = VertexOffset;
            P.FirstInstance = FirstInstance;
            ++_DrawCount;
        }

        void DrawIndexedIndirect(const BufferHandle Args,
                                 const u64 Offset    = 0,
                                 const u32 DrawCount = 1,
                                 const u32 Stride    = sizeof(DrawIndexedIndirectCommand)) {
            auto& P     = Emit<Cmd::DrawIndexedIndirect>(DrawCount > 1 ? CmdType::MultiDrawIndexedIndirect
                                                                   : CmdType::DrawIndexedIndirect);
            P.Buffer    = Args;
            P.Offset    = Offset;
            P.DrawCount = DrawCount;
            P.Stride    = Stride;
            _DrawCount += DrawCount;
        }

        void Dispatch(const u32 GroupsX, const u32 GroupsY = 1, const u32 GroupsZ = 1) {
            auto& P   = Emit<Cmd::Dispatch>(CmdType::Dispatch);
            P.GroupsX = GroupsX;
            P.GroupsY = GroupsY;
            P.GroupsZ = GroupsZ;
        }

        // --- Transfers & sync ---------------------------------------------

        /// @brief Copies Size bytes into the stream. Fine for small updates;
        /// for anything sizeable use AllocateTransient plus CopyBuffer.
        void UpdateBuffer(const BufferHandle Buffer, const u64 Offset, const void* Data, const u32 Size) {
            auto& P  = Emit<Cmd::UpdateBuffer>(CmdType::UpdateBuffer, Size);
            P.Buffer = Buffer;
            P.Offset = Offset;
            P.Size   = Size;
            WriteTrailing(&P, sizeof(P), Data, Size);
        }

        void CopyBuffer(
          const BufferHandle Src, const u64 SrcOffset, const BufferHandle Dst, const u64 DstOffset, const u64 Size) {
            auto& P     = Emit<Cmd::CopyBuffer>(CmdType::CopyBuffer);
            P.Src       = Src;
            P.SrcOffset = SrcOffset;
            P.Dst       = Dst;
            P.DstOffset = DstOffset;
            P.Size      = Size;
        }

        void GenerateMips(const TextureHandle Texture) {
            Emit<Cmd::GenerateMips>(CmdType::GenerateMips).Texture = Texture;
        }

        void MemoryBarrier(const Cmd::BarrierBits Bits) {
            Emit<Cmd::MemoryBarrier>(CmdType::MemoryBarrier).Bits = Bits;
        }

        // --- Debug --------------------------------------------------------
        void PushDebugGroup(const char* Label) { EmitLabel(CmdType::PushDebugGroup, Label); }
        void PopDebugGroup() { Emit<Cmd::EndRenderPass>(CmdType::PopDebugGroup); }
        void InsertDebugMarker(const char* Label) { EmitLabel(CmdType::InsertDebugMarker, Label); }

    private:
        static constexpr size_t Alignment = 8;
        static constexpr size_t AlignUp(const size_t N) { return (N + Alignment - 1) & ~(Alignment - 1); }

        /// @brief Reserves header + payload (+ optional trailing bytes) and
        /// returns a default-constructed payload. The reference is valid only
        /// until the next Emit on this buffer.
        template<typename T>
        T& Emit(const CmdType Type, const size_t TrailingBytes = 0) {
            static_assert(std::is_trivially_copyable_v<T>, "command payloads must be POD");

            const size_t Total  = AlignUp(sizeof(CmdHeader) + sizeof(T) + TrailingBytes);
            const size_t Offset = _Data.size();
            _Data.resize(Offset + Total);

            // Taken after the resize: it may have reallocated.
            auto* Header      = RCAST<CmdHeader*>(_Data.data() + Offset);
            Header->Type      = Type;
            Header->_Pad      = 0;
            Header->TotalSize = CAST<u32>(Total);

            void* Payload = _Data.data() + Offset + sizeof(CmdHeader);
            ++_CommandCount;
            return *(new (Payload) T {});
        }

        static void WriteTrailing(void* Payload, const size_t PayloadSize, const void* Data, const size_t Size) {
            if (Data && Size) std::memcpy(CAST<u8*>(Payload) + PayloadSize, Data, Size);
        }

        void EmitLabel(const CmdType Type, const char* Label) {
            const u32 Length = Label ? CAST<u32>(std::strlen(Label)) : 0;
            auto& P          = Emit<Cmd::DebugLabel>(Type, Length);
            P.Length         = Length;
            WriteTrailing(&P, sizeof(P), Label, Length);
        }

        std::vector<u8> _Data;
        size_t _CommandCount {0};
        size_t _DrawCount {0};
    };

    /// @brief Backend-side walker over a recorded stream.
    class CommandIterator {
    public:
        CommandIterator(const u8* Data, const size_t Size) : _Data(Data), _Size(Size) {}
        explicit CommandIterator(const CommandBuffer& Buffer)
            : _Data(Buffer.GetData()), _Size(Buffer.GetSizeInBytes()) {}

        NODISCARD bool HasNext() const { return _Offset + sizeof(CmdHeader) <= _Size; }

        const CmdHeader& Next() {
            const auto* Header = RCAST<const CmdHeader*>(_Data + _Offset);
            _Current           = _Offset;
            _Offset += Header->TotalSize;
            return *Header;
        }

        /// @brief Payload of the command returned by the most recent Next().
        template<typename T>
        const T& Payload() const {
            return *RCAST<const T*>(_Data + _Current + sizeof(CmdHeader));
        }

        template<typename T>
        NODISCARD const u8* Trailing() const {
            return _Data + _Current + sizeof(CmdHeader) + sizeof(T);
        }

    private:
        const u8* _Data {nullptr};
        size_t _Size {0};
        size_t _Offset {0};
        size_t _Current {0};
    };
}  // namespace Xen::RHI