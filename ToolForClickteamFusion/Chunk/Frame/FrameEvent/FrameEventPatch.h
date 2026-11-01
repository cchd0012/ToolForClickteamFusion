#pragma once

#include "FrameEventModel.h"
#include "../../../Util/JsonUtil.h"

// 프레임 이벤트 보정 (ANSI 확장 문자열 왕복 문제)
//
// ANSI 방식으로 만들어진 확장(예: dtge 의 Alignment Parser)은 유니코드 문자열을 그대로 다루지 못한다.
// 한글이 들어오면 확장이 되돌려주는 문자열이 원문과 달라지고 길이도 바이트 단위가 되어,
// "대사가 바뀌었는지" 검사가 매번 참이 되거나 줄 길이가 어긋나서 글자 배치가 깨진다.
//
// 보정: 대사를 지정하는 동작마다 한글 같은 비-ASCII 글자를 한 글자짜리 대체 글자로 바꾼 "뼈대 문자열"을
//       같은 오브젝트의 다른 문자열 칸에 함께 저장하고, 확장과 주고받는 이벤트(대사 변경 검사, 확장에 대사 넘기기)가
//       원문 대신 뼈대를 읽게 바꾼다. ASCII 만 있는 대사는 뼈대가 원문과 같으므로 동작이 달라지지 않는다.
//
// 대상은 이름이 아니라 이벤트 구조로 찾는다:
//   "오브젝트 A 의 문자열 i 가, 확장 오브젝트 B 가 돌려준 값(문자열 i 의 길이만큼 잘라낸 것)과 다른가" 를 비교하는 조건

namespace CTT {

	namespace Chunk {

		using namespace std;

		typedef struct _parser_skeleton_target {
			UINT16 textObject;			// 대사 문자열을 가진 오브젝트 (핸들)
			INT16 stringIndex;			// 원문이 들어 있는 문자열 번호
			UINT16 parserObject;		// 문자열을 되돌려주는 확장 오브젝트 (핸들)
			INT16 skeletonIndex;		// 뼈대를 저장할 문자열 번호 (-1 = 아직 정해지지 않음, 오브젝트의 문자열을 늘릴 때 정해진다)
			string textObjectName;		// 읽기 위한 이름 (없을 수 있다)
			string parserObjectName;
			_parser_skeleton_target() : textObject(0), stringIndex(0), parserObject(0), skeletonIndex(-1) {}
		} ParserSkeletonTarget;

		typedef struct _parser_skeleton_config {
			bool enabled;
			wstring placeholder;
			vector<ParserSkeletonTarget> targets;
			_parser_skeleton_config() : enabled(true), placeholder(L"x") {}
		} ParserSkeletonConfig;

		typedef struct _parser_skeleton_stats {
			UINT32 skeletonActions;		// 새로 넣거나 갱신한 뼈대 지정 동작 수
			UINT32 copiedActions;		// 상수가 아니라서 원문 식을 그대로 복사한 뼈대 지정 동작 수
			UINT32 retargetedTokens;	// 원문 대신 뼈대를 읽게 바꾼 식 토큰 수
			_parser_skeleton_stats() : skeletonActions(0), copiedActions(0), retargetedTokens(0) {}
		} ParserSkeletonStats;

		// 이벤트 구조에서 보정 대상을 찾는다 (이미 있는 대상은 중복해서 넣지 않는다)
		void detectParserSkeletonTargets(const FrameEventChunkModel& model, vector<ParserSkeletonTarget>& targets);

		// 보정을 적용한다. 대상의 skeletonIndex 가 정해진 것만 적용한다
		bool applyParserSkeletonPatch(FrameEventChunkModel& model, const ParserSkeletonConfig& config, ParserSkeletonStats& stats, string& errorMessage);

		// 설정 json (script/patch/parserSkeleton.json)
		void exportParserSkeletonConfig(const ParserSkeletonConfig& config, Json::Value& value);
		bool importParserSkeletonConfig(const Json::Value& value, ParserSkeletonConfig& config);

	}

}
