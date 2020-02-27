//------------------------------------------------------------------------------
// <copyright file="D3DVizualization.h" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//------------------------------------------------------------------------------

#pragma once

#include <windows.h>
#include <d3d11.h>

#ifdef DIRECTX_SDK  // requires DirectX SDK June 2010
    #include <xnamath.h>
    #define DirectX_NS                 // DirectX SDK requires a blank namespace for several types
#else // Windows SDK
    #include <DirectXMath.h>
    #include <DirectXPackedVector.h>
    #define DirectX_NS DirectX         // Windows SDK requires a DirectX namespace for several types
#endif 

#include "OrbitCamera.h"
#include "DX11Utils.h"
#include "resource.h"

extern "C" {
    __declspec(dllexport) HRESULT __cdecl Init();
}

extern "C" {
    __declspec(dllexport) HRESULT __cdecl Init12(HWND hwnd);
}

extern "C" {
    __declspec(dllexport) void __cdecl Cleanup();
}

extern "C" {
    __declspec(dllexport) HRESULT __cdecl Render(void * pResource, bool isNewSurface);
}

extern "C" {
    __declspec(dllexport) HRESULT __cdecl SetCameraRadius(float r);
}

extern "C" {
    __declspec(dllexport) HRESULT __cdecl SetCameraTheta(float theta);
}

extern "C" {
    __declspec(dllexport) HRESULT __cdecl SetCameraPhi(float phi);
}

class CCube
{

public:
    /// <summary>
    /// Constructor
    /// </summary>
    CCube();

    /// <summary>
    /// Destructor
    /// </summary>
    ~CCube();

    /// <summary>
    /// Create Direct3D device and swap chain
    /// </summary>
    /// <returns>S_OK for success, or failure code</returns>
    virtual HRESULT                             InitDevice();

    /// <summary>
    /// Renders a frame
    /// </summary>
    /// <returns>S_OK for success, or failure code</returns>
    virtual HRESULT                             Render(void * pResource, bool isNewSurface);


    /// <summary>
    /// Method for retrieving the camera
    /// </summary>
    /// <returns>Pointer to the camera</returns>
    CCamera*                            GetCamera();

    // Special function definitions to ensure alignment between c# and c++ 
    void* operator new(size_t size)
    {
        return _aligned_malloc(size, 16);
    }

    void operator delete(void *p)
    {
        _aligned_free(p);
    }

private:

    HRESULT InitRenderTarget(void * pResource);
    void SetUpViewport();

    // 3d camera
    CCamera                             m_camera;

    HINSTANCE                           m_hInst;
    D3D_DRIVER_TYPE                     m_driverType;
    D3D_FEATURE_LEVEL                   m_featureLevel;
    
    ID3D11Device*                       m_pd3dDevice;
    ID3D11DeviceContext*                m_pImmediateContext;
    IDXGISwapChain*                     m_pSwapChain = NULL;
    ID3D11RenderTargetView*             m_pRenderTargetView = NULL;
    ID3D11InputLayout*                  m_pVertexLayout;
    ID3D11Buffer*                       m_pVertexBuffer;
    ID3D11Buffer*                       m_pIndexBuffer = NULL;
    ID3D11Buffer*                       m_pConstantBuffer = NULL;

    DirectX_NS::XMMATRIX                 m_World;
    DirectX_NS::XMMATRIX                 m_View;
    DirectX_NS::XMMATRIX                 m_Projection;
   
    ID3D11VertexShader*                 m_pVertexShader;
    ID3D11PixelShader*                  m_pPixelShader;

    // Initial window resolution
    UINT                                 m_Width;
    UINT                                 m_Height;

    /// <summary>
    /// Compile and set layout for shaders
    /// </summary>
    /// <returns>S_OK for success, or failure code</returns>
    HRESULT                             LoadShaders();
};



#include <d3d12.h>
#include <dxgi1_6.h>
#include "d3dx12.h"
#include <wrl.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

using namespace Microsoft::WRL;

#define TryCom(hr) { HRESULT _hr = hr; if(FAILED(_hr)) throw _hr; }

std::unique_ptr<TCHAR[]> HResultToMessage(HRESULT hr)
{
    TCHAR errorMessage[512] = { 0 };
    FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        hr,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        errorMessage,
        sizeof(errorMessage) / sizeof(TCHAR),
        NULL);
    std::unique_ptr<TCHAR[]> dst(new TCHAR[512]);
    wsprintf(dst.get(), TEXT("0x%08X\n\n%s"), hr, errorMessage);
    return dst;
}

class CCube12 : public CCube
{
public:
    const int FrameBufferCount = 2;

    int m_height;
    int m_width;

