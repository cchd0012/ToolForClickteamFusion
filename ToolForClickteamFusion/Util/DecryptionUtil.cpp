#include "DecryptionUtil.h"
#include "../Archive/Archive.h"
#include "../Common.h"

#define MAGIC_BYTE		0x36

using namespace CTT::DecryptionUtil;
using namespace CTT::Archive;


// 빌드 버전이 284를 넘으면 OldDecryptionInit, 284 이하면 NewDecryptionInit
void initializeOldDecryptionTable(UINT32 tableIndex);
void initializeNewDecryptionTable(UINT32 tableIndex);
void addKeyStr(vector<BYTE>& BYTEs, wstring& keyStr);

vector<BYTE> getMixedKey(vector<BYTE>& seedBytes);
void setDecryptionTable(vector<BYTE>& magicKey, UINT32 tableIndex);

// index가 0이면 decrypt용, 1이면 encrypt용
BYTE forDecEncTables[2][0x100];


void CTT::DecryptionUtil::initializeDecryptionTable()
{
	if (getArchive()->getBuildVersion() > 0x11C) {
		initializeNewDecryptionTable(0);
	}
	else {
		initializeOldDecryptionTable(0);
	}
}


void CTT::DecryptionUtil::initializeEncryptionTable()
{
	if (getArchive()->getBuildVersion() > 0x11C) {
		initializeNewDecryptionTable(1);
	}
	else {
		initializeOldDecryptionTable(1);
	}
}



void initializeOldDecryptionTable(UINT32 tableIndex)
{
	wstring appNameStr = getArchive()->getAppName();
	wstring appEditorFileNameStr = getArchive()->getEditorFileName();
	wstring appCopyright = getArchive()->getCopyright();

	vector<BYTE> seedKeyData;
	addKeyStr(seedKeyData, appEditorFileNameStr);
	addKeyStr(seedKeyData, appNameStr);
	addKeyStr(seedKeyData, appCopyright);

	vector<BYTE> mixedKey = getMixedKey(seedKeyData);
	setDecryptionTable(mixedKey, tableIndex);
}


void initializeNewDecryptionTable(UINT32 tableIndex)
{
	wstring appNameStr = getArchive()->getAppName();
	wstring appEditorFileNameStr = getArchive()->getEditorFileName();
	wstring appCopyright = getArchive()->getCopyright();

	vector<BYTE> seedKeyData;
	addKeyStr(seedKeyData, appNameStr);
	addKeyStr(seedKeyData, appCopyright);
	addKeyStr(seedKeyData, appEditorFileNameStr);

	vector<BYTE> mixedKey = getMixedKey(seedKeyData);
	setDecryptionTable(mixedKey, tableIndex);
}


void addKeyStr(vector<BYTE>& BYTEs, wstring &keyStr)
{
	for (size_t i = 0; i < keyStr.size();i++) {
		WORD value = keyStr[i];
		BYTE v1 = (BYTE)(value & 0xFF);
		if (v1 != 0) {
			BYTEs.push_back(v1);
		}
		BYTE v2 = (BYTE)((value >> 8) & 0xFF);
		if (v2 != 0) {
			BYTEs.push_back(v2);
		}
	}
}


vector<BYTE> getMixedKey(vector<BYTE>& seedBytes)
{
	vector<BYTE> tmpBytes = seedBytes;
	if (tmpBytes.size() > 0x80) {
		tmpBytes.resize(0x80);
	}

	BYTE lastKeyByte = MAGIC_BYTE;
	BYTE v34 = MAGIC_BYTE;
	vector<BYTE> result;
	result.resize(0x100);
	memcpy(result.data(), tmpBytes.data(), tmpBytes.size());

	for (size_t i = 0; i <= tmpBytes.size(); i++) {
		v34 = (BYTE)((v34 << 7) + (v34 >> 1));
		result[i] ^= v34;
		lastKeyByte += (BYTE)(result[i] * ((v34 & 1) + 2));
	}
	result[tmpBytes.size() + 1] = lastKeyByte;

	return result;
}


void setDecryptionTable (vector<BYTE>& mixedBytes, UINT32 tableIndex)
{
	BYTE* decryptionTable = forDecEncTables[tableIndex];
	for (int i = 0; i < 0x100; i++)
	{
		decryptionTable[i] = (BYTE)i;
	}

	BYTE hash = MAGIC_BYTE;
	BYTE changeIndex = 0;
	BYTE key = 0;

	for (UINT32 i = 0; i < 0x100; ++i, ++key){

		hash = (BYTE)((hash << 7) | (hash >> 1));
		if (hash == mixedBytes[key])
		{
			hash = (BYTE)((MAGIC_BYTE << 7) | (MAGIC_BYTE >> 1));
			key = 0;
		}

		changeIndex += (BYTE)((hash ^ mixedBytes[key]) + decryptionTable[i]);

		BYTE t = decryptionTable[changeIndex];
		decryptionTable[changeIndex] = decryptionTable[i];
		decryptionTable[i] = t;
	}
}


void CTT::DecryptionUtil::decrypt(BYTE* buffer, UINT32 bufferSize)
{
	UINT32 v1 = 0;
	UINT32 v2 = 0;
	UINT32 v3 = 0;
	UINT32 v4 = 0;
	UINT32 v5 = 0;
	BYTE copyDecryptionTable[0x100];

	// decryption 테이블에서 복사
	memcpy(copyDecryptionTable, forDecEncTables[0], 0x100);

	for (UINT32 i = 0; i < bufferSize; i++) {

		v1 = (v1 + 1) & 0xFF;
		v3 = copyDecryptionTable[v1];
		v2 = (v3 + v5) & 0xFF;
		v5 = v2;

		v4 = v5;
		v2 = copyDecryptionTable[v2];
		copyDecryptionTable[v1] = v2;
		copyDecryptionTable[v4] = v3;

		v2 = (v2 + v3) & 0xFF;
		v2 = copyDecryptionTable[v2];
		buffer[i] ^= v2;

	}
}


void CTT::DecryptionUtil::encrypt(BYTE* buffer, UINT32 bufferSize)
{
	UINT32 v1 = 0;
	UINT32 v2 = 0;
	UINT32 v3 = 0;
	UINT32 v4 = 0;
	UINT32 v5 = 0;
	BYTE copyEncryptionTable[0x100];

	// encryption 테이블에서 복사
	memcpy(copyEncryptionTable, forDecEncTables[1], 0x100);

	for (UINT32 i = 0; i < bufferSize; i++) {

		v1 = (v1 + 1) & 0xFF;
		v3 = copyEncryptionTable[v1];
		v2 = (v3 + v5) & 0xFF;
		v5 = v2;

		v4 = v5;
		v2 = copyEncryptionTable[v2];
		copyEncryptionTable[v1] = v2;
		copyEncryptionTable[v4] = v3;

		v2 = (v2 + v3) & 0xFF;
		v2 = copyEncryptionTable[v2];
		buffer[i] ^= v2;

	}
}