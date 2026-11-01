#include "TwoFivePlusImage.h"
#include "../../Archive/Archive.h"
#include "../../Util/CommonUtil.h"
#include "../../Util/Lz4Util.h"

using namespace CTT::Chunk;
using namespace CTT::Archive;
using namespace CTT::CommonUtil;
using namespace CTT::Lz4Util;


TwoFivePlusImage::TwoFivePlusImage()
{
	memset(&additional, 0, sizeof(AdditionalImageInfo));
	memset(&info, 0, sizeof(CommonImageInfo));
}


TwoFivePlusImage::~TwoFivePlusImage()
{
}


bool TwoFivePlusImage::loadFromBuffer(BYTE*& bufferPnt)
{
	// 이미지를 그대로 다 가지고 있기에는 용량이 너무 크기 때문에
	// [구조체] + [압축데이터] 정도로만 가지고 있다가
	// 이미지를 추출할 때 일시적으로 압축을 푸는 식으로 운용하는 게 좋겠다

	memcpy(&additional, bufferPnt, sizeof(AdditionalImageInfo));
	bufferPnt += sizeof(AdditionalImageInfo);
	additional.handle -= 1;

	memcpy(&info, bufferPnt, sizeof(CommonImageInfo));
	bufferPnt += sizeof(CommonImageInfo);

	translatingSize = *(UINT32*)(bufferPnt);
	bufferPnt += sizeof(UINT32);

	compressedSize = info.size - sizeof(UINT32);
	compressedData = (BYTE*)malloc(compressedSize);
	memcpy(compressedData, bufferPnt, compressedSize);
	bufferPnt += compressedSize;

	return true;
}


bool TwoFivePlusImage::saveIntoBuffer(BYTE*& bufferPnt)
{
	// 압축버퍼의 크기 및 기타 데이터는 이미 다 반영된 상태로 가정한다
	// handle 값은 위치만 잘 보정해주면 큰 문제는 없을거같고,
	// unknown2는 이번엔 0x9F라고 되어있어서 좀 애매함...

	memcpy(bufferPnt, &additional, sizeof(AdditionalImageInfo));
	((AdditionalImageInfo*)bufferPnt)->handle += 1;
	bufferPnt += sizeof(AdditionalImageInfo);

	memcpy(bufferPnt, &info, sizeof(CommonImageInfo));
	bufferPnt += sizeof(CommonImageInfo);

	*(UINT32*)(bufferPnt) = translatingSize;
	bufferPnt += sizeof(UINT32);

	memcpy(bufferPnt, compressedData, compressedSize);
	bufferPnt += compressedSize;

	return true;
}


// 데이터 렌더링용 함수
void TwoFivePlusImage::renderData(UINT32 index)
{
	stringstream oss;

	// 인덱스 표현
	oss << "  <" << index << ">{";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	// *** 출력 *** //
	ImGui::Text("  }");
}


bool TwoFivePlusImage::deriveTranslatingData()
{
	if ((compressedData == NULL) || (compressedSize == 0)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 디코딩을 수행할 데이터가 없습니다." << endl;
		return false;
	}

	if (translatingData != NULL) {
		free(translatingData);
		translatingData = NULL;
	}

	if (!Lz4Util::decode(compressedData, compressedSize, translatingData, translatingSize)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 이미지 데이터 디코딩을 실패했습니다." << endl;
		return false;
	}

	// 메모리 관리를 위하여 압축 데이터 제거
	free(compressedData);
	compressedData = NULL;
	compressedSize = 0;

	return true;
}


bool TwoFivePlusImage::deriveCompressedData()
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

	if (!Lz4Util::encode(translatingData, translatingSize, compressedData, compressedSize)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 이미지 데이터 인코딩을 실패했습니다." << endl;
		return false;
	}

	// info.size 재세팅
	info.size = sizeof(UINT32) + compressedSize;

	// 메모리 관리를 위하여 변환 데이터 제거, translatingSize는 버퍼 기록시 필요하므로 초기화하지 않는다
	free(translatingData);
	translatingData = NULL;
	//translatingSize = 0;

	return true;
}