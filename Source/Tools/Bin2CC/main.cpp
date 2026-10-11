//
// Created by Jake Rieger on 9/28/2026.
//
// Bin2CC - embeds one or more files as C byte arrays in a single header.
//
// Each input becomes a <SYMBOL>_SIZE / <SYMBOL>_DATA pair. The symbol name is derived from the input file name:
// directory is stripped, letters are uppercased, and anything that isn't a letter or digit becomes '_'.
//   "fonts/My Font.ttf"  ->  MY_FONT_TTF_SIZE / MY_FONT_TTF_DATA
//
// By default the data is compressed with stb_compress() (from dear imgui's binary_to_compressed_c.cpp), for fonts
// loaded with ImGui::GetIO().Fonts->AddFontFromMemoryCompressedTTF(). Pass --raw to embed the bytes as-is instead -
// for formats that are already compressed, like PNG (decode with stbi_load_from_memory).
//
// Usage:
//   Bin2CC <inputfile> [-o outputfile]           -> output defaults to <lowercase symbol>.h
//   Bin2CC --raw -o Icons.h a.png b.png ...      -> -o is required with more than one input

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>

// stb_compress* from stb.h - declaration
typedef unsigned int stb_uint;
typedef unsigned char stb_uchar;
stb_uint stb_compress(stb_uchar* out, stb_uchar* in, stb_uint len);

static const int BYTES_PER_LINE = 12;

static std::string make_symbol_name(const std::string& path);
static bool read_file(const std::filesystem::path& path, std::vector<unsigned char>& data);
static std::vector<unsigned char> compress(const std::vector<unsigned char>& data);
static void write_array(FILE* out, const std::string& symbol, const std::vector<unsigned char>& bytes);

int main(int argc, char** argv) {
    CLI::App app {"Bin2CC - Embeds files as C byte arrays in a header, optionally stb_compress'd."};

    std::vector<std::filesystem::path> inputs;
    std::filesystem::path output;
    bool raw = false;

    app.add_option("inputs", inputs, "Input file(s)")->required()->check(CLI::ExistingFile)->expected(1, -1);
    app.add_option("-o,--output", output, "Output header (defaults to <lowercase symbol>.h for a single input)");
    app.add_flag("--raw", raw, "Embed bytes as-is instead of stb_compress'ing them (for PNG etc.)");

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) { return app.exit(e); }

    if (output.empty()) {
        if (inputs.size() > 1) {
            fprintf(stderr, "Error: -o/--output is required when embedding more than one file\n");
            return 1;
        }
        std::string name = make_symbol_name(inputs.front().string());
        for (char& c : name)
            c = (char)tolower((unsigned char)c);
        output = name + ".h";
    }

    FILE* out = fopen(output.string().c_str(), "w");
    if (!out) {
        fprintf(stderr, "Error opening output file: '%s'\n", output.string().c_str());
        return 1;
    }

    fprintf(out, "#pragma once\n\n");
    fprintf(out, "extern \"C\" {\n");

    bool ok = true;
    for (size_t i = 0; i < inputs.size() && ok; i++) {
        std::vector<unsigned char> data;
        if (!read_file(inputs[i], data)) {
            fprintf(stderr, "Error reading input file: '%s'\n", inputs[i].string().c_str());
            ok = false;
            break;
        }

        const std::string symbol = make_symbol_name(inputs[i].string());
        if (i > 0) fputc('\n', out);
        write_array(out, symbol, raw ? data : compress(data));
        printf("Embedded '%s' (%s_SIZE / %s_DATA)\n", inputs[i].string().c_str(), symbol.c_str(), symbol.c_str());
    }

    fprintf(out, "}\n");

    if (ferror(out)) ok = false;
    if (fclose(out) != 0) ok = false;
    if (!ok) {
        fprintf(stderr, "Error writing output file: '%s'\n", output.string().c_str());
        return 1;
    }

    printf("Wrote '%s'\n", output.string().c_str());
    return 0;
}

static std::string make_symbol_name(const std::string& path) {
    // Strip directory
    const char* base = path.c_str();
    for (const char* p = base; *p; p++)
        if (*p == '/' || *p == '\\') base = p + 1;

    std::string s;
    for (const char* p = base; *p; p++) {
        unsigned char c = (unsigned char)*p;
        s += isalnum(c) ? (char)toupper(c) : '_';
    }

    // C identifiers can't be empty or start with a digit
    if (s.empty() || isdigit((unsigned char)s[0])) s.insert(0, "_");
    return s;
}

static bool read_file(const std::filesystem::path& path, std::vector<unsigned char>& data) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    data.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return !f.bad();
}

static std::vector<unsigned char> compress(const std::vector<unsigned char>& data) {
    // 4 bytes of zero padding past the end, same as imgui's binary_to_compressed_c
    std::vector<unsigned char> padded(data.size() + 4, 0);
    memcpy(padded.data(), data.data(), data.size());

    std::vector<unsigned char> compressed(data.size() + 512 + (data.size() >> 2) + sizeof(int));  // total guess
    const stb_uint size = stb_compress(compressed.data(), padded.data(), (stb_uint)data.size());
    compressed.resize(size);
    return compressed;
}

