#pragma once

#include "CoreMinimal.h"
#include <d3d11.h>

class ENGINE_API FRenderDevice
{
public:
    FRenderDevice() = default;
    ~FRenderDevice()
    {
        Release();
    }

    // D3D11 디바이스, 스왑체인, 백버퍼/씬 렌더 타깃을 생성한다.
    bool Initialize(HWND InHwnd, int32 Width, int32 Height)
    {
        Hwnd = InHwnd;
        if (!CreateDeviceAndSwapChain(InHwnd, Width, Height))
        {
            return false;
        }

        if (!CreateSwapChainRenderTargets(Width, Height))
        {
            return false;
        }

        if (!CreateSceneRenderTargets(Width, Height))
        {
            return false;
        }

        Viewport.TopLeftX = 0.0f;
        Viewport.TopLeftY = 0.0f;
        Viewport.Width = static_cast<float>(Width);
        Viewport.Height = static_cast<float>(Height);
        Viewport.MinDepth = 0.0f;
        Viewport.MaxDepth = 1.0f;
        return true;
    }

    // 씬 렌더 타깃과 깊이 버퍼를 비우고 출력 머저에 바인딩한다.
    void BeginFrame(const float ClearColor[4])
    {
        ClearSceneTargets(ClearColor);
        BindSceneTargets();
    }

    // 스왑체인을 Present하고 가려짐 상태를 갱신한다.
    void EndFrame()
    {
        if (!SwapChain)
        {
            return;
        }

        const UINT SyncInterval = bVSyncEnabled ? 1u : 0u;
        const HRESULT Hr = SwapChain->Present(SyncInterval, 0);
        if (Hr == DXGI_STATUS_OCCLUDED)
        {
            bSwapChainOccluded = true;
        }
    }

    // 스왑체인 백버퍼 RTV/DSV와 기본 뷰포트를 출력 머저에 바인딩한다.
    void BindBackBuffer()
    {
        if (BackBufferRTV && DeviceContext)
        {
            DeviceContext->OMSetRenderTargets(1, &BackBufferRTV, BackBufferDSV);
            DeviceContext->RSSetViewports(1, &Viewport);
        }
    }

    // 씬 컬러 RTV/DSV와 기본 뷰포트를 출력 머저에 바인딩한다.
    void BindSceneTargets()
    {
        if (SceneColorRTV && DeviceContext)
        {
            DeviceContext->OMSetRenderTargets(1, &SceneColorRTV, SceneDepthDSV);
            DeviceContext->RSSetViewports(1, &Viewport);
        }
    }

