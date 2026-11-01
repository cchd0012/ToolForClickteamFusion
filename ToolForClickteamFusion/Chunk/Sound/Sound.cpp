#include "Sound.h"
#include "../../Archive/Archive.h"
#include "../../Util/ZlibUtil.h"
#include "../../Util/CommonUtil.h"

using namespace CTT::Chunk;
using namespace CTT::Archive;
using namespace CTT::ZlibUtil;
using namespace CTT::CommonUtil;


Sound::Sound() 
{
	memset(&info, 0, sizeof(CommonSoundInfo));
	compressedData = NULL;
	compressedSize = 0;
	soundData = NULL;
	soundSize = 0;
}


Sound::~Sound() 
{
	if (compressedData != NULL) {
		free(compressedData);
		compressedData = NULL;
	}
	if (soundData != NULL) {
		free(soundData);
		soundData = NULL;
	}
}


bool Sound::exportData(wstring& dirPath, UINT32 index, bool isFullyExport)
{
	// 압축 데이터에서 사운드 데이터 뽑아내기
	if (!deriveSoundData()) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 사운드 데이터를 얻지 못했습니다. (buffer)" << endl;
		return false;
	}

	// 확장자는 내부 데이터를 가지고 판단한다
	string oggMagic = "OggS";
	string wavMagic = "RIFF";
	string mp3Magic = "ID3";
	string mp3Magic2 = { (char)0xFF, (char)0xFB, (char)0x50, (char)0xC4 };
	wstring extention;
	if (memcmp(soundData, oggMagic.c_str(), oggMagic.size()) == 0) {
		extention = L"ogg";
	}
	else if (memcmp(soundData, wavMagic.c_str(), wavMagic.size()) == 0) {
		extention = L"wav";
	}
	else if ((memcmp(soundData, mp3Magic.c_str(), mp3Magic.size()) == 0)
		|| (memcmp(soundData, mp3Magic2.c_str(), mp3Magic2.size()) == 0)) {
		extention = L"mp3";
	}
	else {
		extention = L"unk";
	}

	// 추출 파일 경로 생성, 내부 데이터를 가지고 판단한다
	wostringstream woss;
	woss << setw(6) << setfill(L'0') << index;
	wstring exportFilePath = dirPath + L"/" + woss.str() + L"_" + name + L"." + extention;

	wcout << infoPrefix << L"exporting " << exportFilePath << L"..." << endl;

	DWORD dWrite;

	// 그대로 기록
	HANDLE hSound = CreateFileW(exportFilePath.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hSound == INVALID_HANDLE_VALUE) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(exportFilePath) << L": 사운드 파일을 추출하지 못했습니다." << endl;
		return false;
	}
	WriteFile(hSound, soundData, soundSize, &dWrite, NULL);
	CloseHandle(hSound);

	// 메모리 관리를 위해 soundData 해제
	free(soundData);
	soundData = NULL;

	return true;
}


bool Sound::importData(wstring& dirPath, UINT32 index)
{
	// 압축 데이터에서 이름 데이터를 뽑아내기. 비효율적이지만 다른 방법이 없다...
	// 그런데 지금 보니까 내부 데이터를 봐야 확장자를 파악할 수가 있어서 어차피 필요할 수밖에 없음
	if (!deriveSoundData()) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": name 데이터를 얻지 못했습니다. (buffer)" << endl;
		return false;
	}

	// 확장자는 내부 데이터를 가지고 판단한다
	string oggMagic = "OggS";
	string wavMagic = "RIFF";
	string mp3Magic = "ID3";
	string mp3Magic2 = { (char)0xFF, (char)0xFB, (char)0x50, (char)0xC4 };
	wstring extention;
	if (memcmp(soundData, oggMagic.c_str(), oggMagic.size()) == 0) {
		extention = L"ogg";
	}
	else if (memcmp(soundData, wavMagic.c_str(), wavMagic.size()) == 0) {
		extention = L"wav";
	}
	else if ((memcmp(soundData, mp3Magic.c_str(), mp3Magic.size()) == 0)
		|| (memcmp(soundData, mp3Magic2.c_str(), mp3Magic2.size()) == 0)) {
		extention = L"mp3";
	}
	else {
		extention = L"unk";
	}

	// import 파일 경로 생성
	wostringstream woss;
	woss << setw(6) << setfill(L'0') << index;
	wstring importFilePath = dirPath + L"/" + woss.str() + L"_" + name + L"." + extention;

	// 존재 확인, 있다면 실제로 import하고, 없다면 그대로 유지한다
	// 다른 것들과 다른 이유는 이 상태에서 압축된 데이터가 없고 원래 데이터만 있기 때문이다
	if (isFileOrDir(importFilePath) == FSOType::FSO_FILE) {

		wcout << infoPrefix << L"importing " << importFilePath << L"..." << endl;

		DWORD dRead;

		if (soundData != NULL) {
			free(soundData);
			soundData = NULL;
		}
		soundSize = 0;

		// 그대로 읽어들이기
		HANDLE hSound = CreateFileW(importFilePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hSound == INVALID_HANDLE_VALUE) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(importFilePath) << L": 사운드 파일을 읽어들이지 못했습니다." << endl;
			return false;
		}
		soundSize = GetFileSize(hSound, NULL);
		soundData = (BYTE*)malloc(soundSize);
		ReadFile(hSound, soundData, soundSize, &dRead, NULL);
		CloseHandle(hSound);

	}

	// 사운드 데이터에서 압축 데이터 뽑아내기
	if (!deriveCompressedData()) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 압축된 데이터를 얻지 못했습니다. (buffer)" << endl;
		return false;
	}

	return true;
}


