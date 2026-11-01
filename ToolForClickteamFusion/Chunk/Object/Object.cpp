#include "Object.h"
#include "ObjectHeaderChunk.h"
#include "../../Archive/Archive.h"
#include "../../Util/JsonUtil.h"
#include "../../Util/CommonUtil.h"

using namespace CTT::Chunk;
using namespace CTT::Archive;
using namespace CTT::JsonUtil;
using namespace CTT::CommonUtil;


Object::Object()
{
}


Object::~Object()
{
	for (size_t i = 0; i < chunkList.size();i++) {
		if (chunkList[i] != NULL) {
			delete chunkList[i];
			chunkList[i] = NULL;
		}
	}
	chunkList.clear();
}


bool Object::exportData(wstring& dirPath, UINT32 index, bool isFullyExport)
{
	// 번역용으로 쓰려면 기본적으로 .ext.txt는 바로 보이는 곳에다 추출하고,
	// 나머지는 폴더를 만들어서 거기 안에 common.bin, info.json 으로 추출하는 게 좋겠다
	// 하지만 그렇다 해서 전부 추출하면 비효율적이니 isFullyExport 여부로 판별한다
	// 기본값은 false

	DWORD dWrite;

	// 추출 파일 경로 생성
	wostringstream woss;
	woss << setw(6) << setfill(L'0') << index;
	wstring numberStr = woss.str();
	wstring exportTxtPath = dirPath + L"/" + numberStr + L"_" + name + L".ext.txt";

	// ***구현중: 기본 .ext.txt 파일 추출
	// wcout << infoPrefix << L"exporting " << exportTxtPath << L"..." << endl;

	// 기타 정보 추출 구현중...
	if (isFullyExport) {

		// json, common 경로 생성
		wstring exportJsonPath = dirPath + L"/" + numberStr + L"_" + name + L".info.json";
		wstring exportCommonPath = dirPath + L"/" + numberStr + L"_" + name + L".common.bin";

		// json 추출 준비
		wcout << infoPrefix << L"exporting " << exportJsonPath << L"..." << endl;

		Json::Value root;

		// 헤더 데이터 추출
		ObjectHeaderChunk* tmpHeaderChunk = NULL;
		for (size_t i = 0; i < chunkList.size();i++) {
			if (chunkList[i]->getType() == ChunkType::CHUNK_OBJECT_HEADER) {
				tmpHeaderChunk = (ObjectHeaderChunk*)chunkList[i];
				break;
			}
		}
		if (tmpHeaderChunk != NULL) {
			Json::Value headerValue = tmpHeaderChunk->exportAsJson();
			root["object_header"] = headerValue;
		}

		// 이펙트 데이터 추출

		// json 추출
		if (!writeIntoJson(root, exportJsonPath)) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": ObjectHeaderChunk: json 데이터를 기록하지 못했습니다." << endl;
			return false;
		}

		// bin 파일 추출 (추후 UnknownChunk에서 ObjectCommonChunk로 바꿔야 함. 물론 원본데이터도 얻을 수 있게 세팅하고)
		UnknownChunk* tmpCommonChunk = NULL;
		for (size_t i = 0; i < chunkList.size(); i++) {
			if (chunkList[i]->getType() == ChunkType::CHUNK_OBJECT_COMMON) {
				tmpCommonChunk = (UnknownChunk*)chunkList[i];
				break;
			}
		}
		if (tmpCommonChunk != NULL) {
			wcout << infoPrefix << L"exporting " << exportCommonPath << L"..." << endl;
			HANDLE hCommon = CreateFileW(exportCommonPath.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			WriteFile(hCommon, tmpCommonChunk->getValue(), tmpCommonChunk->getValueSize(), &dWrite, NULL);
			CloseHandle(hCommon);
		}

	}

	return true;
}


