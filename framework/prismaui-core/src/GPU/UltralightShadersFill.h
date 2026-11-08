#pragma once

namespace PrismaUI::GPU {

inline constexpr char kFillVS[] = R"ULHLSL(
cbuffer Uniforms : register(b0)
{
  float4 State;
  matrix Transform;
  float4 Scalar4[2];
  float4 Vector[8];
  uint ClipSize;
  matrix Clip[8];
};

float4 sRGBToLinear(float4 val) { return float4(val.xyz * (val.xyz * (val.xyz * 0.305306011 + 0.682171111) + 0.012522878), val.w); }

struct VS_OUTPUT
{
  float4 Position    : SV_POSITION;
  float4 Color       : COLOR0;
  float2 TexCoord    : TEXCOORD0;
  float4 Data0       : COLOR1;
  float4 Data1       : COLOR2;
  float4 Data2       : COLOR3;
  float4 Data3       : COLOR4;
  float4 Data4       : COLOR5;
  float4 Data5       : COLOR6;
  float4 Data6       : COLOR7;
  float2 ObjectCoord : TEXCOORD1;
};

VS_OUTPUT VS(float2 Position : POSITION,
             uint4  Color    : COLOR0,
             float2 TexCoord : TEXCOORD0,
             float2 ObjCoord : TEXCOORD1,
             float4 Data0    : COLOR1,
			       float4 Data1    : COLOR2,
             float4 Data2    : COLOR3,
             float4 Data3    : COLOR4,
             float4 Data4    : COLOR5,
             float4 Data5    : COLOR6,
             float4 Data6    : COLOR7)
{
  VS_OUTPUT output;
  output.ObjectCoord = ObjCoord;
  output.Position = mul(Transform, float4(Position, 0.0, 1.0));
  output.Color = float4(Color) / 255.0;
  output.TexCoord = TexCoord;
  output.Data0 = Data0;
  output.Data1 = Data1;
  output.Data2 = Data2;
  output.Data3 = Data3;
  output.Data4 = Data4;
  output.Data5 = Data5;
  output.Data6 = Data6;
  return output;
})ULHLSL";

inline constexpr char kFillPS[] = R"ULHLSL(
cbuffer Uniforms : register(b0)
{
  float4 State;
  matrix Transform;
  float4 Scalar4[2];
  float4 Vector[8];
  uint ClipSize;
  matrix Clip[8];
};

float Scalar(int i) { if (i < 4) return Scalar4[0][i]; else return Scalar4[1][i - 4]; }

Texture2D texture0 : register(t0);
Texture2D texture1 : register(t1);
SamplerState sampler0 : register(s0);

struct VS_OUTPUT
{
  float4 Position    : SV_POSITION;
  float4 Color       : COLOR0;
  float2 TexCoord    : TEXCOORD0;
  float4 Data0       : COLOR1;
  float4 Data1       : COLOR2;
  float4 Data2       : COLOR3;
  float4 Data3       : COLOR4;
  float4 Data4       : COLOR5;
  float4 Data5       : COLOR6;
  float4 Data6       : COLOR7;
  float2 ObjectCoord : TEXCOORD1;
};

uint FillType(VS_OUTPUT input) { return uint(input.Data0.x + 0.5); }
float4 TileRectUV() { return Vector[0]; }
float2 TileSize() { return Vector[1].zw; }
float2 PatternTransformA() { return Vector[2].xy; }
float2 PatternTransformB() { return Vector[2].zw; }
float2 PatternTransformC() { return Vector[3].xy; }
uint Gradient_NumStops(VS_OUTPUT input) { return uint(input.Data0.y + 0.5); }
bool Gradient_IsRadial(VS_OUTPUT input) { return bool(uint(input.Data0.z + 0.5)); }
float Gradient_R0(VS_OUTPUT input) { return input.Data1.x; }
float Gradient_R1(VS_OUTPUT input) { return input.Data1.y; }
float2 Gradient_P0(VS_OUTPUT input) { return input.Data1.xy; }
float2 Gradient_P1(VS_OUTPUT input) { return input.Data1.zw; }
float SDFMaxDistance(VS_OUTPUT input) { return input.Data0.y; }

