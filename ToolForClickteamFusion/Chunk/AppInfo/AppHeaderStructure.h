#pragma once

#include "../Chunk.h"
#include "../../Util/CommonStructure.h"

namespace CTT {

	namespace Chunk {

		using namespace CTT::CommonUtil;

		typedef struct _character_keys
		{
			UINT16 up;
			UINT16 down;
			UINT16 left;
			UINT16 right;
			UINT16 button1;
			UINT16 button2;
			UINT16 button3;
			UINT16 button4;
		} PlayerKeys;

		typedef struct _player_control
		{
			UINT16 controlType;
			PlayerKeys keys;
		} PlayerControl;

		typedef struct _archive_menu_header
		{
			UINT32 size;
			UINT16 commonFlag;
			UINT16 windowFlag;
			UINT16 graphicMode;
			UINT16 deviceFlag;
			UINT16 windowWidth;
			UINT16 windowHeight;
			UINT32 initialScore;
			UINT32 initialLives;
			PlayerControl controls[4];
			Color borderColor;
			UINT32 numberOfFrames;
			// 이하 값은 1.5버전 이하에 존재하지 않으나, 애초에 1.5버전 이하는 지원 대상이 아니므로 그냥 읽어들인다
			UINT32 frameRate;
			UINT32 windowMenuIndex;
		} AppHeader;

	}

}