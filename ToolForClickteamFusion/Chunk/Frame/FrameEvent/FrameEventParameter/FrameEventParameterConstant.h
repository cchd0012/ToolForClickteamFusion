#pragma once

#include "../../../../Common.h"

// script로 보던게 사실 2.5+ object
// script 2로 보던게 사실 그냥 object인듯
// 2.5+는 New 붙여서, 그냥은 Old 붙여서 구분해야겠음

namespace CTT {

	namespace Chunk {

		using namespace std;

		enum class FrameEventObjectType : INT16
		{
			PLAYER = -7,
			KEYBOARD = -6,
			CREATE = -5,
			TIMER = -4,
			GAME = -3,
			SPEAKER = -2,
			SYSTEM = -1,
			QUICK_BACKDROP = 0,
			BACKDROP = 1,
			ACTIVE = 2,
			TEXT = 3,
			QUESTION = 4,
			SCORE = 5,
			LIVES = 6,
			COUNTER = 7,
			RTF = 8,
			SUB_APPLICATION = 9,
			EXTENTION = 32,
			// --------------------
			UNKNOWN = 0x7FFF
		};

		static const char* objectTypeToString(FrameEventObjectType frameEventObjectType)
		{
			switch (frameEventObjectType)
			{
				// --------------------
				case FrameEventObjectType::PLAYER:
				{
					return "player";
				}
				case FrameEventObjectType::KEYBOARD:
				{
					return "keyboard";
				}
				case FrameEventObjectType::CREATE:
				{
					return "create";
				}
				case FrameEventObjectType::TIMER:
				{
					return "timer";
				}
				case FrameEventObjectType::GAME:
				{
					return "game";
				}
				case FrameEventObjectType::SPEAKER:
				{
					return "speaker";
				}
				case FrameEventObjectType::SYSTEM:
				{
					return "system";
				}
				case FrameEventObjectType::QUICK_BACKDROP:
				{
					return "quick_backdrop";
				}
				case FrameEventObjectType::BACKDROP:
				{
					return "backdrop";
				}
				case FrameEventObjectType::ACTIVE:
				{
					return "active";
				}
				case FrameEventObjectType::TEXT:
				{
					return "text";
				}
				case FrameEventObjectType::QUESTION:
				{
					return "question";
				}
				case FrameEventObjectType::SCORE:
				{
					return "score";
				}
				case FrameEventObjectType::LIVES:
				{
					return "lives";
				}
				case FrameEventObjectType::COUNTER:
				{
					return "counter";
				}
				case FrameEventObjectType::RTF:
				{
					return "rtf";
				}
				case FrameEventObjectType::SUB_APPLICATION:
				{
					return "sub_application";
				}
				case FrameEventObjectType::EXTENTION:
				{
					return "extention";
				}
				// --------------------
				default:
				{
					return "(unknown)";
				}
			}
		}

		static FrameEventObjectType StringToObjectType(string str)
		{
			if (str == "player") {
				return FrameEventObjectType::PLAYER;
			}
			else if (str == "keyboard") {
				return FrameEventObjectType::KEYBOARD;
			}
			else if (str == "create") {
				return FrameEventObjectType::CREATE;
			}
			else if (str == "timer") {
				return FrameEventObjectType::TIMER;
			}
			else if (str == "game") {
				return FrameEventObjectType::GAME;
			}
			else if (str == "speaker") {
				return FrameEventObjectType::SPEAKER;
			}
			else if (str == "system") {
				return FrameEventObjectType::SYSTEM;
			}
			else if (str == "quick_backdrop") {
				return FrameEventObjectType::QUICK_BACKDROP;
			}
			else if (str == "backdrop") {
				return FrameEventObjectType::BACKDROP;
			}
			else if (str == "active") {
				return FrameEventObjectType::ACTIVE;
			}
			else if (str == "text") {
				return FrameEventObjectType::TEXT;
			}
			else if (str == "question") {
				return FrameEventObjectType::QUESTION;
			}
			else if (str == "score") {
				return FrameEventObjectType::SCORE;
			}
			else if (str == "lives") {
				return FrameEventObjectType::LIVES;
			}
			else if (str == "counter") {
				return FrameEventObjectType::COUNTER;
			}
			else if (str == "rtf") {
				return FrameEventObjectType::RTF;
			}
			else if (str == "sub_application") {
				return FrameEventObjectType::SUB_APPLICATION;
			}
			else if (str == "extention") {
				return FrameEventObjectType::EXTENTION;
			}
			else {
				return FrameEventObjectType::UNKNOWN;
			}
		}


