#include "Dx9Application.h"
#include "../ImGUI/Headers.h"

using namespace CTT::App;

// 도구 밖에서 바뀐 추출 파일을 다시 읽는다
//
// 번역 중에는 도구 옆에서 JSON, PNG, 글자 파일을 편집기로 고치고 다시 도구로 돌아오는 일이 잦다.
// 그래서 창에 포커스가 돌아올 때(또는 단계를 옮기거나 [새로고침]을 눌렀을 때) 읽어 둔 폴더들을 훑어서,
// 수정 시각이나 크기가 달라진 파일만 다시 읽는다.
//   - 자동으로 시작한 경우 바뀐 내용이 0.35초 뒤에도 같을 때 적용한다 (편집기가 파일을 쓰는 도중에 읽지 않으려는 것이다)
//   - [새로고침]은 확인 없이 바로 적용한다
// 무엇을 다시 읽는지는 화면별로 다르다: Dx9Application_Check.cpp (확인 단계), Dx9Application_Hangul.cpp (글꼴·글자 단계)

void Dx9Application::requestFileRefresh(bool manual)
{
	RefreshState& r = shell.refresh;
	r.scanning = true;
	r.manual = manual;
	r.silent = false;
	r.rounds = 0;
	r.lastFingerprint.clear();
	r.nextScanAt = GetTickCount();
}


void Dx9Application::updateFileRefresh()
{
	RefreshState& r = shell.refresh;
	if (archive == NULL) {
		return;
	}

	// 창이 앞으로 돌아온 순간, 또는 단계를 옮긴 순간 (도구가 직접 쓴 파일이 있을 수 있다)
	const bool focused = (::GetForegroundWindow() == hwnd);
	if (focused && !r.focused) {
		requestFileRefresh(false);
	}
	r.focused = focused;
	if (r.lastStep != shell.step) {
		if (r.lastStep >= 0 && !r.scanning) {
			// 단계를 옮기는 것은 조용히 바로 확인한다 (바뀐 게 없으면 기록하지 않는다)
			requestFileRefresh(true);
			r.silent = true;
		}
		r.lastStep = shell.step;
	}

	if (!r.scanning) {
		return;
	}
	const DWORD now = GetTickCount();
	if ((int)(now - r.nextScanAt) < 0) {
		return;
	}

	CheckChanges checkChanges;
	HangulChanges hangulChanges;
	scanCheckFiles(checkChanges);
	scanHangulFiles(hangulChanges);
	const bool any = checkChanges.any() || hangulChanges.any();
	if (!any) {
		if (r.manual && !r.silent) {
			addShellLog(ShellLogLevel::Info, u8"새로고침: 바뀐 파일이 없습니다");
		}
		r.scanning = false;
		return;
	}

	// 자동 확인: 같은 내용이 두 번 연속 보일 때까지 기다린다 (최대 8번)
	const wstring fingerprint = checkChanges.fingerprint() + L"@" + hangulChanges.fingerprint();
	r.rounds++;
	if (!r.manual && r.rounds < 8 && fingerprint != r.lastFingerprint) {
		r.lastFingerprint = fingerprint;
		r.nextScanAt = now + 350;
		return;
	}

	r.scanning = false;
	if (checkChanges.any()) {
		applyCheckFiles(checkChanges);
	}
	if (hangulChanges.any()) {
		applyHangulFiles(hangulChanges);
	}
}
