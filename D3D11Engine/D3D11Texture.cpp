#define NOMINMAX
#include "pch.h"
#include "D3D11Texture.h"
#include "Engine.h"
#include "D3D11GraphicsEngineBase.h"
#include "GothicAPI.h"
#include <DDSTextureLoader.h>
#include "RenderToTextureBuffer.h"
#include "D3D11_Helpers.h"

#include <windows.h>
#include <iostream>
#include <vector>

static HANDLE hMapFile = nullptr;
static RECT* pBuf = nullptr;

D3D11Texture::D3D11Texture() : lastData() {

    if (!hMapFile) {
        hMapFile = CreateFileMappingA(
            INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
            0, 4096, "Global\\CefDirtyRects");
        if (!hMapFile)
            return;
        pBuf = (RECT*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 4096);
    }

}

D3D11Texture::~D3D11Texture() {
    Thumbnail.Reset();
    Texture.Reset();
    ShaderResourceView.Reset();
}

/** Initializes the texture object */
XRESULT D3D11Texture::Init( INT2 size, ETextureFormat format, UINT mipMapCount, void* data, const std::string& fileName ) {
    
    HRESULT hr;
    D3D11GraphicsEngineBase* engine = reinterpret_cast<D3D11GraphicsEngineBase*>(Engine::GraphicsEngine);

    TextureFormat = static_cast<DXGI_FORMAT>(format);
    TextureSize = size;
    MipMapCount = mipMapCount;

    CD3D11_TEXTURE2D_DESC textureDesc(
        static_cast<DXGI_FORMAT>(format),
        size.x,
        size.y,
        1,
        mipMapCount,
        D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, 1, 0, 0 );

    LE( engine->GetDevice()->CreateTexture2D( &textureDesc, nullptr, Texture.ReleaseAndGetAddressOf() ) );
    SetDebugName( Texture.Get(), "D3D11Texture(\"" + fileName + "\")->Texture" );

    D3D11_SHADER_RESOURCE_VIEW_DESC descRV = {};
    descRV.Format = DXGI_FORMAT_UNKNOWN;
    descRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    descRV.Texture2D.MipLevels = mipMapCount;
    descRV.Texture2D.MostDetailedMip = 0;
    LE( engine->GetDevice()->CreateShaderResourceView( Texture.Get(), &descRV, ShaderResourceView.ReleaseAndGetAddressOf() ) );
    SetDebugName( ShaderResourceView.Get(), "D3D11Texture(\"" + fileName + "\")->ShaderResourceView" );

    if (size.x = 8192 && size.y == 8192) {
        lastData.resize(size.x * size.y * 4, 0);
    }

    return XR_SUCCESS;
}

/** Initializes the texture from a file */
XRESULT D3D11Texture::Init( const std::string& file ) {
    HRESULT hr;
    D3D11GraphicsEngineBase* engine = reinterpret_cast<D3D11GraphicsEngineBase*>(Engine::GraphicsEngine);

    //LogInfo() << "Loading Engine-Texture: " << file;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> res;
    LE( CreateDDSTextureFromFile( engine->GetDevice().Get(), Toolbox::ToWideChar( file.c_str() ).c_str(),
        reinterpret_cast<ID3D11Resource**>(res.ReleaseAndGetAddressOf()), ShaderResourceView.GetAddressOf() ) );

    if ( !ShaderResourceView.Get() || !res.Get() )
        return XR_FAILED;

    D3D11_TEXTURE2D_DESC desc;
    res->GetDesc( &desc );

    Texture = res;
    TextureFormat = desc.Format;

    TextureSize.x = desc.Width;
    TextureSize.y = desc.Height;
    SetDebugName( res.Get(), "D3D11Texture(\"" + file + "\")->Texture" );
    SetDebugName( ShaderResourceView.Get(), "D3D11Texture(\"" + file + "\")->ShaderResourceView" );

    return XR_SUCCESS;
}