		enum class FrameEventParameterType : WORD
		{
			// --------------------
			PARAM_OBJECT = 0x0001,
			TIME_1 = 0x0002,
			SHORT_1 = 0x0003,
			SHORT_2 = 0x0004,
			INT_1 = 0x0005,
			SAMPLE_1 = 0x0006,
			SAMPLE_2 = 0x0007,
			// --------------------
			CREATE_1 = 0x0009,
			SHORT_3 = 0x000A,
			SHORT_4 = 0x000B,
			SHORT_5 = 0x000C,
			EVERY = 0x000D,
			KEY_PARAM_1 = 0x000E,
			EXPRESSION_PARAM_1 = 0x000F,
			POSITION = 0x0010,
			SHORT_6 = 0x0011,
			SHOOT = 0x0012,
			ZONE = 0x0013,
			// --------------------
			CREATE_2 = 0x0015,
			EXPRESSION_PARAM_2 = 0x0016,
			EXPRESSION_PARAM_3 = 0x0017,
			COLOR = 0x0018,
			INT_2 = 0x0019,
			SHORT_7 = 0x001A,
			EXPRESSION_PARAM_4 = 0x001B,
			EXPRESSION_PARAM_5 = 0x001C,
			INT_3 = 0x001D,
			// --------------------
			SHORT_8 = 0x001F,
			CLICK = 0x0020,
			PROGRAM = 0x0021,
			INT_4 = 0x0022,
			SAMPLE_3 = 0x0023,
			SAMPLE_4 = 0x0024,
			// --------------------
			GROUP = 0x0026,
			DATA_POINTER = 0x0027,
			FILENAME = 0x0028,
			STRING_PARAM_1 = 0x0029,
			// --------------------
			SHORT_9 = 0x002B,
			KEY_PARAM_2 = 0x002C,
			EXPRESSION_PARAM_6 = 0x002D,
			EXPRESSION_PARAM_7 = 0x002E,
			TWO_SHORTS_1 = 0x002F,
			INT_5 = 0x0030,
			GLOBAL_VALUE = 0x0031,
			ALTERABLE_VALUE = 0x0032,
			TWO_SHORTS_2 = 0x0033,
			EXPRESSION_PARAM_8 = 0x0034,
			EXPRESSION_PARAM_9 = 0x0035,
			EXPRESSION_PARAM_10 = 0x0036,
			EXTENTION = 0x0037,
			INT_6 = 0x0038,
			SHORT_10 = 0x0039,
			SHORT_11 = 0x003A,
			EXPRESSION_PARAM_11 = 0x003B,
			SHORT_12 = 0x003C,
			SHORT_13 = 0x003D,
			EXPRESSION_PARAM_12 = 0x003E,
			// --------------------
			STRING_PARAM_2 = 0x0040,
			// --------------------
			INT_7 = 0x0043,
			MULTIPLE_VARIABLES = 0x0044,
			CHILD_EVENT = 0x0045,
			INT_8 = 0x0046,
			// --------------------
			UNKNOWN = 0xFFFF
		};

		static const char* getParamTypeStr(FrameEventParameterType frameEventParameterType)
		{
			switch (frameEventParameterType)
			{
				// --------------------
				case FrameEventParameterType::GROUP:
				{
					return "group";
				}
				case FrameEventParameterType::DATA_POINTER:
				{
					return "data_pointer";
				}
				// --------------------
				default:
				{
					return "(unknown)";
				}
			}
		}

	}

}