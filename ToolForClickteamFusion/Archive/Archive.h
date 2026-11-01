#pragma once

#include "ArchiveStructure.h"
#include "../Application/ApplicationState.h"
#include "../Application/ChunkTreeNode.h"
#include "../Chunk/Chunk.h"
#include "../Chunk/Frame/Headers.h"
#include "../Chunk/AppInfo/Headers.h"
#include "../Chunk/Literal/Headers.h"
#include "../Chunk/Object/ObjectText.h"
#include "../Chunk/TwoFivePlusObject/ObjectHeaderListChunk.h"
#include "../Common.h"

namespace CTT {

	namespace Archive {

		using namespace std;
		using namespace CTT::Chunk;

		class Archive
		{

		public:
			Archive(wstring archivePath);
			virtual ~Archive() = 0;
			virtual bool init() = 0;

			HANDLE getArchiveHandle() { return hArchive; }

			// application 정보
			ArchiveType getArchiveType()  { return archiveType; }
			EngineType getEngineType() { return engineType; }
			UINT32 getBuildVersion() { return backHeader.productBuildVersion; }
			wstring& getArchivePath()  { return archivePath; }

			// 이 정보는 확실치 않음
			bool isTwoPointFiveImage() {
				UINT32 imgVersion = productVersionInfo.getValue()[1];
				return (engineType == EngineType::ENGINE_MMF_2_5_PLUS) && (imgVersion >= 3);
			}

			// application 정보 - 로케일 값
			bool isAppLocaleIdRead() { return appLocaleId.hasRead(); }
			UINT32 getAppLocaleId() { return appLocaleId.getValue(); }
			bool isAppCodepageRead() { return appCodepage.hasRead(); }
			UINT32 getAppCodepage() { return appCodepage.getValue(); }

			// application 정보 - protection 관련 값
			wstring& getAppName() { return appName.getValue(); }
			wstring& getEditorFileName() { return editorFileName.getValue(); }
			wstring& getCopyright() { return copyright.getValue(); }

			// application 정보 - frame 연동
			UINT32 getFrameCount() { return appHeader.getValue().numberOfFrames; }
			void setFrameCount(UINT32 frameCount) { appHeader.getValue().numberOfFrames = frameCount; }

			// 아카이브 외부로부터 데이터를 입력받아 세팅
			void setAppNameFromEditor(wstring appNameValue);
			void setAppLocaleIdFromEditor(UINT32 localeIdValue);
			void setAppCodepageFromEditor(UINT32 codepageValue);

			// 아카이브에서 원하는 데이터들을 추출하는 함수
			bool exportFromArchive(vector<wstring>& extractingDataTypes);

			// 아카이브에서 추출할 데이터를 카운트해 세팅하는 함수
			void setDataCount(vector<wstring>& extractingDataTypes);

			// 변경된 데이터들을 적용해 새 아카이브를 만드는 함수
			bool rewriteNewArchive(vector<wstring> &replacingDataTypes);

			// application 정보 - chunk 반환용, shader 정보는 (frame/object/object 2.5+에 쓰임)
			AppNameChunk* getAppNameChunk() { return &appName; }
			EditorFileNameChunk* getEditorFileNameChunk() { return &editorFileName; }
			CopyrightChunk* getCopyrightChunk() { return &copyright; }
			AppHeaderChunk* getAppHeaderChunk() { return &appHeader; }
			ProtectionChunk* getProtectionChunk() { return &protection; }
			ProductVersionInfoChunk* getProductVersionInfoChunk() { return &productVersionInfo; }
			ProductBuildVersionInfoChunk* getProductBuildVersionInfoChunk() { return &productBuildVersionInfo; }
			AppLocaleIdChunk* getAppLocaleIdChunk() { return &appLocaleId; }
			AppCodepageChunk* getAppCodepageChunk() { return &appCodepage; }
			BinaryFileDataChunk* getBinaryFileDataChunk() { return &binaryFileData; }
			ShaderListChunk* getShaderListChunk() { return &shaderList; }