/** Updates the Texture-Object */
XRESULT D3D11Texture::UpdateData( void* data, int mip ) {
    D3D11GraphicsEngineBase* engine = reinterpret_cast<D3D11GraphicsEngineBase*>(Engine::GraphicsEngine);

    UINT TextureWidth = (TextureSize.x >> mip);
    UINT TextureHeight = (TextureSize.y >> mip);

    if (TextureWidth == 8192 && TextureHeight == 8192) {
        size_t texBytes = TextureWidth * TextureHeight * 4;
        if (lastTextureBuffer.size() != texBytes)
            lastTextureBuffer.resize(texBytes, 0);

        std::vector<RECT> dirtyRects;
        if (pBuf) {
            int rectCount = pBuf[0].left;
            if (rectCount > 0 && rectCount < 512) { // sanity check
                for (int i = 0; i < rectCount; ++i)
                    dirtyRects.push_back(pBuf[i + 1]);
            }
        }

        if (!dirtyRects.empty()) {
            auto* src = static_cast<uint8_t*>(data);
            for (const RECT& rect : dirtyRects) {
                int x0 = 0 < rect.left ? rect.left : 0;
                int y0 = 0 < rect.top ? rect.top : 0;
                int x1 = rect.right < (int)TextureWidth ? rect.right : (int)TextureWidth;
                int y1 = rect.bottom < (int)TextureHeight ? rect.bottom : (int)TextureHeight;
                int rowBytes = (x1 - x0) * 4;
                if (rowBytes <= 0) continue;

                for (int y = y0; y < y1; ++y) {
                    memcpy(
                        lastTextureBuffer.data() + y * TextureWidth * 4 + x0 * 4,
                        src + y * TextureWidth * 4 + x0 * 4,
                        rowBytes
                    );
                }
            }
        } else {
            memcpy(lastTextureBuffer.data(), data, texBytes);
        }

        auto ctx = engine->GetContext();
        for (const RECT& rect : dirtyRects) {
            int x0 = rect.left < 0 ? 0 : rect.left;
            int y0 = rect.top < 0 ? 0 : rect.top;
            int x1 = rect.right > 8192 ? 8192 : rect.right;
            int y1 = rect.bottom > 8192 ? 8192 : rect.bottom;

            int width = x1 - x0;
            int height = y1 - y0;
            int rowBytes = width * 4;

            if (width <= 0 || height <= 0) continue;

            std::vector<uint8_t> packedRect(rowBytes * height);
            auto* src = static_cast<uint8_t*>(data);
            for (int y = 0; y < height; ++y) {
                memcpy(
                    packedRect.data() + y * rowBytes,
                    src + (y0 + y) * 8192 * 4 + x0 * 4,
                    rowBytes
                );
            }

            D3D11_BOX box;
            box.left   = x0;
            box.top    = y0;
            box.right  = x1;
            box.bottom = y1;
            box.front  = 0;
            box.back   = 1;

            ctx->UpdateSubresource(
                Texture.Get(), 0, &box, packedRect.data(), rowBytes, 0
            );
        }

        return XR_SUCCESS;
    }

    Microsoft::WRL::ComPtr<ID3D11Texture2D> stagingTexture;
    D3D11_TEXTURE2D_DESC stagingTextureDesc;
    Texture.Get()->GetDesc( &stagingTextureDesc );
    stagingTextureDesc.Width = TextureWidth;
    stagingTextureDesc.Height = TextureHeight;
    stagingTextureDesc.MipLevels = 1;
    stagingTextureDesc.BindFlags = 0;
    stagingTextureDesc.MiscFlags = 0;
    stagingTextureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    stagingTextureDesc.Usage = D3D11_USAGE_STAGING;

    D3D11_SUBRESOURCE_DATA stagingTextureData;
    stagingTextureData.pSysMem = data;
    stagingTextureData.SysMemPitch = GetRowPitchBytes( mip );
    stagingTextureData.SysMemSlicePitch = 0;

    HRESULT result = engine->GetDevice()->CreateTexture2D( &stagingTextureDesc, &stagingTextureData, stagingTexture.ReleaseAndGetAddressOf() );
    if ( FAILED( result ) )
        return XR_FAILED;

    engine->GetContext()->CopySubresourceRegion( Texture.Get(), mip, 0, 0, 0, stagingTexture.Get(), 0, nullptr );

    return XR_SUCCESS;
}

