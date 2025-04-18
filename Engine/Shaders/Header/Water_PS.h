#if 0
//
// Generated by Microsoft (R) HLSL Shader Compiler 10.1
//
//
// Buffer Definitions: 
//
// cbuffer LightBuffer
// {
//
//   struct DirectionalLight
//   {
//       
//       float4 direction;              // Offset:    0
//       float4 color;                  // Offset:   16
//       float intensity;               // Offset:   32
//       float3 _pad1;                  // Offset:   36
//       bool castsShadow;              // Offset:   48
//       bool enabled;                  // Offset:   52
//       float2 _pad2;                  // Offset:   56
//       float4x4 lightViewProj;        // Offset:   64
//
//   } Sun;                             // Offset:    0 Size:   128
//   
//   struct PointLight
//   {
//       
//       float3 position;               // Offset:  128
//       float padding1;                // Offset:  140
//       float3 color;                  // Offset:  144
//       float intensity;               // Offset:  156
//       float constant;                // Offset:  160
//       float lin;                     // Offset:  164
//       float quadratic;               // Offset:  168
//       float radius;                  // Offset:  172
//       bool castsShadow;              // Offset:  176
//       bool enabled;                  // Offset:  180
//       float2 padding2;               // Offset:  184
//
//   } PointLights[16];                 // Offset:  128 Size:  1024 [unused]
//   
//   struct SpotLight
//   {
//       
//       float3 position;               // Offset: 1152
//       float _padding1;               // Offset: 1164
//       float3 direction;              // Offset: 1168
//       float _padding2;               // Offset: 1180
//       float3 color;                  // Offset: 1184
//       float intensity;               // Offset: 1196
//       float innerAngle;              // Offset: 1200
//       float outerAngle;              // Offset: 1204
//       float range;                   // Offset: 1208
//       bool castsShadow;              // Offset: 1212
//       bool enabled;                  // Offset: 1216
//       float3 _pad;                   // Offset: 1220
//
//   } SpotLights[16];                  // Offset: 1152 Size:  1280 [unused]
//   
//   struct AreaLight
//   {
//       
//       float3 position;               // Offset: 2432
//       float _pad1;                   // Offset: 2444
//       float3 direction;              // Offset: 2448
//       float _pad2;                   // Offset: 2460
//       float3 color;                  // Offset: 2464
//       float _pad3;                   // Offset: 2476
//       float2 dimensions;             // Offset: 2480
//       float intensity;               // Offset: 2488
//       bool castsShadow;              // Offset: 2492
//       bool enabled;                  // Offset: 2496
//       float3 _pad;                   // Offset: 2500
//
//   } AreaLights[16];                  // Offset: 2432 Size:  1280 [unused]
//
// }
//
// cbuffer CameraBuffer
// {
//
//   float4 CameraPosition;             // Offset:    0 Size:    16
//
// }
//
//
// Resource Bindings:
//
// Name                                 Type  Format         Dim      HLSL Bind  Count
// ------------------------------ ---------- ------- ----------- -------------- ------
// LightBuffer                       cbuffer      NA          NA            cb1      1 
// CameraBuffer                      cbuffer      NA          NA            cb2      1 
//
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float       
// TEXCOORD                 0   xy          1     NONE   float       
// TEXCOORD                 4     z         1     NONE   float       
// TEXCOORD                 1   xyz         2     NONE   float   xyz 
// TEXCOORD                 2   xyz         3     NONE   float       
// NORMAL                   0   xyz         4     NONE   float   xyz 
// TANGENT                  0   xyz         5     NONE   float       
// BINORMAL                 0   xyz         6     NONE   float       
// TEXCOORD                 3   xyz         7     NONE   float       
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
//
ps_5_0
dcl_globalFlags refactoringAllowed
dcl_constantbuffer CB1[3], immediateIndexed
dcl_constantbuffer CB2[1], immediateIndexed
dcl_input_ps linear v2.xyz
dcl_input_ps linear v4.xyz
dcl_output o0.xyzw
dcl_temps 4
dp3 r0.x, cb1[0].xyzx, cb1[0].xyzx
rsq r0.x, r0.x
mul r0.xyz, r0.xxxx, cb1[0].xyzx
add r1.xyz, -v2.xyzx, cb2[0].xyzx
dp3 r0.w, r1.xyzx, r1.xyzx
rsq r0.w, r0.w
mad r2.xyz, r1.xyzx, r0.wwww, -r0.xyzx
mul r1.xyz, r0.wwww, r1.xyzx
dp3 r0.w, r2.xyzx, r2.xyzx
rsq r0.w, r0.w
mul r2.xyz, r0.wwww, r2.xyzx
dp3 r0.w, v4.xyzx, v4.xyzx
rsq r0.w, r0.w
mul r3.xyz, r0.wwww, v4.xyzx
dp3 r0.w, r3.xyzx, r2.xyzx
max r0.w, r0.w, l(0.000000)
log r0.w, r0.w
mul r0.w, r0.w, l(64.000000)
exp r0.w, r0.w
mul r0.w, r0.w, l(0.500000)
mul r2.xyz, r0.wwww, cb1[1].xyzx
dp3 r0.x, r3.xyzx, -r0.xyzx
dp3 r0.y, r3.xyzx, r1.xyzx
max r0.xy, r0.xyxx, l(0.000000, 0.000000, 0.000000, 0.000000)
mul r0.xzw, r0.xxxx, cb1[1].xxyz
mul r0.xzw, r0.xxzw, cb1[2].xxxx
mad r0.xzw, r0.xxzw, l(0.800000, 0.000000, 0.800000, 0.800000), l(0.200000, 0.000000, 0.200000, 0.200000)
rsq r1.x, r0.y
add r0.y, -r0.y, l(1.000000)
div r1.x, l(1.000000, 1.000000, 1.000000, 1.000000), r1.x
mad r1.xyz, r1.xxxx, l(0.100000, 0.300000, 0.400000, 0.000000), l(0.000000, 0.200000, 0.400000, 0.000000)
mad r0.xzw, r1.xxyz, r0.xxzw, r2.xxyz
add r1.xyz, -r0.xzwx, l(0.300000, 0.500000, 0.900000, 0.000000)
mul r1.w, r0.y, r0.y
mul r1.w, r1.w, r1.w
mul r0.y, r0.y, r1.w
mul r0.y, r0.y, l(0.700000)
mad o0.xyz, r0.yyyy, r1.xyzx, r0.xzwx
mov o0.w, l(0.800000)
ret 
// Approximately 40 instruction slots used
#endif

