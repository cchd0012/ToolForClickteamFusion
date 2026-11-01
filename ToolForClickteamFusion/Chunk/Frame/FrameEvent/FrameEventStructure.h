#pragma once

#include "../../Chunk.h"
#include "FrameEventConstant.h"
#include "../../AppInfo/ShaderStructure.h"
#include "../../../Util/CommonStructure.h"
#include "../../../Util/ByteBufferUtil.h"

// Lossless model of the frame event chunk (0x333D).
//
//   ER>> header | ERop | ERes (total group bytes) | ERev (frame groups) | ERev (shared global groups) | <<ER
//
// Group     : [i16 -size][u8 conditionCount][u8 actionCount][u16 flags] + header + conditions + actions
//             header (build >= 0x11C) : [i16 line][i32 restricted][i32 cpt]
//             header (build <  0x11C) : [i16 restricted][i16 cpt][i16 line][i16 undo]
// Condition : [u16 size][i16 objectType][i16 num][u16 objectInfo][i16 objectList][i8 flags][i8 otherFlags][u8 paramCount][u8 defType][i16 ident] + params
// Action    : same as a condition without the ident
// Parameter : [i16 size][i16 code][body]  (the body is kept as raw bytes, so unknown parameter kinds are never lost)
//
// Group pointer parameters (code 39) hold a relative offset to another group. They are relocated on write,
// so inserting / removing / resizing anything keeps every pointer on the same target group.

namespace CTT {

	namespace Chunk {

		using namespace CTT::CommonUtil;
		using namespace CTT::ByteBufferUtil;

		// parameter code of a group pointer (CTFAK: ParameterCommon.GroupPointer)
		static const INT16 frameEventGroupPointerParameterCode = 39;

		typedef struct _frame_event_qualifier {
			UINT16 objectInfo;
			INT16 type;
		} FrameEventQualifier;

		typedef struct _frame_event_parameter {
			INT16 code;
			ByteBuffer body;
			// code 39 only: id of the group this pointer refers to (-1 = unresolved, the raw pointer is kept)
			INT32 targetGroupId;
			// code 39 only: byte offset inside the target group (0 = the group start). Non zero values are stale
			// editor leftovers that point into the middle of a group and are relocated together with that group
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
			ByteBuffer slack;	// bytes between the last parameter and the declared end (normally empty)
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
			INT16 undo;		// old header layout only
			INT32 groupId;	// identity used by group pointers, assigned while parsing
			vector<FrameEventCondition> conditions;
			vector<FrameEventAction> actions;
			ByteBuffer slack;
			_frame_event_data_group() : flags(0), line(0), restricted(0), cpt(0), undo(0), groupId(-1) {}
		} FrameEventDataGroup;

		enum class FrameEventBlockType
		{
			RAW,		// header / option / end magic, kept as is
			SIZE,		// 'ERes', recomputed on write
			GROUPS		// 'ERev'
		};

		typedef struct _frame_event_block {
			FrameEventBlockType type;
			ByteBuffer raw;							// RAW: the block bytes including the 4 byte magic
			vector<FrameEventDataGroup> groups;		// GROUPS: the groups of one 'ERev' block
			_frame_event_block() : type(FrameEventBlockType::RAW) {}
		} FrameEventBlock;

		typedef struct _frame_event_chunk_model {
			UINT32 build;
			bool newGroupHeader;
			INT32 nextGroupId;
			vector<FrameEventBlock> blocks;
			_frame_event_chunk_model() : build(0), newGroupHeader(true), nextGroupId(0) {}

			// total number of groups over all 'ERev' blocks
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

		// expression parameter: [i16 cmp] + tokens + (0,0) end marker
		typedef struct _frame_event_expression_token {
			INT16 objectType;	// -1 = system / constant token, 0 = operator, >= 2 = object
			INT16 num;
			ByteBuffer body;	// everything after the [i16 size] field
			_frame_event_expression_token() : objectType(0), num(0) {}
			bool isLong() const { return (objectType == -1) && (num == 0) && (body.size() >= 4); }
			bool isString() const { return (objectType == -1) && (num == 3); }
		} FrameEventExpressionToken;

		typedef struct _frame_event_expression {
			INT16 cmp;
			vector<FrameEventExpressionToken> tokens;
			ByteBuffer tail;	// bytes after the (0,0) end marker (normally empty)
			_frame_event_expression() : cmp(0) {}
		} FrameEventExpression;

	}

}
