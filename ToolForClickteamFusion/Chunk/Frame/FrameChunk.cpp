#include "FrameChunk.h"
#include "../../Archive/Archive.h"
#include "../../Util/CommonUtil.h"
#include "../../ImGUI/Headers.h"

using namespace CTT::Chunk;
using namespace CTT::Archive;
using namespace CTT::CommonUtil;


FrameChunk::FrameChunk() 
{
}


FrameChunk::~FrameChunk()
{
	for (size_t i = 0; i < chunkList.size();i++) {
		delete chunkList[i];
	}
	chunkList.clear();
}


bool FrameChunk::loadFromOrgBuffer()
{
	BYTE* orgBufferPnt = orgBuffer;

	// chunkList 초기화
	for (UINT32 i = 0; i < chunkList.size(); i++) {
		if (chunkList[i] != NULL) {
			delete chunkList[i];
		}
	}
	chunkList.clear();

	// FrameLayerDataChunk 와 FrameLayerEffectDataChunk 를 연동하기 위하여 필요한 변수
	FrameLayerDataChunk* frameLayerDataChunk = NULL;
	FrameLayerEffectDataChunk* frameLayerEffectDataChunk = NULL;

	while (orgBufferPnt < (orgBuffer + orgSize)) {

		ChunkInfo* tmpChunkInfo = (ChunkInfo*)orgBufferPnt;
		orgBufferPnt += sizeof(ChunkInfo);

		BYTE* tmpRawBuffer = orgBufferPnt;
		orgBufferPnt += tmpChunkInfo->size;

		Chunk* tmpChunk = NULL;

		switch (tmpChunkInfo->type) 
		{
			case ChunkType::CHUNK_FRAME_HEADER: 
			{
				tmpChunk = new FrameHeaderChunk();
				break;
			}
			case ChunkType::CHUNK_FRAME_NAME:
			{
				tmpChunk = new FrameNameChunk();
				break;
			}
			case ChunkType::CHUNK_FRAME_PASSWORD:
			{
				tmpChunk = new UnknownChunk();
				break;
			}
			case ChunkType::CHUNK_FRAME_PALETTE:
			{
				tmpChunk = new FramePaletteChunk();
				break;
			}
			case ChunkType::CHUNK_FRAME_OBJECT_INSTANCE:
			{
				//*** 이유는 모르겠지만 뒤쪽에 4바이트가 추가로 붙고, 이 데이터가 누락되면 실행이 안된다
				//*** 그게 해결되기 전까지는 일단 저장할 때 UnknownChunk로 저장하도록 해야 함
				//tmpChunk = new FrameObjectDataChunk();
				tmpChunk = new UnknownChunk();
				break;
			}
			case ChunkType::CHUNK_FRAME_FADE_IN_FRAME:
			{
				tmpChunk = new FrameFadeInFrameChunk();
				break;
			}
			case ChunkType::CHUNK_FRAME_FADE_OUT_FRAME:
			{
				tmpChunk = new FrameFadeOutFrameChunk();
				break;
			}
			case ChunkType::CHUNK_FRAME_FADE_IN:
			{
				tmpChunk = new FrameFadeInChunk();
				break;
			}
			case ChunkType::CHUNK_FRAME_FADE_OUT:
			{
				tmpChunk = new FrameFadeOutChunk();
				break;
			}
			case ChunkType::CHUNK_FRAME_EVENTS:
			{
				tmpChunk = new FrameEventChunk();
				break;
			}
			case ChunkType::CHUNK_FRAME_PLAY_HEADER:
			{
				tmpChunk = new UnknownChunk();
				break;
			}
			case ChunkType::CHUNK_FRAME_ADDITIONAL_ITEM:
			{
				tmpChunk = new UnknownChunk();
				break;
			}
			case ChunkType::CHUNK_FRAME_ADDITIONAL_ITEM_INSTANCE:
			{
				tmpChunk = new UnknownChunk();
				break;
			}
			case ChunkType::CHUNK_FRAME_LAYERS:
			{
				tmpChunk = frameLayerDataChunk = new FrameLayerDataChunk();
				break;
			}
			case ChunkType::CHUNK_FRAME_VIRTUAL_RECT:
			{
				tmpChunk = new FrameVirtualRectChunk();
				break;
			}
			case ChunkType::CHUNK_FRAME_DEMO_FILE_PATH:
			{
				tmpChunk = new FrameDemoFilePathChunk();
				break;
			}
			case ChunkType::CHUNK_FRAME_RANDOM_SEED:
			{
				tmpChunk = new FrameRandomSeedChunk();
				break;
			}
			case ChunkType::CHUNK_FRAME_LAYER_EFFECTS:
			{
				tmpChunk = frameLayerEffectDataChunk = new FrameLayerEffectDataChunk();
				// 개수 연동 처리
				if (frameLayerDataChunk != NULL) {
					frameLayerEffectDataChunk->setFrameLayerCount((UINT32)frameLayerDataChunk->getValue().size());
				}
				break;
			}
			case ChunkType::CHUNK_FRAME_BLU_LAY_OPTIONS:
			{
				tmpChunk = new UnknownChunk();
				break;
			}
			case ChunkType::CHUNK_FRAME_MVT_TIMER_BASE:
			{
				tmpChunk = new FrameMvtTimerBaseChunk();
				break;
			}
			case ChunkType::CHUNK_FRAME_MOSIAC_IMAGE_TABLE:
			{
				tmpChunk = new UnknownChunk();
				break;
			}
			case ChunkType::CHUNK_FRAME_EFFECTS:
			{
				tmpChunk = new FrameEffectChunk();
				break;
			}
			case ChunkType::CHUNK_FRAME_INDEX:
			{
				tmpChunk = new FrameIndexChunk();
				break;
			}
			case ChunkType::CHUNK_FRAME_IPHONE_OPTIONS:
			{
				tmpChunk = new UnknownChunk();
				break;
			}
			case ChunkType::CHUNK_END:
			{
				tmpChunk = new EndChunk();
				break;
			}
			case ChunkType::CHUNK_FRAME_UNKNOWN_1:
			default:
			{
				tmpChunk = new UnknownChunk();
				break;
			}
		}

		if (!tmpChunk->readFromBufferAndChunkInfo(tmpRawBuffer, (*tmpChunkInfo))) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": frame chunk 데이터를 읽어오지 못했습니다. (buffer)" << endl;
			delete tmpChunk;
			return false;
		}
		chunkList.push_back(tmpChunk);

		// end면 바로 빠져나가게 해야 할 것 같다
		if (tmpChunkInfo->type == ChunkType::CHUNK_END) {
			break;
		}

	}

	free(orgBuffer);
	orgBuffer = NULL;
	return true;
}


