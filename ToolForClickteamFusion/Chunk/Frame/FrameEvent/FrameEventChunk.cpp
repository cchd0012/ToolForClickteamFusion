#include "FrameEventChunk.h"
#include "FrameEventHeader.h"
#include "FrameEventOption.h"
#include "FrameEventSizeInfo.h"
#include "FrameEventDataGroup.h"
#include "../../../Archive/Archive.h"
#include "../../../Util/CommonUtil.h"
#include "../../../ImGUI/Headers.h"

using namespace CTT::Chunk;
using namespace CTT::Archive;
using namespace CTT::CommonUtil;


// 프레임 이벤트 내 데이터 그룹 / 데이터 순위
INT32 frameEventDataGroupOrder = 0;
INT32 frameEventDataOrder = 0;


// 프레임 이벤트 내 데이터 그룹 순위 반환/지정 함수
INT32 CTT::Chunk::getFrameEventDataGroupOrder() {
	return frameEventDataGroupOrder;
}
void CTT::Chunk::setFrameEventDataGroupOrder(INT32 order) {
	frameEventDataGroupOrder = order;
}
void CTT::Chunk::increaseFrameEventDataGroupOrder() {
	frameEventDataGroupOrder++;
}

// 프레임 이벤트 내 데이터 순위 반환/지정 함수
INT32 CTT::Chunk::getFrameEventDataOrder() {
	return frameEventDataOrder;
}
void CTT::Chunk::setFrameEventDataOrder(INT32 order) {
	frameEventDataOrder = order;
}
void CTT::Chunk::increaseFrameEventDataOrder() {
	frameEventDataOrder++;
}


FrameEventChunk::FrameEventChunk()
{
	frameEventClassList.clear();
	frameEventDataOffsetMapper.clear();
}


FrameEventChunk::~FrameEventChunk()
{
	frameEventDataOffsetMapper.clear();
	for (size_t i = 0; i < frameEventClassList.size();i++) {
		if (frameEventClassList[i] != NULL) {
			delete frameEventClassList[i];
			frameEventClassList[i] = NULL;
		}
	}
	frameEventClassList.clear();
}


bool FrameEventChunk::loadFromOrgBuffer()
{
	// chunk 데이터: [FrameEventHeader + 기타 프레임 이벤트 계열 클래스들]

	BYTE* orgBufferPnt = orgBuffer;

	// 읽어들이기 전 순서 초기화
	setFrameEventDataGroupOrder(0);
	setFrameEventDataOrder(0);

	string magicStr;

	bool analyzeFinished = false;
	while (orgBufferPnt < (orgBuffer + orgSize)) {

		// 매직 문자열 복사
		magicStr.resize(4);
		memcpy((void*)magicStr.data(), orgBufferPnt, 4);
		orgBufferPnt += 4;

		// base class
		FrameEventBaseClass* febClass = NULL;

		// 문자열에 따라 판단
		if (magicStr == frameEventHeaderMagic) {
			// 프레임 이벤트 헤더
			febClass = new FrameEventHeader();
		}
		else if (magicStr == frameEventOptionMagic) {
			// FrameEventOption의 정보를 담는 데이터
			febClass = new FrameEventOption();
		}
		else if (magicStr == frameEventSizeInfoMagic) {
			// 프레임 이벤트 크기 정보를 담는 데이터
			// FrameEventGroupData의 각 데이터 길이의 총합이다
			febClass = new FrameEventSizeInfo();
		}
		else if (magicStr == frameEventDataGroupMagic) {
			// FrameEventGroupData의 정보를 담는 데이터
			febClass = new FrameEventDataGroup();
		}
		else if (magicStr == frameEventEndMagic) {
			// 끝부분
			analyzeFinished = true;
			break;
		}
		else {
			// 옳지 않은 데이터
			break;
		}

		// 읽은 후 추가
		if (!febClass->readFromBuffer(orgBufferPnt, orgBuffer, frameEventDataOffsetMapper)) {
			delete febClass;
			break;
		}
		frameEventClassList.push_back(febClass);

	}

	free(orgBuffer);
	orgBuffer = NULL;

	if (!analyzeFinished) {
		return false;
	}

	// frameEventClass 매핑 처리
	for (UINT32 i = 0; i < (UINT32)frameEventClassList.size(); i++) {
		frameEventClassList[i]->setPointingFrameEventData(frameEventDataOffsetMapper);
	}

	return true;
}


