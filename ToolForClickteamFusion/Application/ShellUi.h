#pragma once

#include <string>
#include <vector>
#include "../Common.h"
#include "../Util/JsonUtil.h"
#include "FileWatch.h"

// 화면 틀(껍데기)의 상태: 왼쪽 단계 목록, 위쪽 상태 줄, 아래 문제/로그 패널
// 구조는 docs/UI_와이어프레임.html 을 따른다.
//   단계: 진단 -> 추출 -> 문구 -> 글꼴·글자 -> 이미지 -> 재합성 -> 확인, 그 아래 고급(청크 트리, 스크립트 JSON)
//   문제 패널은 진단 결과가 모이는 곳이다 (진단 기능은 아직 없어서 비어 있다)

namespace CTT {

	namespace App {

		using namespace std;

		enum ShellStep
		{
			SHELL_STEP_DIAGNOSE = 0,
			SHELL_STEP_EXTRACT,
			SHELL_STEP_TEXT,
			SHELL_STEP_FONT,
			SHELL_STEP_IMAGE,
			SHELL_STEP_REBUILD,
			SHELL_STEP_CHECK,
			SHELL_STEP_CHUNK_TREE,		// 고급
			SHELL_STEP_SCRIPT_JSON,		// 고급
			SHELL_STEP_EVENT_SEARCH,	// 고급
			SHELL_STEP_COUNT
		};

		enum class ShellLogLevel
		{
			Info,
			Warn,
			Error
		};

		struct ShellLogLine
		{
			ShellLogLevel level;
			string time;				// HH:MM:SS
			string text;				// UTF-8
		};

		// 문제 패널의 항목: severity 0 = 확인 필요, 1 = 알 수 없음. goStep 은 해결하는 화면
		struct ShellProblem
		{
			int severity;
			string text;				// UTF-8
			int goStep;
		};

		struct ShellUiState
		{
			int step;
			int bottomTab;				// 0 = 문제, 1 = 로그
			bool bottomOpen;
			bool scrollLogToEnd;
			vector<ShellLogLine> logs;
			vector<ShellProblem> problems;
			Json::Value diagnosis;		// 진단 결과 (Diagnosis/Diagnosis.h 의 JSON 형식)
			bool diagnosed;
			bool suggestDiagnosis;		// 전체 추출이 끝난 뒤 진단을 권하는 창을 띄운다
			RefreshState refresh;		// 도구 밖에서 바뀐 파일을 다시 읽는 과정

			ShellUiState() : step(SHELL_STEP_DIAGNOSE), bottomTab(0), bottomOpen(true), scrollLogToEnd(false), diagnosed(false), suggestDiagnosis(false) {}
		};

	}

}
