//
// Created by Jake Rieger on 9/15/2026.
//

#pragma once

#include "../RHI.hpp"

#include <directx/d3d12.h>
#include <dxgi1_4.h>

namespace Xen::RHI::D3D12Backend {
    inline DXGI_FORMAT ToDXGIFormat(const Format Fmt) {
        switch (Fmt) {
            case Format::R8_UNORM: return DXGI_FORMAT_R8_UNORM;
            case Format::RG8_UNORM: return DXGI_FORMAT_R8G8_UNORM;
            case Format::RGBA8_UNORM: return DXGI_FORMAT_R8G8B8A8_UNORM;
            case Format::RGBA8_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            case Format::BGRA8_UNORM: return DXGI_FORMAT_B8G8R8A8_UNORM;
            case Format::R8_UINT: return DXGI_FORMAT_R8_UINT;
            case Format::RGBA8_UINT: return DXGI_FORMAT_R8G8B8A8_UINT;

            case Format::R16_FLOAT: return DXGI_FORMAT_R16_FLOAT;
            case Format::RG16_FLOAT: return DXGI_FORMAT_R16G16_FLOAT;
            case Format::RGBA16_FLOAT: return DXGI_FORMAT_R16G16B16A16_FLOAT;
            case Format::R16_UINT: return DXGI_FORMAT_R16_UINT;

            case Format::R32_FLOAT: return DXGI_FORMAT_R32_FLOAT;
            case Format::RG32_FLOAT: return DXGI_FORMAT_R32G32_FLOAT;
            case Format::RGB32_FLOAT: return DXGI_FORMAT_R32G32B32_FLOAT;
            case Format::RGBA32_FLOAT: return DXGI_FORMAT_R32G32B32A32_FLOAT;
            case Format::R32_UINT: return DXGI_FORMAT_R32_UINT;
            case Format::RG32_UINT: return DXGI_FORMAT_R32G32_UINT;
            case Format::RGBA32_UINT: return DXGI_FORMAT_R32G32B32A32_UINT;

            case Format::RGB10A2_UNORM: return DXGI_FORMAT_R10G10B10A2_UNORM;
            case Format::R11G11B10_FLOAT: return DXGI_FORMAT_R11G11B10_FLOAT;

            case Format::BC1_UNORM: return DXGI_FORMAT_BC1_UNORM;
            case Format::BC3_UNORM: return DXGI_FORMAT_BC3_UNORM;
            case Format::BC7_UNORM: return DXGI_FORMAT_BC7_UNORM;
            case Format::BC7_SRGB: return DXGI_FORMAT_BC7_UNORM_SRGB;

            case Format::D16_UNORM: return DXGI_FORMAT_D16_UNORM;
            case Format::D24_UNORM_S8_UINT: return DXGI_FORMAT_D24_UNORM_S8_UINT;
            case Format::D32_FLOAT: return DXGI_FORMAT_D32_FLOAT;

            default: return DXGI_FORMAT_R8G8B8A8_UNORM;
        }
    }

    inline D3D12_PRIMITIVE_TOPOLOGY ToD3DTopology(const PrimitiveTopology Topology) {
        switch (Topology) {
            case PrimitiveTopology::PointList: return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
            case PrimitiveTopology::LineList: return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
            case PrimitiveTopology::LineStrip: return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
            case PrimitiveTopology::TriangleList: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            case PrimitiveTopology::TriangleStrip: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        }
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    }

    inline D3D12_PRIMITIVE_TOPOLOGY_TYPE ToD3DTopologyType(const PrimitiveTopology Topology) {
        switch (Topology) {
            case PrimitiveTopology::PointList: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
            case PrimitiveTopology::LineList:
            case PrimitiveTopology::LineStrip: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
            case PrimitiveTopology::TriangleList:
            case PrimitiveTopology::TriangleStrip: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        }
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    }

    inline D3D12_COMPARISON_FUNC ToD3DCompareOp(const CompareOp Op) {
        switch (Op) {
            case CompareOp::Never: return D3D12_COMPARISON_FUNC_NEVER;
            case CompareOp::Less: return D3D12_COMPARISON_FUNC_LESS;
            case CompareOp::Equal: return D3D12_COMPARISON_FUNC_EQUAL;
            case CompareOp::LessEqual: return D3D12_COMPARISON_FUNC_LESS_EQUAL;
            case CompareOp::Greater: return D3D12_COMPARISON_FUNC_GREATER;
            case CompareOp::NotEqual: return D3D12_COMPARISON_FUNC_NOT_EQUAL;
            case CompareOp::GreaterEqual: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
            case CompareOp::Always: return D3D12_COMPARISON_FUNC_ALWAYS;
        }
        return D3D12_COMPARISON_FUNC_LESS;
    }

    inline D3D12_CULL_MODE ToD3DCullMode(const CullMode Mode) {
        switch (Mode) {
            case CullMode::None: return D3D12_CULL_MODE_NONE;
            case CullMode::Front: return D3D12_CULL_MODE_FRONT;
            case CullMode::Back: return D3D12_CULL_MODE_BACK;
        }
        return D3D12_CULL_MODE_NONE;
    }

    inline D3D12_FILL_MODE ToD3DFillMode(const FillMode Mode) {
        return Mode == FillMode::Wireframe ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;
    }

