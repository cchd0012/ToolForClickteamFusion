#include "ObjectElementDataChunk.h"
#include "../../ImGUI/Headers.h"

using namespace CTT::Chunk;


bool ObjectElementDataChunk::loadFromOrgBuffer()
{
	ByteBuffer raw(orgBuffer, orgBuffer + orgSize);
	free(orgBuffer);
	orgBuffer = NULL;

	string errorMessage;
	if (!container.parse(raw, errorMessage)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 오브젝트 속성 데이터를 파싱하지 못했습니다: " << wstring(errorMessage.begin(), errorMessage.end()) << endl;
		return false;
	}
	return true;
}


bool ObjectElementDataChunk::saveIntoOrgBuffer()
{
	ByteBuffer payload;
	string errorMessage;
	if (!container.build(payload, offsetPayload, errorMessage)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 오브젝트 속성 데이터를 만들지 못했습니다: " << wstring(errorMessage.begin(), errorMessage.end()) << endl;
		return false;
	}

	if (orgBuffer != NULL) {
		free(orgBuffer);
		orgBuffer = NULL;
	}
	orgSize = (UINT32)payload.size();
	orgBuffer = (BYTE*)malloc(orgSize > 0 ? orgSize : 1);
	if (orgSize > 0) {
		memcpy(orgBuffer, &payload[0], orgSize);
	}
	return true;
}


void ObjectElementDataChunk::renderData()
{
	ImGui::Text(u8"오브젝트 속성 데이터: 요소 %u개", (unsigned)container.getElements().size());
}