// 오직 정보 읽기용
bool FrameChunk::readChunkTreeInfoFromArchive(HANDLE hArchive)
{
	DWORD dRead;

	// Frame Chunk는 이중 Chunk로 되어있어서 depth를 변경할 필요가 있다
	// 또한 offset도 현재 아카이브 오프셋을 기반으로 추가해야 한다
	UINT32 baseOffset = SetFilePointer(hArchive, 0, NULL, FILE_CURRENT);
	getStates()->chunkTreeNodeContainer.plusDepth();

	// 이 함수를 호출하는 건 이미 정보가 확정된 뒤일 것이므로 그냥 읽는다
	ReadFile(hArchive, &info, sizeof(ChunkInfo), &dRead, NULL);
	BYTE* rawBuffer = (BYTE*)malloc(info.size);
	if (info.size > SEGMENT_SIZE) {
		segmentedReadFile(hArchive, rawBuffer, info.size);
	}
	else {
		ReadFile(hArchive, rawBuffer, info.size, &dRead, NULL);
	}
	importRawData(rawBuffer);

	BYTE* orgBufferPnt = orgBuffer;

	while (orgBufferPnt < (orgBuffer + orgSize)) {

		ChunkInfo* tmpChunkInfo = (ChunkInfo*)orgBufferPnt;
		orgBufferPnt += sizeof(ChunkInfo);

		// chunk 정보 추가
		ChunkTreeNode* node = new ChunkTreeNode();
		node->setValue(*tmpChunkInfo);
		node->setOffset((UINT32)(baseOffset + (orgBufferPnt - orgBuffer)));
		getStates()->chunkTreeNodeContainer.appendChildToCurrentNode(node);

		BYTE* tmpRawBuffer = orgBufferPnt;
		orgBufferPnt += tmpChunkInfo->size;

		// 여기서도 end면 바로 빠져나가게 해야 할 것 같다
		if (tmpChunkInfo->type == ChunkType::CHUNK_END) {
			break;
		}

	}

	// depth 복구
	getStates()->chunkTreeNodeContainer.minusDepth();

	free(orgBuffer);
	orgBuffer = NULL;
	return true;
}