    // 백버퍼 RTV를 비우고, 필요한 경우 깊이 버퍼도 함께 초기화한다.
    void ClearBackBuffer(const float ClearColor[4])
    {
        if (BackBufferRTV && DeviceContext)
        {
            DeviceContext->ClearRenderTargetView(BackBufferRTV, ClearColor);
        }
        if (BackBufferDSV && DeviceContext)
        {
            DeviceContext->ClearDepthStencilView(BackBufferDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
        }
    }

    // 씬 컬러 RTV와 씬 깊이 버퍼를 초기화한다.
    void ClearSceneTargets(const float ClearColor[4], float Depth = 1.0f, uint8 Stencil = 0)
    {
        if (SceneColorRTV && DeviceContext)
        {
            DeviceContext->ClearRenderTargetView(SceneColorRTV, ClearColor);
        }
        if (SceneDepthDSV && DeviceContext)
        {
            DeviceContext->ClearDepthStencilView(SceneDepthDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, Depth, Stencil);
        }
    }

    // 현재 스왑체인이 가려진 상태인지 검사한다.
    bool IsOccluded()
    {
        if (bSwapChainOccluded && SwapChain && SwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
        {
            return true;
        }

        bSwapChainOccluded = false;
        return false;
    }

    // 리사이즈 후 스왑체인/씬 렌더 타깃을 새 크기에 맞게 다시 만든다.
    void OnResize(int32 Width, int32 Height)
    {
        if (Width <= 0 || Height <= 0 || !SwapChain || !DeviceContext)
        {
            return;
        }

        DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

        ReleaseSceneRenderTargets();
        ReleaseSwapChainRenderTargets();

        SwapChain->ResizeBuffers(0, Width, Height, DXGI_FORMAT_UNKNOWN, 0);
        if (CreateSwapChainRenderTargets(Width, Height) && CreateSceneRenderTargets(Width, Height))
        {
            Viewport.Width = static_cast<float>(Width);
            Viewport.Height = static_cast<float>(Height);
        }
    }

    // 디바이스가 소유한 스왑체인과 모든 렌더 타깃 자원을 해제한다.
    void Release()
    {
        ReleaseSceneRenderTargets();
        ReleaseSwapChainRenderTargets();

        if (SwapChain)
        {
            SwapChain->Release();
            SwapChain = nullptr;
        }
        if (DeviceContext)
        {
            DeviceContext->Release();
            DeviceContext = nullptr;
        }
        if (Device)
        {
            Device->Release();
            Device = nullptr;
        }
    }

    // Present 시 VSync 사용 여부를 설정한다.
    void SetVSync(bool bEnable) { bVSyncEnabled = bEnable; }
    // 현재 VSync 설정 상태를 반환한다.
    bool IsVSyncEnabled() const { return bVSyncEnabled; }

    // D3D11 디바이스 접근자다.
    ID3D11Device* GetDevice() const { return Device; }
    // D3D11 디바이스 컨텍스트 접근자다.
    ID3D11DeviceContext* GetDeviceContext() const { return DeviceContext; }
    // 스왑체인 접근자다.
    IDXGISwapChain* GetSwapChain() const { return SwapChain; }

    // 최종 출력용 백버퍼 RTV를 반환한다.
    ID3D11RenderTargetView* GetBackBufferRTV() const { return BackBufferRTV; }
    // 최종 출력용 백버퍼 DSV를 반환한다.
    ID3D11DepthStencilView* GetBackBufferDSV() const { return BackBufferDSV; }

    // 씬 컬러 RTV를 반환한다.
    ID3D11RenderTargetView* GetSceneColorRTV() const { return SceneColorRTV; }
    // 씬 컬러 SRV를 반환한다.
    ID3D11ShaderResourceView* GetSceneColorSRV() const { return SceneColorSRV; }

    // 씬 깊이 DSV를 반환한다.
    ID3D11DepthStencilView* GetSceneDepthDSV() const { return SceneDepthDSV; }
    // 씬 깊이 SRV를 반환한다.
    ID3D11ShaderResourceView* GetSceneDepthSRV() const { return SceneDepthSRV; }

    // 렌더 대상 윈도우 핸들을 반환한다.
    HWND GetHwnd() const { return Hwnd; }
    // 백버퍼 전체를 덮는 기본 뷰포트를 반환한다.
    const D3D11_VIEWPORT& GetViewport() const { return Viewport; }

private:
    // D3D11 디바이스, 컨텍스트, 스왑체인을 생성한다.
    bool CreateDeviceAndSwapChain(HWND InHwnd, int32 Width, int32 Height)
    {
        DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};
        SwapChainDesc.BufferDesc.Width = static_cast<UINT>(Width);
        SwapChainDesc.BufferDesc.Height = static_cast<UINT>(Height);
        SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        SwapChainDesc.SampleDesc.Count = 1;
        SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        SwapChainDesc.BufferCount = 2;
        SwapChainDesc.OutputWindow = InHwnd;
        SwapChainDesc.Windowed = TRUE;
        SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

        UINT CreateDeviceFlags = 0;
#ifdef _DEBUG
        CreateDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        D3D_FEATURE_LEVEL FeatureLevel = D3D_FEATURE_LEVEL_11_0;
        const HRESULT Hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            CreateDeviceFlags,
            &FeatureLevel,
            1,
            D3D11_SDK_VERSION,
            &SwapChainDesc,
            &SwapChain,
            &Device,
            nullptr,
            &DeviceContext);

        if (FAILED(Hr))
        {
            MessageBox(nullptr, L"D3D11CreateDeviceAndSwapChain Failed.", nullptr, 0);
            return false;
        }

        return true;
    }

