#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <string>
#include <DirectXMath.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

// 전역 변수들
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
ID3D11Buffer* g_pVertexBuffer = nullptr;
ID3D11VertexShader* g_pVertexShader = nullptr;
ID3D11PixelShader* g_pPixelShader = nullptr;
ID3D11InputLayout* g_pVertexLayout = nullptr;
ID3D11Buffer* g_pIndexBuffer = nullptr;
ID3D11Buffer* g_pConstantBuffer = nullptr;
ID3D11Texture2D* g_pDepthStencil = nullptr;
ID3D11DepthStencilView* g_pDepthStencilView = nullptr;

XMMATRIX g_World;
XMMATRIX g_View;
XMMATRIX g_Projection;

// 정점 구조체
struct SimpleVertex
{
	XMFLOAT3 Pos;    // 위치
	XMFLOAT3 Normal; // 법선 벡터 (표면이 바라보는 방향)
	XMFLOAT4 Color;  // 색상
};

// 상수 버퍼 구조체 (CPU 쪽 정의)
struct ConstantBuffer
{
	XMMATRIX mWorld; // 월드 행렬 (물체의 위치/회전)
	XMMATRIX mView; // 뷰 행렬 (카메라의 위치)
	XMMATRIX mProjection; // 프로젝션 행렬 (원근감)

	// 조명 방향
	// GPU는 16바이트 단위를 데이터로 읽으므로 XMFLOAT4 사용
	XMFLOAT4 vLightDir;
};

