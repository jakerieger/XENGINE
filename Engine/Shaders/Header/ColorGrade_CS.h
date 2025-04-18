#if 0
//
// Generated by Microsoft (R) HLSL Shader Compiler 10.1
//
//
// Buffer Definitions: 
//
// cbuffer ColorGradeParams
// {
//
//   float saturation;                  // Offset:    0 Size:     4
//   float contrast;                    // Offset:    4 Size:     4
//   float temperature;                 // Offset:    8 Size:     4
//   float exposureAdjustment;          // Offset:   12 Size:     4 [unused]
//
// }
//
//
// Resource Bindings:
//
// Name                                 Type  Format         Dim      HLSL Bind  Count
// ------------------------------ ---------- ------- ----------- -------------- ------
// InputTexture                      texture  float4          2d             t0      1 
// OutputTexture                         UAV  float4          2d             u0      1 
// ColorGradeParams                  cbuffer      NA          NA            cb0      1 
//
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// no Input
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// no Output
cs_5_0
dcl_globalFlags refactoringAllowed
dcl_constantbuffer CB0[1], immediateIndexed
dcl_resource_texture2d (float,float,float,float) t0
dcl_uav_typed_texture2d (float,float,float,float) u0
dcl_input vThreadID.xy
dcl_temps 5
dcl_thread_group 8, 8, 1
resinfo_indexable(texture2d)(float,float,float,float)_uint r0.xy, l(0), u0.xyzw
uge r0.xy, vThreadID.xyxx, r0.xyxx
or r0.x, r0.y, r0.x
if_nz r0.x
  ret 
endif 
mov r0.xy, vThreadID.xyxx
mov r0.zw, l(0,0,0,0)
ld_indexable(texture2d)(float,float,float,float) r0.xyzw, r0.xyzw, t0.xyzw
max r1.xy, cb0[0].zyzz, l(1000.000000, 0.000000, 0.000000, 0.000000)
min r1.x, r1.x, l(40000.000000)
mul r1.z, r1.x, l(0.010000)
ge r2.xy, l(6600.000000, 1900.000000, 0.000000, 0.000000), r1.xxxx
mad r3.xyz, r1.xxxx, l(0.010000, 0.010000, 0.010000, 0.000000), l(-60.000000, -60.000000, -10.000000, 0.000000)
log r3.xyz, r3.xyzx
mul r1.xw, r3.xxxy, l(-0.133205, 0.000000, 0.000000, -0.075515)
exp r1.xw, r1.xxxw
mul r1.xw, r1.xxxw, l(1.292936, 0.000000, 0.000000, 1.129891)
movc r4.x, r2.x, l(1.000000), r1.x
log r1.x, r1.z
mad r1.x, r1.x, l(0.270384), l(-0.631841)
movc r4.y, r2.x, r1.x, r1.w
mad r1.x, r3.z, l(0.376522), l(-1.196254)
movc r1.x, r2.x, r1.x, l(1.000000)
movc r4.z, r2.y, l(0), r1.x
mul r1.xzw, r4.xxyz, l(1.000000, 0.000000, 1.032311, 1.083658)
max r1.xzw, r1.xxzw, l(0.000100, 0.000000, 0.000100, 0.000100)
mad r1.xzw, r0.xxyz, r1.xxzw, l(-0.500000, 0.000000, -0.500000, -0.500000)
mad r1.xyz, r1.xzwx, r1.yyyy, l(0.500000, 0.500000, 0.500000, 0.000000)
dp3 r1.w, r1.xyzx, l(0.212600, 0.715200, 0.072200, 0.000000)
add r1.xyz, -r1.wwww, r1.xyzx
mad r0.xyz, cb0[0].xxxx, r1.xyzx, r1.wwww
store_uav_typed u0.xyzw, vThreadID.xyyy, r0.xyzw
ret 
// Approximately 34 instruction slots used
#endif

