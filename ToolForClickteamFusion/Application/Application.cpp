#include "Application.h"
#include "Dx9Application.h"
#include "../ImGUI/Headers.h"

using namespace CTT::App;
using namespace CTT::Archive;

Application* _application = NULL;


bool CTT::App::openApp(wstring appName) {
	closeApp();
	_application = new Dx9Application(appName);
	return _application->init();
}


Application* CTT::App::getApp() {
	return _application;
}


void CTT::App::closeApp() {
	if (_application != NULL) {
		delete _application;
		_application = NULL;
	}
}




Application::Application(wstring appName)
{
	name = appName;
	archive = NULL;
}


Application::~Application()
{
	if (archive != NULL) {
		delete archive;
		archive = NULL;
	}
}




// app 상태 초기화 함수
void Application::initializeAppState()
{
	states = ApplicationState();

	// 윈도우 오픈 상태 초기화
	states.isOpen = true;
	states.isDirty = false;
	states.isDraggable = false;

	// 윈도우 상태값 초기화 (기본 840*600)
	states.windowWidth = 840;
	states.windowHeight = 600;
	states.menuHeight = 40;
	states.archiveOpenWindowHeight = 120;
	states.chunkListWindowWidth = 400;
	states.buttonGap = 6;

	// 에러 상태 초기화
	states.isError = false;
	memset(states.lastErrorTitle, 0, MAX_ERROR_TITLE_LENGTH);
	memset(states.lastErrorMsg, 0, MAX_ERROR_MSG_LENGTH);

	// open (열기) 관련 상태값 초기화
	states.isOpenModalShow = false;
	memset(states.openArchivePath, 0, MAX_UTF8_PATH);

	// 저장 관련 상태값 초기화
	states.isSaveModalShow = false;
	states.savingThreadState = ThreadState::THREAD_READY;
	states.totalSavingDataCount = 0;
	states.currentSavingDataIndex = 0;

	// 추출 관련 상태값 초기화
	states.isExtractModalShow = false;
	states.isPartExtractModalShow = false;
	states.extractingThreadState = ThreadState::THREAD_READY;

	// 교체 관련 상태값 초기화
	states.isReplaceModalShow = false;
	states.isPartReplaceModalShow = false;
	states.replacingThreadState = ThreadState::THREAD_READY;
	states.totalReplacingDataCount = 0;
	states.currentReplacingDataIndex = 0;

	// 추출/교체 공통 상태값 초기화
	states.usingImage = true;
	states.usingImageAndExtractNoneTransparent = false;
	states.usingSound = true;
	states.usingFont = true;
	states.usingModule = true;
	states.usingShader = true;
	states.usingBinaryData = true;
	states.usingText = true;
	states.usingScript = true;
	states.totalDataCount = 0;
	states.currentDataIndex = 0;

	// 아카이브 관련 상태값 초기화
	states.isArchiveOpened = false;
	states.isArchiveGameNameEditing = false;
	states.archiveType = ArchiveType::ARCHIVE_UNK;
	states.engineType = EngineType::ENGINE_UNK;
	memset(states.archiveGameName, 0, MAX_GAME_NAME_LENGTH);

	// Chunk 관련 상태값 초기화
	states.chunkTreeNodeContainer = ChunkTreeNodeContainer();
	states.chunkTreeNodeContainer.init();
	states.renderingChunkLoaded = false;
	states.isRenderingChunkAppInfo = false;
	states.renderingChunk = NULL;
	states.chunkDataCount = 0;

	// 메시지 이벤트 처리용 변수 초기화
	resizeWidth = 0;
	resizeHeight = 0;
}


// 에러 세팅 함수
void Application::triggerError(const char* errTitle, const char* errMsg, bool closeAllModal = false)
{
	states.isError = true;
	memset(states.lastErrorTitle, 0, MAX_ERROR_TITLE_LENGTH);
	memcpy(states.lastErrorTitle, errTitle, min(strlen(errTitle), (MAX_ERROR_TITLE_LENGTH - 1)));
	memset(states.lastErrorMsg, 0, MAX_ERROR_MSG_LENGTH);
	memcpy(states.lastErrorMsg, errMsg, min(strlen(errMsg), (MAX_ERROR_MSG_LENGTH - 1)));
	// closeAllModal이 true면 현재 열린 모든 modal을 닫도록 조치한다
	if (closeAllModal) 
	{
		states.isOpenModalShow = false;
		states.isSaveModalShow = false;
		states.isExtractModalShow = false;
		states.isPartExtractModalShow = false;
		states.isReplaceModalShow = false;
		states.isPartReplaceModalShow = false;
	}
}




// imgui_impl_win32.cpp 파일에 선언된 WndProc 핸들러
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI Application::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static bool isDragging = false;
	static int offsetX = 0, offsetY = 0;

	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) 
	{ 
		return true; 
	}

	switch (msg)
	{
		case WM_SIZE:
		{
			if (wParam == SIZE_MINIMIZED) { return 0; }
			getApp()->resizeWidth = (UINT)LOWORD(lParam); // Queue resize
			getApp()->resizeHeight = (UINT)HIWORD(lParam);
			return 0;
		}
		case WM_SYSCOMMAND:
		{
			// Disable ALT application menu
			if ((wParam & 0xfff0) == SC_KEYMENU) { return 0; }
			break;
		}
		case WM_LBUTTONDOWN: {
			// 마우스 왼쪽 버튼 클릭 시 이동 시작, 드래그가 가능할 때만 처리한다
			if (wParam == MK_LBUTTON && getApp()->getStates()->isDraggable) 
			{
				isDragging = true;
				// 마우스 클릭 위치 기록
				offsetX = GET_X_LPARAM(lParam);
				offsetY = GET_Y_LPARAM(lParam);
			}
			break;
		}
		case WM_LBUTTONUP: {
			// 마우스 왼쪽 버튼 뗄 때 이동 종료, 드래그가 가능할 때만 처리한다
			if (getApp()->getStates()->isDraggable)
			{
				isDragging = false;
			}
			break;
		}
		case WM_MOUSEMOVE: {
			// 마우스를 이동할 때 창을 이동 (현재 위치 기반으로), 드래그가 가능할 때만 처리한다
			if (getApp()->getStates()->isDraggable)
			{
				if (isDragging) 
				{
					RECT rect;
					int moveX = GET_X_LPARAM(lParam) - offsetX;
					int moveY = GET_Y_LPARAM(lParam) - offsetY;
					GetWindowRect(hWnd, &rect);
					int x = rect.left + moveX;
					int y = rect.top + moveY;
					SetWindowPos(hWnd, HWND_TOP, x, y, 0, 0, SWP_NOSIZE);
				}
			}
			break;
		}
		case WM_DESTROY:
		{
			::PostQuitMessage(0);
			return 0;
		}
	}
	return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}