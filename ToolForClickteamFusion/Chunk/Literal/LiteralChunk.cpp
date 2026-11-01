#include "BooleanChunk.h"
#include "IntegerChunk.h"
#include "UnsignedShortChunk.h"
#include "StringChunk.h"
#include "ListChunk.h"
#include "../../Util/CommonUtil.h"
#include "../../ImGUI/Headers.h"

using namespace CTT::Chunk;
using namespace CTT::CommonUtil;


// StringChunk 데이터 렌더링용
void StringChunk::renderData()
{
	stringstream oss;
	oss << " - \"" << getAnsiStr(value, CodePage::CP_UTF_8) << "\"";
	ImGui::Text(oss.str().c_str());
	oss.str("");
}


// IntegerChunk 데이터 렌더링용
void IntegerChunk::renderData()
{
	stringstream oss;
	oss << " - " << dec << value << " (0x" << hex << uppercase << value << ")";
	ImGui::Text(oss.str().c_str());
	oss.str("");
}


// UnsignedShortChunk 데이터 렌더링용
void UnsignedShortChunk::renderData()
{
	stringstream oss;
	oss << " - " << dec << value << " (0x" << hex << uppercase << value << ")";
	ImGui::Text(oss.str().c_str());
	oss.str("");
}


// BooleanChunk 데이터 렌더링용
void BooleanChunk::renderData()
{
	stringstream oss;
	oss << " - " << (value ? "true" : "false");
	ImGui::Text(oss.str().c_str());
	oss.str("");
}


// ListChunk<string> 데이터 렌더링용
void ListChunk<string>::renderData()
{
	// 최대 30개까지만
	stringstream oss;
	UINT32 maxCount = ((UINT32)value.size() > 30) ? 30 : (UINT32)value.size();
	oss << "[\n";
	for (UINT32 i = 0; i < maxCount; i++) {
		oss << "  \"" << value[i] << "\"";
		if ((i + 1) < (UINT32)value.size()) {
			oss << ",";
		}
		oss << "\n";
	}
	if (maxCount != (UINT32)value.size()) {
		oss << "  ...\n";
	}
	oss << "] (" << dec << (UINT32)value.size() << u8"개)";
	ImGui::Text(oss.str().c_str());
	oss.str("");
}


// ListChunk<UINT32> 데이터 렌더링용
void ListChunk<UINT32>::renderData()
{
	// 최대 30개까지만
	stringstream oss;
	UINT32 maxCount = ((UINT32)value.size() > 30) ? 30 : (UINT32)value.size();
	oss << "[\n";
	for (UINT32 i = 0; i < maxCount; i++) {
		oss << "  " << dec << value[i] << " (0x" << hex << uppercase << value[i] << ")";
		if ((i + 1) < (UINT32)value.size()) {
			oss << ",";
		}
		oss << "\n";
	}
	if (maxCount != (UINT32)value.size()) {
		oss << "  ...\n";
	}
	oss << "] (" << dec << (UINT32)value.size() << u8"개)";
	ImGui::Text(oss.str().c_str());
	oss.str("");
}




// ListChunk<string> 데이터
bool ListChunk<string>::loadFromOrgBuffer() 
{
	// chunk 데이터: [string 리스트(vector)]
	BYTE* orgBufferPnt = orgBuffer;

	UINT32 count = 0;

	// CHUNK_APP_OTHER_EXTENTIONS일 경우는 UINT32가 아니라 UINT16만큼 더해야 한다
	if (info.type == ChunkType::CHUNK_APP_OTHER_EXTENTIONS) {
		count = *(UINT16*)orgBufferPnt;
		orgBufferPnt += sizeof(UINT16);
	}
	else {
		count = *(UINT32*)orgBufferPnt;
		orgBufferPnt += sizeof(UINT32);
	}

	value.resize(count);
	for (UINT32 i = 0; i < count;i++) {
		wchar_t* wstrRaw = (wchar_t*)orgBufferPnt;
		wstring wstr = (wchar_t*)wstrRaw;
		orgBufferPnt += ((wcslen(wstrRaw) + 1) * sizeof(wchar_t));
		value.push_back(getAnsiStr(wstr, CodePage::CP_UTF_8));
	}

	free(orgBuffer);
	orgBuffer = NULL;
	return true;
}


bool ListChunk<string>::saveIntoOrgBuffer()
{
	if (orgBuffer != NULL) {
		free(orgBuffer);
		orgBuffer = NULL;
	}
	
	// 크기 계산

	// CHUNK_APP_OTHER_EXTENTIONS일 경우는 UINT32가 아니라 UINT16만큼 더해야 한다
	if (info.type == ChunkType::CHUNK_APP_OTHER_EXTENTIONS) {
		orgSize = sizeof(UINT16);
	}
	else {
		orgSize = sizeof(UINT32);
	}

	for (UINT32 i = 0; i < value.size(); i++) {
		wstring wstr = getUniStr(value[i], CodePage::CP_UTF_8);
		orgSize += ((wcslen(wstr.c_str()) + 1) * sizeof(wchar_t));
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

	// CHUNK_APP_OTHER_EXTENTIONS일 경우는 UINT32가 아니라 UINT16만큼 더해야 한다
	if (info.type == ChunkType::CHUNK_APP_OTHER_EXTENTIONS) {
		*(UINT16*)(orgBufferPnt) = value.size();
		orgBufferPnt += sizeof(UINT16);
	}
	else {
		*(UINT32*)(orgBufferPnt) = value.size();
		orgBufferPnt += sizeof(UINT32);
	}

	for (UINT32 i = 0; i < value.size(); i++) {
		wstring wstr = getUniStr(value[i], CodePage::CP_UTF_8);
		memcpy(orgBufferPnt, wstr.c_str(), (wcslen(wstr.c_str()) * sizeof(wchar_t)));
		orgBufferPnt += ((wcslen(wstr.c_str()) + 1) * sizeof(wchar_t));
	}

	return true;
}


// ListChunk<UINT32> 데이터
bool ListChunk<UINT32>::loadFromOrgBuffer() 
{
	// chunk 데이터: [UINT32 리스트(vector)]
	UINT32 count = orgSize / sizeof(UINT32);
	value.resize(count);
	memcpy(value.data(), orgBuffer, (sizeof(UINT32) * count));
	free(orgBuffer);
	orgBuffer = NULL;
	return true;
}


bool ListChunk<UINT32>::saveIntoOrgBuffer()
{
	if (orgBuffer != NULL) {
		free(orgBuffer);
		orgBuffer = NULL;
	}
	orgSize = sizeof(UINT32) * value.size();
	orgBuffer = (BYTE*)malloc(orgSize);
	memcpy(orgBuffer, value.data(), orgSize);
	return true;
}