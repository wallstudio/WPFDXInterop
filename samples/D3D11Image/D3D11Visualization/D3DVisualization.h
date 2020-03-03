//------------------------------------------------------------------------------
// <copyright file="D3DVizualization.h" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//------------------------------------------------------------------------------

#pragma once

#include <windows.h>
#include <d3d12.h>
#include "d3dx12.h"
#include <dxgi1_6.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

#ifdef DIRECTX_SDK  // requires DirectX SDK June 2010
    #include <xnamath.h>
    #define DirectX_NS                 // DirectX SDK requires a blank namespace for several types
#else // Windows SDK
    #include <DirectXMath.h>
    #include <DirectXPackedVector.h>
    #define DirectX_NS DirectX         // Windows SDK requires a DirectX namespace for several types
#endif 

#include "OrbitCamera.h"
//#include "DX11Utils.h"
#include "resource.h"

#include <filesystem>
#include <exception>
#include <fstream>
#include <filesystem>
#include <wrl.h>
#include <dxcapi.h>
#include <d3dcompiler.h>
#pragma comment(lib, "dxcompiler.lib")

using namespace Microsoft::WRL;


extern "C" {
    __declspec(dllexport) HRESULT __cdecl Init();
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
    HRESULT                             InitDevice();

    /// <summary>
    /// Renders a frame
    /// </summary>
    /// <returns>S_OK for success, or failure code</returns>
    HRESULT                             Render(void * pResource, bool isNewSurface);


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
    
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12CommandAllocator> m_commandAlocator;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12RootSignature> m_signeture;
    ComPtr<ID3D12PipelineState> m_pipeline;

    CD3DX12_CPU_DESCRIPTOR_HANDLE m_rtv;
    CD3DX12_CPU_DESCRIPTOR_HANDLE m_dsv;

    ComPtr<IDXGIResource> pDXGIResource;
    ComPtr<ID3D12Resource> rtResource;
    ComPtr<ID3D12Resource> dsResource;
    ComPtr<ID3D12Resource> m_constantBuffer;

    ComPtr<ID3D12DescriptorHeap> m_rtvDescHeap;
    ComPtr<ID3D12DescriptorHeap> m_dsvDescHeap;
    ComPtr<ID3D12DescriptorHeap> m_cbDescHeap;

    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
    D3D12_CONSTANT_BUFFER_VIEW_DESC m_constantBufferView;


    DirectX_NS::XMMATRIX                 m_World;
    DirectX_NS::XMMATRIX                 m_View;
    DirectX_NS::XMMATRIX                 m_Projection;
   

    // Initial window resolution
    UINT                                 m_Width;
    UINT                                 m_Height;

    /// <summary>
    /// Compile and set layout for shaders
    /// </summary>
    /// <returns>S_OK for success, or failure code</returns>
    HRESULT                             LoadShaders();

    inline void Ok(HRESULT hr)
    {
        if (FAILED(hr))
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

            MessageBox(nullptr, errorMessage, TEXT("Exception"), MB_OK);

            throw errorMessage;
        }
    }

    inline HRESULT CompileShaderFromFile(
        const std::wstring& fileName, const std::wstring& entry, const std::wstring& profile, ComPtr<ID3DBlob>& shaderBlob, ComPtr<ID3DBlob>& errorBlob)
    {
        using namespace std::filesystem;
        path filePath(fileName);
        std::ifstream infile(filePath, std::ifstream::binary);
        std::vector<char> srcData;
        if (!infile)
            throw std::runtime_error("shader not found");
        srcData.resize(uint32_t(infile.seekg(0, infile.end).tellg()));
        infile.seekg(0, infile.beg).read(srcData.data(), srcData.size());

        // DXC によるコンパイル処理
        ComPtr<IDxcLibrary> library;
        ComPtr<IDxcCompiler> compiler;
        ComPtr<IDxcBlobEncoding> source;
        ComPtr<IDxcOperationResult> dxcResult;

        DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library));
        library->CreateBlobWithEncodingFromPinned(srcData.data(), UINT(srcData.size()), CP_ACP, &source);
        DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));

        LPCWSTR compilerFlags[] = {
      #if _DEBUG
          L"/Zi", L"/O0",
      #else
          L"/O2" // リリースビルドでは最適化
      #endif
        };
        compiler->Compile(source.Get(), filePath.wstring().c_str(),
            entry.c_str(), profile.c_str(),
            compilerFlags, _countof(compilerFlags),
            nullptr, 0, // Defines
            nullptr,
            &dxcResult);

        HRESULT hr;
        dxcResult->GetStatus(&hr);
        if (SUCCEEDED(hr))
        {
            dxcResult->GetResult(
                reinterpret_cast<IDxcBlob**>(shaderBlob.GetAddressOf())
            );
        }
        else
        {
            dxcResult->GetErrorBuffer(
                reinterpret_cast<IDxcBlobEncoding**>(errorBlob.GetAddressOf()));
            std::string msg = (char*)errorBlob->GetBufferPointer();
            OutputDebugStringA(msg.c_str());
        }
        return hr;
    }

};