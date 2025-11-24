// 1. 입력 데이터 구조(C++ SimpleVertex와 매칭)
struct VS_INPUT
{
	float4 Pos : POSITION; // : POSITION은 "이 변수는 좌표 데이터야"라는 의미표(Semantic)
};

// 2. 버텍스 쉐이더 출력 (= 픽셀 쉐이더 입력)
struct PS_INPUT
{
	float4 Pos : SV_POSITION; // SV_POSITION: 시스템이 화면 위치로 사용할 필수 값
};

// --------------------------------------------------------
// 버텍스 쉐이더 (Vertex Shader)
// 역할: 정점 하나하나의 위치를 결정함
// --------------------------------------------------------
PS_INPUT VS(VS_INPUT input)
{
	PS_INPUT output = (PS_INPUT)0;

	// 들어온 좌표 그대로 내보내기 (지금은 행렬 연산 없이 바로 출력)
	// w값을 1.0으로 설정 (동차 좌표계)
	output.Pos = float4(input.Pos.xyz, 1.0f);

	return output;
}

// --------------------------------------------------------
// 픽셀 쉐이더 (Pixel Shader)
// 역할: 픽셀 하나하나의 색상을 결정함
// --------------------------------------------------------
float4 PS(PS_INPUT input) : SV_Target
{
	return float4(1.0f, 1.0f, 0.0f, 1.0f);
}