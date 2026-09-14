//
// Created by Jake Rieger on 9/11/2026.
//

#pragma once

#define GLFW_INCLUDE_NONE
#include <glad.h>

#include "../RHI.hpp"

namespace Xen::RHI::GL {
    struct GLFormat {
        GLenum InternalFormat;  // sized, for glTextureStorage
        GLenum BaseFormat;      // for glTextureSubImage
        GLenum DataType;
        bool Compressed;
    };

    inline GLFormat ToGLFormat(const Format Fmt) {
        switch (Fmt) {
            case Format::R8_UNORM:
                return {.InternalFormat = GL_R8,
                        .BaseFormat     = GL_RED,
                        .DataType       = GL_UNSIGNED_BYTE,
                        .Compressed     = false};
            case Format::RG8_UNORM:
                return {.InternalFormat = GL_RG8,
                        .BaseFormat     = GL_RG,
                        .DataType       = GL_UNSIGNED_BYTE,
                        .Compressed     = false};
            case Format::RGBA8_UNORM:
                return {.InternalFormat = GL_RGBA8,
                        .BaseFormat     = GL_RGBA,
                        .DataType       = GL_UNSIGNED_BYTE,
                        .Compressed     = false};
            case Format::RGBA8_SRGB:
                return {.InternalFormat = GL_SRGB8_ALPHA8,
                        .BaseFormat     = GL_RGBA,
                        .DataType       = GL_UNSIGNED_BYTE,
                        .Compressed     = false};
            case Format::BGRA8_UNORM:
                return {.InternalFormat = GL_RGBA8,
                        .BaseFormat     = GL_BGRA,
                        .DataType       = GL_UNSIGNED_BYTE,
                        .Compressed     = false};
            case Format::R8_UINT:
                return {.InternalFormat = GL_R8UI,
                        .BaseFormat     = GL_RED_INTEGER,
                        .DataType       = GL_UNSIGNED_BYTE,
                        .Compressed     = false};
            case Format::RGBA8_UINT:
                return {.InternalFormat = GL_RGBA8UI,
                        .BaseFormat     = GL_RGBA_INTEGER,
                        .DataType       = GL_UNSIGNED_BYTE,
                        .Compressed     = false};

            case Format::R16_FLOAT:
                return {.InternalFormat = GL_R16F,
                        .BaseFormat     = GL_RED,
                        .DataType       = GL_HALF_FLOAT,
                        .Compressed     = false};
            case Format::RG16_FLOAT:
                return {.InternalFormat = GL_RG16F,
                        .BaseFormat     = GL_RG,
                        .DataType       = GL_HALF_FLOAT,
                        .Compressed     = false};
            case Format::RGBA16_FLOAT:
                return {.InternalFormat = GL_RGBA16F,
                        .BaseFormat     = GL_RGBA,
                        .DataType       = GL_HALF_FLOAT,
                        .Compressed     = false};
            case Format::R16_UINT:
                return {.InternalFormat = GL_R16UI,
                        .BaseFormat     = GL_RED_INTEGER,
                        .DataType       = GL_UNSIGNED_SHORT,
                        .Compressed     = false};

            case Format::R32_FLOAT:
                return {.InternalFormat = GL_R32F, .BaseFormat = GL_RED, .DataType = GL_FLOAT, .Compressed = false};
            case Format::RG32_FLOAT:
                return {.InternalFormat = GL_RG32F, .BaseFormat = GL_RG, .DataType = GL_FLOAT, .Compressed = false};
            case Format::RGB32_FLOAT:
                return {.InternalFormat = GL_RGB32F, .BaseFormat = GL_RGB, .DataType = GL_FLOAT, .Compressed = false};
            case Format::RGBA32_FLOAT:
                return {.InternalFormat = GL_RGBA32F, .BaseFormat = GL_RGBA, .DataType = GL_FLOAT, .Compressed = false};
            case Format::R32_UINT:
                return {.InternalFormat = GL_R32UI,
                        .BaseFormat     = GL_RED_INTEGER,
                        .DataType       = GL_UNSIGNED_INT,
                        .Compressed     = false};
            case Format::RG32_UINT:
                return {.InternalFormat = GL_RG32UI,
                        .BaseFormat     = GL_RG_INTEGER,
                        .DataType       = GL_UNSIGNED_INT,
                        .Compressed     = false};
            case Format::RGBA32_UINT:
                return {.InternalFormat = GL_RGBA32UI,
                        .BaseFormat     = GL_RGBA_INTEGER,
                        .DataType       = GL_UNSIGNED_INT,
                        .Compressed     = false};

            case Format::RGB10A2_UNORM:
                return {.InternalFormat = GL_RGB10_A2,
                        .BaseFormat     = GL_RGBA,
                        .DataType       = GL_UNSIGNED_INT_2_10_10_10_REV,
                        .Compressed     = false};
            case Format::R11G11B10_FLOAT:
                return {.InternalFormat = GL_R11F_G11F_B10F,
                        .BaseFormat     = GL_RGB,
                        .DataType       = GL_UNSIGNED_INT_10F_11F_11F_REV,
                        .Compressed     = false};

            case Format::BC1_UNORM:
                return {.InternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
                        .BaseFormat     = GL_RGBA,
                        .DataType       = GL_UNSIGNED_BYTE,
                        .Compressed     = true};
            case Format::BC3_UNORM:
                return {.InternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
                        .BaseFormat     = GL_RGBA,
                        .DataType       = GL_UNSIGNED_BYTE,
                        .Compressed     = true};
            case Format::BC7_UNORM:
                return {.InternalFormat = GL_COMPRESSED_RGBA_BPTC_UNORM,
                        .BaseFormat     = GL_RGBA,
                        .DataType       = GL_UNSIGNED_BYTE,
                        .Compressed     = true};
            case Format::BC7_SRGB:
                return {.InternalFormat = GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM,
                        .BaseFormat     = GL_RGBA,
                        .DataType       = GL_UNSIGNED_BYTE,
                        .Compressed     = true};

            case Format::D16_UNORM:
                return {.InternalFormat = GL_DEPTH_COMPONENT16,
                        .BaseFormat     = GL_DEPTH_COMPONENT,
                        .DataType       = GL_UNSIGNED_SHORT,
                        .Compressed     = false};
            case Format::D24_UNORM_S8_UINT:
                return {.InternalFormat = GL_DEPTH24_STENCIL8,
                        .BaseFormat     = GL_DEPTH_STENCIL,
                        .DataType       = GL_UNSIGNED_INT_24_8,
                        .Compressed     = false};
            case Format::D32_FLOAT:
                return {.InternalFormat = GL_DEPTH_COMPONENT32F,
                        .BaseFormat     = GL_DEPTH_COMPONENT,
                        .DataType       = GL_FLOAT,
                        .Compressed     = false};

            default:
                return {.InternalFormat = GL_RGBA8,
                        .BaseFormat     = GL_RGBA,
                        .DataType       = GL_UNSIGNED_BYTE,
                        .Compressed     = false};
        }
    }