static void write_array(FILE* out, const std::string& symbol, const std::vector<unsigned char>& bytes) {
    const char* sym = symbol.c_str();
    const int size  = (int)bytes.size();
    fprintf(out, "static const unsigned int %s_SIZE = %d;\n", sym, size);
    fprintf(out, "static const unsigned char %s_DATA[%d] = {", sym, size);
    for (int i = 0; i < size; i++) {
        if ((i % BYTES_PER_LINE) == 0) fprintf(out, "\n    ");
        else fputc(' ', out);
        fprintf(out, "0x%02x,", bytes[i]);
    }
    fprintf(out, "\n};\n");
}

// stb_compress* from stb.h - definition

////////////////////           compressor         ///////////////////////

static stb_uint stb_adler32(stb_uint adler32, stb_uchar* buffer, stb_uint buflen) {
    const unsigned long ADLER_MOD = 65521;
    unsigned long s1 = adler32 & 0xffff, s2 = adler32 >> 16;
    unsigned long blocklen, i;

    blocklen = buflen % 5552;
    while (buflen) {
        for (i = 0; i + 7 < blocklen; i += 8) {
            s1 += buffer[0], s2 += s1;
            s1 += buffer[1], s2 += s1;
            s1 += buffer[2], s2 += s1;
            s1 += buffer[3], s2 += s1;
            s1 += buffer[4], s2 += s1;
            s1 += buffer[5], s2 += s1;
            s1 += buffer[6], s2 += s1;
            s1 += buffer[7], s2 += s1;

            buffer += 8;
        }

        for (; i < blocklen; ++i)
            s1 += *buffer++, s2 += s1;

        s1 %= ADLER_MOD, s2 %= ADLER_MOD;
        buflen -= blocklen;
        blocklen = 5552;
    }
    return (s2 << 16) + s1;
}

static unsigned int stb_matchlen(stb_uchar* m1, stb_uchar* m2, stb_uint maxlen) {
    stb_uint i;
    for (i = 0; i < maxlen; ++i)
        if (m1[i] != m2[i]) return i;
    return i;
}

// simple implementation that just takes the source data in a big block

static stb_uchar* stb__out;
static FILE* stb__outfile;
static stb_uint stb__outbytes;

static void stb__write(unsigned char v) {
    fputc(v, stb__outfile);
    ++stb__outbytes;
}

// #define stb_out(v)    (stb__out ? *stb__out++ = (stb_uchar) (v) : stb__write((stb_uchar) (v)))
#define stb_out(v)                                                                                                     \
    do {                                                                                                               \
        if (stb__out) *stb__out++ = (stb_uchar)(v);                                                                    \
        else stb__write((stb_uchar)(v));                                                                               \
    } while (0)

static void stb_out2(stb_uint v) {
    stb_out(v >> 8);
    stb_out(v);
}
static void stb_out3(stb_uint v) {
    stb_out(v >> 16);
    stb_out(v >> 8);
    stb_out(v);
}
static void stb_out4(stb_uint v) {
    stb_out(v >> 24);
    stb_out(v >> 16);
    stb_out(v >> 8);
    stb_out(v);
}

static void outliterals(stb_uchar* in, int numlit) {
    while (numlit > 65536) {
        outliterals(in, 65536);
        in += 65536;
        numlit -= 65536;
    }

    if (numlit == 0)
        ;
    else if (numlit <= 32) stb_out(0x000020 + numlit - 1);
    else if (numlit <= 2048) stb_out2(0x000800 + numlit - 1);
    else /*  numlit <= 65536) */ stb_out3(0x070000 + numlit - 1);

    if (stb__out) {
        memcpy(stb__out, in, numlit);
        stb__out += numlit;
    } else fwrite(in, 1, numlit, stb__outfile);
}

static int stb__window = 0x40000;  // 256K

static int stb_not_crap(int best, int dist) {
    return ((best > 2 && dist <= 0x00100) || (best > 5 && dist <= 0x04000) || (best > 7 && dist <= 0x80000));
}

static stb_uint stb__hashsize = 32768;

// note that you can play with the hashing functions all you
// want without needing to change the decompressor
#define stb__hc(q, h, c) (((h) << 7) + ((h) >> 25) + q[c])
#define stb__hc2(q, h, c, d) (((h) << 14) + ((h) >> 18) + (q[c] << 7) + q[d])
#define stb__hc3(q, c, d, e) ((q[c] << 14) + (q[d] << 7) + q[e])

static unsigned int stb__running_adler;