		protected:
			ArchiveType archiveType;
			EngineType engineType;

			wstring archivePath;
			HANDLE hArchive;

			FrontHeader frontHeader;
			BackHeader backHeader;

			UINT32 chunkStartOffset;

			// application 정보
			AppNameChunk appName;
			EditorFileNameChunk editorFileName;
			CopyrightChunk copyright;
			AppHeaderChunk appHeader;
			ProtectionChunk protection;
			ProductVersionInfoChunk productVersionInfo;
			ProductBuildVersionInfoChunk productBuildVersionInfo;
			AppLocaleIdChunk appLocaleId;
			AppCodepageChunk appCodepage;

			// application 데이터 - 내부 데이터 공유용, shaderList는 추출만 가능
			BinaryFileDataChunk binaryFileData;
			ShaderListChunk shaderList;

			// 프레임 이벤트 텍스트 번역 표 (추출할 때 모으고, 교체할 때 읽어 들인다)
			FrameEventTextTable frameEventTextTable;

			// 오브젝트 텍스트 번역 표 (2.5+ 오브젝트의 Alterable 문자열과 Text 문단)
			ObjectTextTable objectTextTable;

			// 프레임 스크립트 디컴파일 / 리컴파일에 쓰는 정보 (오브젝트 이름은 처음 필요할 때 한 번만 읽는다)
			FrameEventScriptContext frameScriptContext;
			bool frameScriptContextLoaded;
			// 교체할 때 몇 번째 프레임인지 세는 변수
			UINT32 rewriteFrameIndex;

			// application 버전 등을 포함한 정보 세팅
			bool setApplicationInfo();

		private:
			// 모든 chunk를 재암호화하여 기록하는 함수
			bool rewriteAllChunk(HANDLE hNewArchive, vector<wstring>& replacingDataTypes);

			// 각 chunk 기록함수 (복수형: DataChunk) (ttfOffsetList: TTF 오프셋을 공유용 변수)
			bool rewriteBinaryFilesChunk(HANDLE hNewArchive);
			bool rewriteFrameChunk(HANDLE hNewArchive, vector<wstring>& replacingDataTypes);
			bool rewriteProtectionChunk(HANDLE hNewArchive);
			bool rewriteImagesChunk(HANDLE hNewArchive);
			bool rewriteSoundsChunk(HANDLE hNewArchive);
			bool rewriteTrueTypeFontsChunk(HANDLE hNewArchive, vector<UINT32> &ttfOffsetList);
			bool rewriteTrueTypeFontInfosChunk(HANDLE hNewArchive, vector<UINT32> &ttfOffsetList);
			bool rewriteLogFontsChunk(HANDLE hNewArchive);
			bool rewriteUnknownChunk(HANDLE hNewArchive);

			// 2.5+ 오브젝트 속성 데이터(0x2256)와 이어지는 오프셋(0x2257)을 오브젝트 텍스트 번역을 적용하여 다시 쓴다
			bool rewriteObjectElementChunks(HANDLE hNewArchive, vector<wstring>& replacingDataTypes);
			// 2.5+ 오브젝트 헤더(0x2253)와 이름(0x2254)을 읽는다 (아카이브 핸들 위치는 그대로 유지된다)
			bool readObjectHeaders(vector<ObjectHeaderEntry>& headers, vector<string>& names);

			// 프로텍션 체크섬 계산 함수 (프로텍션 체크 실패시 false)
			bool caculateProtectionChecksum(HANDLE hTargetArchive, UINT32& protectionChecksum, bool notNeedToCheckProtectionChunk = false);
		};

		// 열려있는 아카이브 클래스 포인터 반환 함수
		Archive* getArchive();

		// app 상태 포인터 반환 함수
		ApplicationState* getStates();

		// 에러 트리거 함수
		// closeAllModal이 true면 현재 열린 모든 modal을 닫도록 조치한다. default는 false
		void triggerError(const char* errTitle, const char* errMsg, bool closeAllModal = false);

	}

}