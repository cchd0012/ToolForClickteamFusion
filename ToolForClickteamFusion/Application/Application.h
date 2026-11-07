#pragma once

#include "../Common.h"
#include "../Archive/Archive.h"
#include "ApplicationState.h"

namespace CTT {

	namespace App {

		using namespace std;
		using namespace CTT::Archive;

		class Application
		{
		public:
			Application(wstring appName);
			virtual ~Application() = 0;

			// app의 기본 함수
			virtual bool init() = 0;
			virtual void run() = 0;
			virtual void terminate() = 0;

			// 아카이브 포인터 획득 함수
			CTT::Archive::Archive* getArchive() { return archive; }

			// 화면 없이 아카이브를 다루는 쪽(dtge 한글 빌드)이 현재 아카이브를 잠깐 등록/해제할 때 쓴다
			void setCurrentArchive(CTT::Archive::Archive* a) { archive = a; }

			// app 상태 포인터 획득 함수
			ApplicationState* getStates() { return &states; }

			// app 상태 초기화 함수
			void initializeAppState();

			// 에러 세팅 함수
			void triggerError(const char* errTitle, const char* errMsg, bool closeAllModal);

			// 루프를 전역으로 처리하기 위함
			static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

		private:
			// app별 렌더링 함수
			virtual void render() = 0;

			// app별 프로세싱 함수
			virtual void terminateProcess() = 0;

		protected:
			wstring name;
			CTT::Archive::Archive* archive;
			// resizeWidth, resizeHeight는 WindowMessage 이벤트 받을 시 리사이징 처리 변수
			ApplicationState states;
			UINT32 resizeWidth;
			UINT32 resizeHeight;

		};

		bool openApp(wstring appName);
		Application* getApp();
		void closeApp();

	}

}