const BYTE kColorGrade_CSBytes[] =
{
     68,  88,  66,  67, 136, 104, 
    224, 170, 193, 200,  10, 106, 
    117, 168, 157, 241,  67, 174, 
     19,  50,   1,   0,   0,   0, 
    140,   7,   0,   0,   5,   0, 
      0,   0,  52,   0,   0,   0, 
     68,   2,   0,   0,  84,   2, 
      0,   0, 100,   2,   0,   0, 
    240,   6,   0,   0,  82,  68, 
     69,  70,   8,   2,   0,   0, 
      1,   0,   0,   0, 200,   0, 
      0,   0,   3,   0,   0,   0, 
     60,   0,   0,   0,   0,   5, 
     83,  67,   0,   1,   0,   0, 
    224,   1,   0,   0,  82,  68, 
     49,  49,  60,   0,   0,   0, 
     24,   0,   0,   0,  32,   0, 
      0,   0,  40,   0,   0,   0, 
     36,   0,   0,   0,  12,   0, 
      0,   0,   0,   0,   0,   0, 
    156,   0,   0,   0,   2,   0, 
      0,   0,   5,   0,   0,   0, 
      4,   0,   0,   0, 255, 255, 
    255, 255,   0,   0,   0,   0, 
      1,   0,   0,   0,  13,   0, 
      0,   0, 169,   0,   0,   0, 
      4,   0,   0,   0,   5,   0, 
      0,   0,   4,   0,   0,   0, 
    255, 255, 255, 255,   0,   0, 
      0,   0,   1,   0,   0,   0, 
     13,   0,   0,   0, 183,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   1,   0, 
      0,   0,   1,   0,   0,   0, 
     73, 110, 112, 117, 116,  84, 
    101, 120, 116, 117, 114, 101, 
      0,  79, 117, 116, 112, 117, 
    116,  84, 101, 120, 116, 117, 
    114, 101,   0,  67, 111, 108, 
    111, 114,  71, 114,  97, 100, 
    101,  80,  97, 114,  97, 109, 
    115,   0, 183,   0,   0,   0, 
      4,   0,   0,   0, 224,   0, 
      0,   0,  16,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0, 128,   1,   0,   0, 
      0,   0,   0,   0,   4,   0, 
      0,   0,   2,   0,   0,   0, 
    148,   1,   0,   0,   0,   0, 
      0,   0, 255, 255, 255, 255, 
      0,   0,   0,   0, 255, 255, 
    255, 255,   0,   0,   0,   0, 
    184,   1,   0,   0,   4,   0, 
      0,   0,   4,   0,   0,   0, 
      2,   0,   0,   0, 148,   1, 
      0,   0,   0,   0,   0,   0, 
    255, 255, 255, 255,   0,   0, 
      0,   0, 255, 255, 255, 255, 
      0,   0,   0,   0, 193,   1, 
      0,   0,   8,   0,   0,   0, 
      4,   0,   0,   0,   2,   0, 
      0,   0, 148,   1,   0,   0, 
      0,   0,   0,   0, 255, 255, 
    255, 255,   0,   0,   0,   0, 
    255, 255, 255, 255,   0,   0, 
      0,   0, 205,   1,   0,   0, 
     12,   0,   0,   0,   4,   0, 
      0,   0,   0,   0,   0,   0, 
    148,   1,   0,   0,   0,   0, 
      0,   0, 255, 255, 255, 255, 
      0,   0,   0,   0, 255, 255, 
    255, 255,   0,   0,   0,   0, 
    115,  97, 116, 117, 114,  97, 
    116, 105, 111, 110,   0, 102, 
    108, 111,  97, 116,   0, 171, 
    171, 171,   0,   0,   3,   0, 
      1,   0,   1,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0, 139,   1, 
      0,   0,  99, 111, 110, 116, 
    114,  97, 115, 116,   0, 116, 
    101, 109, 112, 101, 114,  97, 
    116, 117, 114, 101,   0, 101, 
    120, 112, 111, 115, 117, 114, 
    101,  65, 100, 106, 117, 115, 
    116, 109, 101, 110, 116,   0, 
     77, 105,  99, 114, 111, 115, 
    111, 102, 116,  32,  40,  82, 
     41,  32,  72,  76,  83,  76, 
     32,  83, 104,  97, 100, 101, 
    114,  32,  67, 111, 109, 112, 
    105, 108, 101, 114,  32,  49, 
     48,  46,  49,   0,  73,  83, 
     71,  78,   8,   0,   0,   0, 
      0,   0,   0,   0,   8,   0, 
      0,   0,  79,  83,  71,  78, 
      8,   0,   0,   0,   0,   0, 
      0,   0,   8,   0,   0,   0, 
     83,  72,  69,  88, 132,   4, 
      0,   0,  80,   0,   5,   0, 
     33,   1,   0,   0, 106,   8, 
      0,   1,  89,   0,   0,   4, 
     70, 142,  32,   0,   0,   0, 
      0,   0,   1,   0,   0,   0, 
     88,  24,   0,   4,   0, 112, 
     16,   0,   0,   0,   0,   0, 
     85,  85,   0,   0, 156,  24, 
      0,   4,   0, 224,  17,   0, 
      0,   0,   0,   0,  85,  85, 
      0,   0,  95,   0,   0,   2, 
     50,   0,   2,   0, 104,   0, 
      0,   2,   5,   0,   0,   0, 
    155,   0,   0,   4,   8,   0, 
      0,   0,   8,   0,   0,   0, 
      1,   0,   0,   0,  61,  16, 
      0, 137, 194,   0,   0, 128, 
     67,  85,  21,   0,  50,   0, 
     16,   0,   0,   0,   0,   0, 
      1,  64,   0,   0,   0,   0, 
      0,   0,  70, 238,  17,   0, 
      0,   0,   0,   0,  80,   0, 
      0,   6,  50,   0,  16,   0, 
      0,   0,   0,   0,  70,   0, 
      2,   0,  70,   0,  16,   0, 
      0,   0,   0,   0,  60,   0, 
      0,   7,  18,   0,  16,   0, 
      0,   0,   0,   0,  26,   0, 
     16,   0,   0,   0,   0,   0, 
     10,   0,  16,   0,   0,   0, 
      0,   0,  31,   0,   4,   3, 
     10,   0,  16,   0,   0,   0, 
      0,   0,  62,   0,   0,   1, 
     21,   0,   0,   1,  54,   0, 
      0,   4,  50,   0,  16,   0, 
      0,   0,   0,   0,  70,   0, 
      2,   0,  54,   0,   0,   8, 
    194,   0,  16,   0,   0,   0, 
      0,   0,   2,  64,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,  45,   0, 
      0, 137, 194,   0,   0, 128, 
     67,  85,  21,   0, 242,   0, 
     16,   0,   0,   0,   0,   0, 
     70,  14,  16,   0,   0,   0, 
      0,   0,  70, 126,  16,   0, 
      0,   0,   0,   0,  52,   0, 
      0,  11,  50,   0,  16,   0, 
      1,   0,   0,   0, 102, 138, 
     32,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   2,  64, 
      0,   0,   0,   0, 122,  68, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
     51,   0,   0,   7,  18,   0, 
     16,   0,   1,   0,   0,   0, 
     10,   0,  16,   0,   1,   0, 
      0,   0,   1,  64,   0,   0, 
      0,  64,  28,  71,  56,   0, 
      0,   7,  66,   0,  16,   0, 
      1,   0,   0,   0,  10,   0, 
     16,   0,   1,   0,   0,   0, 
      1,  64,   0,   0,  10, 215, 
     35,  60,  29,   0,   0,  10, 
     50,   0,  16,   0,   2,   0, 
      0,   0,   2,  64,   0,   0, 
      0,  64, 206,  69,   0, 128, 
    237,  68,   0,   0,   0,   0, 
      0,   0,   0,   0,   6,   0, 
     16,   0,   1,   0,   0,   0, 
     50,   0,   0,  15, 114,   0, 
     16,   0,   3,   0,   0,   0, 
      6,   0,  16,   0,   1,   0, 
      0,   0,   2,  64,   0,   0, 
     10, 215,  35,  60,  10, 215, 
     35,  60,  10, 215,  35,  60, 
      0,   0,   0,   0,   2,  64, 
      0,   0,   0,   0, 112, 194, 
      0,   0, 112, 194,   0,   0, 
     32, 193,   0,   0,   0,   0, 
     47,   0,   0,   5, 114,   0, 
     16,   0,   3,   0,   0,   0, 
     70,   2,  16,   0,   3,   0, 
      0,   0,  56,   0,   0,  10, 
    146,   0,  16,   0,   1,   0, 
      0,   0,   6,   4,  16,   0, 
      3,   0,   0,   0,   2,  64, 
      0,   0, 212, 102,   8, 190, 
      0,   0,   0,   0,   0,   0, 
      0,   0, 135, 167, 154, 189, 
     25,   0,   0,   5, 146,   0, 
     16,   0,   1,   0,   0,   0, 
      6,  12,  16,   0,   1,   0, 
      0,   0,  56,   0,   0,  10, 
    146,   0,  16,   0,   1,   0, 
      0,   0,   6,  12,  16,   0, 
      1,   0,   0,   0,   2,  64, 
      0,   0, 239, 126, 165,  63, 
      0,   0,   0,   0,   0,   0, 
      0,   0,  68, 160, 144,  63, 
     55,   0,   0,   9,  18,   0, 
     16,   0,   4,   0,   0,   0, 
     10,   0,  16,   0,   2,   0, 
      0,   0,   1,  64,   0,   0, 
      0,   0, 128,  63,  10,   0, 
     16,   0,   1,   0,   0,   0, 
     47,   0,   0,   5,  18,   0, 
     16,   0,   1,   0,   0,   0, 
     42,   0,  16,   0,   1,   0, 
      0,   0,  50,   0,   0,   9, 
     18,   0,  16,   0,   1,   0, 
      0,   0,  10,   0,  16,   0, 
      1,   0,   0,   0,   1,  64, 
      0,   0, 196, 111, 138,  62, 
      1,  64,   0,   0,  92, 192, 
     33, 191,  55,   0,   0,   9, 
     34,   0,  16,   0,   4,   0, 
      0,   0,  10,   0,  16,   0, 
      2,   0,   0,   0,  10,   0, 
     16,   0,   1,   0,   0,   0, 
     58,   0,  16,   0,   1,   0, 
      0,   0,  50,   0,   0,   9, 
     18,   0,  16,   0,   1,   0, 
      0,   0,  42,   0,  16,   0, 
      3,   0,   0,   0,   1,  64, 
      0,   0, 135, 199, 192,  62, 
      1,  64,   0,   0, 219,  30, 
    153, 191,  55,   0,   0,   9, 
     18,   0,  16,   0,   1,   0, 
      0,   0,  10,   0,  16,   0, 
      2,   0,   0,   0,  10,   0, 
     16,   0,   1,   0,   0,   0, 
      1,  64,   0,   0,   0,   0, 
    128,  63,  55,   0,   0,   9, 
     66,   0,  16,   0,   4,   0, 
      0,   0,  26,   0,  16,   0, 
      2,   0,   0,   0,   1,  64, 
      0,   0,   0,   0,   0,   0, 
     10,   0,  16,   0,   1,   0, 
      0,   0,  56,   0,   0,  10, 
    210,   0,  16,   0,   1,   0, 
      0,   0,   6,   9,  16,   0, 
      4,   0,   0,   0,   2,  64, 
      0,   0,   0,   0, 128,  63, 
      0,   0,   0,   0, 199,  34, 
    132,  63,  82, 181, 138,  63, 
     52,   0,   0,  10, 210,   0, 
     16,   0,   1,   0,   0,   0, 
      6,  14,  16,   0,   1,   0, 
      0,   0,   2,  64,   0,   0, 
     23, 183, 209,  56,   0,   0, 
      0,   0,  23, 183, 209,  56, 
     23, 183, 209,  56,  50,   0, 
      0,  12, 210,   0,  16,   0, 
      1,   0,   0,   0,   6,   9, 
     16,   0,   0,   0,   0,   0, 
      6,  14,  16,   0,   1,   0, 
      0,   0,   2,  64,   0,   0, 
      0,   0,   0, 191,   0,   0, 
      0,   0,   0,   0,   0, 191, 
      0,   0,   0, 191,  50,   0, 
      0,  12, 114,   0,  16,   0, 
      1,   0,   0,   0, 134,   3, 
     16,   0,   1,   0,   0,   0, 
     86,   5,  16,   0,   1,   0, 
      0,   0,   2,  64,   0,   0, 
      0,   0,   0,  63,   0,   0, 
      0,  63,   0,   0,   0,  63, 
      0,   0,   0,   0,  16,   0, 
      0,  10, 130,   0,  16,   0, 
      1,   0,   0,   0,  70,   2, 
     16,   0,   1,   0,   0,   0, 
      2,  64,   0,   0, 208, 179, 
     89,  62,  89,  23,  55,  63, 
    152, 221, 147,  61,   0,   0, 
      0,   0,   0,   0,   0,   8, 
    114,   0,  16,   0,   1,   0, 
      0,   0, 246,  15,  16, 128, 
     65,   0,   0,   0,   1,   0, 
      0,   0,  70,   2,  16,   0, 
      1,   0,   0,   0,  50,   0, 
      0,  10, 114,   0,  16,   0, 
      0,   0,   0,   0,   6, 128, 
     32,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,  70,   2, 
     16,   0,   1,   0,   0,   0, 
    246,  15,  16,   0,   1,   0, 
      0,   0, 164,   0,   0,   6, 
    242, 224,  17,   0,   0,   0, 
      0,   0,  70,   5,   2,   0, 
     70,  14,  16,   0,   0,   0, 
      0,   0,  62,   0,   0,   1, 
     83,  84,  65,  84, 148,   0, 
      0,   0,  34,   0,   0,   0, 
      5,   0,   0,   0,   0,   0, 
      0,   0,   1,   0,   0,   0, 
     19,   0,   0,   0,   0,   0, 
      0,   0,   2,   0,   0,   0, 
      2,   0,   0,   0,   1,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   1,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      2,   0,   0,   0,   4,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   1,   0,   0,   0
};