const BYTE kWater_PSBytes[] =
{
     68,  88,  66,  67,  86,  26, 
     12, 177,   1, 194,  69, 220, 
     17,  51, 161,  41, 238, 178, 
    210, 189,   1,   0,   0,   0, 
    220,  13,   0,   0,   5,   0, 
      0,   0,  52,   0,   0,   0, 
    224,   6,   0,   0, 248,   7, 
      0,   0,  44,   8,   0,   0, 
     64,  13,   0,   0,  82,  68, 
     69,  70, 164,   6,   0,   0, 
      2,   0,   0,   0, 152,   0, 
      0,   0,   2,   0,   0,   0, 
     60,   0,   0,   0,   0,   5, 
    255, 255,   0,   1,   0,   0, 
    124,   6,   0,   0,  82,  68, 
     49,  49,  60,   0,   0,   0, 
     24,   0,   0,   0,  32,   0, 
      0,   0,  40,   0,   0,   0, 
     36,   0,   0,   0,  12,   0, 
      0,   0,   0,   0,   0,   0, 
    124,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   1,   0,   0,   0, 
      1,   0,   0,   0,   1,   0, 
      0,   0, 136,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   2,   0, 
      0,   0,   1,   0,   0,   0, 
      1,   0,   0,   0,  76, 105, 
    103, 104, 116,  66, 117, 102, 
    102, 101, 114,   0,  67,  97, 
    109, 101, 114,  97,  66, 117, 
    102, 102, 101, 114,   0, 171, 
    171, 171, 124,   0,   0,   0, 
      4,   0,   0,   0, 200,   0, 
      0,   0, 128,  14,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0, 136,   0,   0,   0, 
      1,   0,   0,   0,  32,   6, 
      0,   0,  16,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0, 104,   1,   0,   0, 
      0,   0,   0,   0, 128,   0, 
      0,   0,   2,   0,   0,   0, 
     52,   3,   0,   0,   0,   0, 
      0,   0, 255, 255, 255, 255, 
      0,   0,   0,   0, 255, 255, 
    255, 255,   0,   0,   0,   0, 
     88,   3,   0,   0, 128,   0, 
      0,   0,   0,   4,   0,   0, 
      0,   0,   0,   0,  44,   4, 
      0,   0,   0,   0,   0,   0, 
    255, 255, 255, 255,   0,   0, 
      0,   0, 255, 255, 255, 255, 
      0,   0,   0,   0,  80,   4, 
      0,   0, 128,   4,   0,   0, 
      0,   5,   0,   0,   0,   0, 
      0,   0,  44,   5,   0,   0, 
      0,   0,   0,   0, 255, 255, 
    255, 255,   0,   0,   0,   0, 
    255, 255, 255, 255,   0,   0, 
      0,   0,  80,   5,   0,   0, 
    128,   9,   0,   0,   0,   5, 
      0,   0,   0,   0,   0,   0, 
    252,   5,   0,   0,   0,   0, 
      0,   0, 255, 255, 255, 255, 
      0,   0,   0,   0, 255, 255, 
    255, 255,   0,   0,   0,   0, 
     83, 117, 110,   0,  68, 105, 
    114, 101,  99, 116, 105, 111, 
    110,  97, 108,  76, 105, 103, 
    104, 116,   0, 100, 105, 114, 
    101,  99, 116, 105, 111, 110, 
      0, 102, 108, 111,  97, 116, 
     52,   0, 171, 171,   1,   0, 
      3,   0,   1,   0,   4,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
    135,   1,   0,   0,  99, 111, 
    108, 111, 114,   0, 105, 110, 
    116, 101, 110, 115, 105, 116, 
    121,   0, 102, 108, 111,  97, 
    116,   0, 171, 171,   0,   0, 
      3,   0,   1,   0,   1,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
    196,   1,   0,   0,  95, 112, 
     97, 100,  49,   0, 102, 108, 
    111,  97, 116,  51,   0, 171, 
    171, 171,   1,   0,   3,   0, 
      1,   0,   3,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0, 246,   1, 
      0,   0,  99,  97, 115, 116, 
    115,  83, 104,  97, 100, 111, 
    119,   0,  98, 111, 111, 108, 
      0, 171, 171, 171,   0,   0, 
      1,   0,   1,   0,   1,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
     48,   2,   0,   0, 101, 110, 
     97,  98, 108, 101, 100,   0, 
     95, 112,  97, 100,  50,   0, 
    102, 108, 111,  97, 116,  50, 
      0, 171, 171, 171,   1,   0, 
      3,   0,   1,   0,   2,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
    106,   2,   0,   0, 108, 105, 
    103, 104, 116,  86, 105, 101, 
    119,  80, 114, 111, 106,   0, 
    102, 108, 111,  97, 116,  52, 
    120,  52,   0, 171,   3,   0, 
      3,   0,   4,   0,   4,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
    166,   2,   0,   0, 125,   1, 
      0,   0, 144,   1,   0,   0, 
      0,   0,   0,   0, 180,   1, 
      0,   0, 144,   1,   0,   0, 
     16,   0,   0,   0, 186,   1, 
      0,   0, 204,   1,   0,   0, 
     32,   0,   0,   0, 240,   1, 
      0,   0,   0,   2,   0,   0, 
     36,   0,   0,   0,  36,   2, 
      0,   0,  56,   2,   0,   0, 
     48,   0,   0,   0,  92,   2, 
      0,   0,  56,   2,   0,   0, 
     52,   0,   0,   0, 100,   2, 
      0,   0, 116,   2,   0,   0, 
     56,   0,   0,   0, 152,   2, 
      0,   0, 176,   2,   0,   0, 
     64,   0,   0,   0,   5,   0, 
      0,   0,   1,   0,  32,   0, 
      0,   0,   8,   0, 212,   2, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
    108,   1,   0,   0,  80, 111, 
    105, 110, 116,  76, 105, 103, 
    104, 116, 115,   0,  80, 111, 
    105, 110, 116,  76, 105, 103, 
    104, 116,   0, 112, 111, 115, 
    105, 116, 105, 111, 110,   0, 
    112,  97, 100, 100, 105, 110, 
    103,  49,   0,  99, 111, 110, 
    115, 116,  97, 110, 116,   0, 
    108, 105, 110,   0, 113, 117, 
     97, 100, 114,  97, 116, 105, 
     99,   0, 114,  97, 100, 105, 
    117, 115,   0, 112,  97, 100, 
    100, 105, 110, 103,  50,   0, 
    111,   3,   0,   0,   0,   2, 
      0,   0,   0,   0,   0,   0, 
    120,   3,   0,   0, 204,   1, 
      0,   0,  12,   0,   0,   0, 
    180,   1,   0,   0,   0,   2, 
      0,   0,  16,   0,   0,   0, 
    186,   1,   0,   0, 204,   1, 
      0,   0,  28,   0,   0,   0, 
    129,   3,   0,   0, 204,   1, 
      0,   0,  32,   0,   0,   0, 
    138,   3,   0,   0, 204,   1, 
      0,   0,  36,   0,   0,   0, 
    142,   3,   0,   0, 204,   1, 
      0,   0,  40,   0,   0,   0, 
    152,   3,   0,   0, 204,   1, 
      0,   0,  44,   0,   0,   0, 
     36,   2,   0,   0,  56,   2, 
      0,   0,  48,   0,   0,   0, 
     92,   2,   0,   0,  56,   2, 
      0,   0,  52,   0,   0,   0, 
    159,   3,   0,   0, 116,   2, 
      0,   0,  56,   0,   0,   0, 
      5,   0,   0,   0,   1,   0, 
     16,   0,  16,   0,  11,   0, 
    168,   3,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0, 100,   3,   0,   0, 
     83, 112, 111, 116,  76, 105, 
    103, 104, 116, 115,   0,  83, 
    112, 111, 116,  76, 105, 103, 
    104, 116,   0,  95, 112,  97, 
    100, 100, 105, 110, 103,  49, 
      0,  95, 112,  97, 100, 100, 
    105, 110, 103,  50,   0, 105, 
    110, 110, 101, 114,  65, 110, 
    103, 108, 101,   0, 111, 117, 
    116, 101, 114,  65, 110, 103, 
    108, 101,   0, 114,  97, 110, 
    103, 101,   0,  95, 112,  97, 
    100,   0, 171, 171, 111,   3, 
      0,   0,   0,   2,   0,   0, 
      0,   0,   0,   0, 101,   4, 
      0,   0, 204,   1,   0,   0, 
     12,   0,   0,   0, 125,   1, 
      0,   0,   0,   2,   0,   0, 
     16,   0,   0,   0, 111,   4, 
      0,   0, 204,   1,   0,   0, 
     28,   0,   0,   0, 180,   1, 
      0,   0,   0,   2,   0,   0, 
     32,   0,   0,   0, 186,   1, 
      0,   0, 204,   1,   0,   0, 
     44,   0,   0,   0, 121,   4, 
      0,   0, 204,   1,   0,   0, 
     48,   0,   0,   0, 132,   4, 
      0,   0, 204,   1,   0,   0, 
     52,   0,   0,   0, 143,   4, 
      0,   0, 204,   1,   0,   0, 
     56,   0,   0,   0,  36,   2, 
      0,   0,  56,   2,   0,   0, 
     60,   0,   0,   0,  92,   2, 
      0,   0,  56,   2,   0,   0, 
     64,   0,   0,   0, 149,   4, 
      0,   0,   0,   2,   0,   0, 
     68,   0,   0,   0,   5,   0, 
      0,   0,   1,   0,  20,   0, 
     16,   0,  12,   0, 156,   4, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
     91,   4,   0,   0,  65, 114, 
    101,  97,  76, 105, 103, 104, 
    116, 115,   0,  65, 114, 101, 
     97,  76, 105, 103, 104, 116, 
      0,  95, 112,  97, 100,  51, 
      0, 100, 105, 109, 101, 110, 
    115, 105, 111, 110, 115,   0, 
    171, 171, 111,   3,   0,   0, 
      0,   2,   0,   0,   0,   0, 
      0,   0, 240,   1,   0,   0, 
    204,   1,   0,   0,  12,   0, 
      0,   0, 125,   1,   0,   0, 
      0,   2,   0,   0,  16,   0, 
      0,   0, 100,   2,   0,   0, 
    204,   1,   0,   0,  28,   0, 
      0,   0, 180,   1,   0,   0, 
      0,   2,   0,   0,  32,   0, 
      0,   0, 101,   5,   0,   0, 
    204,   1,   0,   0,  44,   0, 
      0,   0, 107,   5,   0,   0, 
    116,   2,   0,   0,  48,   0, 
      0,   0, 186,   1,   0,   0, 
    204,   1,   0,   0,  56,   0, 
      0,   0,  36,   2,   0,   0, 
     56,   2,   0,   0,  60,   0, 
      0,   0,  92,   2,   0,   0, 
     56,   2,   0,   0,  64,   0, 
      0,   0, 149,   4,   0,   0, 
      0,   2,   0,   0,  68,   0, 
      0,   0,   5,   0,   0,   0, 
      1,   0,  20,   0,  16,   0, 
     11,   0, 120,   5,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,  91,   5, 
      0,   0,  72,   6,   0,   0, 
      0,   0,   0,   0,  16,   0, 
      0,   0,   2,   0,   0,   0, 
     88,   6,   0,   0,   0,   0, 
      0,   0, 255, 255, 255, 255, 
      0,   0,   0,   0, 255, 255, 
    255, 255,   0,   0,   0,   0, 
     67,  97, 109, 101, 114,  97, 
     80, 111, 115, 105, 116, 105, 
    111, 110,   0, 171,   1,   0, 
      3,   0,   1,   0,   4,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
    135,   1,   0,   0,  77, 105, 
     99, 114, 111, 115, 111, 102, 
    116,  32,  40,  82,  41,  32, 
     72,  76,  83,  76,  32,  83, 
    104,  97, 100, 101, 114,  32, 
     67, 111, 109, 112, 105, 108, 
    101, 114,  32,  49,  48,  46, 
     49,   0,  73,  83,  71,  78, 
     16,   1,   0,   0,   9,   0, 
      0,   0,   8,   0,   0,   0, 
    224,   0,   0,   0,   0,   0, 
      0,   0,   1,   0,   0,   0, 
      3,   0,   0,   0,   0,   0, 
      0,   0,  15,   0,   0,   0, 
    236,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      3,   0,   0,   0,   1,   0, 
      0,   0,   3,   0,   0,   0, 
    236,   0,   0,   0,   4,   0, 
      0,   0,   0,   0,   0,   0, 
      3,   0,   0,   0,   1,   0, 
      0,   0,   4,   0,   0,   0, 
    236,   0,   0,   0,   1,   0, 
      0,   0,   0,   0,   0,   0, 
      3,   0,   0,   0,   2,   0, 
      0,   0,   7,   7,   0,   0, 
    236,   0,   0,   0,   2,   0, 
      0,   0,   0,   0,   0,   0, 
      3,   0,   0,   0,   3,   0, 
      0,   0,   7,   0,   0,   0, 
    245,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      3,   0,   0,   0,   4,   0, 
      0,   0,   7,   7,   0,   0, 
    252,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      3,   0,   0,   0,   5,   0, 
      0,   0,   7,   0,   0,   0, 
      4,   1,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      3,   0,   0,   0,   6,   0, 
      0,   0,   7,   0,   0,   0, 
    236,   0,   0,   0,   3,   0, 
      0,   0,   0,   0,   0,   0, 
      3,   0,   0,   0,   7,   0, 
      0,   0,   7,   0,   0,   0, 
     83,  86,  95,  80,  79,  83, 
     73,  84,  73,  79,  78,   0, 
     84,  69,  88,  67,  79,  79, 
     82,  68,   0,  78,  79,  82, 
     77,  65,  76,   0,  84,  65, 
     78,  71,  69,  78,  84,   0, 
     66,  73,  78,  79,  82,  77, 
     65,  76,   0, 171, 171, 171, 
     79,  83,  71,  78,  44,   0, 
      0,   0,   1,   0,   0,   0, 
      8,   0,   0,   0,  32,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   3,   0, 
      0,   0,   0,   0,   0,   0, 
     15,   0,   0,   0,  83,  86, 
     95,  84,  97, 114, 103, 101, 
    116,   0, 171, 171,  83,  72, 
     69,  88,  12,   5,   0,   0, 
     80,   0,   0,   0,  67,   1, 
      0,   0, 106,   8,   0,   1, 
     89,   0,   0,   4,  70, 142, 
     32,   0,   1,   0,   0,   0, 
      3,   0,   0,   0,  89,   0, 
      0,   4,  70, 142,  32,   0, 
      2,   0,   0,   0,   1,   0, 
      0,   0,  98,  16,   0,   3, 
    114,  16,  16,   0,   2,   0, 
      0,   0,  98,  16,   0,   3, 
    114,  16,  16,   0,   4,   0, 
      0,   0, 101,   0,   0,   3, 
    242,  32,  16,   0,   0,   0, 
      0,   0, 104,   0,   0,   2, 
      4,   0,   0,   0,  16,   0, 
      0,   9,  18,   0,  16,   0, 
      0,   0,   0,   0,  70, 130, 
     32,   0,   1,   0,   0,   0, 
      0,   0,   0,   0,  70, 130, 
     32,   0,   1,   0,   0,   0, 
      0,   0,   0,   0,  68,   0, 
      0,   5,  18,   0,  16,   0, 
      0,   0,   0,   0,  10,   0, 
     16,   0,   0,   0,   0,   0, 
     56,   0,   0,   8, 114,   0, 
     16,   0,   0,   0,   0,   0, 
      6,   0,  16,   0,   0,   0, 
      0,   0,  70, 130,  32,   0, 
      1,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   9, 
    114,   0,  16,   0,   1,   0, 
      0,   0,  70,  18,  16, 128, 
     65,   0,   0,   0,   2,   0, 
      0,   0,  70, 130,  32,   0, 
      2,   0,   0,   0,   0,   0, 
      0,   0,  16,   0,   0,   7, 
    130,   0,  16,   0,   0,   0, 
      0,   0,  70,   2,  16,   0, 
      1,   0,   0,   0,  70,   2, 
     16,   0,   1,   0,   0,   0, 
     68,   0,   0,   5, 130,   0, 
     16,   0,   0,   0,   0,   0, 
     58,   0,  16,   0,   0,   0, 
      0,   0,  50,   0,   0,  10, 
    114,   0,  16,   0,   2,   0, 
      0,   0,  70,   2,  16,   0, 
      1,   0,   0,   0, 246,  15, 
     16,   0,   0,   0,   0,   0, 
     70,   2,  16, 128,  65,   0, 
      0,   0,   0,   0,   0,   0, 
     56,   0,   0,   7, 114,   0, 
     16,   0,   1,   0,   0,   0, 
    246,  15,  16,   0,   0,   0, 
      0,   0,  70,   2,  16,   0, 
      1,   0,   0,   0,  16,   0, 
      0,   7, 130,   0,  16,   0, 
      0,   0,   0,   0,  70,   2, 
     16,   0,   2,   0,   0,   0, 
     70,   2,  16,   0,   2,   0, 
      0,   0,  68,   0,   0,   5, 
    130,   0,  16,   0,   0,   0, 
      0,   0,  58,   0,  16,   0, 
      0,   0,   0,   0,  56,   0, 
      0,   7, 114,   0,  16,   0, 
      2,   0,   0,   0, 246,  15, 
     16,   0,   0,   0,   0,   0, 
     70,   2,  16,   0,   2,   0, 
      0,   0,  16,   0,   0,   7, 
    130,   0,  16,   0,   0,   0, 
      0,   0,  70,  18,  16,   0, 
      4,   0,   0,   0,  70,  18, 
     16,   0,   4,   0,   0,   0, 
     68,   0,   0,   5, 130,   0, 
     16,   0,   0,   0,   0,   0, 
     58,   0,  16,   0,   0,   0, 
      0,   0,  56,   0,   0,   7, 
    114,   0,  16,   0,   3,   0, 
      0,   0, 246,  15,  16,   0, 
      0,   0,   0,   0,  70,  18, 
     16,   0,   4,   0,   0,   0, 
     16,   0,   0,   7, 130,   0, 
     16,   0,   0,   0,   0,   0, 
     70,   2,  16,   0,   3,   0, 
      0,   0,  70,   2,  16,   0, 
      2,   0,   0,   0,  52,   0, 
      0,   7, 130,   0,  16,   0, 
      0,   0,   0,   0,  58,   0, 
     16,   0,   0,   0,   0,   0, 
      1,  64,   0,   0,   0,   0, 
      0,   0,  47,   0,   0,   5, 
    130,   0,  16,   0,   0,   0, 
      0,   0,  58,   0,  16,   0, 
      0,   0,   0,   0,  56,   0, 
      0,   7, 130,   0,  16,   0, 
      0,   0,   0,   0,  58,   0, 
     16,   0,   0,   0,   0,   0, 
      1,  64,   0,   0,   0,   0, 
    128,  66,  25,   0,   0,   5, 
    130,   0,  16,   0,   0,   0, 
      0,   0,  58,   0,  16,   0, 
      0,   0,   0,   0,  56,   0, 
      0,   7, 130,   0,  16,   0, 
      0,   0,   0,   0,  58,   0, 
     16,   0,   0,   0,   0,   0, 
      1,  64,   0,   0,   0,   0, 
      0,  63,  56,   0,   0,   8, 
    114,   0,  16,   0,   2,   0, 
      0,   0, 246,  15,  16,   0, 
      0,   0,   0,   0,  70, 130, 
     32,   0,   1,   0,   0,   0, 
      1,   0,   0,   0,  16,   0, 
      0,   8,  18,   0,  16,   0, 
      0,   0,   0,   0,  70,   2, 
     16,   0,   3,   0,   0,   0, 
     70,   2,  16, 128,  65,   0, 
      0,   0,   0,   0,   0,   0, 
     16,   0,   0,   7,  34,   0, 
     16,   0,   0,   0,   0,   0, 
     70,   2,  16,   0,   3,   0, 
      0,   0,  70,   2,  16,   0, 
      1,   0,   0,   0,  52,   0, 
      0,  10,  50,   0,  16,   0, 
      0,   0,   0,   0,  70,   0, 
     16,   0,   0,   0,   0,   0, 
      2,  64,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,  56,   0,   0,   8, 
    210,   0,  16,   0,   0,   0, 
      0,   0,   6,   0,  16,   0, 
      0,   0,   0,   0,   6, 137, 
     32,   0,   1,   0,   0,   0, 
      1,   0,   0,   0,  56,   0, 
      0,   8, 210,   0,  16,   0, 
      0,   0,   0,   0,   6,  14, 
     16,   0,   0,   0,   0,   0, 
      6, 128,  32,   0,   1,   0, 
      0,   0,   2,   0,   0,   0, 
     50,   0,   0,  15, 210,   0, 
     16,   0,   0,   0,   0,   0, 
      6,  14,  16,   0,   0,   0, 
      0,   0,   2,  64,   0,   0, 
    205, 204,  76,  63,   0,   0, 
      0,   0, 205, 204,  76,  63, 
    205, 204,  76,  63,   2,  64, 
      0,   0, 205, 204,  76,  62, 
      0,   0,   0,   0, 205, 204, 
     76,  62, 205, 204,  76,  62, 
     68,   0,   0,   5,  18,   0, 
     16,   0,   1,   0,   0,   0, 
     26,   0,  16,   0,   0,   0, 
      0,   0,   0,   0,   0,   8, 
     34,   0,  16,   0,   0,   0, 
      0,   0,  26,   0,  16, 128, 
     65,   0,   0,   0,   0,   0, 
      0,   0,   1,  64,   0,   0, 
      0,   0, 128,  63,  14,   0, 
      0,  10,  18,   0,  16,   0, 
      1,   0,   0,   0,   2,  64, 
      0,   0,   0,   0, 128,  63, 
      0,   0, 128,  63,   0,   0, 
    128,  63,   0,   0, 128,  63, 
     10,   0,  16,   0,   1,   0, 
      0,   0,  50,   0,   0,  15, 
    114,   0,  16,   0,   1,   0, 
      0,   0,   6,   0,  16,   0, 
      1,   0,   0,   0,   2,  64, 
      0,   0, 205, 204, 204,  61, 
    154, 153, 153,  62, 205, 204, 
    204,  62,   0,   0,   0,   0, 
      2,  64,   0,   0,   0,   0, 
      0,   0, 205, 204,  76,  62, 
    205, 204, 204,  62,   0,   0, 
      0,   0,  50,   0,   0,   9, 
    210,   0,  16,   0,   0,   0, 
      0,   0,   6,   9,  16,   0, 
      1,   0,   0,   0,   6,  14, 
     16,   0,   0,   0,   0,   0, 
      6,   9,  16,   0,   2,   0, 
      0,   0,   0,   0,   0,  11, 
    114,   0,  16,   0,   1,   0, 
      0,   0, 134,   3,  16, 128, 
     65,   0,   0,   0,   0,   0, 
      0,   0,   2,  64,   0,   0, 
    154, 153, 153,  62,   0,   0, 
      0,  63, 102, 102, 102,  63, 
      0,   0,   0,   0,  56,   0, 
      0,   7, 130,   0,  16,   0, 
      1,   0,   0,   0,  26,   0, 
     16,   0,   0,   0,   0,   0, 
     26,   0,  16,   0,   0,   0, 
      0,   0,  56,   0,   0,   7, 
    130,   0,  16,   0,   1,   0, 
      0,   0,  58,   0,  16,   0, 
      1,   0,   0,   0,  58,   0, 
     16,   0,   1,   0,   0,   0, 
     56,   0,   0,   7,  34,   0, 
     16,   0,   0,   0,   0,   0, 
     26,   0,  16,   0,   0,   0, 
      0,   0,  58,   0,  16,   0, 
      1,   0,   0,   0,  56,   0, 
      0,   7,  34,   0,  16,   0, 
      0,   0,   0,   0,  26,   0, 
     16,   0,   0,   0,   0,   0, 
      1,  64,   0,   0,  51,  51, 
     51,  63,  50,   0,   0,   9, 
    114,  32,  16,   0,   0,   0, 
      0,   0,  86,   5,  16,   0, 
      0,   0,   0,   0,  70,   2, 
     16,   0,   1,   0,   0,   0, 
    134,   3,  16,   0,   0,   0, 
      0,   0,  54,   0,   0,   5, 
    130,  32,  16,   0,   0,   0, 
      0,   0,   1,  64,   0,   0, 
    205, 204,  76,  63,  62,   0, 
      0,   1,  83,  84,  65,  84, 
    148,   0,   0,   0,  40,   0, 
      0,   0,   4,   0,   0,   0, 
      0,   0,   0,   0,   3,   0, 
      0,   0,  38,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   1,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   1,   0,   0,   0, 
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
      0,   0,   0,   0,   0,   0, 
      0,   0
};