bool Sound::loadFromBuffer(BYTE*& bufferPnt)
{
	memcpy(&info, bufferPnt, sizeof(CommonSoundInfo));
	bufferPnt += sizeof(CommonSoundInfo);
	if (getArchive()->getBuildVersion() >= 0x11C) {
		info.handle -= 1;
	}

	compressedSize = *(UINT32*)(bufferPnt);
	bufferPnt += sizeof(UINT32);

	if (compressedData != NULL) {
		free(compressedData);
		compressedData = NULL;
	}

	compressedData = (BYTE*)malloc(compressedSize);
	memcpy(compressedData, bufferPnt, compressedSize);
	bufferPnt += compressedSize;

	return true;
}


bool Sound::saveIntoBuffer(BYTE*& bufferPnt)
{
	memcpy(bufferPnt, &info, sizeof(CommonSoundInfo));
	if (getArchive()->getBuildVersion() >= 0x11C) {
		((CommonSoundInfo*)bufferPnt)->handle += 1;
	}
	bufferPnt += sizeof(CommonSoundInfo);

	*(UINT32*)(bufferPnt) = compressedSize;
	bufferPnt += sizeof(UINT32);

	memcpy(bufferPnt, compressedData, compressedSize);
	bufferPnt += compressedSize;

	return true;
}


// 데이터 렌더링용 함수
void Sound::renderData(UINT32 index)
{
	stringstream oss;

	// 인덱스 표현
	oss << "  <" << index << ">{";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	// *** 출력 *** //
	ImGui::Text("  }");
}


bool Sound::deriveSoundData()
{
	if ((compressedData == NULL) || (compressedSize == 0)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 디코딩을 수행할 데이터가 없습니다." << endl;
		return false;
	}

	if (soundData != NULL) {
		free(soundData);
		soundData = NULL;
	}
	soundSize = 0;

	// 압축 해제용 변수
	UINT32 tmpSize = info.orgSize;
	BYTE* tmpData = NULL;

	// 압축푼 데이터: [wchar형 파일명] + [00 00 ~ ] + [사운드 데이터]
	// ([wchar형 파일명] + [00 00 ~ ]) 길이는 nameLength * 2이며, 이 값은 4의 배수가 된다
	// 고로 orgSize는 (nameLength * sizeof(WCHAR)) + [사운드 데이터] 인 셈이다
	if (!ZlibUtil::decode(compressedData, compressedSize, tmpData, tmpSize)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 사운드 데이터 디코딩을 실패했습니다." << endl;
		if (tmpData != NULL) {
			free(tmpData);
		}
		return false;
	}

	// 이름 반영
	name = (WCHAR*)tmpData;

	// soundData 반영
	soundSize = (tmpSize - (info.nameLength * sizeof(WCHAR)));
	soundData = (BYTE*)malloc(soundSize);
	memcpy(soundData, (tmpData + (info.nameLength * sizeof(WCHAR))), soundSize);

	// 역할 끝난 임시변수 제거
	free(tmpData);

	// 메모리 관리를 위하여 압축 데이터 제거... 하려고 했는데, 이러니까 데이터가 기록이 안됨
	// 어쩔 수 없이 가지고 있어야 한다
	free(compressedData);
	compressedData = NULL;
	compressedSize = 0;

	return true;
}


bool Sound::deriveCompressedData()
{
	if ((soundData == NULL) || (soundSize == 0)) {
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

	// 이름 길이 세팅 (4의 배수가 되도록)
	info.nameLength = ((UINT32)(name.size() / 4) + 1) * 4;

	// 원본 크기 세팅
	info.orgSize = (info.nameLength * sizeof(WCHAR)) + soundSize;

	// 이름과 soundData를 그대로 복사해서 세팅한 후 압축 수행
	tmpSize = info.orgSize;
	tmpData = (BYTE*)malloc(tmpSize);
	memset(tmpData, 0, tmpSize);
	memcpy(tmpData, name.c_str(), (name.size() * sizeof(WCHAR)));
	memcpy(tmpData + (tmpSize - soundSize), soundData, soundSize);
	if (!ZlibUtil::encode(tmpData, tmpSize, compressedData, compressedSize)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 사운드 데이터 인코딩을 실패했습니다." << endl;
		return false;
	}

	// 필요없는 버퍼들 추가 해제
	free(tmpData);

	// 메모리 관리를 위하여 사운드 데이터 제거
	free(soundData);
	soundData = NULL;
	soundSize = 0;

	return true;
}