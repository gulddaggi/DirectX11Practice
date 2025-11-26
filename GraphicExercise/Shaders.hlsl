// 상수 버퍼 정의 (C++ 구조체와 메모리 구조 일치)
cbuffer ConstantBuffer : register(b0) // b0: 0번 슬롯 버퍼
{
    matrix World;
    matrix View;
    matrix Projection;
}

// 입력 데이터 구조
struct VS_INPUT
{
    float3 Pos : POSITION; // 위치 (x, y, z)
    float4 Color : COLOR;  // 색상 (r, g, b, a)
};

// 픽셀 쉐이더로 넘겨줄 데이터
struct PS_INPUT
{
    float4 Pos : SV_POSITION; // 화면 좌표
    float4 Color : COLOR;     // 보간된 색상
};

// 버텍스 쉐이더 (Vertex Shader)
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT)0;

    // 좌표 변환 (Local -> World -> View -> Projection)
    // 원래 위치(Pos)에 1.0을 붙여 float4로 전환.
    float4 pos = float4(input.Pos, 1.0f);

    // 행렬 곱셈: mul(벡터, 행렬)
    pos = mul(pos, World); // 월드 변환 (회전)
    pos = mul(pos, View); // 뷰 변환 (카메라 앞)
    pos = mul(pos, Projection); // 프로젝션 변환 (원근감 적용)

    output.Pos = pos;

    // 색상 전달: 들어온 색을 그대로 픽셀 쉐이더로
    output.Color = input.Color;

    return output;
}

// 픽셀 쉐이더 (Pixel Shader)
float4 PS(PS_INPUT input) : SV_Target
{
    return input.Color;
}