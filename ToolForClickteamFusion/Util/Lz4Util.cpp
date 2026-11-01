#include "Lz4Util.h"
#include "CommonUtil.h"
#include <liblz4/lz4.h>
#include <liblz4/lz4hc.h>

using namespace CTT::Lz4Util;
using namespace CTT::CommonUtil;

#pragma warning (disable:6031)
#pragma warning (disable:4996)


bool CTT::Lz4Util::decode(BYTE* encodedData, UINT32 encodedSize, BYTE*& decodedData, UINT32& decodedSize, bool isStrict)
{
	UINT32 tempDecodedSize = decodedSize;
	BYTE* tempDecodedData = (BYTE*)malloc(tempDecodedSize);
	int realDecodedSize = LZ4_decompress_safe((char*)encodedData, (char*)tempDecodedData, encodedSize, tempDecodedSize);
	if (realDecodedSize >= 0) {
		if (realDecodedSize != tempDecodedSize) {
			if (isStrict) {
				wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 디코딩된 크기가 다릅니다 ("
					<< L"expected: 0x" << uppercase << hex << decodedSize
					<< L", result: 0x" << uppercase << hex << realDecodedSize
					<< L")" << endl;
				free(tempDecodedData);
				return false;
			}
			decodedData = (BYTE*)malloc(realDecodedSize);
			memcpy(decodedData, tempDecodedData, realDecodedSize);
			free(tempDecodedData);
			decodedSize = realDecodedSize;
		}
		else {
			decodedData = tempDecodedData;
		}
		return true;
	}
	else {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 디코딩 실패" << endl;
		free(tempDecodedData);
		return false;
	}
}


bool CTT::Lz4Util::encode(BYTE* decodedData, UINT32 decodedSize, BYTE*& encodedData, UINT32& encodedSize)
{
	UINT32 tempEncodedSize = decodedSize * 4;
	BYTE* tempEncodedData = (BYTE*)malloc(tempEncodedSize);
	int realEncodedSize = LZ4_compress_HC((char*)decodedData, (char*)tempEncodedData, decodedSize, tempEncodedSize, LZ4HC_CLEVEL_MAX);
	if (realEncodedSize >= 0) {
		// 잘 인코딩되면 크기 맞춰서 반환
		encodedSize = realEncodedSize;
		encodedData = (BYTE*)malloc(encodedSize);
		memcpy(encodedData, tempEncodedData, encodedSize);
		free(tempEncodedData);
		return true;
	}
	else {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 인코딩 실패" << endl;
		free(tempEncodedData);
		return false;
	}
}