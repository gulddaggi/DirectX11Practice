// 1. 입력 데이터 구조
struct VS_INPUT
{
    float3 Pos : POSITION; // 위치 (x, y, z)
    float4 Color : COLOR;  // 색상 (r, g, b, a)
};

// 2. 픽셀 쉐이더로 넘겨줄 데이터
struct PS_INPUT
{
    float4 Pos : SV_POSITION; // 화면 좌표
    float4 Color : COLOR;     // 보간된 색상
};

// 버텍스 쉐이더 (Vertex Shader)
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT)0;

    // 1. 위치 변환: 그대로 통과 (w = 1.0)
    output.Pos = float4(input.Pos, 1.0f);

    // 2. 색상 전달: 들어온 색을 그대로 픽셀 쉐이더로 토스
    output.Color = input.Color;

    return output;
}

// 픽셀 쉐이더 (Pixel Shader)
float4 PS(PS_INPUT input) : SV_Target
{
    return input.Color;
}