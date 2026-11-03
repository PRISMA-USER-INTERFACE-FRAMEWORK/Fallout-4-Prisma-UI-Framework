R"ULHLSL(// Returns two values:
// [0] = distance of p to line segment.
// [1] = closest t on line segment, clamped to [0, 1]
float2 sdSegment(in float2 p, in float2 a, in float2 b)
{
  float2 pa = p - a, ba = b - a;
  float t = dot(pa, ba) / dot(ba, ba);
  return float2(length(pa - ba * t), t);
}

float testCross(float2 a, float2 b, float2 p) {
  return (b.y - a.y) * (p.x - a.x) - (b.x - a.x) * (p.y - a.y);
}

float sdLine(in float2 a, in float2 b, in float2 p)
{
  float2 pa = p - a, ba = b - a;
  float t = dot(pa, ba) / dot(ba, ba);
  return length(pa - ba*t) * sign(testCross(a, b, p));
}


float4 blend(float4 src, float4 dest) {
  float4 result;
  result.rgb = src.rgb + dest.rgb * (1.0 - src.a);
  result.a = src.a + dest.a * (1.0 - src.a);
  return result;
}

float innerStroke(float stroke_width, float d) {
  return min(antialias(-d, AA_WIDTH, 0.0), 1.0 - antialias(-d, AA_WIDTH, stroke_width));
}

float4 fillRoundedRect(VS_OUTPUT input) {
  float2 p = input.TexCoord;
  float2 size = input.Data0.zw;
  p = (p - 0.5) * size;
  float d = sdRoundRect(p, size, input.Data1, input.Data2);

  // Fill background
  float alpha = antialias(-d, AA_WIDTH, 0.0) * input.Color.a;
  float4 outColor = float4(input.Color.rgb * alpha, alpha);

  // Draw stroke
  float stroke_width = input.Data3.x;
  float4 stroke_color = input.Data4;

  if (stroke_width > 0.0) {
    alpha = innerStroke(stroke_width, d);
    alpha *= stroke_color.a;
    float4 stroke = float4(stroke_color.rgb * alpha, alpha);
    outColor = blend(stroke, outColor);
  }

  return outColor;
}

float4 fillBoxShadow(VS_OUTPUT input) {
  float2 p = input.ObjectCoord;
  bool inset = bool(uint(input.Data0.y + 0.5));
  float radius = input.Data0.z;
  float2 origin = input.Data1.xy;
  float2 size = input.Data1.zw;
  float2 clip_origin = input.Data4.xy;
  float2 clip_size = input.Data4.zw;
  
  float sdClip = sdRoundRect(p - clip_origin, clip_size, input.Data5, input.Data6);
  float sdRect = sdRoundRect(p - origin, size, input.Data2, input.Data3);
  
  float clip = inset ? -sdRect : sdClip;
  float d = inset ? -sdClip : sdRect;

  if (clip < 0.0) {
    discard;
    return float4(0.0, 0.0, 0.0, 0.0);
  }

  float alpha = radius >= 1.0? pow(antialias(-d, radius * 2 + 0.2, 0.0), 1.9) * 3.3 / pow(radius * 1.2, 0.15) :
                               antialias(-d, AA_WIDTH, inset ? -1.0 : 1.0);

  alpha = clamp(alpha, 0.0, 1.0) * input.Color.a;
  return float4(input.Color.rgb * alpha, alpha);
}

float3 blendOverlay(float3 src, float3 dest) {
  float3 col;
  for (uint i = 0; i < 3; ++i)
    col[i] = dest[i] < 0.5 ? (2.0 * dest[i] * src[i]) : (1.0 - 2.0 * (1.0 - dest[i]) * (1.0 - src[i]));
  return col;
}

float3 blendColorDodge(float3 src, float3 dest) {
  float3 col;
  for (uint i = 0; i < 3; ++i)
    col[i] = (src[i] == 1.0) ? src[i] : min(dest[i] / (1.0 - src[i]), 1.0);
  return col;
}

float3 blendColorBurn(float3 src, float3 dest) {
  float3 col;
  for (uint i = 0; i < 3; ++i)
    col[i] = (src[i] == 0.0) ? src[i] : max((1.0 - ((1.0 - dest[i]) / src[i])), 0.0);
  return col;
}

float3 blendHardLight(float3 src, float3 dest) {
  float3 col;
  for (uint i = 0; i < 3; ++i)
    col[i] = dest[i] < 0.5 ? (2.0 * dest[i] * src[i]) : (1.0 - 2.0 * (1.0 - dest[i]) * (1.0 - src[i]));
  return col;
}

float3 blendSoftLight(float3 src, float3 dest) {
  float3 col;
  for (uint i = 0; i < 3; ++i)
    col[i] = (src[i] < 0.5) ? (2.0 * dest[i] * src[i] + dest[i] * dest[i] * (1.0 - 2.0 * src[i])) : (sqrt(dest[i]) * (2.0 * src[i] - 1.0) + 2.0 * dest[i] * (1.0 - src[i]));
  return col;
}