bool FrameEventChunk::saveIntoOrgBuffer()
{
	if (orgBuffer != NULL) {
		free(orgBuffer);
		orgBuffer = NULL;
	}

	// 미리 루프를 한 번 돌려서 FrameEventGroupData의 크기를 최신화해준다
	// 이렇게 해야 오프셋도 올바르게 보정되고 FrameEventSizeData가 잘못된 값을 기록하는 걸 막을 수 있다
	for (UINT32 i = 0; i < (UINT32)frameEventClassList.size(); i++) {
		frameEventClassList[i]->recalculateSize(frameEventClassList);
	}

	// 오프셋 업데이트
	INT32 accumOffset = 0;
	for (UINT32 i = 0; i < (UINT32)frameEventClassList.size(); i++) {
		accumOffset += 4;
		frameEventClassList[i]->updateOffset(accumOffset);
	}

	// 클래스->오프셋 보정
	for (UINT32 i = 0; i < (UINT32)frameEventClassList.size(); i++) {
		accumOffset += 4;
		frameEventClassList[i]->recorrectOffset();
	}

	// 크기 재계산 (magic 문자도 포함해서)
	orgSize = 0;
	for (UINT32 i = 0; i < (UINT32)frameEventClassList.size(); i++) {
		orgSize += 4;
		orgSize += frameEventClassList[i]->recalculateSize(frameEventClassList);
	}
	orgBuffer = (BYTE*)malloc(orgSize);

	// 데이터 기록
	BYTE* orgBufferPnt = orgBuffer;
	for (UINT32 i = 0; i < (UINT32)frameEventClassList.size(); i++) {
		if (!frameEventClassList[i]->writeIntoBuffer(orgBufferPnt)) {
			return false;
		}
	}

	return true;
}


// 데이터 렌더링용
void FrameEventChunk::renderData()
{
	stringstream oss;

	ImGui::Text("{");

	// *** 진행중 *** //

	ImGui::Text("}");
}


// json으로 텍스트 추출
bool FrameEventChunk::extractText(Json::Value& extractValue)
{
	for (size_t i = 0; i < frameEventClassList.size(); i++) {
		if (!frameEventClassList[i]->extractText(extractValue)) {
			return false;
		}
	}
	return true;
}


// json으로부터 텍스트 교체
bool FrameEventChunk::replaceText(Json::Value& replaceValue)
{
	UINT32 replaceIndex = 0;
	for (size_t i = 0; i < frameEventClassList.size(); i++) {
		if (!frameEventClassList[i]->replaceText(replaceValue, replaceIndex)) {
			return false;
		}
	}
	return true;
}


// json으로 디컴파일
bool FrameEventChunk::decompile(Json::Value& decompileValue)
{
	for (size_t i = 0; i < frameEventClassList.size(); i++) {
		Json::Value decompiledData;
		if (!frameEventClassList[i]->decompile(decompiledData)) {
			return false;
		}
		decompileValue.append(decompiledData);
	}
	return true;
}


// json으로부터 리컴파일
bool FrameEventChunk::recompile(Json::Value& recompileValue)
{
	// 깔끔하게 제거
	for (size_t i = 0; i < frameEventClassList.size(); i++) {
		if (frameEventClassList[i] != NULL) {
			delete frameEventClassList[i];
		}
	}
	frameEventClassList.clear();

	// 현재 클래스의 그룹 데이터 목록
	vector<FrameEventDataGroup*> frameEventDataGroupList;

	// 이후 다시 읽어들이기, "__type__"에 따라 분류
	frameEventClassList.resize(recompileValue.size());
	for (UINT32 i = 0; i < (UINT32)frameEventClassList.size(); i++) {

		// 클래스 타입 정하기
		FrameEventBaseClass* data = NULL;
		if (recompileValue[i]["__type__"] == "header") {
			data = new FrameEventHeader();
		}
		else if (recompileValue[i]["__type__"] == "size_info") {
			data = new FrameEventSizeInfo();
		}
		else if (recompileValue[i]["__type__"] == "option") {
			data = new FrameEventOption();
		}
		else if (recompileValue[i]["__type__"] == "data_group") {
			data = new FrameEventDataGroup();
			// 그룹 데이터 목록에 추가
			FrameEventDataGroup* fedgPnt = reinterpret_cast<FrameEventDataGroup*>(data);
			frameEventDataGroupList.push_back(fedgPnt);
		}
		else {
			return false;
		}

		// 리컴파일 처리
		if (!data->recompile(recompileValue[i])) {
			delete data;
			return false;
		}

		// 리스트에 추가
		frameEventClassList[i] = data;

	}

	// 이후 프레임 이벤트 데이터 클래스 매핑 작업 수행
	for (UINT32 i = 0; i < (UINT32)frameEventClassList.size(); i++) {
		frameEventClassList[i]->setPointingFrameEventData(frameEventDataGroupList);
	}

	return true;
}