std::wstring GetShaderPath()
{
	WCHAR buffer[MAX_PATH];
	GetModuleFileName(NULL, buffer, MAX_PATH); // 현재 실행 파일(.exe)의 전체 경로를 가져옴
	std::wstring path = buffer;

	// 1. 실행 파일 이름 제거 (GraphicExercise.exe)
	path = path.substr(0, path.find_last_of(L"\\/"));

	// 2. 만약 Shaders.hlsl이 바로 옆에 있으면 리턴
	if (GetFileAttributes((path + L"\\Shaders.hlsl").c_str()) != INVALID_FILE_ATTRIBUTES)
		return path + L"\\Shaders.hlsl";

	// 소스 코드 경로를 직접 반환.
	return L"Shaders.hlsl";
}

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
	ID3DBlob* pErrorBlob = nullptr; // 에러 메시지를 받을 변수

	// 컴파일 시도
	DWORD flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG; // 디버그 정보 포함
	hr = D3DCompileFromFile(
		L"Shaders.hlsl",      // 파일 경로
		nullptr, nullptr,
		"VS", "vs_4_0",       // 함수 이름, 버전
		flags, 0,
		&pVSBlob,
		&pErrorBlob           // [NEW] 에러 내용을 여기에 담아옴
	);

	if (FAILED(hr))
	{
		if (pErrorBlob)
		{
			// 컴파일 에러인 경우 (오타 등)
			MessageBoxA(nullptr, (char*)pErrorBlob->GetBufferPointer(), "Shader Compile Error", MB_OK);
			pErrorBlob->Release();
		}
		else
		{
			// 파일 자체가 없는 경우
			MessageBox(nullptr, L"Shaders.hlsl 파일을 찾을 수 없습니다.\n(Main.cpp와 같은 폴더에 있나요?)", L"File Not Found", MB_OK);
		}
		return hr;
	}

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
		// 1. 위치 정보
		// "POSITION"이라는 이름의 데이터는 -> float 3개(R32G32B32)로 되어 있고 -> 0번 슬롯에서 시작한다.
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },

		// 2. 법선
		// 위치(12바이트) 바로 뒤
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },

		// 3. 색상 정보
		// "COLOR"라는 이름표를 달고, float4(128비트) 크기이며,
		// 위치(12) + 법선(12) 데이터가 끝나는 지점(24바이트)부터 시작.
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
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
	hr = D3DCompileFromFile(L"Shaders.hlsl", nullptr, nullptr, "PS", "ps_4_0", 0, 0, &pPSBlob, nullptr);
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

	// 깊이 버퍼 생성
	// 1. 깊이 텍스처 생성
	D3D11_TEXTURE2D_DESC descDepth = {};
	descDepth.Width = 800;
	descDepth.Height = 600;
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // 24비트 Depth, 8비트 Stencil
	descDepth.SampleDesc.Count = 1;
	descDepth.SampleDesc.Quality = 0;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL; // 깊이 버퍼 용도
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;

	hr = g_pd3dDevice->CreateTexture2D(&descDepth, nullptr, &g_pDepthStencil);
	if (FAILED(hr)) return hr;

	// 2. 깊이 스텐실 뷰(DSV) 생성
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV = {};
	descDSV.Format = descDepth.Format;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;

	hr = g_pd3dDevice->CreateDepthStencilView(g_pDepthStencil, &descDSV, &g_pDepthStencilView);
	if (FAILED(hr)) return hr;

	// 3. 렌더 타겟과 깊이 버퍼를 파이프라인에 연결(기존 코드 수정)
	g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDepthStencilView);

	// 24개의 정점 (각 면마다 4개씩 분리, 법선 정보 포함)
	SimpleVertex vertices[] =
	{
		// 윗면 (Normal: 0, 1, 0) - 초록색 계열
		{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(1.0f, 1.0f,  1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, 1.0f,  1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },

		// 아랫면 (Normal: 0, -1, 0) - 주황색 계열
		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT4(1.0f, 0.5f, 0.0f, 1.0f) },
		{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT4(1.0f, 0.5f, 0.0f, 1.0f) },
		{ XMFLOAT3(1.0f, -1.0f,  1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT4(1.0f, 0.5f, 0.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, -1.0f,  1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT4(1.0f, 0.5f, 0.0f, 1.0f) },

		// 앞면 (Normal: 0, 0, -1) - 빨간색 계열
		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(1.0f,  1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },

		// 뒷면 (Normal: 0, 0, 1) - 청록색 계열
		{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(1.0f,  1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(-1.0f,  1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },

		// 왼쪽면 (Normal: -1, 0, 0) - 파란색 계열
		{ XMFLOAT3(-1.0f, -1.0f,  1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(-1.0f,  1.0f,  1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },

		// 오른쪽면 (Normal: 1, 0, 0) - 노란색 계열
		{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(1.0f, -1.0f,  1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(1.0f,  1.0f,  1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(1.0f,  1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
	};

	// 정점 버퍼
	// 1. 정점 버퍼 생성
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(SimpleVertex) * 24; // Buffer size
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER; 
	bd.CPUAccessFlags = 0;

	// 2. 초기 데이터 지정
	D3D11_SUBRESOURCE_DATA InitData = {};
	InitData.pSysMem = vertices;

	// 3. 버퍼 생성 
	hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pVertexBuffer);
	if (FAILED(hr)) return hr;

	// 4. 정점 버퍼를 파이프라인(Input Assembler)에 묶기
	UINT stride = sizeof(SimpleVertex); // 정점 1개의 크기
	UINT offset = 0; // 처음부터 읽기

	g_pImmediateContext->IASetVertexBuffers(
		0,                  // 슬롯 번호 (보통 0번)
		1,                  // 버퍼 개수
		&g_pVertexBuffer,   // 버퍼 주소
		&stride,            // 보폭 (Stride)
		&offset             // 오프셋
	);

	// 5. 도형의 모양 설정 (Topology)
	// Topology 설정: 정점들을 이어 삼각형 리스트로 해석 (인덱스 버퍼가 이를 참조하여 사각형 구성)
	g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 24개 정점에 대한 인덱스 (각 면마다 0-1-2, 0-2-3 패턴)
	WORD indices[] =
	{
		3,1,0, 2,1,3,      // 윗면
		6,4,5, 7,4,6,      // 아랫면
		11,9,8, 10,9,11,   // 앞면
		14,12,13, 15,12,14,// 뒷면
		19,17,16, 18,17,19,// 왼쪽면
		22,20,21, 23,20,22 // 오른쪽면
	};

	D3D11_BUFFER_DESC ibd = {};
	ibd.Usage = D3D11_USAGE_DEFAULT;
	ibd.ByteWidth = sizeof(WORD) * 36; // 2바이트(WORD) * 36개
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER; // 인덱스 버퍼 용도
	ibd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA iinitData = {};
	iinitData.pSysMem = indices;

	hr = g_pd3dDevice->CreateBuffer(&ibd, &iinitData, &g_pIndexBuffer);
	if (FAILED(hr))
	{
		return hr;
	}

	// 인덱스 버퍼를 파이프라인에 묶기 (Bind)
	g_pImmediateContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

	// 뷰포트(Viewport) 설정
	// NDC 좌표(-1~1)를 실제 윈도우 픽셀 좌표(0~800, 0~600)로 변환하는 설정
	D3D11_VIEWPORT vp;
	vp.Width = 800.0f;
	vp.Height = 600.0f;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;

	g_pImmediateContext->RSSetViewports(1, &vp);

	// 상수 버퍼
	// 상수 버퍼 생성
	D3D11_BUFFER_DESC cbd = {};
	cbd.Usage = D3D11_USAGE_DEFAULT;
	cbd.ByteWidth = sizeof(ConstantBuffer); // 크기는 구조체 크기만큼
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER; // 용도는 상수 버퍼
	cbd.CPUAccessFlags = 0;

	// 초기값 없이 껍데기만 (나중에 Render에서 채울 예정)
	hr = g_pd3dDevice->CreateBuffer(&cbd, nullptr, &g_pConstantBuffer);
	if (FAILED(hr)) return hr;

	// 행렬 초기화 (카메라 세팅)
	// 1. 월드 행렬: 단위 행렬
	g_World = XMMatrixIdentity();

	// 2. 뷰 행렬: 카메라 위치 설정
	// Eye: 카메라 위치 (0, 0, -3) -> Z축 뒤로 3칸 물러남
	// At:  바라보는 곳 (0, 0, 0)  -> 원점을 바라봄
	// Up:  천장 방향   (0, 1, 0)  -> Y축이 위쪽
	XMVECTOR Eye = XMVectorSet(0.0f, 0.0f, -3.0f, 0.0f);
	XMVECTOR At = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	g_View = XMMatrixLookAtLH(Eye, At, Up);

	// 3. 프로젝션 행렬: 렌즈 세팅 (원근감)
	// FOV(시야각): 90도 (XM_PIDIV2)
	// 화면 비율: 800 / 600
	// Near: 0.01 (이보다 가까우면 안 그림)
	// Far: 100.0 (이보다 멀면 안 그림)
	g_Projection = XMMatrixPerspectiveFovLH(XM_PIDIV2, 800.0f / 600.0f, 0.01f, 100.0f);

	// 래스터라이저 스테이트 설정 (CullMode 비활성화)
	D3D11_RASTERIZER_DESC wfdesc = {};
	wfdesc.FillMode = D3D11_FILL_SOLID; // 채우기 모드
	wfdesc.CullMode = D3D11_CULL_NONE; // 컬링 비활성화
	wfdesc.FrontCounterClockwise = false;

	ID3D11RasterizerState* pRasterizerState = nullptr;
	hr = g_pd3dDevice->CreateRasterizerState(&wfdesc, &pRasterizerState);
	if (FAILED(hr)) return hr;

	g_pImmediateContext->RSSetState(pRasterizerState);
	pRasterizerState->Release();

	return S_OK;
}

// Render
void Render()
{
	// 화면 지우기 (파란색)
	float ClearColor[4] = { 0.0f, 0.125f, 0.6f, 1.0f };
	g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, ClearColor);

	// 깊이 버퍼 지우기
	g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

	// 월드 행렬 업데이트 (회전)
	// 시간에 따라 각도를 계속 증가
	static float t = 0.0f;
	if (g_pd3dDevice->GetFeatureLevel() == D3D_FEATURE_LEVEL_11_1)
		t += 0.0001f; // 좀 더 빠르게
	else
		t += 0.0005f;

	// Y축을 기준으로 t만큼 회전하는 행렬 생성
	g_World = XMMatrixRotationY(t);

	// 상수 버퍼 업데이트 (CPU -> GPU)
	ConstantBuffer cb;
	// 행렬 Transpose (전치)
	// DirectXMath(CPU)는 행 우선(Row-Major), HLSL(GPU)은 열 우선(Column-Major).
	// 그래서 보내기 전에 행렬을 뒤집어(Transpose) 줘야 쉐이더가 똑바로 읽는다.
	cb.mWorld = XMMatrixTranspose(g_World);
	cb.mView = XMMatrixTranspose(g_View);
	cb.mProjection = XMMatrixTranspose(g_Projection);
	// 빛이 비추는 방향
	cb.vLightDir = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);

	// GPU에 있는 상수 버퍼 메모리 갱신
	g_pImmediateContext->UpdateSubresource(g_pConstantBuffer, 0, nullptr, &cb, 0, 0);

	// 버텍스 쉐이더 단계에 상수 버퍼 연결 (슬롯 0번)
	g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);

	// 픽쉘 쉐이더에도 전달
	g_pImmediateContext->PSSetConstantBuffers(0, 1, &g_pConstantBuffer);

	// 쉐이더 장착 (버텍스 쉐이더, 픽셀 쉐이더)
	g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
	g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);

	// 그리기 명령 (Draw)
	
	// 기존 코드 삭제
	// g_pImmediateContext->Draw(3, 0);

	// 그리기 명령 (인덱스 36개)
	g_pImmediateContext->DrawIndexed(36, 0, 0);

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
	if (g_pIndexBuffer) g_pIndexBuffer->Release();
	if (g_pConstantBuffer) g_pConstantBuffer->Release();
	if (g_pDepthStencil) g_pDepthStencil->Release();
	if (g_pDepthStencilView) g_pDepthStencilView->Release();
}

// main
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int)
{
	// Create Window and assign(Basic Win32 API)
	WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, L"DX11Win", NULL };
	RegisterClassEx(&wc);
	HWND hWnd = CreateWindow(L"DX11Win", L"DirectX 11 Tutorial 06", WS_OVERLAPPEDWINDOW, 100, 100, 800, 600, NULL, NULL, wc.hInstance, NULL);

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