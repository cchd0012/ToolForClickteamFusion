#pragma once

#include "Application.h"
#include "../ImGUI/Headers.h"
#include "HangulUi.h"
#include "ShellUi.h"
#include "CheckUi.h"

namespace CTT {

	namespace App {

		class Dx9Application : public Application
		{
		public:
			Dx9Application(wstring appName);
			~Dx9Application();
			bool init();
			void run();
			void terminate();
		private:
			// 헬퍼 함수들
			bool createDeviceD3D(HWND hWnd);
			void cleanupDeviceD3D();
			void resetDevice();
			// 렌더링 함수
			void render();
			void renderMenuTab();
			void renderDirtyCloseModal();
			void renderDirtyArchiveOpenModal();
			void renderErrorModal();
			void renderArchiveOpenModal();
			void renderArchiveSaveModal();
			void renderArchiveExtractModal();
			void renderArchivePartExtractModal();
			void renderArchiveReplaceModal();
			void renderArchivePartReplaceModal();
			void renderArchiveInfoContent();
			void renderTreeNodes(ChunkTreeNode* node, UINT32 depth, UINT32 index, bool isRoot);
			void renderArchiveChunkListContent();
			void renderArchiveEachChunkInfoContent();
			// 화면 틀(껍데기): 단계 목록, 상태 줄, 작업 영역, 문제/로그 패널, Dx9Application_Shell.cpp
			void renderShell();
			void renderShellStart(float x, float y, float w, float h);
			void renderShellStatus(float x, float y, float w, float h);
			void renderShellNav(float x, float y, float w, float h);
			void renderShellWork(float x, float y, float w, float h);
			void renderShellBottom(float x, float y, float w, float h);
			void renderShellSide(float x, float y, float w, float h);
			void renderShellStepDiagnose();
			void renderShellStepExtract();
			void renderShellStepText();
			void renderShellStepFont();
			void renderShellStepImage();
			void renderShellStepRebuild();
			void renderShellStepCheck();
			void renderCheckInspector();
			void renderShellStepChunkTree();
			void renderShellStepScriptJson();
			int shellStepStatus(int step);
			void addShellLog(ShellLogLevel level, const string& text);
			void runDiagnosis();
			void renderDiagnosisSuggestModal();
			// 한글화 도구 (글꼴·글자 단계 화면 안에서 그린다), Dx9Application_Hangul.cpp
			void renderHangulToolContent();
			void renderHangulBlitterInspector();
			void renderHangulBlitterPreview();
			void renderHangulAtlasTab();
			void renderHangulBlitterTab();
			void renderHangulModuleTab();
			// 프로세싱 함수
			void terminateProcess();
			bool openArchive();
			void closeArchive();
			// 추가 변수들
			HWND hwnd;
			WNDCLASSEXW wc;
			ImGuiIO *io;
			LPDIRECT3D9 pD3d;
			LPDIRECT3DDEVICE9 pD3dDevice;
			bool isDeviceLost;
			D3DPRESENT_PARAMETERS d3dPp;
			HangulUiState hangul;
			ShellUiState shell;
			CheckUiState check;
		};

	}

}