bool Object::importData(wstring& dirPath, UINT32 index)
{
	// 여기서는 그냥 존재하는대로 교체하는 게 좋을것 같다
	// 즉 [특정명]/info.json, [특정명]/common.bin, [특정명].ext.txt
	// 이 파일들이 전부 존재하는 대로 교체작업을 수행한다

	// 근데 이거 실행하면 그냥 import 처리하면서 데이터 교체되는거 아닌가?
	// 그럼 GetDataSize()할때 삽질할 이유가 없음

	return true;
}


bool Object::loadFromBuffer(BYTE*& bufferPnt)
{
	bool isEnd = false;

	while (!isEnd) {

		ChunkInfo* tmpChunkInfo = (ChunkInfo*)bufferPnt;
		bufferPnt += sizeof(ChunkInfo);

		BYTE* tmpRawBuffer = bufferPnt;
		bufferPnt += tmpChunkInfo->size;

		Chunk* tmpChunk = NULL;

		switch (tmpChunkInfo->type)
		{
			case ChunkType::CHUNK_OBJECT_HEADER:
			{
				tmpChunk = new ObjectHeaderChunk();
				break;
			}
			case ChunkType::CHUNK_OBJECT_NAME:
			{
				tmpChunk = new ObjectNameChunk();
				break;
			}
			case ChunkType::CHUNK_OBJECT_COMMON:
			{
				tmpChunk = new UnknownChunk();
				break;
			}
			case ChunkType::CHUNK_OBJECT_UNKNOWN:
			{
				tmpChunk = new UnknownChunk();
				break;
			}
			case ChunkType::CHUNK_OBJECT_EFFECTS:
			{
				// 이 데이터를 의미있게 추출하려면 archive의 shader 데이터를 가지고 있어야 한다
				tmpChunk = new UnknownChunk();
				break;
			}
			case ChunkType::CHUNK_END:
			{
				tmpChunk = new EndChunk();
				isEnd = true;
				break;
			}
			default:
			{
				tmpChunk = new UnknownChunk();
				break;
			}
		}

		if (!tmpChunk->readFromBufferAndChunkInfo(tmpRawBuffer, (*tmpChunkInfo))) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": object chunk 데이터를 읽어오지 못했습니다. (buffer)" << endl;
			delete tmpChunk;
			return false;
		}
		chunkList.push_back(tmpChunk);

		// 이름이면 name에 기록해주기
		if (tmpChunkInfo->type == ChunkType::CHUNK_OBJECT_NAME) {
			ObjectNameChunk* tmpNameChunk = (ObjectNameChunk*)tmpChunk;
			name = tmpNameChunk->getValue();
		}

	}
	return true;
}


bool Object::saveIntoBuffer(BYTE*& bufferPnt)
{
	UINT32 newChunkSize = 0;

	for (size_t i = 0; i < chunkList.size(); i++) {

		// 이름이면 name에서 얻어와 먼저 기록해두기
		if (chunkList[i]->getType() == ChunkType::CHUNK_OBJECT_NAME) {
			((ObjectNameChunk*)chunkList[i])->setValue(name);
		}
		
		ChunkInfo tmpChunkInfo;
		BYTE* tmpRawBuffer = NULL;

		if (!chunkList[i]->writeIntoBufferAndChunkInfo(tmpRawBuffer, tmpChunkInfo)) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": object chunk 데이터를 기록하지 못했습니다. (buffer)" << endl;
			return false;
		}

		memcpy(bufferPnt, &tmpChunkInfo, sizeof(ChunkInfo));
		bufferPnt += sizeof(ChunkInfo);

		memcpy(bufferPnt, tmpRawBuffer, tmpChunkInfo.size);
		bufferPnt += tmpChunkInfo.size;

		free(tmpRawBuffer);
	}

	return true;
}


// 데이터 렌더링용 함수
void Object::renderData(UINT32 index)
{
	stringstream oss;

	// 인덱스 표현
	oss << "  <" << index << ">{";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	// *** 출력 *** //
	ImGui::Text("  }");
}