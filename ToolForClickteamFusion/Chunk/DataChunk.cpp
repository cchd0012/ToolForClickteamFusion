#include "DataChunk.h"
#include "../Archive/Archive.h"
#include "../Util/JsonUtil.h"
#include "../Util/CommonUtil.h"
#include "../ImGUI/Headers.h"

using namespace CTT::Chunk;
using namespace CTT::Archive;
using namespace CTT::JsonUtil;
using namespace CTT::CommonUtil;


DataChunk::DataChunk()
{
}


DataChunk::~DataChunk()
{
	for (size_t i = 0; i < value.size(); i++) {
		if (value[i] != NULL) {
			delete value[i];
		}
	}
	value.clear();
}


bool DataChunk::exportData(wstring& exportDirPath, bool isFullyExport)
{
	// 디렉토리 생성
	createDirectoryRecursively(exportDirPath);

	// exportData를 호출할 당시 name이 지정돼있지 않은 경우가 있어서
	// 경로를 넣는 대신 추출 디렉토리, 추출 인덱스, 추출 확장자를 직접 넣어줘야 할거같다
	// 아니면 추출을 수행한 후 넣어주는 것도 좋을 듯함

	// 모든 데이터 추출
	for (size_t i = 0; i < value.size(); i++) {
		// 추출 개수 갱신
		getStates()->currentDataIndex++;
		// NULL 대비 코드
		if (value[i] == NULL) {
			continue;
		}
		// 추출
		if (!value[i]->exportData(exportDirPath, (UINT32)i, isFullyExport)) {
			return false;
		}
	}
	return true;
}


bool DataChunk::importData(wstring& importDirPath)
{
	// 경로 존재 확인
	bool isDirExists = true;
	if (isFileOrDir(importDirPath) != FSOType::FSO_DIRECTORY) {
		wcout << infoPrefix << importDirPath << L": 해당 디렉토리가 존재하지 않으므로 교체 없이 진행됩니다";
		isDirExists = false;
	}

	// importData를 호출할 당시 name이 지정돼있지 않은 경우가 있어서
	// 경로를 넣는 대신 import 디렉토리, import 인덱스, import 확장자를 직접 넣어줘야 할거같다
	// 하지만 이런 식으로 하면 파일을 추가하기가 엄청 까다로움.
	// 그러니까 리스트에서 참조하는 식으로 바꿔야 하긴 한다
	// offsetList는 덤인가?

	// TrueTypeFont 때문에 여기서 미리 수행을 해야 offsetList를 보존할 수 있다
	offsetList.clear();
	UINT32 accumOffset = 0;

	// 존재하는 데이터는 전부 교체
	for (size_t i = 0; i < value.size(); i++) {
		// NULL 대비 코드
		if (value[i] == NULL) {
			continue;
		}
		// import 개수 갱신
		getStates()->currentDataIndex++;
		// 데이터가 존재하면 그냥 replace, 없다면 교체 없이 import
		if (!value[i]->importData(importDirPath, (UINT32)i)) {
			return false;
		}
		offsetList.push_back(accumOffset);
		accumOffset += value[i]->getDataSize();
	}

	return true;
}


bool DataChunk::exportJson(wstring& exportJsonPath)
{
	Json::Value root;

	// json에 전부 추가
	for (UINT32 i = 0; i < value.size(); i++) {
		// NULL 대비 코드
		if (value[i] == NULL) {
			continue;
		}
		// 추출 개수 갱신
		getStates()->currentDataIndex++;
		// 추출
		Json::Value exportValue;
		if (!value[i]->exportJson(exportValue, offsetList)) {
			return false;
		}
		root.append(exportValue);
	}

	if (!writeIntoJson(root, exportJsonPath)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": DataChunk: json 데이터를 기록하지 못했습니다." << endl;
		return false;
	}
	else {
		return true;
	}
}


bool DataChunk::importJson(wstring& importJsonPath)
{
	Json::Value root;
	if (!readFromJson(root, importJsonPath)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": DataChunk: json 데이터를 읽어들이지 못했습니다." << endl;
		return false;
	}

	// json으로부터 전부 추가, 최대 크기만큼. ...아니면 그냥 전부 다 읽어들일까?
	UINT32 maxCount = (root.size() > value.size()) ? (UINT32)value.size() : (UINT32)root.size();
	for (UINT32 i = 0; i < maxCount;i++) {
		// NULL 대비 코드
		if (value[i] == NULL) {
			continue;
		}
		// import 개수 갱신
		getStates()->currentDataIndex++;
		// import
		if (!value[i]->importJson(root[i], offsetList)) {
			return false;
		}
	}

	return true;
}