    inline D3D12_BLEND ToD3DBlendFactor(const BlendFactor Factor) {
        switch (Factor) {
            case BlendFactor::Zero: return D3D12_BLEND_ZERO;
            case BlendFactor::One: return D3D12_BLEND_ONE;
            case BlendFactor::SrcColor: return D3D12_BLEND_SRC_COLOR;
            case BlendFactor::OneMinusSrcColor: return D3D12_BLEND_INV_SRC_COLOR;
            case BlendFactor::DstColor: return D3D12_BLEND_DEST_COLOR;
            case BlendFactor::OneMinusDstColor: return D3D12_BLEND_INV_DEST_COLOR;
            case BlendFactor::SrcAlpha: return D3D12_BLEND_SRC_ALPHA;
            case BlendFactor::OneMinusSrcAlpha: return D3D12_BLEND_INV_SRC_ALPHA;
            case BlendFactor::DstAlpha: return D3D12_BLEND_DEST_ALPHA;
            case BlendFactor::OneMinusDstAlpha: return D3D12_BLEND_INV_DEST_ALPHA;
            case BlendFactor::ConstantColor: return D3D12_BLEND_BLEND_FACTOR;
            case BlendFactor::OneMinusConstantColor: return D3D12_BLEND_INV_BLEND_FACTOR;
        }
        return D3D12_BLEND_ONE;
    }

    inline D3D12_BLEND_OP ToD3DBlendOp(const BlendOp Op) {
        switch (Op) {
            case BlendOp::Add: return D3D12_BLEND_OP_ADD;
            case BlendOp::Subtract: return D3D12_BLEND_OP_SUBTRACT;
            case BlendOp::ReverseSubtract: return D3D12_BLEND_OP_REV_SUBTRACT;
            case BlendOp::Min: return D3D12_BLEND_OP_MIN;
            case BlendOp::Max: return D3D12_BLEND_OP_MAX;
        }
        return D3D12_BLEND_OP_ADD;
    }

    inline D3D12_STENCIL_OP ToD3DStencilOp(const StencilOp Op) {
        switch (Op) {
            case StencilOp::Keep: return D3D12_STENCIL_OP_KEEP;
            case StencilOp::Zero: return D3D12_STENCIL_OP_ZERO;
            case StencilOp::Replace: return D3D12_STENCIL_OP_REPLACE;
            case StencilOp::IncrementClamp: return D3D12_STENCIL_OP_INCR_SAT;
            case StencilOp::DecrementClamp: return D3D12_STENCIL_OP_DECR_SAT;
            case StencilOp::Invert: return D3D12_STENCIL_OP_INVERT;
            case StencilOp::IncrementWrap: return D3D12_STENCIL_OP_INCR;
            case StencilOp::DecrementWrap: return D3D12_STENCIL_OP_DECR;
        }
        return D3D12_STENCIL_OP_KEEP;
    }

    inline D3D12_TEXTURE_ADDRESS_MODE ToD3DAddressMode(const AddressMode Mode) {
        switch (Mode) {
            case AddressMode::Repeat: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            case AddressMode::MirrorRepeat: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
            case AddressMode::ClampToEdge: return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            case AddressMode::ClampToBorder: return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        }
        return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    }

    /// @brief The underlying resource format a depth texture must be created
    /// with if it also needs an SRV: a DSV and an SRV over the same memory
    /// can't both use a depth-typed format (D32_FLOAT, ...), so the resource
    /// itself is typeless and each view picks its own compatible format.
    /// Returns UNKNOWN for a non-depth format - callers only use this when
    /// TextureUsage::DepthTarget is set.
    inline DXGI_FORMAT ToTypelessDepthFormat(const Format Fmt) {
        switch (Fmt) {
            case Format::D16_UNORM: return DXGI_FORMAT_R16_TYPELESS;
            case Format::D24_UNORM_S8_UINT: return DXGI_FORMAT_R24G8_TYPELESS;
            case Format::D32_FLOAT: return DXGI_FORMAT_R32_TYPELESS;
            default: return DXGI_FORMAT_UNKNOWN;
        }
    }

    /// @brief The SRV format that reads the depth channel of a typeless depth
    /// resource created via ToTypelessDepthFormat. Stencil sampling isn't
    /// exposed - depth is the overwhelmingly common case (soft particles,
    /// SSAO, depth-based post-process) and stencil-as-SRV needs its own
    /// format/slot, not needed yet.
    inline DXGI_FORMAT ToDepthSrvFormat(const Format Fmt) {
        switch (Fmt) {
            case Format::D16_UNORM: return DXGI_FORMAT_R16_UNORM;
            case Format::D24_UNORM_S8_UINT: return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
            case Format::D32_FLOAT: return DXGI_FORMAT_R32_FLOAT;
            default: return DXGI_FORMAT_UNKNOWN;
        }
    }

    inline D3D12_FILTER ToD3DFilter(const FilterMode Min, const FilterMode Mag, const MipMode Mip, const bool Aniso) {
        if (Aniso) return D3D12_FILTER_ANISOTROPIC;

        const bool LinearMin = Min == FilterMode::Linear;
        const bool LinearMag = Mag == FilterMode::Linear;
        const bool LinearMip = Mip == MipMode::Linear;

        // Encodes as a 3-bit (min,mag,mip) index into D3D12_FILTER's point/linear enum ordering.
        const u32 Bits = (LinearMin ? 0b100u : 0u) | (LinearMag ? 0b010u : 0u) | (LinearMip ? 0b001u : 0u);
        return CAST<D3D12_FILTER>(Bits);
    }
}  // namespace Xen::RHI::D3D12Backend
