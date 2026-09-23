// SPDX-License-Identifier: NOASSERTION
// DxLib 3.25a pixel-lighting VS ABI. GGX + Smith correlated visibility + Schlick Fresnel.
#define DX_D3D11_SHADER_FLOAT float
#define DX_D3D11_SHADER_FLOAT2 float2
#define DX_D3D11_SHADER_FLOAT3 float3
#define DX_D3D11_SHADER_FLOAT4 float4
#define DX_D3D11_SHADER_INT int
#define DX_D3D11_SHADER_INT2 int2
#define DX_D3D11_SHADER_INT3 int3
#define DX_D3D11_SHADER_INT4 int4
#include "DxShader_Common_D3D11.h"
cbuffer Common : register(b0) { DX_D3D11_CONST_BUFFER_COMMON Scene; }
cbuffer Override : register(b4)
{
    // Negative values select imported material factors / UV.
    float4 Factors;
    float4 AmbientOrtho;
}
Texture2D BaseTexture : register(t0);
SamplerState BaseSampler : register(s0);
struct Input
{
    float4 Diffuse : COLOR0;
    // Keep this unused register: DxLib binary VS emits UV at v2, position at v3, normal at v4.
    float4 Specular : COLOR1;
    float4 Uv : TEXCOORD0;
    float3 Position : TEXCOORD1;
    float3 Normal : TEXCOORD2;
};
float3 ToLinear(float3 c)
{
    c = abs(c);
    return float3(c.r <= 0.04045 ? c.r / 12.92 : pow((c.r + 0.055) / 1.055, 2.4),
                  c.g <= 0.04045 ? c.g / 12.92 : pow((c.g + 0.055) / 1.055, 2.4),
                  c.b <= 0.04045 ? c.b / 12.92 : pow((c.b + 0.055) / 1.055, 2.4));
}
float3 ToSrgb(float3 c)
{
    c = saturate(c);
    return float3(c.r <= 0.0031308 ? c.r * 12.92 : 1.055 * pow(max(c.r, 1e-10), 1.0 / 2.4) - 0.055,
                  c.g <= 0.0031308 ? c.g * 12.92 : 1.055 * pow(max(c.g, 1e-10), 1.0 / 2.4) - 0.055,
                  c.b <= 0.0031308 ? c.b * 12.92 : 1.055 * pow(max(c.b, 1e-10), 1.0 / 2.4) - 0.055);
}
float3 SafeUnit(float3 v)
{
    return v * rsqrt(max(dot(v, v), 1e-20));
}
float4 main(Input i) : SV_TARGET
{
    const float Pi = 3.141592653589793;
    bool imported = Scene.Material.Specular.g > 0.5;
    float metal = Factors.x >= 0 ? Factors.x : (imported ? Scene.Material.Specular.r : 0);
    float rough = max(Factors.y >= 0 ? Factors.y : (imported ? Scene.Material.Power : 0.5), 0.045);
    float uv = Factors.z >= 0 ? Factors.z : (imported ? Scene.Material.Specular.b : 0);
    float3 base = ToLinear(BaseTexture.Sample(BaseSampler, uv > 0.5 ? i.Uv.zw : i.Uv.xy).rgb) * i.Diffuse.rgb;
    float3 n = SafeUnit(i.Normal);
    float3 v = AmbientOrtho.w > 0.5 ? float3(0, 0, -1) : SafeUnit(-i.Position);
    float nv = max(dot(n, v), 1e-5);
    DX_D3D11_CONST_LIGHT light = Scene.Light[0];
    float3 delta = light.Position - i.Position;
    float distance2 = dot(delta, delta);
    float distance = sqrt(max(distance2, 1e-20));
    float3 l = light.Type == 3 ? SafeUnit(-light.Direction) : delta / distance;
    float attenuation = 1;
    if (light.Type != 3)
    {
        attenuation = distance2 <= light.RangePow2 ? rcp(max(light.Attenuation0 + light.Attenuation1 * distance + light.Attenuation2 * distance2, 1e-5)) : 0;
        if (light.Type == 2)
            attenuation *= saturate((dot(-l, SafeUnit(light.Direction)) - light.SpotParam0) * light.SpotParam1);
    }
    float3 h = SafeUnit(v + l);
    float nl = saturate(dot(n, l));
    float nh = saturate(dot(n, h));
    float vh = saturate(dot(v, h));
    float a = rough * rough;
    float a2 = a * a;
    float denominator = (nh * a2 - nh) * nh + 1;
    float distribution = a2 / max(Pi * denominator * denominator, 1e-12);
    float visibility = 0.5 / max(nl * sqrt(nv * nv * (1 - a2) + a2) + nv * sqrt(nl * nl * (1 - a2) + a2), 1e-6);
    float3 f0 = lerp(0.04.xxx, base, metal);
    float3 fresnel = f0 + (1 - f0) * pow(1 - vh, 5);
    float3 brdf = (1 - fresnel) * (1 - metal) * base / Pi + distribution * visibility * fresnel;
    float3 color = brdf * light.Diffuse * (nl * attenuation) + AmbientOrtho.rgb * base * (1 - metal);
    return float4(ToSrgb(color), 1);
}
