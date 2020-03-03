//------------------------------------------------------------------------------
// <copyright file="D3DVisualization.cpp" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//------------------------------------------------------------------------------

#include "D3DVisualization.h"
#include <dxcapi.h>
//#include <filesystem>
//#pragma comment(lib, "dxcompiler.lib")
#include <D3DCompiler.h>

#ifndef DIRECTX_SDK
    using namespace DirectX;
    using namespace DirectX::PackedVector;
#endif

// Global Variables
CCube * pApplication;  // Application class

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct SimpleVertex
{
	XMFLOAT3 Pos;
    XMFLOAT4 Color;
};

struct ConstantBuffer
{
    XMMATRIX mWorld;
    XMMATRIX mView;
    XMMATRIX mProjection;
};

BOOL WINAPI DllMain(HINSTANCE hInstance,DWORD fwdReason, LPVOID lpvReserved) 
{
    return TRUE;
}

/// <summary>
/// Init global class instance
/// </summary>
extern HRESULT __cdecl Init()
{
	pApplication = new CCube();

    HRESULT hr = S_OK;

    if ( FAILED( hr = pApplication->InitDevice() ) )
    {
        return hr;
    }

    return hr;
}

/// <summary>
/// Cleanup global class instance
/// </summary>
extern void __cdecl Cleanup()
{
    delete pApplication;
    pApplication = NULL;
}

/// <summary>
/// Render for global class instance
/// </summary>
extern HRESULT __cdecl Render(void * pResource, bool isNewSurface)
{
    if ( NULL == pApplication )
    {
        return E_FAIL;
    }

    return pApplication->Render(pResource, isNewSurface);
}

/// <summary>
/// Sets the Radius value of the camera
/// </summary>
extern HRESULT _cdecl SetCameraRadius(float r)
{
    if ( NULL == pApplication )
    {
        return E_FAIL;
    }

    pApplication->GetCamera()->SetRadius(r);
    return 0;
}

/// <summary>
/// Sets the Theta value of the camera
/// Theta represents the angle (in radians) of the camera around the 
/// center in the x-y plane
/// </summary>
extern HRESULT _cdecl SetCameraTheta(float theta)
{
    if ( NULL == pApplication )
    {
        return E_FAIL;
    }

    pApplication->GetCamera()->SetTheta(theta);
    return 0;
}

/// <summary>
/// Sets the Phi value of the camera
/// Phi represents angle (in radians) of the camera around the center 
/// in the y-z plane (over the top and below the scene)
/// </summary>
extern HRESULT _cdecl SetCameraPhi(float phi)
{
    if ( NULL == pApplication )
    {
        return E_FAIL;
    }

    pApplication->GetCamera()->SetPhi(phi);
    return 0;
}

/// <summary>
/// Constructor
/// </summary>
CCube::CCube()
{
}

/// <summary>
/// Destructor
/// </summary>
CCube::~CCube()
{
}


/// <summary>
/// Compile and set layout for shaders
/// </summary>
/// <returns>S_OK for success, or failure code</returns>
HRESULT CCube::LoadShaders()
{
	throw 0;
}