    inline GLenum ToGLTextureTarget(const TextureType Type, const u32 SampleCount) {
        const bool Multisample = SampleCount > 1;
        switch (Type) {
            case TextureType::Texture2D:
                return Multisample ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
            case TextureType::Texture2DArray:
                return Multisample ? GL_TEXTURE_2D_MULTISAMPLE_ARRAY : GL_TEXTURE_2D_ARRAY;
            case TextureType::TextureCube:
                return GL_TEXTURE_CUBE_MAP;
        }
        return GL_TEXTURE_2D;
    }

    inline GLenum ToGLShaderStage(const ShaderStage Stage) {
        switch (Stage) {
            case ShaderStage::Vertex:
                return GL_VERTEX_SHADER;
            case ShaderStage::Fragment:
                return GL_FRAGMENT_SHADER;
            case ShaderStage::Geometry:
                return GL_GEOMETRY_SHADER;
            case ShaderStage::Compute:
                return GL_COMPUTE_SHADER;
        }
        return GL_VERTEX_SHADER;
    }

    inline GLenum ToGLTopology(const PrimitiveTopology Topology) {
        switch (Topology) {
            case PrimitiveTopology::PointList:
                return GL_POINTS;
            case PrimitiveTopology::LineList:
                return GL_LINES;
            case PrimitiveTopology::LineStrip:
                return GL_LINE_STRIP;
            case PrimitiveTopology::TriangleList:
                return GL_TRIANGLES;
            case PrimitiveTopology::TriangleStrip:
                return GL_TRIANGLE_STRIP;
        }
        return GL_TRIANGLES;
    }

    inline GLenum ToGLCompareOp(const CompareOp Op) {
        switch (Op) {
            case CompareOp::Never:
                return GL_NEVER;
            case CompareOp::Less:
                return GL_LESS;
            case CompareOp::Equal:
                return GL_EQUAL;
            case CompareOp::LessEqual:
                return GL_LEQUAL;
            case CompareOp::Greater:
                return GL_GREATER;
            case CompareOp::NotEqual:
                return GL_NOTEQUAL;
            case CompareOp::GreaterEqual:
                return GL_GEQUAL;
            case CompareOp::Always:
                return GL_ALWAYS;
        }
        return GL_LESS;
    }