/** Updates the Texture-Object using the deferred context (For loading in an other thread) */
XRESULT D3D11Texture::UpdateDataDeferred( void* data, int mip ) {
    D3D11GraphicsEngineBase* engine = reinterpret_cast<D3D11GraphicsEngineBase*>(Engine::GraphicsEngine);

    UINT TextureWidth = (TextureSize.x >> mip);
    UINT TextureHeight = (TextureSize.y >> mip);

    ID3D11Texture2D* stagingTexture;
    D3D11_TEXTURE2D_DESC stagingTextureDesc;
    Texture.Get()->GetDesc( &stagingTextureDesc );
    stagingTextureDesc.Width = TextureWidth;
    stagingTextureDesc.Height = TextureHeight;
    stagingTextureDesc.MipLevels = 1;
    stagingTextureDesc.BindFlags = 0;
    stagingTextureDesc.MiscFlags = 0;
    stagingTextureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    stagingTextureDesc.Usage = D3D11_USAGE_STAGING;

    D3D11_SUBRESOURCE_DATA stagingTextureData;
    stagingTextureData.pSysMem = data;
    stagingTextureData.SysMemPitch = GetRowPitchBytes( mip );
    stagingTextureData.SysMemSlicePitch = 0;

    HRESULT result = engine->GetDevice()->CreateTexture2D( &stagingTextureDesc, &stagingTextureData, &stagingTexture );
    if ( FAILED( result ) )
        return XR_FAILED;

    Engine::GAPI->AddStagingTexture( mip, stagingTexture, Texture.Get() );
    return XR_SUCCESS;
}

/** Returns the RowPitch-Bytes */
UINT D3D11Texture::GetRowPitchBytes( int mip ) {
    int px = (TextureSize.x >> mip);
    //int py = (TextureSize.y >> mip);

    if ( TextureFormat == DXGI_FORMAT_R8_UNORM ) {
        return px;
    } else if ( TextureFormat == DXGI_FORMAT_BC1_UNORM || TextureFormat == DXGI_FORMAT_BC2_UNORM ||
        TextureFormat == DXGI_FORMAT_BC3_UNORM ) {
        return Toolbox::GetDDSRowPitchSize( px, TextureFormat == DXGI_FORMAT_BC1_UNORM );
    } else { // Use B8G8R8A8
        return px * 4;
    }
}

/** Returns the size of the texture in bytes */
UINT D3D11Texture::GetSizeInBytes( int mip ) {
    int px = (TextureSize.x >> mip);
    int py = (TextureSize.y >> mip);

    if ( TextureFormat == DXGI_FORMAT_R8_UNORM ) {
        return px * py;
    } else if ( TextureFormat == DXGI_FORMAT_BC1_UNORM || TextureFormat == DXGI_FORMAT_BC2_UNORM ||
        TextureFormat == DXGI_FORMAT_BC3_UNORM ) {
        return Toolbox::GetDDSStorageRequirements( px, py, TextureFormat == DXGI_FORMAT_BC1_UNORM );
    } else { // Use B8G8R8A8
        return px * py * 4;
    }
}
static std::vector<uint8_t> fakeData(1024 * 1024 * 4, 255);
/** Binds this texture to a pixelshader */
XRESULT D3D11Texture::BindToPixelShader( int slot ) {
    reinterpret_cast<D3D11GraphicsEngineBase*>(Engine::GraphicsEngine)->GetContext()->PSSetShaderResources( slot, 1, ShaderResourceView.GetAddressOf() );
    return XR_SUCCESS;
}

/** Binds this texture to a pixelshader */
XRESULT D3D11Texture::BindToVertexShader( int slot ) {
    D3D11GraphicsEngineBase* engine = reinterpret_cast<D3D11GraphicsEngineBase*>(Engine::GraphicsEngine);

    engine->GetContext()->VSSetShaderResources( slot, 1, ShaderResourceView.GetAddressOf() );
    engine->GetContext()->DSSetShaderResources( slot, 1, ShaderResourceView.GetAddressOf() );

    return XR_SUCCESS;
}