float3 rgb2hsl( float3 col )
{
  const float eps = 0.0000001;
  float minc = min( col.r, min(col.g, col.b) );
  float maxc = max( col.r, max(col.g, col.b) );
  float3 mask = step(col.grr,col.rgb) * step(col.bbg,col.rgb);
  float3 h = mask * (float3(0.0,2.0,4.0) + (col.gbr-col.brg)/(maxc-minc + eps)) / 6.0;
  return float3(frac( 1.0 + h.x + h.y + h.z ),                // H
                (maxc-minc)/(1.0-abs(minc+maxc-1.0) + eps),   // S
                (minc+maxc)*0.5 );                            // L
}

float3 hsl2rgb( float3 c )
{
  float3 rgb = clamp( abs(fmod(c.x*6.0+float3(0.0,4.0,2.0),6.0)-3.0)-1.0, 0.0, 1.0 );
  return c.z + c.y * (rgb-0.5)*(1.0-abs(2.0*c.z-1.0));
}

float3 blendHue(float3 src, float3 dest) {
  float3 baseHSL = rgb2hsl(dest);
  return hsl2rgb(float3(rgb2hsl(src).r, baseHSL.g, baseHSL.b));
}

float3 blendSaturation(float3 src, float3 dest) {
  float3 baseHSL = rgb2hsl(dest);
  return hsl2rgb(float3(baseHSL.r, rgb2hsl(src).g, baseHSL.b));
}

float3 blendColor(float3 src, float3 dest) {
  float3 blendHSL = rgb2hsl(src);
  return hsl2rgb(float3(blendHSL.r, blendHSL.g, rgb2hsl(dest).b));
}

float3 blendLuminosity(float3 src, float3 dest) {
  float3 baseHSL = rgb2hsl(dest);
  return hsl2rgb(float3(baseHSL.r, baseHSL.g, rgb2hsl(src).b));
}

float4 fillBlend(VS_OUTPUT input) { 
  const uint BlendOp_Clear = 0u;
  const uint BlendOp_Source = 1u;
  const uint BlendOp_Over = 2u;
  const uint BlendOp_In = 3u;
  const uint BlendOp_Out = 4u;
  const uint BlendOp_Atop = 5u;
  const uint BlendOp_DestOver = 6u;
  const uint BlendOp_DestIn = 7u;
  const uint BlendOp_DestOut = 8u;
  const uint BlendOp_DestAtop = 9u;
  const uint BlendOp_XOR = 10u;
  const uint BlendOp_Darken = 11u;
  const uint BlendOp_Add = 12u;
  const uint BlendOp_Difference = 13u;
  const uint BlendOp_Multiply = 14u;
  const uint BlendOp_Screen = 15u;
  const uint BlendOp_Overlay = 16u;
  const uint BlendOp_Lighten = 17u;
  const uint BlendOp_ColorDodge = 18u;
  const uint BlendOp_ColorBurn = 19u;
  const uint BlendOp_HardLight = 20u;
  const uint BlendOp_SoftLight = 21u;
  const uint BlendOp_Exclusion = 22u;
  const uint BlendOp_Hue = 23u;
  const uint BlendOp_Saturation = 24u;
  const uint BlendOp_Color = 25u;
  const uint BlendOp_Luminosity = 26u;

  float4 src = fillImage(input);
  float4 dest = texture1.Sample(sampler0, input.ObjectCoord);

  switch(uint(input.Data0.y + 0.5))
  {
  case BlendOp_Clear: return float4(0.0, 0.0, 0.0, 0.0);
  case BlendOp_Source: return src;
  case BlendOp_Over: return src + dest * (1.0 - src.a);
  case BlendOp_In: return src * dest.a;
  case BlendOp_Out: return src * (1.0 - dest.a);
  case BlendOp_Atop: return src * dest.a + dest * (1.0 - src.a);
  case BlendOp_DestOver: return src * (1.0 - dest.a) + dest;
  case BlendOp_DestIn: return dest * src.a;
  case BlendOp_DestOut: return dest * (1.0 - src.a);
  case BlendOp_DestAtop: return src * (1.0 - dest.a) + dest * src.a;
  case BlendOp_XOR: return saturate(src * (1.0 - dest.a) + dest * (1.0 - src.a));
  case BlendOp_Darken: return float4(min(src.rgb, dest.rgb) * src.a, dest.a * src.a);
  case BlendOp_Add: return saturate(src + dest);
  case BlendOp_Difference: return float4(abs(dest.rgb - src.rgb) * src.a, dest.a * src.a);
  case BlendOp_Multiply: return float4(src.rgb * dest.rgb * src.a, dest.a * src.a);
  case BlendOp_Screen: return float4((1.0 - ((1.0 - dest.rgb) * (1.0 - src.rgb))) * src.a, dest.a * src.a);
  case BlendOp_Overlay: return float4(blendOverlay(src.rgb, dest.rgb) * src.a, dest.a * src.a);
  case BlendOp_Lighten: return float4(max(src.rgb, dest.rgb) * src.a, dest.a * src.a);
  case BlendOp_ColorDodge: return float4(blendColorDodge(src.rgb, dest.rgb) * src.a, dest.a * src.a);
  case BlendOp_ColorBurn: return float4(blendColorBurn(src.rgb, dest.rgb) * src.a, dest.a * src.a);
  case BlendOp_HardLight: return float4(blendOverlay(dest.rgb, src.rgb) * src.a, dest.a * src.a);
  case BlendOp_SoftLight: return float4(blendSoftLight(src.rgb, dest.rgb) * src.a, dest.a * src.a);
  case BlendOp_Exclusion: return float4((dest.rgb + src.rgb - 2.0 * dest.rgb * src.rgb) * src.a, dest.a * src.a);
  case BlendOp_Hue: return float4(blendHue(src.rgb, dest.rgb) * src.a, dest.a * src.a);
  case BlendOp_Saturation: return float4(blendSaturation(src.rgb, dest.rgb) * src.a, dest.a * src.a);
  case BlendOp_Color: return float4(blendColor(src.rgb, dest.rgb) * src.a, dest.a * src.a);
  case BlendOp_Luminosity: return float4(blendLuminosity(src.rgb, dest.rgb) * src.a, dest.a * src.a);
  }

  return src;
}

