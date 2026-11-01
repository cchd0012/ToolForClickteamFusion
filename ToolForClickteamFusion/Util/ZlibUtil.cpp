#include "ZlibUtil.h"
#include "CommonUtil.h"
#include <zlib-1.2.3/zlib.h>

using namespace CTT::ZlibUtil;
using namespace CTT::CommonUtil;

#pragma warning (disable:6031)
#pragma warning (disable:4996)


bool CTT::ZlibUtil::decode(BYTE* encodedData, UINT32 encodedSize, BYTE*& decodedData, UINT32& decodedSize, bool isStrict)
{
	UINT32 tempDecodedSize = decodedSize;
	BYTE* tempDecodedData = (BYTE*)malloc(tempDecodedSize);
	if (uncompress(tempDecodedData, (ULONG*)&tempDecodedSize, encodedData, encodedSize) == Z_OK) {
		if (decodedSize != tempDecodedSize) {
			if (isStrict) {
				wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 디코딩된 크기가 다릅니다 ("
					<< L"expected: 0x" << uppercase << hex << decodedSize
					<< L", result: 0x" << uppercase << hex << tempDecodedSize
					<< L")" << endl;
				free(tempDecodedData);
				return false;
			}
			decodedSize = tempDecodedSize;
		}
		decodedData = tempDecodedData;
		return true;
	}
	else {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 디코딩 실패" << endl;
		decodedData = tempDecodedData;
		decodedSize = tempDecodedSize;
		return true;
		//free(tempDecodedData);
		//return false;
	}
}


bool CTT::ZlibUtil::encode(BYTE* decodedData, UINT32 decodedSize, BYTE*& encodedData, UINT32& encodedSize)
{
	UINT32 tempEncodedSize = (decodedSize * 3) + 0x200;
	BYTE* tempEncodedData = (BYTE*)malloc(tempEncodedSize);
	if (compress2(tempEncodedData, (ULONG*)&tempEncodedSize, decodedData, decodedSize, Z_BEST_COMPRESSION) == Z_OK) {
		// 잘 인코딩되면 크기 맞춰서 반환
		encodedSize = tempEncodedSize;
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