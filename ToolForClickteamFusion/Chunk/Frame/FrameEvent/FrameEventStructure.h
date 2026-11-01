#pragma once

#include "../../Chunk.h"
#include "FrameEventConstant.h"
#include "../../AppInfo/ShaderStructure.h"
#include "../../../Util/CommonStructure.h"
#include "../../../Util/ByteBufferUtil.h"

// 프레임 이벤트 청크(0x333D)의 무손실 모델
//
//   ER>> 헤더 | ERop | ERes (전체 그룹 바이트 수) | ERev (프레임 그룹들) | ERev (공유되는 전역 그룹들) | <<ER
//
// 그룹     : [i16 -크기][u8 조건 수][u8 액션 수][u16 플래그] + 헤더 + 조건들 + 액션들
//            헤더 (build >= 0x11C) : [i16 line][i32 restricted][i32 cpt]
//            헤더 (build <  0x11C) : [i16 restricted][i16 cpt][i16 line][i16 undo]
// 조건     : [u16 크기][i16 objectType][i16 num][u16 objectInfo][i16 objectList][i8 flags][i8 otherFlags][u8 파라미터 수][u8 defType][i16 ident] + 파라미터들
// 액션     : 조건에서 ident 가 없는 형태
// 파라미터 : [i16 크기][i16 코드][본문]  (본문은 원본 바이트 그대로 보관하므로 모르는 종류의 파라미터도 손실되지 않는다)
//
// 그룹 포인터 파라미터(코드 39)는 다른 그룹까지의 상대 오프셋을 담고 있으며, 쓸 때 다시 계산한다.
// 따라서 무엇을 끼워 넣거나 지우거나 크기를 바꿔도 모든 포인터가 원래 가리키던 그룹을 그대로 가리킨다.

namespace CTT {

	namespace Chunk {

		using namespace CTT::CommonUtil;
		using namespace CTT::ByteBufferUtil;

		// 그룹 포인터 파라미터의 코드 (CTFAK: ParameterCommon.GroupPointer)
		static const INT16 frameEventGroupPointerParameterCode = 39;

		typedef struct _frame_event_qualifier {
			UINT16 objectInfo;
			INT16 type;
		} FrameEventQualifier;

		typedef struct _frame_event_parameter {
			INT16 code;
			ByteBuffer body;
			// 코드 39 전용: 이 포인터가 가리키는 그룹의 id (-1 = 찾지 못함, 이때는 원본 포인터 값을 그대로 유지한다)
			INT32 targetGroupId;
			// 코드 39 전용: 대상 그룹 안에서의 바이트 오프셋 (0 = 그룹의 시작). 0 이 아닌 값은 에디터가 남긴
			// 오래된 포인터로, 그룹 중간을 가리키며 해당 그룹과 함께 재배치된다
			INT32 targetGroupDelta;
			_frame_event_parameter() : code(0), targetGroupId(-1), targetGroupDelta(0) {}
		} FrameEventParameter;

		typedef struct _frame_event_condition {
			INT16 objectType;
			INT16 num;
			UINT16 objectInfo;
			INT16 objectList;
			INT8 flags;
			INT8 otherFlags;
			BYTE defType;
			INT16 ident;
			vector<FrameEventParameter> params;
			ByteBuffer slack;	// 마지막 파라미터와 선언된 끝 사이의 바이트 (보통은 비어 있다)
			_frame_event_condition() : objectType(0), num(0), objectInfo(0), objectList(0), flags(0), otherFlags(0), defType(0), ident(0) {}
		} FrameEventCondition;

		typedef struct _frame_event_action {
			INT16 objectType;
			INT16 num;
			UINT16 objectInfo;
			INT16 objectList;
			INT8 flags;
			INT8 otherFlags;
			BYTE defType;
			vector<FrameEventParameter> params;
			ByteBuffer slack;
			_frame_event_action() : objectType(0), num(0), objectInfo(0), objectList(0), flags(0), otherFlags(0), defType(0) {}
		} FrameEventAction;

		typedef struct _frame_event_data_group {
			UINT16 flags;
			INT16 line;
			INT32 restricted;
			INT32 cpt;
			INT16 undo;		// 구버전 헤더 레이아웃 전용
			INT32 groupId;	// 그룹 포인터가 사용하는 식별자 (파싱하면서 부여한다)
			vector<FrameEventCondition> conditions;
			vector<FrameEventAction> actions;
			ByteBuffer slack;
			_frame_event_data_group() : flags(0), line(0), restricted(0), cpt(0), undo(0), groupId(-1) {}
		} FrameEventDataGroup;

		enum class FrameEventBlockType
		{
			RAW,		// 헤더 / 옵션 / 끝 매직, 그대로 보존한다
			SIZE,		// 'ERes', 쓸 때 다시 계산한다
			GROUPS		// 'ERev'
		};

		typedef struct _frame_event_block {
			FrameEventBlockType type;
			ByteBuffer raw;							// RAW: 4바이트 매직을 포함한 블록 바이트
			vector<FrameEventDataGroup> groups;		// GROUPS: 'ERev' 블록 하나의 그룹들
			_frame_event_block() : type(FrameEventBlockType::RAW) {}
		} FrameEventBlock;

		typedef struct _frame_event_chunk_model {
			UINT32 build;
			bool newGroupHeader;
			INT32 nextGroupId;
			vector<FrameEventBlock> blocks;
			_frame_event_chunk_model() : build(0), newGroupHeader(true), nextGroupId(0) {}

			// 모든 'ERev' 블록에 있는 그룹의 총 개수
			size_t getGroupCount() const
			{
				size_t count = 0;
				for (size_t i = 0; i < blocks.size(); i++) {
					if (blocks[i].type == FrameEventBlockType::GROUPS) {
						count += blocks[i].groups.size();
					}
				}
				return count;
			}
		} FrameEventChunkModel;

		// 수식 파라미터: [i16 cmp] + 토큰들 + (0,0) 끝 표시
		typedef struct _frame_event_expression_token {
			INT16 objectType;	// -1 = 시스템/상수 토큰, 0 = 연산자, 2 이상 = 오브젝트
			INT16 num;
			ByteBuffer body;	// [i16 크기] 필드 뒤의 모든 바이트
			_frame_event_expression_token() : objectType(0), num(0) {}
			bool isLong() const { return (objectType == -1) && (num == 0) && (body.size() >= 4); }
			bool isString() const { return (objectType == -1) && (num == 3); }
		} FrameEventExpressionToken;

		typedef struct _frame_event_expression {
			INT16 cmp;
			vector<FrameEventExpressionToken> tokens;
			ByteBuffer tail;	// (0,0) 끝 표시 뒤의 바이트 (보통은 비어 있다)
			_frame_event_expression() : cmp(0) {}
		} FrameEventExpression;

	}

}
