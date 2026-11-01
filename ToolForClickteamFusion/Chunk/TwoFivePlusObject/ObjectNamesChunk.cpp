#include "ObjectNamesChunk.h"
#include "../../Archive/Archive.h"
#include "../../Util/CommonUtil.h"

using namespace CTT::Chunk;
using namespace CTT::Archive;
using namespace CTT::CommonUtil;


bool ObjectNamesChunk::loadFromOrgBuffer()
{
	// chunk 데이터: [string 리스트(vector)]
	BYTE* orgBufferPnt = orgBuffer;

	UINT32 count = 0;

	value.clear();
	while (orgBufferPnt < (orgBuffer + orgSize)) {
		wchar_t* wstrRaw = (wchar_t*)orgBufferPnt;
		wstring wstr = (wchar_t*)wstrRaw;
		orgBufferPnt += ((wcslen(wstrRaw) + 1) * sizeof(wchar_t));
		value.push_back(getAnsiStr(wstr, CodePage::CP_UTF_8));
	}

	free(orgBuffer);
	orgBuffer = NULL;
	return true;
}


bool ObjectNamesChunk::saveIntoOrgBuffer()
{
	if (orgBuffer != NULL) {
		free(orgBuffer);
		orgBuffer = NULL;
	}

	// 크기 계산
	orgSize = 0;
	for (UINT32 i = 0; i < value.size(); i++) {
		wstring wstr = getUniStr(value[i], CodePage::CP_UTF_8);
		orgSize += ((UINT32)(wcslen(wstr.c_str()) + 1) * sizeof(wchar_t));
	}

	// 버퍼 할당
	if (orgBuffer != NULL) {
		free(orgBuffer);
		orgBuffer = NULL;
	}
	orgBuffer = (BYTE*)malloc(orgSize);
	memset(orgBuffer, 0, orgSize);

	// orgBuffer에 기록
	BYTE* orgBufferPnt = orgBuffer;

	for (UINT32 i = 0; i < value.size(); i++) {
		wstring wstr = getUniStr(value[i], CodePage::CP_UTF_8);
		memcpy(orgBufferPnt, wstr.c_str(), (wcslen(wstr.c_str()) * sizeof(wchar_t)));
		orgBufferPnt += ((wcslen(wstr.c_str()) + 1) * sizeof(wchar_t));
	}

	return true;
}