static int stb_compress_chunk(stb_uchar* history,
                              stb_uchar* start,
                              stb_uchar* end,
                              int length,
                              int* pending_literals,
                              stb_uchar** chash,
                              stb_uint mask) {
    (void)history;
    int window = stb__window;
    stb_uint match_max;
    stb_uchar* lit_start = start - *pending_literals;
    stb_uchar* q         = start;

#define STB__SCRAMBLE(h) (((h) + ((h) >> 16)) & mask)

    // stop short of the end so we don't scan off the end doing
    // the hashing; this means we won't compress the last few bytes
    // unless they were part of something longer
    while (q < start + length && q + 12 < end) {
        int m;
        stb_uint h1, h2, h3, h4, h;
        stb_uchar* t;
        int best = 2, dist = 0;

        if (q + 65536 > end) match_max = (stb_uint)(end - q);
        else match_max = 65536;

#define stb__nc(b, d) ((d) <= window && ((b) > 9 || stb_not_crap((int)(b), (int)(d))))

#define STB__TRY(t, p) /* avoid retrying a match we already tried */                                                   \
    if (p ? dist != (int)(q - t) : 1)                                                                                  \
        if ((m = stb_matchlen(t, q, match_max)) > best)                                                                \
            if (stb__nc(m, q - (t))) best = m, dist = (int)(q - (t))

        // rather than search for all matches, only try 4 candidate locations,
        // chosen based on 4 different hash functions of different lengths.
        // this strategy is inspired by LZO; hashing is unrolled here using the
        // 'hc' macro
        h  = stb__hc3(q, 0, 1, 2);
        h1 = STB__SCRAMBLE(h);
        t  = chash[h1];
        if (t) STB__TRY(t, 0);
        h  = stb__hc2(q, h, 3, 4);
        h2 = STB__SCRAMBLE(h);
        h  = stb__hc2(q, h, 5, 6);
        t  = chash[h2];
        if (t) STB__TRY(t, 1);
        h  = stb__hc2(q, h, 7, 8);
        h3 = STB__SCRAMBLE(h);
        h  = stb__hc2(q, h, 9, 10);
        t  = chash[h3];
        if (t) STB__TRY(t, 1);
        h  = stb__hc2(q, h, 11, 12);
        h4 = STB__SCRAMBLE(h);
        t  = chash[h4];
        if (t) STB__TRY(t, 1);

        // because we use a shared hash table, can only update it
        // _after_ we've probed all of them
        chash[h1] = chash[h2] = chash[h3] = chash[h4] = q;

        if (best > 2) assert(dist > 0);

        // see if our best match qualifies
        if (best < 3) {  // fast path literals
            ++q;
        } else if (best > 2 && best <= 0x80 && dist <= 0x100) {
            outliterals(lit_start, (int)(q - lit_start));
            lit_start = (q += best);
            stb_out(0x80 + best - 1);
            stb_out(dist - 1);
        } else if (best > 5 && best <= 0x100 && dist <= 0x4000) {
            outliterals(lit_start, (int)(q - lit_start));
            lit_start = (q += best);
            stb_out2(0x4000 + dist - 1);
            stb_out(best - 1);
        } else if (best > 7 && best <= 0x100 && dist <= 0x80000) {
            outliterals(lit_start, (int)(q - lit_start));
            lit_start = (q += best);
            stb_out3(0x180000 + dist - 1);
            stb_out(best - 1);
        } else if (best > 8 && best <= 0x10000 && dist <= 0x80000) {
            outliterals(lit_start, (int)(q - lit_start));
            lit_start = (q += best);
            stb_out3(0x100000 + dist - 1);
            stb_out2(best - 1);
        } else if (best > 9 && dist <= 0x1000000) {
            if (best > 65536) best = 65536;
            outliterals(lit_start, (int)(q - lit_start));
            lit_start = (q += best);
            if (best <= 0x100) {
                stb_out(0x06);
                stb_out3(dist - 1);
                stb_out(best - 1);
            } else {
                stb_out(0x04);
                stb_out3(dist - 1);
                stb_out2(best - 1);
            }
        } else {  // fallback literals if no match was a balanced tradeoff
            ++q;
        }
    }

    // if we didn't get all the way, add the rest to literals
    if (q - start < length) q = start + length;

    // the literals are everything from lit_start to q
    *pending_literals = (int)(q - lit_start);

    stb__running_adler = stb_adler32(stb__running_adler, start, (stb_uint)(q - start));
    return (int)(q - start);
}

static int stb_compress_inner(stb_uchar* input, stb_uint length) {
    int literals = 0;
    stb_uint len, i;

    stb_uchar** chash;
    chash = (stb_uchar**)malloc(stb__hashsize * sizeof(stb_uchar*));
    if (chash == nullptr) return 0;  // failure
    for (i = 0; i < stb__hashsize; ++i)
        chash[i] = nullptr;

    // stream signature
    stb_out(0x57);
    stb_out(0xbc);
    stb_out2(0);

    stb_out4(0);  // 64-bit length requires 32-bit leading 0
    stb_out4(length);
    stb_out4(stb__window);

    stb__running_adler = 1;

    len = stb_compress_chunk(input, input, input + length, length, &literals, chash, stb__hashsize - 1);
    assert(len == length);

    outliterals(input + length - literals, literals);

    free(chash);

    stb_out2(0x05fa);  // end opcode

    stb_out4(stb__running_adler);

    return 1;  // success
}

stb_uint stb_compress(stb_uchar* out, stb_uchar* input, stb_uint length) {
    stb__out     = out;
    stb__outfile = nullptr;

    stb_compress_inner(input, length);

    return (stb_uint)(stb__out - out);
}