/// <summary>
/// Create Direct3D device
/// </summary>
/// <returns>S_OK for success, or failure code</returns>
HRESULT CCube::InitDevice()
{
	ComPtr<ID3D12Debug> debug;
	Ok(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));
	debug->EnableDebugLayer();
	ComPtr<IDXGIFactory5> factory;
	Ok(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&factory)));
	ComPtr<IDXGIAdapter> adp;
	Ok(factory->EnumAdapters(0, &adp));
	DXGI_ADAPTER_DESC ad;
	Ok(adp->GetDesc(&ad));

	Ok(D3D12CreateDevice(adp.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&m_device)));
	Ok(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAlocator)));
	Ok(m_device->CreateCommandList(
		0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAlocator.Get(),
		nullptr, IID_PPV_ARGS(&m_commandList)));
	D3D12_COMMAND_QUEUE_DESC qd;
	qd.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	qd.NodeMask = 0;
	qd.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	Ok(m_device->CreateCommandQueue(&qd, IID_PPV_ARGS(&m_commandQueue)));

	// RootSignerute & PSO
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psod = {};

		psod.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psod.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psod.NumRenderTargets = 1;
		psod.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psod.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		psod.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psod.SampleDesc = { 1, 0 };
		psod.SampleMask = UINT_MAX;
		psod.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

		// Shader
		ComPtr<ID3DBlob> vsBlob, psBlob, errorBlob;
		Ok(CompileShaderFromFile(L"D3DVisualization.fx", L"VS", L"vs_6_0", vsBlob, errorBlob));
		Ok(CompileShaderFromFile(L"D3DVisualization.fx", L"PS", L"ps_6_0", psBlob, errorBlob));
		psod.VS = CD3DX12_SHADER_BYTECODE(vsBlob.Get());
		psod.PS = CD3DX12_SHADER_BYTECODE(psBlob.Get());
		D3D12_INPUT_ELEMENT_DESC ied[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(SimpleVertex, Pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT,0, offsetof(SimpleVertex, Color), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA}
		};
		psod.InputLayout = { ied, _countof(ied) };

		// Signiture
		CD3DX12_ROOT_SIGNATURE_DESC rsd {};
		CD3DX12_ROOT_PARAMETER rootParams[1];
		rootParams[0].InitAsConstantBufferView(0);
		rsd.Init(
			_countof(rootParams), rootParams, 0, nullptr,
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
		ComPtr<ID3DBlob> sigBlob;
		Ok(D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1_0, &sigBlob, &errorBlob));
		Ok(m_device->CreateRootSignature(
			0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&m_signeture)));
		psod.pRootSignature = m_signeture.Get();
		Ok(m_device->CreateGraphicsPipelineState(&psod, IID_PPV_ARGS(&m_pipeline)));
	}

	// Vertex
	{
		SimpleVertex vertices[] =
		{
			// pos, color
			{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 0.5f) },
			{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 0.5f) },
			{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 0.5f) },
			{ XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 0.5f) },
			{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(1.0f, 0.0f, 1.0f, 0.5f) },
			{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 0.5f) },
			{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 0.5f) },
			{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT4(0.0f, 0.0f, 0.0f, 0.5f) },
		};
		ComPtr<ID3D12Resource> vertexBuffer;
		Ok(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(sizeof(vertices)),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&vertexBuffer)));
		void* pVertexBuffer;
		CD3DX12_RANGE range(0, 0);
		Ok(vertexBuffer->Map(0, &range, &pVertexBuffer));
		memcpy(pVertexBuffer, &vertices, sizeof(vertexBuffer));
		vertexBuffer->Unmap(0, nullptr);
		m_vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
		m_vertexBufferView.SizeInBytes = sizeof(vertices);
		m_vertexBufferView.StrideInBytes = sizeof(SimpleVertex);
	}

	// Index
	{
		WORD indices[] =
		{
			3, 1, 0,  2, 1, 3,
			0, 5, 4,  1, 5, 0,
			3, 4, 7,  0, 4, 3,
			1, 6, 5,  2, 6, 1,
			2, 7, 6,  3, 7, 2,
			6, 4, 5,  7, 4, 6,
		};
		ComPtr<ID3D12Resource> indexBuffer;
		Ok(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(sizeof(indices)),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&indexBuffer)));
		void* pIndexBuffer;
		CD3DX12_RANGE range(0, 0);
		Ok(indexBuffer->Map(0, &range, &pIndexBuffer));
		memcpy(pIndexBuffer, &indices, sizeof(indices));
		indexBuffer->Unmap(0, nullptr);
		m_indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
		m_indexBufferView.SizeInBytes = sizeof(indices);
		m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
	}

	// Constant (WVP)
	{
		ConstantBuffer constat;
		constat.mWorld= XMMATRIX();
		constat.mView = XMMATRIX();
		constat.mProjection = XMMATRIX();
		int alignedCbSize = (sizeof(ConstantBuffer) + 255) & ~255;
		Ok(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(alignedCbSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_constantBuffer)));
		void* pConstantBuffer;
		CD3DX12_RANGE range(0, 0);
		Ok(m_constantBuffer->Map(0, &range, &pConstantBuffer));
		memcpy(pConstantBuffer, &constat, sizeof(ConstantBuffer));
		m_constantBuffer->Unmap(0, nullptr);
		D3D12_DESCRIPTOR_HEAP_DESC cbvhd;
		cbvhd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		cbvhd.NodeMask = 0;
		cbvhd.NumDescriptors = 1;
		cbvhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		Ok(m_device->CreateDescriptorHeap(&cbvhd, IID_PPV_ARGS(&m_cbDescHeap)));
		m_constantBufferView.BufferLocation = m_constantBuffer->GetGPUVirtualAddress();
		m_constantBufferView.SizeInBytes = alignedCbSize;
		m_device->CreateConstantBufferView(&m_constantBufferView, m_cbDescHeap->GetCPUDescriptorHandleForHeapStart());

	}

	m_World = XMMatrixIdentity();
	XMVECTOR Eye = XMVectorSet(0.0f, 1.0f, -5.0f, 0.0f);
	XMVECTOR At = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	m_View = XMMatrixLookAtLH(Eye, At, Up);

    return S_OK;
}

