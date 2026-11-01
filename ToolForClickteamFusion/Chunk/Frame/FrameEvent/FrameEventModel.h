#pragma once

#include "FrameEventStructure.h"

namespace CTT {

	namespace Chunk {

		using namespace std;

		// 수식(expression)을 담는 파라미터 코드인지 여부
		bool isFrameEventExpressionParameter(INT16 code);

		// 복호화된 0x333D 데이터를 파싱한다. 데이터가 잘못되었다면 false 를 반환하고 errorMessage 를 채운다
		bool parseFrameEventChunk(const BYTE* data, size_t size, UINT32 build, FrameEventChunkModel& model, string& errorMessage);
		bool parseFrameEventChunk(const ByteBuffer& data, UINT32 build, FrameEventChunkModel& model, string& errorMessage);

		// 모델을 다시 0x333D 데이터로 직렬화한다 ('ERes' / 'ERev' 크기는 다시 계산하고, 그룹 포인터는 재배치한다)
		bool writeFrameEventChunk(const FrameEventChunkModel& model, ByteBuffer& data, string& errorMessage);

		// 대상 그룹을 찾지 못한 그룹 포인터의 개수 (진단용)
		size_t countUnresolvedFrameEventPointers(const FrameEventChunkModel& model);

		// 수식 파라미터 보조 함수
		bool parseFrameEventExpression(const ByteBuffer& body, FrameEventExpression& expression, string& errorMessage);
		ByteBuffer buildFrameEventExpression(const FrameEventExpression& expression);

		// 문자열 토큰 보조 함수: body = UTF-16 문자열 + 0x0000 + (있다면) 뒤따르는 바이트
		bool getFrameEventExpressionString(const FrameEventExpressionToken& token, wstring& value);
		bool setFrameEventExpressionString(FrameEventExpressionToken& token, const wstring& value);

	}

}
