#pragma once

#include "../Common.h"
#include "../Glyph/DtgeBuilder.h"

namespace CTT {

	namespace App {

		using namespace std;

		// dtge 번역 빌드 화면의 상태 (Dx9Application_Dtge.cpp)
		// 빌드는 다른 스레드에서 돈다. 스레드와 화면이 함께 쓰는 값(단계 상태, 로그, 결과)은 lock 으로 보호한다.
		struct DtgeUiState
		{
			bool active;				// 이 화면을 보이는 중
			bool loaded;				// 저장해 둔 경로를 읽었는지

			char exePath[520];			// 원본 dtge exe
			char translationDir[520];	// 번역 폴더
			char fontDir[520];			// 한글 폰트 폴더
			char outputParent[520];		// 결과 폴더를 만들 위치
			char fontOverride[CTT::Dtge::kDtgeFontSlotCount][520];	// 자리별로 직접 고른 폰트
			bool noDamage;				// 시험용: HP 무감소
			bool showFonts;				// 글꼴 지정 칸을 펼쳤는지

			// 입력 점검 (바뀌었거나 0.5초가 지나면 다시 한다)
			vector<CTT::Dtge::DtgeCheck> checks;
			string checkedKey;
			DWORD checkedTick;

			// 폰트 자리 파일 선택창이 어느 자리를 고르는 중인지
			int pickingSlot;

			// 실행
			std::mutex lock;
			bool running;
			HANDLE thread;
			int stepState[CTT::Dtge::kDtgeStepCount];	// 0 대기, 1 진행 중, 2 끝남, 3 실패
			vector<string> logLines;
			bool logChanged;
			bool finished;
			bool success;
			wstring resultExe;
			wstring resultDir;

			DtgeUiState()
			{
				active = false;
				loaded = false;
				exePath[0] = 0;
				translationDir[0] = 0;
				fontDir[0] = 0;
				outputParent[0] = 0;
				for (int i = 0; i < CTT::Dtge::kDtgeFontSlotCount; i++) {
					fontOverride[i][0] = 0;
				}
				noDamage = false;
				showFonts = false;
				checkedTick = 0;
				pickingSlot = -1;
				running = false;
				thread = NULL;
				for (int i = 0; i < CTT::Dtge::kDtgeStepCount; i++) {
					stepState[i] = 0;
				}
				logChanged = false;
				finished = false;
				success = false;
			}

		private:
			DtgeUiState(const DtgeUiState&);
			DtgeUiState& operator=(const DtgeUiState&);
		};

	}

}