struct GradientStop { float percent; float4 color; };

GradientStop GetGradientStop(VS_OUTPUT input, uint offset) {
  GradientStop result;
  if (offset < 4) {
    result.percent = input.Data2[offset];
    if (offset == 0)
      result.color = input.Data3;
    else if (offset == 1)
      result.color = input.Data4;
    else if (offset == 2)
      result.color = input.Data5;
    else if (offset == 3)
      result.color = input.Data6;
  } else {
    result.percent = Scalar(offset - 4);
    result.color = Vector[offset - 4];
  }
  return result;
}

#define AA_WIDTH 0.354

float antialias(in float d, in float width, in float median) {
  return smoothstep(median - width, median + width, d);
}

float sdRect(float2 p, float2 size) {
  float2 d = abs(p) - size;
  return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

// The below function "sdEllipse" is MIT licensed with following text:
//
// The MIT License
// Copyright 2013 Inigo Quilez
// Permission is hereby granted, free of charge, to any person obtaining a 
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the Software
// is furnished to do so, subject to the following conditions: The above copyright
// notice and this permission notice shall be included in all copies or substantial
// portions of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
// ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO
// EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

float sdEllipse(float2 p, float2 ab) {
  if (abs(ab.x - ab.y) < 0.1)
    return length(p) - ab.x;

  p = abs(p); if (p.x > p.y) { p = p.yx; ab = ab.yx; }

  float l = ab.y*ab.y - ab.x*ab.x;

  float m = ab.x*p.x / l;
  float n = ab.y*p.y / l;
  float m2 = m*m;
  float n2 = n*n;

  float c = (m2 + n2 - 1.0) / 3.0;
  float c3 = c*c*c;

  float q = c3 + m2*n2*2.0;
  float d = c3 + m2*n2;
  float g = m + m*n2;

  float co;

  if (d < 0.0) {
    float p = acos(q / c3) / 3.0;
    float s = cos(p);
    float t = sin(p)*sqrt(3.0);
    float rx = sqrt(-c*(s + t + 2.0) + m2);
    float ry = sqrt(-c*(s - t + 2.0) + m2);
    co = (ry + sign(l)*rx + abs(g) / (rx*ry) - m) / 2.0;
  } else {
    float h = 2.0*m*n*sqrt(d);
    float s = sign(q + h)*pow(abs(q + h), 1.0 / 3.0);
    float u = sign(q - h)*pow(abs(q - h), 1.0 / 3.0);
    float rx = -s - u - c*4.0 + 2.0*m2;
    float ry = (s - u)*sqrt(3.0);
    float rm = sqrt(rx*rx + ry*ry);
    float p = ry / sqrt(rm - rx);
    co = (p + 2.0*g / rm - m) / 2.0;
  }

  float si = sqrt(1.0 - co*co);

  float2 r = float2(ab.x*co, ab.y*si);

  return length(r - p) * sign(p.y - r.y);
}

float sdRoundRect(float2 p, float2 size, float4 rx, float4 ry) {
  size *= 0.5;
  float2 corner;

  corner = float2(-size.x + rx.x, -size.y + ry.x);  // Top-Left
  float2 local = p - corner;
  if (dot(rx.x, ry.x) > 0.0 && p.x < corner.x && p.y <= corner.y)
    return sdEllipse(local, float2(rx.x, ry.x));

  corner = float2(size.x - rx.y, -size.y + ry.y);   // Top-Right
  local = p - corner;
  if (dot(rx.y, ry.y) > 0.0 && p.x >= corner.x && p.y <= corner.y)
    return sdEllipse(local, float2(rx.y, ry.y));

  corner = float2(size.x - rx.z, size.y - ry.z);  // Bottom-Right
  local = p - corner;
  if (dot(rx.z, ry.z) > 0.0 && p.x >= corner.x && p.y >= corner.y)
    return sdEllipse(local, float2(rx.z, ry.z));

  corner = float2(-size.x + rx.w, size.y - ry.w); // Bottom-Left
  local = p - corner;
  if (dot(rx.w, ry.w) > 0.0 && p.x < corner.x && p.y > corner.y)
    return sdEllipse(local, float2(rx.w, ry.w));

  return sdRect(p, size);
}

float4 fillSolid(VS_OUTPUT input) {
  return input.Color;
}

float4 fillImage(VS_OUTPUT input) {
  return texture0.Sample(sampler0, input.TexCoord) * input.Color;
}

float2 transformAffine(float2 val, float2 a, float2 b, float2 c) {
  return val.x * a + val.y * b + c;
}

float4 fillPatternImage(VS_OUTPUT input) {
  float4 tile_rect_uv = TileRectUV();
  float2 tile_size = TileSize();

  float2 p = input.ObjectCoord;

  // Apply the affine matrix
  float2 transformed_coords = transformAffine(p,
    PatternTransformA(), PatternTransformB(), PatternTransformC());

  // Convert back to uv coordinate space
  transformed_coords /= tile_size;

  // Wrap UVs to [0.0, 1.0] so texture repeats properly
  float2 uv = frac(transformed_coords);

  // Clip to tile-rect UV
  uv *= tile_rect_uv.zw - tile_rect_uv.xy;
  uv += tile_rect_uv.xy;

  return texture0.Sample(sampler0, uv) * input.Color;
}

// Gradient noise from Jorge Jimenez's presentation:
// http://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare
float gradientNoise(in float2 uv)
{
    const float3 magic = float3(0.06711056, 0.00583715, 52.9829189);
    return frac(magic.z * frac(dot(uv, magic.xy)));
}

float ramp(in float inMin, in float inMax, in float val)
{
    return clamp((val - inMin) / (inMax - inMin), 0.0, 1.0);
}

float4 fillPatternGradient(VS_OUTPUT input) {
  float num_stops = Gradient_NumStops(input);
  bool is_radial = Gradient_IsRadial(input);
  float2 p0 = Gradient_P0(input);
  float2 p1 = Gradient_P1(input);
  float4 out_Color = float4(0.0, 0.0, 0.0, 0.0);

  float t = 0.0;
  if (is_radial) {
    float r0 = p1.x;
	float r1 = p1.y;
    t = distance(input.TexCoord, p0);
	float rDelta = r1 - r0;
	t = saturate((t / rDelta) - (r0 / rDelta));
  } else {
    float2 V = p1 - p0;
    t = saturate(dot(input.TexCoord - p0, V) / dot(V, V));
  }

  GradientStop stop0 = GetGradientStop(input, 0u);
  GradientStop stop1 = GetGradientStop(input, 1u);

  out_Color = lerp(stop0.color, stop1.color, ramp(stop0.percent, stop1.percent, t));
  if (num_stops > 2) {
    GradientStop stop2 = GetGradientStop(input, 2u);
    out_Color = lerp(out_Color, stop2.color, ramp(stop1.percent, stop2.percent, t));
    if (num_stops > 3) {
      GradientStop stop3 = GetGradientStop(input, 3u);
      out_Color = lerp(out_Color, stop3.color, ramp(stop2.percent, stop3.percent, t));
      if (num_stops > 4) {
        GradientStop stop4 = GetGradientStop(input, 4u);
        out_Color = lerp(out_Color, stop4.color, ramp(stop3.percent, stop4.percent, t));
        if (num_stops > 5) {
          GradientStop stop5 = GetGradientStop(input, 5u);
          out_Color = lerp(out_Color, stop5.color, ramp(stop4.percent, stop5.percent, t));
          if (num_stops > 6) {
            GradientStop stop6 = GetGradientStop(input, 6u);
            out_Color = lerp(out_Color, stop6.color, ramp(stop5.percent, stop6.percent, t));
          }
        }
      }
    }
  }

  return float4(out_Color.rgb, out_Color.a);
}

void Unpack(float4 x, out float4 a, out float4 b) {
  const float s = 65536.0;
  a = floor(x / s);
  b = floor(x - a * s);
}

float antialias2(float d) {
  return smoothstep(-0.6180469, 0.6180469, d/fwidth(d));
}

)ULHLSL"
#include "UltralightShadersFillTail.inl"

}
