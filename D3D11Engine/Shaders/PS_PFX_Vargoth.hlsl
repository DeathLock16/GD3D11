SamplerState SS_Linear : register( s0 );
SamplerState SS_samMirror : register( s1 );
Texture2D   TX_Texture0 : register( t0 );
Texture2D   TX_Depth    : register( t1 );

cbuffer GammaCorrectConstantBuffer : register( b0 )
{
    float G_Gamma;
    float G_Brightness;
    float2 G_TextureSize;
    float G_SharpenStrength;
    float G_blendAmount;
    float2 G_pad1;
}

struct PS_INPUT
{
    float2 vTexcoord     : TEXCOORD0;
    float3 vEyeRay       : TEXCOORD1;
    float4 vPosition     : SV_POSITION;
};

float4 PSMain( PS_INPUT Input ) : SV_TARGET
{
    float4 color = TX_Texture0.Sample(SS_Linear, Input.vTexcoord);

    // --------- Wersja "normalna" ---------
    float3 normalColor = pow(color.rgb * G_Brightness, G_Gamma);

    // --------- Wersja "martwa" / cold-dark ---------
    float3 darkColor = normalColor;
    float gray = dot(darkColor, float3(0.299, 0.587, 0.114));
    float desat = 0.8;
    darkColor = lerp(darkColor, gray.xxx, desat);
    darkColor.b += 0.06;
    darkColor *= 0.80;
    darkColor = saturate(darkColor);

    // --------- Blend ---------
    float3 finalColor = lerp(normalColor, darkColor, G_blendAmount);

    return float4(saturate(finalColor), color.a);
}
