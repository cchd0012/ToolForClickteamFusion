#pragma once

#include "FrameEventStructure.h"

namespace CTT {

	namespace Chunk {

		using namespace std;

		// true for the parameter codes that hold an expression
		bool isFrameEventExpressionParameter(INT16 code);

		// parse a decoded 0x333D payload. returns false and fills errorMessage on malformed data
		bool parseFrameEventChunk(const BYTE* data, size_t size, UINT32 build, FrameEventChunkModel& model, string& errorMessage);
		bool parseFrameEventChunk(const ByteBuffer& data, UINT32 build, FrameEventChunkModel& model, string& errorMessage);

		// serialize the model back into a 0x333D payload ('ERes' / 'ERev' sizes recomputed, group pointers relocated)
		bool writeFrameEventChunk(const FrameEventChunkModel& model, ByteBuffer& data, string& errorMessage);

		// number of group pointers whose target group could not be found (diagnostics)
		size_t countUnresolvedFrameEventPointers(const FrameEventChunkModel& model);

		// expression parameter helpers
		bool parseFrameEventExpression(const ByteBuffer& body, FrameEventExpression& expression, string& errorMessage);
		ByteBuffer buildFrameEventExpression(const FrameEventExpression& expression);

		// string token helpers: body = UTF-16 text + 0x0000 + optional trailing bytes
		bool getFrameEventExpressionString(const FrameEventExpressionToken& token, wstring& value);
		bool setFrameEventExpressionString(FrameEventExpressionToken& token, const wstring& value);

	}

}
