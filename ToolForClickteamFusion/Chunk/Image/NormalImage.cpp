#include "NormalImage.h"
#include "../../Archive/Archive.h"
#include "../../Util/ZlibUtil.h"
#include "../../Util/CommonUtil.h"

using namespace CTT::Chunk;
using namespace CTT::Archive;
using namespace CTT::ZlibUtil;
using namespace CTT::CommonUtil;


NormalImage::NormalImage() {
	handle = 0;
	originalSize = 0;
	memset(&info, 0, sizeof(CommonImageInfo));
}


NormalImage::~NormalImage() {
}


bool NormalImage::loadFromBuffer(BYTE*& bufferPnt)
{
	// 데이터를 그대로 다 가지고 있기에는 용량이 너무 크기 때문에
	// [구조체] + [압축데이터] 정도로만 가지고 있다가
	// 추출할 때 일시적으로 압축을 푸는 식으로 운용하는 게 좋겠다

	handle = *(UINT32*)bufferPnt;
	if (getArchive()->getBuildVersion() >= 0x11C) {
		handle--;
	}
	bufferPnt += sizeof(UINT32);

	originalSize = *(UINT32*)bufferPnt;
	bufferPnt += sizeof(UINT32);

	if (compressedData != NULL) {
		free(compressedData);
		compressedData = NULL;
	}
	compressedSize = 0;

	compressedSize = *(UINT32*)bufferPnt;
	bufferPnt += sizeof(UINT32);

	compressedData = (BYTE*)malloc(compressedSize);
	memcpy(compressedData, bufferPnt, compressedSize);
	bufferPnt += compressedSize;

	return true;
}


bool NormalImage::saveIntoBuffer(BYTE*& bufferPnt)
{
	*(UINT32*)bufferPnt = handle;
	if (getArchive()->getBuildVersion() >= 0x11C) {
		(*(UINT32*)(bufferPnt))++;
	}
	bufferPnt += sizeof(UINT32);

	*(UINT32*)bufferPnt = originalSize;
	bufferPnt += sizeof(UINT32);

	*(UINT32*)bufferPnt = compressedSize;
	bufferPnt += sizeof(UINT32);

	memcpy(bufferPnt, compressedData, compressedSize);
	bufferPnt += compressedSize;

	return true;
}


// 데이터 렌더링용 함수
void NormalImage::renderData(UINT32 index)
{
	stringstream oss;

	// 인덱스 표현
	oss << "  <" << index << ">{";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	// *** 출력 *** //
	ImGui::Text("  }");
}


bool NormalImage::deriveTranslatingData()
{
	if ((compressedData == NULL) || (compressedSize == 0)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 디코딩을 수행할 데이터가 없습니다." << endl;
		return false;
	}

	if (translatingData != NULL) {
		free(translatingData);
		translatingData = NULL;
	}
	translatingSize = 0;

	// 압축 해제용 변수
	UINT32 tmpSize = originalSize;
	BYTE* tmpData = NULL;

	// 압축푼 데이터: [imageInfo] + [(translatingData 혹은 한번 더 압축된 데이터)]
	if (!ZlibUtil::decode(compressedData, compressedSize, tmpData, tmpSize)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 이미지 데이터 디코딩을 실패했습니다." << endl;
		if (tmpData != NULL) {
			free(tmpData);
		}
		return false;
	}

	memcpy(&info, tmpData, sizeof(CommonImageInfo));

	if (isLZX()) {
		// 한번 더 압축: LZX일 때: [원래 크기 4바이트] + [압축데이터]
		translatingSize = *(UINT32*)(tmpData + sizeof(CommonImageInfo));
		UINT32 startOfs = (sizeof(CommonImageInfo) + sizeof(UINT32));
		if (!ZlibUtil::decode((tmpData + startOfs), (tmpSize - startOfs), translatingData, translatingSize)) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 이미지 데이터 디코딩을 실패했습니다." << endl;
			free(tmpData);
			return false;
		}
		free(tmpData);
	}
	else {
		// LZX가 아닐 때: 변환데이터를 그대로 옮겨와서 복사
		translatingSize = (tmpSize - sizeof(CommonImageInfo));
		translatingData = (BYTE*)malloc(translatingSize);
		memcpy(translatingData, (tmpData + sizeof(CommonImageInfo)), translatingSize);
	}

	// 역할 끝난 임시변수 제거
	free(tmpData);

	// 메모리 관리를 위하여 압축 데이터 제거
	free(compressedData);
	compressedData = NULL;
	compressedSize = 0;

	return true;
}


bool NormalImage::deriveCompressedData()
{
	if ((translatingData == NULL) || (translatingSize == 0)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 인코딩을 수행할 데이터가 없습니다." << endl;
		return false;
	}

	if (compressedData != NULL) {
		free(compressedData);
		compressedData = NULL;
	}
	compressedSize = 0;

	UINT32 tmpSize = 0;
	BYTE* tmpData = NULL;

	// 플래그에 따라 데이터 변환
	if (isLZX()) {
		// [원래크기 4바이트] + [압축데이터]로 변경하여 세팅
		BYTE* tmpCompData = NULL;
		UINT32 tmpCompSize = 0;
		if (!ZlibUtil::encode(translatingData, translatingSize, tmpCompData, tmpCompSize)) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 이미지 데이터 인코딩을 실패했습니다(1)." << endl;
			if (tmpCompData != NULL) {
				free(tmpCompData);
			}
			return false;
		}
		tmpSize = sizeof(UINT32) + tmpCompSize;
		tmpData = (BYTE*)malloc(tmpSize);
		*(UINT32*)(tmpData) = translatingSize;
		memcpy((tmpData + sizeof(UINT32)), tmpCompData, tmpCompSize);
		free(tmpCompData);
	}
	else {
		// 그대로 복사해서 세팅
		tmpSize = translatingSize;
		tmpData = (BYTE*)malloc(tmpSize);
		memcpy(tmpData, translatingData, tmpSize);
	}

	// imageInfo 및 originalSize 수정
	info.size = tmpSize;
	originalSize = sizeof(CommonImageInfo) + tmpSize;

	// imageInfo와 tmpData를 붙여서 fullOrgData로 만들고 압축을 수행한다
	BYTE* fullOrgData = (BYTE*)malloc(originalSize);
	memcpy(fullOrgData, &info, sizeof(CommonImageInfo));
	memcpy((fullOrgData + sizeof(CommonImageInfo)), tmpData, tmpSize);
	if (!ZlibUtil::encode(fullOrgData, originalSize, compressedData, compressedSize)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 이미지 데이터 인코딩을 실패했습니다." << endl;
		return false;
	}

	// 필요없는 버퍼들 추가 해제
	free(tmpData);
	free(fullOrgData);

	// 메모리 관리를 위하여 변환 데이터 제거
	free(translatingData);
	translatingData = NULL;
	translatingSize = 0;

	return true;
}