void DataChunk::fillOffsets(ListChunk<UINT32>& offsetsChunk)
{
	offsetsChunk.setValue(offsetList);
}


bool DataChunk::readOnlyDataCount(HANDLE hArchive)
{
	DWORD dRead;

	// Data Chunk는 depth를 조절할 필요는 없으나, 개수는 저장해야 한다
	// 또한 개수 정보만 읽으면 되니 전부 다 읽을 필요도 없긴 한데...
	// 일단 DataChunk 자체는 암호화 및 압축되지 않은 것으로 판단하고 4바이트만 읽도록 카운트한다
	// 가 아니라, 분명 압축된 데이터도 있음... 이럴때는 어쩔수 없이 다 읽어들여야 할거 같다
	// 아마 용량은 알아서 조절되어 있으리라 생각됨
	// 기왕 다 읽어들일 때는 value까지 세팅하는게 좋지 않을까?

	// 이미 다 읽어들였으면 개수만 세팅하고 나가면 됨
	if (hasRead()) {
		getStates()->totalDataCount += (UINT32)value.size();
		getStates()->chunkDataCount = (UINT32)value.size();
		return true;
	}

	UINT32 totalCount = 0;
	ReadFile(hArchive, &info, sizeof(ChunkInfo), &dRead, NULL);

	if (info.flag == 0) {
		// 비압축 & 비암호화 데이터일 때
		ReadFile(hArchive, &totalCount, sizeof(UINT32), &dRead, NULL);
	}
	else {
		// 압축 or 암호화 데이터일 때
		BYTE* rawBuffer = (BYTE*)malloc(info.size);
		if (info.size > SEGMENT_SIZE) {
			segmentedReadFile(hArchive, rawBuffer, info.size);
		}
		else {
			ReadFile(hArchive, rawBuffer, info.size, &dRead, NULL);
		}
		importRawData(rawBuffer);
		free(rawBuffer);

		BYTE* orgBufferPnt = orgBuffer;
		UINT32 totalCount = *(UINT32*)orgBufferPnt;
		orgBufferPnt += sizeof(UINT32);

		wcout << infoPrefix << L"ChunkType: " << uppercase << hex << getNameUni(info.type) << L": loading " << to_wstring(totalCount) << L" data from buffer..." << endl;
		for (UINT32 i = 0; i < totalCount; i++) {
			//wcout << flush << L"\r";
			//wcout << infoPrefix << L"loading " << to_wstring(i + 1) << L"/" << to_wstring(totalCount) << L" data from buffer...";
			BaseData* data = allocateData();
			if (!data->loadFromBuffer(orgBufferPnt)) {
				delete data;
				wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": DataChunk: 데이터를 로드하지 못했습니다." << endl;
				return false;
			}
			value.push_back(data);
		}

		free(orgBuffer);
		orgBuffer = NULL;
	}

	getStates()->totalDataCount += totalCount;
	getStates()->chunkDataCount = totalCount;

	return true;
}


// 데이터 렌더링용
void DataChunk::renderData()
{
	// 기본적으로는 getStates()->chunkDataCount만 가지고 처리하되,
	// value 크기가 0이 아니면 데이터를 렌더링하는 게 좋을 것 같다

	stringstream oss;

	if (value.size() == 0) {
		oss << u8" - Data 수: " << getStates()->chunkDataCount;
		ImGui::Text(oss.str().c_str());
		oss.str("");
	}

	else {
		// 최대 30개까지만
		stringstream oss;
		UINT32 maxCount = (value.size() > 30) ? 30 : (UINT32)value.size();

		ImGui::Text("[");

		for (size_t i = 0; i < maxCount; i++) {

			float commaStartPosX = ImGui::GetCursorPos().x;
			if (value[i] != NULL) {
				value[i]->renderData((UINT32)i);
				commaStartPosX += ImGui::CalcTextSize("  }").x;
			}
			else {
				ImGui::TextDisabled("  null");
				commaStartPosX += ImGui::CalcTextSize("  null").x;
			}

			if ((i + 1) < value.size()) {
				ImGui::SameLine(commaStartPosX);
				ImGui::Text(",");
			}
		}

		if (maxCount != value.size()) {
			ImGui::Text("  ...");
		}

		oss << "] (" << dec << value.size() << u8"개)";
		ImGui::Text(oss.str().c_str());
		oss.str("");
	}

}


