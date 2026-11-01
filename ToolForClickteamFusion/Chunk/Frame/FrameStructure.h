#pragma once

#include "../Chunk.h"
#include "../AppInfo/ShaderStructure.h"
#include "../../Util/CommonStructure.h"

namespace CTT {

	namespace Chunk {

		using namespace CTT::CommonUtil;

		// 프레임 헤더
		typedef struct _frame_header
		{
			UINT32 width;
			UINT32 height;
			Color bgColor;
			UINT32 flag;
		} FrameHeader;

		// 프레임 레이어
		typedef struct _frame_layer_info
		{
			UINT32 flag;
			float XCoeff;
			float YCoeff;
			UINT32 NumberOfBackgrounds;
			UINT32 BackgroudIndex;
		} FrameLayerInfo;

		// 프레임 이펙트
		// effect 값이 0보다 작거나 shaderIndex 값이 -1이면 셰이더가 없는 걸로 판단한다
		// shaderIndex 값은 곧 셰이더의 인덱스를 가리킨다
		typedef struct _frame_effect_info
		{
			INT16 effect;
			INT16 effectParam;
			Color RGBCoeff;
			INT32 shaderIndex;
		} FrameEffectInfo;

		// 프레임 이펙트 명령어 단위?
		typedef struct _frame_effect_instruction
		{
			FrameEffectInfo info;
			wstring shaderName;
			vector<ShaderArgument> shaderArguments;
		} FrameEffectInstruction;

		// 프레임 차지 공간
		typedef struct _frame_virtual_rect
		{
			UINT32 left;
			UINT32 top;
			UINT32 right;
			UINT32 bottom;
		} FrameVirtualRect;

		// 프레임 아이템 정보?
		typedef struct _frame_object_info
		{
			UINT16 handle;
			UINT16 objectInfo;
			INT32 posX;
			INT32 posY;
			UINT16 parentType;
			UINT16 layer;
			UINT16 instance;
			UINT16 parentHandle;
		} FrameObjectInfo;

		// 프레임 전환 정보?
		typedef struct _frame_transition_info
		{
			// moduleType과 name은 데이터상으로 ansi이고 4바이트 고정이지만
			// module_file_name은 unicode이며(ansi는 안읽기 때문) NULL로 끝난다
			wstring moduleType;
			wstring name;
			UINT32 duration;
			UINT32 flags;
			Color color;
			wstring moduleFileName;
			vector<BYTE> parameterData;
		} FrameTransitionInfo;

	}

}