// 데이터 렌더링용 (사실 이 함수는 호출하지 않을 것이다)
void FrameChunk::renderData()
{
	ImGui::TextDisabled(u8"--");
}


bool FrameChunk::saveIntoOrgBuffer()
{
	UINT32 newChunkSize = 0;

	for (size_t i = 0; i < chunkList.size();i++) {

		ChunkInfo tmpChunkInfo;
		BYTE* tmpRawBuffer = NULL;

		if (!chunkList[i]->writeIntoBufferAndChunkInfo(tmpRawBuffer, tmpChunkInfo)) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": frame chunk 데이터를 기록하지 못했습니다. (buffer)" << endl;
			return false;
		}

		newChunkSize += sizeof(ChunkInfo);
		newChunkSize += tmpChunkInfo.size;

		free(tmpRawBuffer);
	}

	// 크기 계산하여 재세팅
	orgSize = newChunkSize;

	// 버퍼 할당
	if (orgBuffer != NULL) {
		free(orgBuffer);
		orgBuffer = NULL;
	}
	orgBuffer = (BYTE*)malloc(orgSize);
	memset(orgBuffer, 0, orgSize);

	// orgBuffer에 기록, 기록을 마친 데이터는 정리
	BYTE *orgBufferPnt = orgBuffer;

	for (size_t i = 0; i < chunkList.size(); i++) {

		ChunkInfo tmpChunkInfo;
		BYTE* tmpRawBuffer = NULL;

		// frame뿐만 아니라 object에도 적용해야 할 것 같음...
		if (!chunkList[i]->writeIntoBufferAndChunkInfo(tmpRawBuffer, tmpChunkInfo)) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": frame chunk 데이터를 기록하지 못했습니다. (buffer)" << endl;
			return false;
		}

		memcpy(orgBufferPnt, &tmpChunkInfo, sizeof(ChunkInfo));
		orgBufferPnt += sizeof(ChunkInfo);

		memcpy(orgBufferPnt, tmpRawBuffer, tmpChunkInfo.size);
		orgBufferPnt += tmpChunkInfo.size;

		free(tmpRawBuffer);
	}

	return true;
}


// 프레임에 들어 있는 이벤트 청크를 찾는다 (없으면 NULL)
static FrameEventChunk* findFrameEventChunk(vector<CTT::Chunk::Chunk*>& chunkList)
{
	for (size_t i = 0; i < chunkList.size(); i++) {
		if (chunkList[i]->getType() == ChunkType::CHUNK_FRAME_EVENTS) {
			return dynamic_cast<FrameEventChunk*>(chunkList[i]);
		}
	}
	return NULL;
}


bool FrameChunk::extractEventText(UINT32 frameIndex, FrameEventTextTable& table)
{
	FrameEventChunk* eventChunk = findFrameEventChunk(chunkList);
	if (eventChunk == NULL) {
		return true;
	}
	return eventChunk->extractText(frameIndex, table);
}


bool FrameChunk::replaceEventText(const FrameEventTextTable& table, UINT32& replacedCount)
{
	replacedCount = 0;
	FrameEventChunk* eventChunk = findFrameEventChunk(chunkList);
	if (eventChunk == NULL) {
		return true;
	}
	return eventChunk->replaceText(table, replacedCount);
}
