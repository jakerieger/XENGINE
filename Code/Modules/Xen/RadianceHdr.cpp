//
// Created by Jake Rieger on 9/19/2026.
//

#include "RadianceHdr.hpp"

#include <DirectXPackedVector.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace Xen {
    namespace {
        using DirectX::PackedVector::HALF;

        constexpr float HalfMax = 65504.0f;

        // A bounds-checked read cursor over the encoded file.
        struct Cursor {
            const u8* Data;
            size_t Size;
            size_t Pos {0};

            NODISCARD size_t Remaining() const { return Size - Pos; }
            NODISCARD bool Has(const size_t N) const { return Remaining() >= N; }
        };

        // One header line, without its newline; false at end of data.
        bool ReadLine(Cursor& C, std::string& Line) {
            Line.clear();
            if (C.Pos >= C.Size) return false;
            while (C.Pos < C.Size && C.Data[C.Pos] != '\n') Line.push_back(CAST<char>(C.Data[C.Pos++]));
            if (C.Pos < C.Size) ++C.Pos;  // the newline
            if (!Line.empty() && Line.back() == '\r') Line.pop_back();
            return true;
        }

        // One scanline of RGBE bytes (Width * 4) - new-style run-length
        // encoded (each of the four channels coded separately, marked by a
        // 2,2,hi,lo prefix) or flat, decided per scanline as the spec says.
        bool ReadScanline(Cursor& C, const u32 Width, std::vector<u8>& Rgbe) {
            Rgbe.resize(CAST<size_t>(Width) * 4);
            u8* const Dst = Rgbe.data();  // raw pointer: this is the per-texel hot path of a multi-hundred-MB decode

            const bool CanBeRle = Width >= 8 && Width < 32768;
            if (CanBeRle && C.Has(4) && C.Data[C.Pos] == 2 && C.Data[C.Pos + 1] == 2 &&
                (C.Data[C.Pos + 2] & 0x80) == 0) {
                const u32 EncodedWidth = (CAST<u32>(C.Data[C.Pos + 2]) << 8) | C.Data[C.Pos + 3];
                if (EncodedWidth != Width) return false;
                C.Pos += 4;

                for (u32 Channel = 0; Channel < 4; ++Channel) {
                    u32 X = 0;
                    while (X < Width) {
                        if (!C.Has(1)) return false;
                        u32 Count = C.Data[C.Pos++];

                        if (Count > 128) {  // a run: one value repeated
                            Count -= 128;
                            if (!C.Has(1) || Count > Width - X) return false;
                            const u8 Value = C.Data[C.Pos++];
                            for (u32 i = 0; i < Count; ++i) Dst[(CAST<size_t>(X++)) * 4 + Channel] = Value;
                        } else {  // a dump: Count literal values
                            if (Count == 0 || !C.Has(Count) || Count > Width - X) return false;
                            for (u32 i = 0; i < Count; ++i) Dst[(CAST<size_t>(X++)) * 4 + Channel] = C.Data[C.Pos++];
                        }
                    }
                }
                return true;
            }

            const size_t Bytes = Rgbe.size();
            if (!C.Has(Bytes)) return false;
            std::memcpy(Rgbe.data(), C.Data + C.Pos, Bytes);
            C.Pos += Bytes;
            return true;
        }

        void PackRow(const float* Rgba, const size_t Count, u8* OutHalves) {
            // Clamp first: a float above half range converts to infinity.
            std::vector<float> Clamped(Rgba, Rgba + Count);
            for (float& V : Clamped) V = std::min(V, HalfMax);
            DirectX::PackedVector::XMConvertFloatToHalfStream(RCAST<HALF*>(OutHalves),
                                                              sizeof(HALF),
                                                              Clamped.data(),
                                                              sizeof(float),
                                                              Count);
        }

        // The next pyramid level from Prev (RGBA16F, PrevW x PrevH): each
        // output texel averages a 2x2 block, its two rows weighted by cos of
        // their latitude (see RadianceImage::MipTail).
        std::vector<u8> DownsampleLevel(const std::vector<u8>& Prev, const u32 PrevW, const u32 PrevH) {
            const u32 NextW = std::max(PrevW / 2, 1u);
            const u32 NextH = std::max(PrevH / 2, 1u);

            std::vector<u8> Next(CAST<size_t>(NextW) * NextH * 4 * sizeof(HALF));
            std::vector<float> RowA(CAST<size_t>(PrevW) * 4), RowB(CAST<size_t>(PrevW) * 4);
            std::vector<float> Out(CAST<size_t>(NextW) * 4);

            const auto LoadRow = [&](const u32 Y, std::vector<float>& Row) {
                DirectX::PackedVector::XMConvertHalfToFloatStream(
                  Row.data(),
                  sizeof(float),
                  RCAST<const HALF*>(Prev.data() + CAST<size_t>(Y) * PrevW * 4 * sizeof(HALF)),
                  sizeof(HALF),
                  CAST<size_t>(PrevW) * 4);
            };
            const auto RowWeight = [&](const u32 Y) {
                const float Latitude = (0.5f - (CAST<float>(Y) + 0.5f) / CAST<float>(PrevH)) * 3.14159265f;
                return std::max(std::cos(Latitude), 0.0001f);
            };

            for (u32 y = 0; y < NextH; ++y) {
                const u32 y0 = std::min(y * 2, PrevH - 1);
                const u32 y1 = std::min(y * 2 + 1, PrevH - 1);
                LoadRow(y0, RowA);
                LoadRow(y1, RowB);

                const float W0    = RowWeight(y0);
                const float W1    = y1 == y0 ? 0.0f : RowWeight(y1);
                const float WSum  = W0 + W1;

                for (u32 x = 0; x < NextW; ++x) {
                    const u32 x0 = std::min(x * 2, PrevW - 1);
                    const u32 x1 = std::min(x * 2 + 1, PrevW - 1);
                    for (u32 c = 0; c < 4; ++c) {
                        const float A = 0.5f * (RowA[x0 * 4 + c] + RowA[x1 * 4 + c]);
                        const float B = 0.5f * (RowB[x0 * 4 + c] + RowB[x1 * 4 + c]);
                        Out[x * 4 + c] = (W0 * A + W1 * B) / WSum;
                    }
                }

                PackRow(Out.data(), Out.size(), Next.data() + CAST<size_t>(y) * NextW * 4 * sizeof(HALF));
            }

            return Next;
        }
    }  // namespace

    bool IsRadianceHdr(const u8* Bytes, const size_t Size) {
        constexpr char Radiance[] = "#?RADIANCE";
        constexpr char Rgbe[]     = "#?RGBE";
        return (Size >= sizeof(Radiance) - 1 && std::memcmp(Bytes, Radiance, sizeof(Radiance) - 1) == 0) ||
               (Size >= sizeof(Rgbe) - 1 && std::memcmp(Bytes, Rgbe, sizeof(Rgbe) - 1) == 0);
    }

    bool DecodeRadianceHdr(const u8* Bytes, const size_t Size, const u32 MaxWidth, RadianceImage& Out, std::string& Error) {
        Cursor C {Bytes, Size};

        std::string Line;
        if (!ReadLine(C, Line) || (Line != "#?RADIANCE" && Line != "#?RGBE")) {
            Error = "not a Radiance HDR file";
            return false;
        }

        bool FormatOk = false;
        while (ReadLine(C, Line) && !Line.empty()) {
            if (Line == "FORMAT=32-bit_rle_rgbe") FormatOk = true;
        }
        if (!FormatOk) {
            Error = "unsupported Radiance format (only 32-bit_rle_rgbe)";
            return false;
        }

        int Height = 0, Width = 0;
        if (!ReadLine(C, Line) || std::sscanf(Line.c_str(), "-Y %d +X %d", &Height, &Width) != 2 || Width <= 0 ||
            Height <= 0) {
            Error = "unsupported Radiance orientation (only \"-Y H +X W\")";
            return false;
        }

        Out.SourceWidth  = CAST<u32>(Width);
        Out.SourceHeight = CAST<u32>(Height);

        // Smallest power-of-two reduction that fits MaxWidth. Output texels
        // are K x K box averages of the source, so the file is never held in
        // memory at its full size.
        u32 Shift = 0;
        while ((Out.SourceWidth >> Shift) > MaxWidth && (Out.SourceWidth >> Shift) > 1) ++Shift;
        const u32 K = 1u << Shift;

        Out.Width  = std::max(Out.SourceWidth >> Shift, 1u);
        Out.Height = std::max(Out.SourceHeight >> Shift, 1u);

        Out.Mip0.assign(CAST<size_t>(Out.Width) * Out.Height * 4 * sizeof(HALF), 0);

        // 2^(E - 136) for every shared exponent byte, so the per-texel loop is
        // three multiplies rather than a ldexp call (E == 0 is black).
        float ExponentScale[256];
        ExponentScale[0] = 0.0f;
        for (int E = 1; E < 256; ++E) ExponentScale[E] = std::ldexp(1.0f, E - (128 + 8));

        std::vector<u8> Rgbe;
        std::vector<float> Accum(CAST<size_t>(Out.Width) * 4);

        for (u32 OutY = 0; OutY < Out.Height; ++OutY) {
            std::fill(Accum.begin(), Accum.end(), 0.0f);

            for (u32 Row = 0; Row < K; ++Row) {
                if (!ReadScanline(C, Out.SourceWidth, Rgbe)) {
                    Error = "truncated or corrupt Radiance scanline";
                    return false;
                }

                // RGBE -> linear RGB: mantissas share one exponent. Only the
                // columns that fall in a whole output texel are used (a
                // source width that isn't a multiple of K drops its remainder).
                const u8* P = Rgbe.data();
                for (u32 OutX = 0; OutX < Out.Width; ++OutX) {
                    float* Dst = Accum.data() + CAST<size_t>(OutX) * 4;
                    for (u32 i = 0; i < K; ++i, P += 4) {
                        const float Scale = ExponentScale[P[3]];
                        Dst[0] += P[0] * Scale;
                        Dst[1] += P[1] * Scale;
                        Dst[2] += P[2] * Scale;
                    }
                }
            }

            const float Norm = 1.0f / CAST<float>(K * K);
            for (u32 X = 0; X < Out.Width; ++X) {
                Accum[CAST<size_t>(X) * 4 + 0] *= Norm;
                Accum[CAST<size_t>(X) * 4 + 1] *= Norm;
                Accum[CAST<size_t>(X) * 4 + 2] *= Norm;
                Accum[CAST<size_t>(X) * 4 + 3] = 1.0f;
            }

            PackRow(Accum.data(), Accum.size(), Out.Mip0.data() + CAST<size_t>(OutY) * Out.Width * 4 * sizeof(HALF));
        }

        u32 W = Out.Width, H = Out.Height;
        const std::vector<u8>* Prev = &Out.Mip0;
        while (W > 1 || H > 1) {
            Out.MipTail.push_back(DownsampleLevel(*Prev, W, H));
            Prev = &Out.MipTail.back();
            W    = std::max(W / 2, 1u);
            H    = std::max(H / 2, 1u);
        }

        return true;
    }
}  // namespace Xen