/** Binds this texture to a domainshader */
XRESULT D3D11Texture::BindToDomainShader( int slot ) {
    reinterpret_cast<D3D11GraphicsEngineBase*>(Engine::GraphicsEngine)->GetContext()->DSSetShaderResources( slot, 1, ShaderResourceView.GetAddressOf() );
    return XR_SUCCESS;
}

/** Creates a thumbnail for this */
XRESULT D3D11Texture::CreateThumbnail() {
    D3D11GraphicsEngineBase* engine = reinterpret_cast<D3D11GraphicsEngineBase*>(Engine::GraphicsEngine);
    HRESULT hr;
    // Since D2D can't load DXTn-Textures on Windows 7, we copy it over to a smaller texture here in d3d11

    // Create texture
    CD3D11_TEXTURE2D_DESC textureDesc(
        DXGI_FORMAT_B8G8R8A8_UNORM,
        256,
        256,
        1,
        1,
        D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, 1, 0, 0 );

    LE( engine->GetDevice()->CreateTexture2D( &textureDesc, nullptr, Thumbnail.ReleaseAndGetAddressOf() ) );

    // Create temporary RTV
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> tempRTV;
    LE( engine->GetDevice()->CreateRenderTargetView( Thumbnail.Get(), nullptr, tempRTV.GetAddressOf() ) );
    if ( !tempRTV.Get() )
        return XR_FAILED;

    engine->GetContext()->ClearRenderTargetView( tempRTV.Get(), reinterpret_cast<float*>(&float4( 1, 0, 0, 1 )) );

    // Copy main texture to it
    engine->GetContext()->PSSetShaderResources( 0, 1, ShaderResourceView.GetAddressOf() );

    ID3D11RenderTargetView* oldRTV[2];
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> oldDSV;

    engine->GetContext()->OMGetRenderTargets( 2, oldRTV, oldDSV.GetAddressOf() );

    engine->GetContext()->OMSetRenderTargets( 1, tempRTV.GetAddressOf(), nullptr );
    engine->DrawQuad( INT2( 0, 0 ), INT2( 256, 256 ) );

    engine->GetContext()->OMSetRenderTargets( 2, oldRTV, oldDSV.Get() );

    return XR_SUCCESS;
}

/** Returns the thumbnail of this texture. If this returns nullptr, you need to create one first */
const Microsoft::WRL::ComPtr<ID3D11Texture2D>& D3D11Texture::GetThumbnail() {
    return Thumbnail;
}

/** Generates mipmaps for this texture (may be slow!) */
XRESULT D3D11Texture::GenerateMipMaps() {
    if ( MipMapCount == 1 )
        return XR_SUCCESS;

    D3D11GraphicsEngineBase* engine = reinterpret_cast<D3D11GraphicsEngineBase*>(Engine::GraphicsEngine);

    std::unique_ptr<RenderToTextureBuffer> b = std::make_unique<RenderToTextureBuffer>( engine->GetDevice().Get(), TextureSize.x, TextureSize.y,
        DXGI_FORMAT_B8G8R8A8_UNORM, nullptr, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, MipMapCount );

    // Copy the main image
    engine->GetContext()->CopySubresourceRegion( b->GetTexture().Get(), 0, 0, 0, 0, Texture.Get(), 0, nullptr );

    // Generate mips
    engine->GetContext()->GenerateMips( b->GetShaderResView().Get() );

    // Copy the full chain back
    engine->GetContext()->CopyResource( Texture.Get(), b->GetTexture().Get() );

    return XR_SUCCESS;
}

XRESULT D3D11Texture::GenerateMipMapsDeferred() {
    if ( MipMapCount == 1 )
        return XR_SUCCESS;

    Engine::GAPI->AddMipMapGeneration( this );

    return XR_SUCCESS;
}