    ComPtr<IDXGIFactory4> m_factory;
    ComPtr<ID3D12Device> m_device;
    ComPtr<IDXGISwapChain4> m_swapchain;
    ComPtr<ID3D12DescriptorHeap> m_heapRtv;
    ComPtr<ID3D12DescriptorHeap> m_heapDsv;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    UINT m_rtvDescriptorSize = 0;
    std::vector<ComPtr<ID3D12CommandAllocator>> m_commandAllocators;
    std::vector<ComPtr<ID3D12Fence1>> m_frameFences;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;

    CCube12(HWND hwnd) : CCube()
    {
        try
        {
            ComPtr<ID3D12Debug> debug;
            TryCom(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));
            debug->EnableDebugLayer();

            TryCom(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&m_factory)));


            // Adapter列挙
            std::vector<DXGI_ADAPTER_DESC1> adapterDescriptions;
            for (size_t i = 0; ; i++)
            {
                ComPtr<IDXGIAdapter1> adapter;
                if (m_factory->EnumAdapters1(i, &adapter) == DXGI_ERROR_NOT_FOUND)
                {
                    break;
                }
                TryCom(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)); // 生成可能チェック
                DXGI_ADAPTER_DESC1 desc;
                adapter->GetDesc1(&desc);
                adapterDescriptions.push_back(desc);
            }

            // デバイス生成
            std::vector<DXGI_ADAPTER_DESC1>::iterator hardwareAdapterDescriptionIterator = std::find_if(
                adapterDescriptions.begin(), adapterDescriptions.end(),
                [](DXGI_ADAPTER_DESC1& desc) { return desc.Flags ^ DXGI_ADAPTER_FLAG_SOFTWARE; });
            ComPtr<IDXGIAdapter1> hardwareAdapter;
            if (hardwareAdapterDescriptionIterator != adapterDescriptions.end())
            {
                m_factory->EnumAdapterByLuid(hardwareAdapterDescriptionIterator->AdapterLuid, IID_PPV_ARGS(&hardwareAdapter));
            }
            else
            {
                m_factory->EnumWarpAdapter(IID_PPV_ARGS(&hardwareAdapter));
            }
            TryCom(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));

            // コマンドキューの生成
            D3D12_COMMAND_QUEUE_DESC queueDesc{
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                0,
                D3D12_COMMAND_QUEUE_FLAG_NONE,
                0
            };
            TryCom(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

            // スワップチェインの生成
            // HWND からクライアント領域サイズを判定する。
            // (ウィンドウサイズをもらってそれを使用するのもよい)
            RECT rect;
            GetClientRect(hwnd, &rect);
            m_width = rect.right - rect.left;
            m_height = rect.bottom - rect.top;

            // スワップチェインの生成
            {
                DXGI_SWAP_CHAIN_DESC1 scDesc{};
                scDesc.BufferCount = FrameBufferCount;
                scDesc.Width = m_width;
                scDesc.Height = m_height;
                scDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
                scDesc.SampleDesc.Count = 1;
                ComPtr<IDXGISwapChain1> swapchain;
                TryCom(m_factory->CreateSwapChainForHwnd(
                    m_commandQueue.Get(),
                    hwnd,
                    &scDesc,
                    nullptr,
                    nullptr,
                    &swapchain));
                swapchain.As(&m_swapchain); // IDXGISwapChain4 取得
            }

            // RTV のディスクリプタヒープ
            D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{
                D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
                FrameBufferCount,
                D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
                0
            };
            TryCom(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_heapRtv)));
            m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

            // DSV のディスクリプタヒープ
            D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{
                D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
                1,
                D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
                0
            };
            TryCom(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_heapDsv)));

            // コマンドアロケータ
            m_commandAllocators.resize(FrameBufferCount);
            for (UINT i = 0; i < FrameBufferCount; ++i)
            {
                TryCom(m_device->CreateCommandAllocator(
                    D3D12_COMMAND_LIST_TYPE_DIRECT,
                    IID_PPV_ARGS(&m_commandAllocators[i])
                ));
            }

            // フェンス
            m_frameFences.resize(FrameBufferCount);
            for (UINT i = 0; i < FrameBufferCount; ++i)
            {
                TryCom(m_device->CreateFence(
                    0,  // 初期値
                    D3D12_FENCE_FLAG_NONE,
                    IID_PPV_ARGS(&m_frameFences[i])));
            }

            // コマンドリストの生成.
            TryCom(m_device->CreateCommandList(
                0,
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                m_commandAllocators[0].Get(),
                nullptr,
                IID_PPV_ARGS(&m_commandList)
            ));
            m_commandList->Close();
        }
        catch (const HRESULT hr)
        {
            MessageBox(nullptr, HResultToMessage(hr).get(), TEXT("Exception"), MB_OK);
        }
    }

    HRESULT Render(void* pResource, bool  isNewSurface) override
    {
        return CCube::Render(pResource, isNewSurface);
        
        try
        {
            //ComPtr<IDXGIResource> pDXGIResource;
            //TryCom(((IUnknown *)pResource)->QueryInterface(IID_PPV_ARGS(&pDXGIResource)));

            //HANDLE sharedHandle;
            //TryCom(pDXGIResource->GetSharedHandle(&sharedHandle));

            //ComPtr<ID3D12Resource> resource;
            //TryCom(m_device->OpenSharedHandle(sharedHandle, IID_PPV_ARGS(&resource)));

            //CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
            //    m_heapRtv->GetCPUDescriptorHandleForHeapStart());
            //m_device->CreateRenderTargetView(resource.Get(), nullptr, rtvHandle);
            //D3D12_RESOURCE_DESC desc = resource->GetDesc();

            static bool first = true;

            // RT
            if(first)
            {
                std::vector<ComPtr<ID3D12Resource1>> m_renderTargets;
                // スワップチェインイメージへのレンダーターゲットビュー生成
                CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
                    m_heapRtv->GetCPUDescriptorHandleForHeapStart());
                for (UINT i = 0; i < FrameBufferCount; ++i)
                {
                    m_swapchain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i]));
                    m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
                    // 参照するディスクリプタの変更
                    rtvHandle.Offset(1, m_rtvDescriptorSize);
                }
            }

            // DS
            if (first)
            {
                // デプスバッファの生成
                ComPtr<ID3D12Resource1> m_depthBuffer;
                auto depthBufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(
                    DXGI_FORMAT_D32_FLOAT,
                    m_width,
                    m_height,
                    1, 0,
                    1, 0,
                    D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
                );
                D3D12_CLEAR_VALUE depthClearValue{};
                depthClearValue.Format = depthBufferDesc.Format;
                depthClearValue.DepthStencil.Depth = 1.0f;
                depthClearValue.DepthStencil.Stencil = 0;
                TryCom(m_device->CreateCommittedResource(
                    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                    D3D12_HEAP_FLAG_NONE,
                    &depthBufferDesc,
                    D3D12_RESOURCE_STATE_DEPTH_WRITE,
                    &depthClearValue,
                    IID_PPV_ARGS(&m_depthBuffer)
                ));

                // デプスステンシルビュー生成
                D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc
                {
                  DXGI_FORMAT_D32_FLOAT,  // Format
                  D3D12_DSV_DIMENSION_TEXTURE2D,  // ViewDimension
                  D3D12_DSV_FLAG_NONE,    // Flags
                  { // D3D12_TEX2D_DSV
                    0 // MipSlice
                  }
                };
                CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_heapDsv->GetCPUDescriptorHandleForHeapStart());
                m_device->CreateDepthStencilView(m_depthBuffer.Get(), &dsvDesc, dsvHandle);
            }
            first = false;

            // 描画
            {
                int frameIndex = m_swapchain->GetCurrentBackBufferIndex();

                m_commandAllocators[frameIndex]->Reset();
                m_commandList->Reset(
                    m_commandAllocators[frameIndex].Get(),
                    nullptr
                );

                // スワップチェイン表示可能からレンダーターゲット描画可能へ
                auto barrierToRT = CD3DX12_RESOURCE_BARRIER::Transition(
                    m_renderTargets[frameIndex].Get(),
                    D3D12_RESOURCE_STATE_PRESENT,
                    D3D12_RESOURCE_STATE_RENDER_TARGET);
                m_commandList->ResourceBarrier(1, &barrierToRT);

                CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(
                    m_heapRtv->GetCPUDescriptorHandleForHeapStart(),
                    frameIndex, m_rtvDescriptorSize);
                CD3DX12_CPU_DESCRIPTOR_HANDLE dsv(
                    m_heapDsv->GetCPUDescriptorHandleForHeapStart()
                );

                // カラーバッファ(レンダーターゲットビュー)のクリア
                const float clearColor[] = { 0.1f,0.25f,0.5f,0.0f }; // クリア色
                m_commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

                // デプスバッファ(デプスステンシルビュー)のクリア
                m_commandList->ClearDepthStencilView(
                    dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

                // 描画先をセット
                m_commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

                MakeCommand(m_commandList);

                // レンダーターゲットからスワップチェイン表示可能へ
                auto barrierToPresent = CD3DX12_RESOURCE_BARRIER::Transition(
                    m_renderTargets[frameIndex].Get(),
                    D3D12_RESOURCE_STATE_RENDER_TARGET,
                    D3D12_RESOURCE_STATE_PRESENT
                );
                m_commandList->ResourceBarrier(1, &barrierToPresent);

                m_commandList->Close();

                ID3D12CommandList* lists[] = { m_commandList.Get() };
                m_commandQueue->ExecuteCommandLists(1, lists);

                m_swapchain->Present(1, 0);
            }
        }
        catch (HRESULT hr)
        {
            MessageBox(nullptr, HResultToMessage(hr).get(), TEXT("Exception"), MB_OK);
        }
        return S_OK;
    }
};
