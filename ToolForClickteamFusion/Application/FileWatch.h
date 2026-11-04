#pragma once

#include <map>
#include <string>
#include <vector>
#include "../Common.h"

// 추출 폴더의 파일이 도구 밖에서 바뀌었는지 알아내는 도구
//
// 폴더 하나의 "어느 파일이 언제, 얼마 크기로 있었는지"를 DirSnapshot 으로 기억해 두고, 나중에 다시 훑어서 비교한다.
// 화면은 파일을 읽을 때 그 폴더의 스냅샷을 같이 만들어 두고, 창에 포커스가 돌아오거나 [새로고침]을 누르면
// 비교해서 바뀐 파일만 다시 읽는다 (Dx9Application_Refresh.cpp).

namespace CTT {

	namespace App {

		using namespace std;

		struct FileStamp
		{
			ULONGLONG time;				// 마지막 수정 시각 (FILETIME)
			ULONGLONG size;
			FileStamp() : time(0), size(0) {}
			bool operator==(const FileStamp& other) const { return time == other.time && size == other.size; }
		};

		struct DirSnapshot
		{
			bool valid;					// 만든 적이 있다 (false 이면 그 폴더를 지켜보지 않는다)
			map<wstring, FileStamp> files;
			DirSnapshot() : valid(false) {}
		};

		// 비교 결과: 지난 스냅샷과 달라진 파일 이름 (추가, 삭제, 수정 모두)
		struct WatchResult
		{
			vector<wstring> changed;
			DirSnapshot after;			// 지금 상태
			bool any() const { return !changed.empty(); }
		};

		// 폴더 안의 파일(패턴 예: L"*.json", L"*")을 훑어 스냅샷을 만든다. 폴더가 없으면 빈 스냅샷이다
		DirSnapshot takeDirSnapshot(const wstring& dir, const wstring& pattern);

		// before 가 유효할 때만 다시 훑어 비교한다
		WatchResult compareDir(const DirSnapshot& before, const wstring& dir, const wstring& pattern);

		// 바뀐 내용을 하나의 문자열로 만든다. 두 번 연속 같으면 파일 쓰기가 끝난 것으로 본다
		wstring fingerprintOf(const WatchResult& result);

		// 포커스를 되찾았을 때 훑는 과정의 상태 (두 번 훑어 같으면 적용한다. 쓰다 만 파일을 읽지 않으려는 것이다)
		struct RefreshState
		{
			bool focused;				// 지난 프레임에 이 창이 앞에 있었다
			bool scanning;				// 훑는 중
			bool manual;				// [새로고침]을 눌러서(또는 단계를 옮겨서) 시작했다 (확인 없이 바로 적용)
			bool silent;				// 바뀐 파일이 없을 때 기록을 남기지 않는다
			DWORD nextScanAt;			// GetTickCount 기준
			int rounds;					// 이번에 몇 번째로 훑는지
			wstring lastFingerprint;
			int lastStep;				// 단계가 바뀌면 한 번 훑는다
			RefreshState() : focused(true), scanning(false), manual(false), silent(false), nextScanAt(0), rounds(0), lastStep(-1) {}
		};

	}

}