    inline GLenum ToGLBlendFactor(const BlendFactor Factor) {
        switch (Factor) {
            case BlendFactor::Zero:
                return GL_ZERO;
            case BlendFactor::One:
                return GL_ONE;
            case BlendFactor::SrcColor:
                return GL_SRC_COLOR;
            case BlendFactor::OneMinusSrcColor:
                return GL_ONE_MINUS_SRC_COLOR;
            case BlendFactor::DstColor:
                return GL_DST_COLOR;
            case BlendFactor::OneMinusDstColor:
                return GL_ONE_MINUS_DST_COLOR;
            case BlendFactor::SrcAlpha:
                return GL_SRC_ALPHA;
            case BlendFactor::OneMinusSrcAlpha:
                return GL_ONE_MINUS_SRC_ALPHA;
            case BlendFactor::DstAlpha:
                return GL_DST_ALPHA;
            case BlendFactor::OneMinusDstAlpha:
                return GL_ONE_MINUS_DST_ALPHA;
            case BlendFactor::ConstantColor:
                return GL_CONSTANT_COLOR;
            case BlendFactor::OneMinusConstantColor:
                return GL_ONE_MINUS_CONSTANT_COLOR;
        }
        return GL_ONE;
    }

    inline GLenum ToGLBlendOp(const BlendOp Op) {
        switch (Op) {
            case BlendOp::Add:
                return GL_FUNC_ADD;
            case BlendOp::Subtract:
                return GL_FUNC_SUBTRACT;
            case BlendOp::ReverseSubtract:
                return GL_FUNC_REVERSE_SUBTRACT;
            case BlendOp::Min:
                return GL_MIN;
            case BlendOp::Max:
                return GL_MAX;
        }
        return GL_FUNC_ADD;
    }

    inline GLenum ToGLStencilOp(const StencilOp Op) {
        switch (Op) {
            case StencilOp::Keep:
                return GL_KEEP;
            case StencilOp::Zero:
                return GL_ZERO;
            case StencilOp::Replace:
                return GL_REPLACE;
            case StencilOp::IncrementClamp:
                return GL_INCR;
            case StencilOp::DecrementClamp:
                return GL_DECR;
            case StencilOp::Invert:
                return GL_INVERT;
            case StencilOp::IncrementWrap:
                return GL_INCR_WRAP;
            case StencilOp::DecrementWrap:
                return GL_DECR_WRAP;
        }
        return GL_KEEP;
    }

    inline GLenum ToGLAddressMode(const AddressMode Mode) {
        switch (Mode) {
            case AddressMode::Repeat:
                return GL_REPEAT;
            case AddressMode::MirrorRepeat:
                return GL_MIRRORED_REPEAT;
            case AddressMode::ClampToEdge:
                return GL_CLAMP_TO_EDGE;
            case AddressMode::ClampToBorder:
                return GL_CLAMP_TO_BORDER;
        }
        return GL_REPEAT;
    }

    inline GLenum ToGLMinFilter(const FilterMode Min, const MipMode Mip) {
        if (Mip == MipMode::None) return Min == FilterMode::Linear ? GL_LINEAR : GL_NEAREST;
        if (Min == FilterMode::Linear)
            return Mip == MipMode::Linear ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_NEAREST;
        return Mip == MipMode::Linear ? GL_NEAREST_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST;
    }

    inline GLenum ToGLIndexType(const IndexType Type) {
        return Type == IndexType::U16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
    }

    inline GLsizei GetIndexTypeSize(const IndexType Type) {
        return Type == IndexType::U16 ? 2 : 4;
    }

    struct GLVertexFormat {
        GLint Size;  // component count
        GLenum Type;
        GLboolean Normalized;
        bool Integer;  // needs glVertexArrayAttribIFormat
    };

    inline GLVertexFormat ToGLVertexFormat(const Format Fmt) {
        const GLFormat Gl      = ToGLFormat(Fmt);
        const FormatInfo& Info = GetFormatInfo(Fmt);

        GLVertexFormat Out {};
        Out.Size       = Info.ComponentCount;
        Out.Type       = Gl.DataType;
        Out.Normalized = Info.IsNormalized ? GL_TRUE : GL_FALSE;
        Out.Integer    = Info.IsInteger;

        // Integer base formats report *_INTEGER; the attribute path wants the
        // raw type with normalization off.
        if (Gl.BaseFormat == GL_RED_INTEGER || Gl.BaseFormat == GL_RG_INTEGER || Gl.BaseFormat == GL_RGB_INTEGER ||
            Gl.BaseFormat == GL_RGBA_INTEGER) {
            Out.Normalized = GL_FALSE;
        }

        return Out;
    }
}  // namespace Xen::RHI::GL