    // 스왑체인 백버퍼 RTV와 대응하는 깊이 스텐실 뷰를 생성한다.
    bool CreateSwapChainRenderTargets(int32 Width, int32 Height)
    {
        if (!Device || !SwapChain)
        {
            return false;
        }

        HRESULT Hr = SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&BackBufferTexture));
        if (FAILED(Hr) || !BackBufferTexture)
        {
            return false;
        }

        Hr = Device->CreateRenderTargetView(BackBufferTexture, nullptr, &BackBufferRTV);
        if (FAILED(Hr))
        {
            return false;
        }

        D3D11_TEXTURE2D_DESC DepthDesc = {};
        DepthDesc.Width = static_cast<UINT>(Width);
        DepthDesc.Height = static_cast<UINT>(Height);
        DepthDesc.MipLevels = 1;
        DepthDesc.ArraySize = 1;
        DepthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        DepthDesc.SampleDesc.Count = 1;
        DepthDesc.Usage = D3D11_USAGE_DEFAULT;
        DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

        Hr = Device->CreateTexture2D(&DepthDesc, nullptr, &BackBufferDepthTexture);
        if (FAILED(Hr) || !BackBufferDepthTexture)
        {
            return false;
        }

        Hr = Device->CreateDepthStencilView(BackBufferDepthTexture, nullptr, &BackBufferDSV);
        return SUCCEEDED(Hr);
    }

    // 씬 렌더링에 사용하는 컬러 RTV/SRV와 깊이 DSV/SRV를 생성한다.
    bool CreateSceneRenderTargets(int32 Width, int32 Height)
    {
        if (!Device)
        {
            return false;
        }

        D3D11_TEXTURE2D_DESC ColorDesc = {};
        ColorDesc.Width = static_cast<UINT>(Width);
        ColorDesc.Height = static_cast<UINT>(Height);
        ColorDesc.MipLevels = 1;
        ColorDesc.ArraySize = 1;
        ColorDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        ColorDesc.SampleDesc.Count = 1;
        ColorDesc.Usage = D3D11_USAGE_DEFAULT;
        ColorDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        HRESULT Hr = Device->CreateTexture2D(&ColorDesc, nullptr, &SceneColorTexture);
        if (FAILED(Hr) || !SceneColorTexture)
        {
            return false;
        }

        Hr = Device->CreateRenderTargetView(SceneColorTexture, nullptr, &SceneColorRTV);
        if (FAILED(Hr))
        {
            return false;
        }

        Hr = Device->CreateShaderResourceView(SceneColorTexture, nullptr, &SceneColorSRV);
        if (FAILED(Hr))
        {
            return false;
        }

        D3D11_TEXTURE2D_DESC DepthDesc = {};
        DepthDesc.Width = static_cast<UINT>(Width);
        DepthDesc.Height = static_cast<UINT>(Height);
        DepthDesc.MipLevels = 1;
        DepthDesc.ArraySize = 1;
        DepthDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
        DepthDesc.SampleDesc.Count = 1;
        DepthDesc.Usage = D3D11_USAGE_DEFAULT;
        DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

        Hr = Device->CreateTexture2D(&DepthDesc, nullptr, &SceneDepthTexture);
        if (FAILED(Hr) || !SceneDepthTexture)
        {
            return false;
        }

        D3D11_DEPTH_STENCIL_VIEW_DESC DepthStencilViewDesc = {};
        DepthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        DepthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        DepthStencilViewDesc.Texture2D.MipSlice = 0;

        Hr = Device->CreateDepthStencilView(SceneDepthTexture, &DepthStencilViewDesc, &SceneDepthDSV);
        if (FAILED(Hr))
        {
            return false;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC DepthSRVDesc = {};
        DepthSRVDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        DepthSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        DepthSRVDesc.Texture2D.MostDetailedMip = 0;
        DepthSRVDesc.Texture2D.MipLevels = 1;

        Hr = Device->CreateShaderResourceView(SceneDepthTexture, &DepthSRVDesc, &SceneDepthSRV);
        return SUCCEEDED(Hr);
    }

    // 스왑체인 백버퍼에 연결된 RTV/DSV 자원을 해제한다.
    void ReleaseSwapChainRenderTargets()
    {
        if (BackBufferDSV)
        {
            BackBufferDSV->Release();
            BackBufferDSV = nullptr;
        }
        if (BackBufferDepthTexture)
        {
            BackBufferDepthTexture->Release();
            BackBufferDepthTexture = nullptr;
        }
        if (BackBufferRTV)
        {
            BackBufferRTV->Release();
            BackBufferRTV = nullptr;
        }
        if (BackBufferTexture)
        {
            BackBufferTexture->Release();
            BackBufferTexture = nullptr;
        }
    }

    // 씬 렌더링에 사용하는 RTV/SRV/DSV 자원을 해제한다.
    void ReleaseSceneRenderTargets()
    {
        if (SceneDepthSRV)
        {
            SceneDepthSRV->Release();
            SceneDepthSRV = nullptr;
        }
        if (SceneDepthDSV)
        {
            SceneDepthDSV->Release();
            SceneDepthDSV = nullptr;
        }
        if (SceneDepthTexture)
        {
            SceneDepthTexture->Release();
            SceneDepthTexture = nullptr;
        }
        if (SceneColorSRV)
        {
            SceneColorSRV->Release();
            SceneColorSRV = nullptr;
        }
        if (SceneColorRTV)
        {
            SceneColorRTV->Release();
            SceneColorRTV = nullptr;
        }
        if (SceneColorTexture)
        {
            SceneColorTexture->Release();
            SceneColorTexture = nullptr;
        }
    }

private:
    HWND Hwnd = nullptr;
    ID3D11Device* Device = nullptr;
    ID3D11DeviceContext* DeviceContext = nullptr;
    IDXGISwapChain* SwapChain = nullptr;
    D3D11_VIEWPORT Viewport = {};
    bool bSwapChainOccluded = false;
    bool bVSyncEnabled = false;

    // 최종 출력용 백버퍼 자원이다.
    ID3D11Texture2D* BackBufferTexture = nullptr;
    ID3D11RenderTargetView* BackBufferRTV = nullptr;
    ID3D11Texture2D* BackBufferDepthTexture = nullptr;
    ID3D11DepthStencilView* BackBufferDSV = nullptr;

    // 씬 중간 렌더링용 컬러 자원이다.
    ID3D11Texture2D* SceneColorTexture = nullptr;
    ID3D11RenderTargetView* SceneColorRTV = nullptr;
    ID3D11ShaderResourceView* SceneColorSRV = nullptr;

    // 씬 중간 렌더링용 깊이 자원이다.
    ID3D11Texture2D* SceneDepthTexture = nullptr;
    ID3D11DepthStencilView* SceneDepthDSV = nullptr;
    ID3D11ShaderResourceView* SceneDepthSRV = nullptr;
};
