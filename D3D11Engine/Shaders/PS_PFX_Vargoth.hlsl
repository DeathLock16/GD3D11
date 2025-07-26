//--------------------------------------------------------------------------------------
// World/VOB-Pixelshader for G2D3D11 by Degenerated - COLD/DARK VERSION
//--------------------------------------------------------------------------------------

SamplerState SS_Linear : register( s0 );
SamplerState SS_samMirror : register( s1 );
Texture2D	TX_Texture0 : register( t0 );
Texture2D	TX_Depth : register( t1 );

cbuffer GammaCorrectConstantBuffer : register( b0 )
{
	float G_Gamma;
	float G_Brightness;
	float2 G_TextureSize;
	float G_SharpenStrength;
	float3 G_pad1;
}

struct PS_INPUT
{
	float2 vTexcoord		: TEXCOORD0;
	float3 vEyeRay			: TEXCOORD1;
	float4 vPosition		: SV_POSITION;
};

// -- Mo¿esz zachowaæ funkcjê SharpenSample jeœli chcesz ostrzyæ obraz (niekoniecznie potrzebna do cold/dark efektu)

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PSMain( PS_INPUT Input ) : SV_TARGET
{
    float4 color = TX_Texture0.Sample(SS_Linear, Input.vTexcoord);

    // Gamma/brightness
    color.rgb = pow(color.rgb * G_Brightness, G_Gamma);

    // --- DESATURACJA, "MARTWE KOLORY" ---
    float gray = dot(color.rgb, float3(0.299, 0.587, 0.114)); // klasyczna luminancja
    float desat = 0.8; // 0 = orygina³, 1 = czarno-bia³e; próbuj wartoœci 0.7 - 0.9
    color.rgb = lerp(color.rgb, gray.xxx, desat);

    // Delikatny zimny tint (opcjonalnie, mo¿esz usun¹æ jeœli nie chcesz)
    color.b += 0.06;

    // Przyciemnienie, jeœli chcesz bardziej mrocznie:
    color.rgb *= 0.80;

    color.rgb = saturate(color.rgb);

    return color;
}
