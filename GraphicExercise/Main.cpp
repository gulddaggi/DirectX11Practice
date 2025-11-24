#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// 전역 변수들
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
ID3D11Buffer* g_pVertexBuffer = nullptr;
ID3D11VertexShader* g_pVertexShader = nullptr;
ID3D11PixelShader* g_pPixelShader = nullptr;
ID3D11InputLayout* g_pVertexLayout = nullptr;

// 정점 구조체
struct SimpleVertex
{
	float x, y, z; // Position
};

// Window Procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

// Init
HRESULT InitDevice(HWND hWnd)
{
	DXGI_SWAP_CHAIN_DESC sd = {}; // 0
	sd.BufferCount = 1;
	sd.BufferDesc.Width = 800;
	sd.BufferDesc.Height = 600;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; //Render Target
	sd.OutputWindow = hWnd;
	sd.SampleDesc.Count = 1;
	sd.Windowed = TRUE;

	// Create Device, Context, SwapChain
	HRESULT	hr = D3D11CreateDeviceAndSwapChain(
		nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
		D3D11_SDK_VERSION, &sd,
		&g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext);

	if (FAILED(hr)) return hr;

	// 1. 버텍스 쉐이더 컴파일 및 생성
	ID3DBlob* pVSBlob = nullptr; // 컴파일된 기계어 코드를 담을 덩어리

	// Shaders.hlsl 파일에서 "VS"라는 함수를 찾아 "vs_4_0" 버전으로 컴파일
	hr = D3DCompileFromFile(L"Shaders.hlsl", nullptr, nullptr, "VS", "vs_4_0", 0, 0, &pVSBlob, nullptr);
	if (FAILED(hr))
	{
		MessageBox(nullptr, L"Shader.hlsl 파일을 찾을 수 없거나 컴파일 에러!", L"Error", MB_OK);
		return hr;
	}

	// 컴파일된 Blob으로 진짜 쉐이더 객체 생성
	hr = g_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &g_pVertexShader);
	if (FAILED(hr))
	{
		pVSBlob->Release();
		return hr;
	}

	// 2. 인풋 레이아웃 생성
	// "C++의 구조체 데이터가 HLSL의 어떤 변수랑 매칭되는지" 알려주는 정의
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		// "POSITION"이라는 이름의 데이터는 -> float 3개(R32G32B32)로 되어 있고 -> 0번 슬롯에서 시작한다.
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	UINT numElements = ARRAYSIZE(layout);

	// 쉐이더 코드(VSBlob)를 보고 레이아웃이 맞는지 검증하며 생성
	hr = g_pd3dDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &g_pVertexLayout);
	pVSBlob->Release(); // 쉐이더 만들었으니 Blob은 해제
	if (FAILED(hr))
	{
		return hr;
	}

	// 생성한 레이아웃을 파이프라인에 장착
	g_pImmediateContext->IASetInputLayout(g_pVertexLayout);

	// 3. 픽셀 쉐이더 컴파일 및 생성
	ID3DBlob* pPSBlob = nullptr;
	// "PS" 함수를 "ps_4_0" 버전으로 컴파일
	hr = D3DCompileFromFile(L"Shader.hlsl", nullptr, nullptr, "PS", "ps_4_0", 0, 0, &pPSBlob, nullptr);
	if (FAILED(hr))
	{
		return hr;
	}
	hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &g_pPixelShader);
	pPSBlob->Release();
	if (FAILED(hr))
	{
		return hr;
	}

	// Create RTV (Bring backbuffer -> Create RTV)
	ID3D11Texture2D* pBackBuffer = nullptr;
	g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);

	g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
	pBackBuffer->Release(); // Release backbuffer pointer

	// Set render target
	g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

	// 삼각형 데이터 정의 (ClockWise)
	SimpleVertex vertices[] =
	{
		{0.0f, 0.5f, 0.5f}, //Top
		{0.5f, -0.5f, 0.5f}, // Bottom Right
		{-0.5f, -0.5f, 0.5f}, // Bottom Left
	};

	// 1. 정점 버퍼 생성
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(SimpleVertex) * 3; // Buffer size
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER; 
	bd.CPUAccessFlags = 0;

	// 2. 초기 데이터 지정
	D3D11_SUBRESOURCE_DATA InitData = {};
	InitData.pSysMem = vertices;

	// 3. 버퍼 생성 
	hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pVertexBuffer);
	if (FAILED(hr)) return hr;

	// 1. 정점 버퍼를 파이프라인(Input Assembler)에 묶기
	UINT stride = sizeof(SimpleVertex); // 정점 1개의 크기
	UINT offset = 0; // 처음부터 읽기

	g_pImmediateContext->IASetVertexBuffers(
		0,                  // 슬롯 번호 (보통 0번)
		1,                  // 버퍼 개수
		&g_pVertexBuffer,   // 버퍼 주소
		&stride,            // 보폭 (Stride)
		&offset             // 오프셋
	);

	// 2. 도형의 모양 설정 (Topology)
	// Triangle List: 점 3개마다 삼각형 하나를 만듦
	g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	return S_OK;
}

// Render
void Render()
{
	// 1. 화면 지우기 (파란색)
	float ClearColor[4] = { 0.0f, 0.125f, 0.6f, 1.0f };
	g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, ClearColor);

	// 2. 쉐이더 장착 (버텍스 쉐이더, 픽셀 쉐이더)
	g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
	g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);

	// 3. 그리기 명령 (Draw)
	g_pImmediateContext->Draw(3, 0);

	// 4. 화면 출력
	g_pSwapChain->Present(0, 0);
}

// Clear
void CleanupDevice()
{
	if (g_pImmediateContext) g_pImmediateContext->ClearState();
	if (g_pRenderTargetView) g_pRenderTargetView->Release();
	if (g_pSwapChain) g_pSwapChain->Release();
	if (g_pImmediateContext) g_pImmediateContext->Release();
	if (g_pd3dDevice) g_pd3dDevice->Release();
	if (g_pVertexShader) g_pVertexShader->Release();
	if (g_pPixelShader) g_pPixelShader->Release();
	if (g_pVertexLayout) g_pVertexLayout->Release();
}

// main
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int)
{
	// Create Window and assign(Basic Win32 API)
	WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, L"DX11Win", NULL };
	RegisterClassEx(&wc);
	HWND hWnd = CreateWindow(L"DX11Win", L"DirectX 11 Tutorial 01", WS_OVERLAPPEDWINDOW, 100, 100, 800, 600, NULL, NULL, wc.hInstance, NULL);

	//Init DirectX
	if (FAILED(InitDevice(hWnd)))
	{
		CleanupDevice();
		return 0;
	}

	ShowWindow(hWnd, SW_SHOWDEFAULT);
	UpdateWindow(hWnd);

	// Message loop(Game loop)
	MSG msg = { 0 };
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			// Rendering(Game loop)
			Render();
		}
	}

	CleanupDevice();
	return (int)msg.wParam;
}