void CCube::SetUpViewport()
{
	// Setup the viewport
	D3D12_VIEWPORT vp;
	vp.Width = (float)m_Width;
	vp.Height = (float)m_Height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	m_commandList->RSSetViewports(1, &vp);

	// Initialize the projection matrix
	m_Projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, m_Width / (FLOAT)m_Height, 0.01f, 100.0f);
}

/// <summary>
/// Initializes RenderTarget
/// </summary>
/// <returns>S_OK for success, or failure code</returns>
HRESULT CCube::InitRenderTarget(void * pResource)
{
	IUnknown* unknown = (IUnknown*)pResource;
	unknown->QueryInterface(IID_PPV_ARGS(&pDXGIResource));
	HANDLE sharedHandle;
    Ok(pDXGIResource->GetSharedHandle(&sharedHandle));
	Ok(m_device->OpenSharedHandle(sharedHandle, IID_PPV_ARGS(&rtResource)));
	m_Width = rtResource->GetDesc().Width;
	m_Height = rtResource->GetDesc().Height;

	D3D12_DESCRIPTOR_HEAP_DESC rtvhd;
	rtvhd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvhd.NodeMask = 0;
	rtvhd.NumDescriptors = 1;
	rtvhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	Ok(m_device->CreateDescriptorHeap(&rtvhd, IID_PPV_ARGS(&m_rtvDescHeap)));
	m_device->CreateRenderTargetView(
		rtResource.Get(), nullptr,
		m_rtvDescHeap->GetCPUDescriptorHandleForHeapStart());

	auto dsvHeapDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_D32_FLOAT,
		m_Width, m_Height,
		1, // arraySize
		0, // mipLv
		1, // sampleCount
		0, // sampleQuality
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
	D3D12_CLEAR_VALUE dscv;
	dscv.Format = dsvHeapDesc.Format;
	dscv.DepthStencil.Depth = 1.0f;
	dscv.DepthStencil.Stencil = 0.0f;
	Ok(m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&dsvHeapDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&dscv, IID_PPV_ARGS(&dsResource)));

	D3D12_DESCRIPTOR_HEAP_DESC dsvhd;
	dsvhd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvhd.NodeMask = 0;
	dsvhd.NumDescriptors = 1;
	dsvhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	Ok(m_device->CreateDescriptorHeap(&dsvhd, IID_PPV_ARGS(&m_dsvDescHeap)));
	m_device->CreateDepthStencilView(
		dsResource.Get(), nullptr,
		m_dsvDescHeap->GetCPUDescriptorHandleForHeapStart());

	SetUpViewport();

	m_rtv = CD3DX12_CPU_DESCRIPTOR_HANDLE(
		m_rtvDescHeap->GetCPUDescriptorHandleForHeapStart(), 0,
		m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
	m_dsv = CD3DX12_CPU_DESCRIPTOR_HANDLE(
		m_dsvDescHeap->GetCPUDescriptorHandleForHeapStart(), 0,
		m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV));
	m_commandList->OMSetRenderTargets(1, &m_rtv, FALSE, &m_dsv);

	return S_OK;
}