float4 fillMask(VS_OUTPUT input) {
  float4 col = fillImage(input);
  float alpha = texture1.Sample(sampler0, input.ObjectCoord).a;
  return float4(col.rgb * alpha, col.a * alpha);
}

float4 GetCol(in matrix m, uint i) { return float4(m[0][i], m[1][i], m[2][i], m[3][i]); }

#define VISUALIZE_CLIP 0

void applyClip(VS_OUTPUT input, inout float4 outColor) {
  for (uint i = 0; i < ClipSize; i++) {
    matrix data = Clip[i];
    float2 origin = GetCol(data, 0).xy;
    float2 size = GetCol(data, 0).zw;
    float4 radii_x, radii_y;
    Unpack(GetCol(data, 1), radii_x, radii_y);
    bool inverse = bool(GetCol(data, 3).z);

    float2 p = input.ObjectCoord;
    p = transformAffine(p, GetCol(data, 2).xy, GetCol(data, 2).zw, GetCol(data, 3).xy);
    p -= origin;
    float d_clip = sdRoundRect(p, size, radii_x, radii_y) * (inverse ? -1.0 : 1.0);

#if VISUALIZE_CLIP
    if (abs(d_clip) < 3.0)
      outColor = float4(0.9, 1.0, 0.0, 1.0);
#else
    float alpha = antialias2(-d_clip);
    outColor = float4(outColor.rgb * alpha, outColor.a * alpha);
#endif
  }
}

float4 fillGlyph(VS_OUTPUT input) {
  float alpha = texture0.Sample(sampler0, input.TexCoord).a * input.Color.a;
  float fill_color_luma = input.Data0.y;
  float corrected_alpha = texture1.Sample(sampler0, float2(alpha, fill_color_luma)).a;

  return float4(input.Color.rgb * corrected_alpha, corrected_alpha);
}

float4 PS(VS_OUTPUT input) : SV_Target
{
  const uint FillType_Solid = 0u;
  const uint FillType_Image = 1u;
  const uint FillType_Pattern_Image = 2u;
  const uint FillType_Pattern_Gradient = 3u;
  const uint FillType_RESERVED_1 = 4u;
  const uint FillType_RESERVED_2 = 5u;
  const uint FillType_RESERVED_3 = 6u;
  const uint FillType_Rounded_Rect = 7u;
  const uint FillType_Box_Shadow = 8u;
  const uint FillType_Blend = 9u;
  const uint FillType_Mask = 10u;
  const uint FillType_Glyph = 11u;

  float4 outColor = input.Color;

  switch (FillType(input))
  {
  case FillType_Solid: outColor = fillSolid(input); break;
  case FillType_Image: outColor = fillImage(input); break;
  case FillType_Pattern_Image: outColor = fillPatternImage(input); break;
  case FillType_Pattern_Gradient: outColor = fillPatternGradient(input); break;
  case FillType_Rounded_Rect: outColor = fillRoundedRect(input); break;
  case FillType_Box_Shadow: outColor = fillBoxShadow(input); break;
  case FillType_Blend: outColor = fillBlend(input); break;
  case FillType_Mask: outColor = fillMask(input); break;
  case FillType_Glyph: outColor = fillGlyph(input); break;
  }

  applyClip(input, outColor);

  return outColor;
}
)ULHLSL";
