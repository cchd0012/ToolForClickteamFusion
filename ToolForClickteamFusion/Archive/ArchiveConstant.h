#pragma once
#include "../Common.h"

namespace CTT {

	namespace Archive {

		using namespace std;

		static UINT32 FIRST_MAGIC_VALUE = 0x77777777;		// "wwww"
		static UINT32 SECOND_MAGIC_VALUE = 0x12478749;

		static UINT32 PAMU_VALUE = 0x554D4150;				// "PAMU"
		static UINT32 PAME_VALUE = 0x454D4150;				// "PAME"

		enum class ArchiveType 
		{
			ARCHIVE_EXE,
			ARCHIVE_DAT,
			ARCHIVE_CCN,
			ARCHIVE_UNK
		};

		enum class EngineType 
		{
			ENGINE_NORMAL,			// 디폴트 버전
			ENGINE_MMF_1_5,			// "PAME" 면 버전이 그대로 1.5로 적용됨
			ENGINE_MMF_2,			// "PAME" 일 때만 쓰므로 지금은 신경 안써도 되는 버전일듯
			ENGINE_MMF_2_5_PLUS,	// 중간에 NewObject 관련 chunk가 존재한다면 이 버전으로 변경됨
			ENGINE_ANDROID,
			ENGINE_F3,
			ENGINE_IOS,
			ENGINE_UNK
		};

	}

}

