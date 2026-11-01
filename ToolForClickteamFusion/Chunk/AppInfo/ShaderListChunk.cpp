#include "ShaderListChunk.h"
#include "../../Archive/Archive.h"
#include "../../Util/CommonUtil.h"

using namespace CTT::Chunk;
using namespace CTT::Archive;
using namespace CTT::CommonUtil;


bool ShaderListChunk::loadFromOrgBuffer()
{
	BYTE* orgBufferPnt = orgBuffer;

	UINT32 totalCount = *(UINT32*)orgBufferPnt;
	orgBufferPnt += sizeof(UINT32);

	// 오프셋 데이터는 기존 방식과 조금 다름
	// 오프셋이 바로 다음에 붙어나온다
	UINT32* offsetPnt = (UINT32*)orgBufferPnt;

	// value 초기화
	for (UINT32 i = 0; i < value.size();i++) {
		if (value[i] != NULL) {
			delete value[i];
		}
	}
	value.clear();

	wcout << infoPrefix << L"ChunkType: " << uppercase << hex << getNameUni(info.type) << L": loading " << to_wstring(totalCount) << L" data from buffer..." << endl;
	for (UINT32 i = 0; i < totalCount; i++) {
		//wcout << flush << L"\r";
		//wcout << infoPrefix << L"loading " << to_wstring(i + 1) << L"/" << to_wstring(totalCount) << L" data from buffer...";
		Shader* data = new Shader();
		
		// 필요한 버퍼 크기 판별
		UINT32 eachBufferSize = 0;

		// offsetPnt[i] 값이 0인 경우가 있을 줄은 몰랐음... 이때는 NULL을 푸시해 주고 넘긴다
		if (offsetPnt[i] == 0) {
			value.push_back(NULL);
			continue;
		}
		else {
			// 0이 아닌 값이 나올때까지, 그리고 크기 제한 내에서 돌림
			UINT32 nextNotZeroValueIndex = (i + 1);
			for (; (offsetPnt[nextNotZeroValueIndex] != 0) && (nextNotZeroValueIndex < totalCount); nextNotZeroValueIndex++) { ; }
			if (nextNotZeroValueIndex == totalCount) {
				// nextNotZeroValueIndex가 totalCount가 되었다는 건, 0이 아닌 값을 찾지 못하거나 애초에 끝이라는 뜻이므로 그냥 크기차를 계산해서 넣어준다
				eachBufferSize = orgSize - offsetPnt[i];
			}
			else {
				// nextNotZeroValueIndex가 totalCount가 아니라는 것은, 끝이 아닌 데서 0이 아닌 값을 찾았다는 뜻이므로 그 인덱스와의 값차를 계산해서 넣어준다
				eachBufferSize = orgSize - offsetPnt[nextNotZeroValueIndex];
			}
		}

		if (eachBufferSize > orgSize) {
			delete data;
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getNameUni(info.type) << L": 오프셋 데이터가 깨졌습니다." << endl;
			return false;
		}

		// 버퍼 크기만큼 복사
		BYTE* eachBuffer = (BYTE*)malloc(eachBufferSize);
		memcpy(eachBuffer, (orgBuffer + offsetPnt[i]), eachBufferSize);
		BYTE* eachBufferPnt = eachBuffer;

		// 기존과 똑같이 로드, 다만 totalDataSize를 세팅해줘야 정상적으로 로드할 수 있다
		data->setTotalDataSize(eachBufferSize);
		if (!data->loadFromBuffer(eachBufferPnt)) {
			delete data;
			free(eachBuffer);
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getNameUni(info.type) << L": 데이터를 로드하지 못했습니다." << endl;
			return false;
		}
		value.push_back(data);

		// 정리
		free(eachBuffer);
	}

	free(orgBuffer);
	orgBuffer = NULL;
	return true;
}


bool ShaderListChunk::saveIntoOrgBuffer()
{
	// *** 미구현 ***

	return true;
}