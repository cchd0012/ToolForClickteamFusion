#pragma once

#include "Application.h"
#include "../ImGUI/Headers.h"

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
			void renderArchiveInfoWindow();
			void renderTreeNodes(ChunkTreeNode* node, UINT32 depth, UINT32 index, bool isRoot);
			void renderArchiveChunkListWindow();
			void renderArchiveEachChunkInfoWindow();
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
		};

	}

}