// 상수 버퍼 (C++ 구조체와 일치)
cbuffer ConstantBuffer : register(b0)
{
    matrix World;
    matrix View;
    matrix Projection;
    float4 LightDir; // 빛의 방향
}

// 입력 데이터 구조
struct VS_INPUT
{
    float3 Pos : POSITION;
    float3 Normal : NORMAL; // 법선 벡터
    float4 Color : COLOR;
};

// 픽셀 쉐이더로 넘겨줄 데이터
struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float3 Norm : TEXCOORD0; // 월드 공간 법선
    float4 Color : COLOR;
};

// 버텍스 쉐이더
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT) 0;

    // 1. 위치 변환 (WVP)
    float4 pos = float4(input.Pos, 1.0f);
    pos = mul(pos, World);
    pos = mul(pos, View);
    pos = mul(pos, Projection);
    output.Pos = pos;

    // 2. 법선 변환 (회전만 적용하기 위해 w=0)
    float4 normal = float4(input.Normal, 0.0f);
    normal = mul(normal, World);
    output.Norm = normalize(normal.xyz);

    // 3. 색상 전달
    output.Color = input.Color;

    return output;
}

// 픽셀 쉐이더
float4 PS(PS_INPUT input) : SV_Target
{
    // 1. 법선과 빛의 방향 정규화
    float3 N = normalize(input.Norm);
    float3 L = normalize(-LightDir.xyz); // 빛이 들어오는 방향의 반대(광원 쪽)를 봐야 함

    // 2. 램버트 조명 (내적)
    // 면이 빛을 바라보면(내적>0) 밝아짐
    float diffuse = saturate(dot(N, L));

    // 3. 최종 색상 = 원래 색 * (조명 강도 + 환경광)
    // 환경광(0.1)을 더해 완전한 암흑 방지
    return input.Color * (diffuse + 0.05f);
}