bool DataChunk::loadFromOrgBuffer()
{
	BYTE* orgBufferPnt = orgBuffer;

	UINT32 totalCount = *(UINT32*)orgBufferPnt;
	orgBufferPnt += sizeof(UINT32);

	// value 초기화
	for (UINT32 i = 0; i < value.size(); i++) {
		if (value[i] != NULL) {
			delete value[i];
		}
	}
	value.clear();

	wcout << infoPrefix << L"ChunkType: " << uppercase << hex << getNameUni(info.type) << L": loading " << to_wstring(totalCount) << L" data from buffer..." << endl;
	for (UINT32 i = 0; i < totalCount; i++) {

		//wcout << flush << L"\r";
		//wcout << infoPrefix << L"loading " << to_wstring(i + 1) << L"/" << to_wstring(totalCount) << L" data from buffer...";
		BaseData* data = allocateData();
		if (!data->loadFromBuffer(orgBufferPnt)) {
			delete data;
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getNameUni(info.type) << L": 데이터를 로드하지 못했습니다." << endl;
			return false;
		}
		value.push_back(data);

	}

	free(orgBuffer);
	orgBuffer = NULL;
	return true;
}


bool DataChunk::saveIntoOrgBuffer()
{
	// 기록과 동시에 각 handle별 오프셋도 누적해서 세팅을 해둬야 한다
	// 그러려면 먼저 handle 최대값을 판별한 다음 (최대 handle + 1)만큼 할당해둘 필요가 있을 것이다

	wcout << infoPrefix << L"making new offset array..." << endl;

	UINT32 maxHandleValue = getMaxHandleValue();
	offsetList.clear();
	offsetList.resize(maxHandleValue + 1);

	// 0x11C 버전을 경계로 baseOffset이 0x8인 경우와 0x108인 경우가 나뉜다.
	UINT32 baseOffset = sizeof(info.size) + sizeof(UINT32);
	if (getArchive()->getBuildVersion() >= 0x11C) {
		baseOffset += 0x100;
	}

	// 누적크기 계산 및 오프셋 세팅
	// 그런데 압축된 데이터를 기준으로 계산해야 하는 경우는 getDataSize() 내부에서 압축/비압축처리로 계산해야 한다
	UINT32 accumOffset = baseOffset;
	for (size_t i = 0; i < value.size(); i++) {
		// NULL 대비 코드
		if (value[i] == NULL) {
			offsetList[value[i]->getHandle()] = 0;
		}
		else {
			offsetList[value[i]->getHandle()] = accumOffset;
			accumOffset += value[i]->getDataSize();
		}
	}

	// 크기 계산하여 재세팅
	orgSize = sizeof(UINT32) + (accumOffset - baseOffset);

	// 버퍼 할당
	if (orgBuffer != NULL) {
		free(orgBuffer);
		orgBuffer = NULL;
	}
	orgBuffer = (BYTE*)malloc(orgSize);
	memset(orgBuffer, 0, orgSize);

	// orgBuffer에 기록, 기록을 마친 데이터는 정리
	BYTE* orgBufferPnt = orgBuffer;

	// 개수 세팅
	*(UINT32*)(orgBufferPnt) = (UINT32)value.size();
	orgBufferPnt += sizeof(UINT32);

	wcout << infoPrefix << L"ChunkType: " << uppercase << hex << getNameUni(info.type) << L": saving " << to_wstring(value.size()) << L" data to buffer..." << endl;
	for (size_t i = 0; i < value.size(); i++) {
		// NULL 대비 코드
		if (value[i] == NULL) {
			continue;
		}
		//wcout << flush << L"\r";
		//wcout << infoPrefix << L"saving " << to_wstring(i + 1) << L"/" << to_wstring(value.size()) << L" data to buffer...";
		if (!value[i]->saveIntoBuffer(orgBufferPnt)) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getNameUni(info.type) << L": 데이터를 저장하지 못했습니다." << endl;
			return false;
		}
	}

	return true;
}


UINT32 DataChunk::getMaxHandleValue()
{
	UINT32 maxHandleValue = 0;
	for (size_t i = 0; i < value.size(); i++) {
		if (maxHandleValue < value[i]->getHandle()) {
			maxHandleValue = value[i]->getHandle();
		}
	}
	return maxHandleValue;
}