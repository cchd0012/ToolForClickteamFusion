#pragma once

#include "../Common.h"

namespace CTT {

	namespace Dtge {

		using namespace std;

		// dtge (DustTale: The Genocide's End) 0.1.4 한글 빌드.
		// 원본 exe 에서 한글 exe 까지 만드는 7단계(docs/dtge/build_dtge.ps1 과 같은 순서)를 C++ 로 옮긴 것이다.
		//   1 추출  2 한글 폰트 교체  3 번역 넣기(+시험용 HP 무감소)  4 대사 글자 이미지  5 파티클 문구  6 유니코드 파서 모듈  7 재합성
		// 안전: 결과는 항상 새로 만든 폴더 안에만 쓴다. 이미 있던 폴더와 파일은 지우거나 덮어쓰지 않는다
//       (지우는 것은 이번 실행이 새 폴더 안에 추출한 파일 중 원본 PNG 와 옛 파서 모듈뿐이다).
		const int kDtgeStepCount = 7;

		// 한글 폰트로 바꾸는 자리: 내장 TTF 5개(font/ttf) + 설치용 TTF 9개(binaries) = 14개
		// 대사 글자 이미지는 binaries 의 폰트로 그리므로, 이 자리의 폰트가 곧 대사 글씨체이다
		const int kDtgeFontSlotCount = 14;

		struct DtgeOptions
		{
			wstring originalExe;		// 원본 dtge exe (읽기만 한다)
			wstring translationDir;		// frameEventText.json, objectText.json 이 든 폴더
			wstring fontDir;			// 한글 폰트 폴더 (000000.ttf ~ 000009.ttf). 자리별로 지정하지 않은 폰트의 기본값
			wstring fontOverride[kDtgeFontSlotCount];	// 자리별로 직접 고른 폰트 파일 (비어 있으면 폰트 폴더의 기본 파일)
			wstring outputParent;		// 결과 폴더를 새로 만들 위치
			wstring overrideDir;		// (선택) 고친 스크립트 폴더. 이 안의 script\... 파일을 추출한 같은 경로에 덮어쓴다 (비우면 쓰지 않는다)
			bool noDamage;				// 켜면 한글판(_kor.exe)에 더해 시험용 HP 무감소판(_kor_nodamage.exe)도 만든다

			DtgeOptions() : noDamage(false) {}
		};

		// 폰트 자리 정보
		const char* dtgeFontSlotTarget(int slot);		// 바꿀 파일 (예: "binaries/DTM-Mono-RU.ttf")
		const char* dtgeFontSlotRole(int slot);			// 어디에 쓰이는지 (UTF-8)
		const char* dtgeFontSlotDefault(int slot);		// 폰트 폴더에서 기본으로 쓰는 파일 이름 (예: "000001.ttf")

		// 입력 점검 한 줄
		struct DtgeCheck
		{
			int level;			// 0 오류, 1 경고, 2 정상
			string text;		// UTF-8
			DtgeCheck() : level(0) {}
		};

		// 진행 알림. state: 0 안내, 1 단계 시작, 2 단계 끝남, 3 단계 실패 (step 은 0 부터, 단계 밖의 알림은 -1)
		typedef void (*DtgeLogFn)(void* user, int step, int state, const string& utf8Message);

		const char* dtgeStepName(int step);

		// 입력을 읽기만 하면서 점검한다 (원본 exe, 번역 파일, 폰트 파일, 출력 위치)
		void checkDtgeInputs(const DtgeOptions& options, vector<DtgeCheck>& checks);

		// 빌드를 끝까지 실행한다 (오래 걸리므로 다른 스레드에서 부른다). 성공하면 resultExe 와 resultDir 을 채운다
		bool runDtgeBuild(const DtgeOptions& options, DtgeLogFn log, void* user, wstring& resultExe, wstring& resultDir);

	}

}
