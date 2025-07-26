SamplerState SS_Linear : register( s0 );
Texture2D   TX_Texture0 : register( t0 );

cbuffer GammaCorrectConstantBuffer : register( b0 )
{
    float G_Gamma;
    float G_Brightness;
    float2 G_TextureSize;
    float G_SharpenStrength;
    float G_blendAmount;
    float2 G_pad1;
};

struct PS_INPUT
{
    float2 vTexcoord : TEXCOORD0;
    float3 vEyeRay   : TEXCOORD1;
    float4 vPosition : SV_POSITION;
};

// ---------- Blur & Flash effect ----------
float4 StrongBlurSample(Texture2D tx, float2 uv)
{
    float2 texelSize = 1.0 / G_TextureSize;
    float4 sum = float4(0,0,0,0);
    int count = 0;

    // 7x7 blur kernel, z odleg³oœci¹ *2 (bardzo rozmazane)
    for(int y = -4; y <= 4; ++y)
    {
        for(int x = -4; x <= 4; ++x)
        {
            float2 offset = float2(x, y) * texelSize * 2.0;
            sum += tx.Sample(SS_Linear, uv + offset);
            count++;
        }
    }
    return sum / count;
}

// ---------- Default sharpen (opcjonalnie) ----------
float4 SharpenSample(Texture2D tx, float2 uv, float strength = 0.8f)
{
    float3 mask = 0.0f;
    float4 sample = tx.Sample(SS_Linear, uv);

    const int N = 3;
    for(int y = 0; y < N; ++y)
    {
        for(int x = 0; x < N; ++x)
        {
            float2 offset = float2(x - N/2, y - N/2) * (1.0f / G_TextureSize);
            mask += tx.Sample(SS_Linear, uv + offset).rgb;
        }
    }

    mask /= (N*N);
    mask = max(0, sample.rgb - mask);
    return sample + float4(mask * strength, 0);
}

// ---------- Pixel Shader ----------
float4 PSMain( PS_INPUT Input ) : SV_TARGET
{
    // Domyœlny efekt (normal)
    float4 normalColor = TX_Texture0.Sample(SS_Linear, Input.vTexcoord);
    normalColor.rgb = pow(normalColor.rgb * G_Brightness, G_Gamma);

    // Flashbang/blur efekt
    float4 blurredColor = StrongBlurSample(TX_Texture0, Input.vTexcoord);
    blurredColor.rgb *= 1.35;
    blurredColor.rgb += 0.15;
    blurredColor.rgb = pow(blurredColor.rgb * G_Brightness, G_Gamma);

    // Blendujemy p³ynnie oba efekty
    float3 finalColor = lerp(normalColor.rgb, blurredColor.rgb, G_blendAmount);

    return float4(saturate(finalColor), normalColor.a);
}
