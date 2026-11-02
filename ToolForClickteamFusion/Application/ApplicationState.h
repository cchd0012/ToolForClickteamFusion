#pragma once
#include "../Common.h"
#include "ChunkTreeNode.h"
#include "../Archive/ArchiveConstant.h"
#include "../Chunk/ChunkStructure.h"

#define MAX_UTF8_PATH					(MAX_PATH * 3)
#define MAX_ERROR_TITLE_LENGTH			100
#define MAX_ERROR_MSG_LENGTH			1000
#define MAX_GAME_NAME_LENGTH			150

enum class ThreadState {
	THREAD_READY,
	THREAD_EXECUTING,
	THREAD_FINISHED,
};

// ImGUI의 특성상 모든 char 배열은 utf-8로 인코딩해 저장한다
typedef struct _application_state {

	// 윈도우 상태값
	UINT32 windowWidth;
	UINT32 windowHeight;
	UINT32 menuHeight;
	UINT32 archiveOpenWindowHeight;
	UINT32 chunkListWindowWidth;
	UINT32 buttonGap;

	// 윈도우 오픈 상태
	bool isOpen;
	bool isDirty;
	bool isDraggable;

	// 시작할 때 바로 열 아카이브 (개발용 스위치 --open <아카이브> [atlas] [blitter])
	char startupArchivePath[MAX_UTF8_PATH];
	bool startupShowAtlas;
	bool startupShowBlitter;
	bool startupShowModule;

	// 에러 관련 상태
	bool isError;
	char lastErrorTitle[MAX_ERROR_TITLE_LENGTH];
	char lastErrorMsg[MAX_ERROR_MSG_LENGTH];

	// open (열기) 관련 상태값
	bool isOpenModalShow;
	char openArchivePath[MAX_UTF8_PATH];

	// 저장 관련 상태값
	bool isSaveModalShow;
	ThreadState savingThreadState;
	UINT32 totalSavingDataCount;
	UINT32 currentSavingDataIndex;

	// 추출 관련 상태값
	bool isExtractModalShow;
	bool isPartExtractModalShow;
	ThreadState extractingThreadState;

	// 교체 관련 상태값
	bool isReplaceModalShow;
	bool isPartReplaceModalShow;
	ThreadState replacingThreadState;
	UINT32 totalReplacingDataCount;
	UINT32 currentReplacingDataIndex;

	// 추출/교체 공통 상태값 (usingShader는 추출만 적용)
	// Text와 Script는, 만일 공존한다면 Script가 우선시되도록 조치해야 할 것 같다
	bool usingImage;
	bool usingImageAndExtractNoneTransparent;
	bool usingSound;
	bool usingFont;
	bool usingModule;
	bool usingShader;
	bool usingBinaryData;
	bool usingText;
	bool usingScript;
	bool usingFlow;						// 텍스트 흐름 분석 (추출만 가능, 스크립트 디컴파일 결과를 본다)
	UINT32 totalDataCount;
	UINT32 currentDataIndex;

	// 아카이브 관련 상태값
	bool isArchiveOpened;
	bool isArchiveGameNameEditing;
	CTT::Archive::ArchiveType archiveType;
	CTT::Archive::EngineType engineType;
	char archiveGameName[MAX_GAME_NAME_LENGTH];

	// Chunk 관련 상태값
	ChunkTreeNodeContainer chunkTreeNodeContainer;
	bool renderingChunkLoaded;
	bool isRenderingChunkAppInfo;
	CTT::Chunk::Chunk* renderingChunk;
	UINT32 chunkDataCount;

} ApplicationState;