/// <summary>
/// Renders a frame
/// </summary>
/// <returns>S_OK for success, or failure code</returns>
HRESULT CCube::Render(void * pResource, bool isNewSurface)
{
    HRESULT hr = S_OK;

    // If we've gotten a new Surface, need to initialize the renderTarget.
    // One of the times that this happens is on a resize.
    if ( isNewSurface )
    {
        Ok(InitRenderTarget(pResource));
    }

	// Update our time
	static float t = 0.0f;
	if (m_driverType == D3D_DRIVER_TYPE_REFERENCE)
	{
		t += (float)XM_PI * 0.0125f;
	}
	else
	{
		static DWORD dwTimeStart = 0;
		DWORD dwTimeCur = GetTickCount();
		if (dwTimeStart == 0)
			dwTimeStart = dwTimeCur;
		t = (dwTimeCur - dwTimeStart) / 1000.0f;
	}

	// RT DS
	m_commandList->OMSetRenderTargets(1, &m_rtv, FALSE, &m_dsv);
    static float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_commandList->ClearRenderTargetView(m_rtv, ClearColor, 0, nullptr);

    // Update the view matrix
	m_World = XMMatrixRotationX(t) * XMMatrixRotationY(t);
    m_camera.Update();
    XMMATRIX viewProjection = XMMatrixMultiply(m_camera.View, m_Projection);
	ConstantBuffer cb;
	cb.mWorld = XMMatrixTranspose(m_World);
	cb.mView = XMMatrixTranspose(m_View);
	cb.mProjection = XMMatrixTranspose(viewProjection);
	void* pConstantBuffer;
	CD3DX12_RANGE range(0, 0);
	Ok(m_constantBuffer->Map(0, &range, &pConstantBuffer));
	memcpy(pConstantBuffer, &cb, sizeof(ConstantBuffer));
	m_constantBuffer->Unmap(0, nullptr);

	// Renders a triangle
	m_commandList->SetPipelineState(m_pipeline.Get());
	m_commandList->SetGraphicsRootSignature(m_signeture.Get());
	m_commandList->RSSetViewports(1, &CD3DX12_VIEWPORT(0.0f, 0.0f, float(m_Width), float(m_Height)));
	m_commandList->RSSetScissorRects(1, &CD3DX12_RECT(0, 0, LONG(m_Width), LONG(m_Height)));
	ID3D12DescriptorHeap* heap[] = { m_cbDescHeap.Get() };
	m_commandList->SetDescriptorHeaps(_countof(heap), heap);
	m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
	m_commandList->IASetIndexBuffer(&m_indexBufferView);
	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_commandList->SetGraphicsRootDescriptorTable(0, m_cbDescHeap->GetGPUDescriptorHandleForHeapStart());
	m_commandList->DrawIndexedInstanced(36, 1, 0, 0, 0);

	ID3D12CommandList* lists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(1, lists);

	ComPtr<ID3D12Fence1> fence;
	Ok(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
	Ok(m_commandQueue->Signal(fence.Get(), 1));
	HANDLE handle = CreateEvent(NULL, FALSE, FALSE, NULL);
	fence->SetEventOnCompletion(1, handle);
	WaitForSingleObject(handle, INFINITE);
    return 0;
}

/// <summary>
/// Method for retreiving the camera
/// </summary>
/// <returns>Pointer to the camera</returns>
CCamera* CCube::GetCamera()
{
    return &m_camera;
}