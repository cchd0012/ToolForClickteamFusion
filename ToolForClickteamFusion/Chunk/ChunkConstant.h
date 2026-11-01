#pragma once

#include "../Common.h"

// script로 보던게 사실 2.5+ object
// script 2로 보던게 사실 그냥 object인듯
// 2.5+는 New 붙여서, 그냥은 Old 붙여서 구분해야겠음

namespace CTT {

	namespace Chunk {

		using namespace std;

		enum class ChunkType : WORD 
		{

			CHUNK_PREVIEW = 0x1122,
			
			CHUNK_APP_MINI_HEADER = 0x2222,
			CHUNK_APP_HEADER = 0x2223,
			CHUNK_APP_NAME = 0x2224,
			CHUNK_APP_AUTHOR = 0x2225,
			CHUNK_APP_MENU = 0x2226,
			CHUNK_APP_EXTENTION_PATH = 0x2227,
			CHUNK_APP_EXTENTIONS = 0x2228,
			CHUNK_OBJECT_PROPERTIES = 0x2229,		// 그냥 오브젝트 프로퍼티 (헤더, 이름, 커먼, unknown, 이펙트 등 전부 포함)
			CHUNK_APP_GLOBAL_EVENTS = 0x222A,
			CHUNK_APP_FRAME_HANDLES = 0x222B,
			CHUNK_APP_EXTENTION_DATA = 0x222C,
			CHUNK_APP_ADDITIONAL_EXTENTION = 0x222D,
			CHUNK_APP_EDITOR_FILENAME = 0x222E,
			CHUNK_APP_TARGET_FILENAME = 0x222F,
			CHUNK_APP_DOCS = 0x2230,
			CHUNK_APP_OTHER_EXTENTIONS = 0x2231,
			CHUNK_APP_GLOBAL_VALUES = 0x2232,
			CHUNK_APP_GLOBAL_STRINGS = 0x2233,
			CHUNK_APP_EXTENTION_LIST = 0x2234,
			CHUNK_APP_ICON_16_16 = 0x2235,
			CHUNK_IS_DEMO = 0x2236,
			CHUNK_SERIAL_NUMBER = 0x2237,
			CHUNK_BINARY_FILE_DATA = 0x2238,
			CHUNK_MENU_IMAGES = 0x2239,
			CHUNK_ABOUT_TEXT = 0x223A,
			CHUNK_COPYRIGHT = 0x223B,
			CHUNK_GLOBAL_VALUE_NAMES = 0x223C,
			CHUNK_GLOBAL_STRING_NAMES = 0x223D,
			CHUNK_MOVEMENT_EXTENTIONS = 0x223E,
			CHUNK_FRAME_ITEMS_2 = 0x223F,
			CHUNK_EXE_ONLY = 0x2240,
			// 
			CHUNK_PROTECTION = 0x2242,
			CHUNK_SHADER_LIST = 0x2243,
			//
			CHUNK_EXTENTED_HEADER = 0x2245,
			CHUNK_APP_CODEPAGE = 0x2246,		// 이 app의 코드페이지
			CHUNK_FRAME_OFFSET = 0x2247,
			CHUNK_AD_MOB_ID = 0x2248,
			//
			CHUNK_ANDROID_MENU = 0x224B,
			//
			CHUNK_PRODUCT_VERSION_INFO = 0x224D,		// 4바이트 정수 2개, 앞의 값은 0, 뒤의 값은 product version, 근데 앞의 값이 2, 뒤의 값이 2인 경우도 있음. 이걸 기준으로 이미지를 분해해야 하나?
			//
			CHUNK_PRODUCT_BUILD_VERSION_INFO = 0x224F,	// 4바이트 정수 3개, 첫번째 값은 product build version, 두번째 값은 0x16, 세번째 값은 0x1인데 정확하게는 잘 모르겠음
			//
			CHUNK_APP_LOCALE_ID = 0x2251,		// 이 app의 로케일 id
			//
			CHUNK_2_5_PLUS_OBJECT_HEADERS = 0x2253,		// +2.5 버전 오브젝트 관련 chunk들
			CHUNK_2_5_PLUS_OBJECT_NAMES = 0x2254,
			CHUNK_2_5_PLUS_OBJECT_SHADERS = 0x2255,
			CHUNK_2_5_PLUS_OBJECT_DATA = 0x2256,
			CHUNK_2_5_PLUS_OBJECT_OFFSETS = 0x2257,		// +2.5 버전 오프젝트의 오프셋을 담은 정보인데, 압축된게 있고 아닌게 있어서 build 버전 좀 확인해봐야 할듯
			CHUNK_2_5_TTF_INFO_LIST = 0x2258,			// 여기 껴있는거 보니까 이 폰트들은 object 2.5+ 이상에서만 쓰는건가봄...
			CHUNK_2_5_TTF_DATA_LIST = 0x2259,
			CHUNK_2_5_PLUS_SHADER_DATA = 0x225A,
			//
			CHUNK_2_5_UNKNOWN_1 = 0x225C,				// 정확히 뭔지 모르겠는데, 일단 흔히 쓰는 값들은 아닌 것 같음, unknown2는 input과 관련 있어 보이긴 하는데 그 이상 알 수 없다
			CHUNK_2_5_UNKNOWN_2 = 0x225D,				// unknown1은 그냥 00 이 0x10개 있을 뿐이고...

			CHUNK_FRAME = 0x3333,		// 프레임 chunk
			CHUNK_FRAME_HEADER = 0x3334,
			CHUNK_FRAME_NAME = 0x3335,
			CHUNK_FRAME_PASSWORD = 0x3336,						// 생략?
			CHUNK_FRAME_PALETTE = 0x3337,
			CHUNK_FRAME_OBJECT_INSTANCE = 0x3338,
			CHUNK_FRAME_FADE_IN_FRAME = 0x3339,					// 생략?
			CHUNK_FRAME_FADE_OUT_FRAME = 0x333A,				// 생략?
			CHUNK_FRAME_FADE_IN = 0x333B,
			CHUNK_FRAME_FADE_OUT = 0x333C,
			CHUNK_FRAME_EVENTS = 0x333D,
			CHUNK_FRAME_PLAY_HEADER = 0x333E,					// 생략?
			CHUNK_FRAME_ADDITIONAL_ITEM = 0x333F,				// 생략?
			CHUNK_FRAME_ADDITIONAL_ITEM_INSTANCE = 0x3340,		// 생략?
			CHUNK_FRAME_LAYERS = 0x3341,
			CHUNK_FRAME_VIRTUAL_RECT = 0x3342,
			CHUNK_FRAME_DEMO_FILE_PATH = 0x3343,
			CHUNK_FRAME_RANDOM_SEED = 0x3344,
			CHUNK_FRAME_LAYER_EFFECTS = 0x3345,
			CHUNK_FRAME_BLU_LAY_OPTIONS = 0x3346,				// 생략?
			CHUNK_FRAME_MVT_TIMER_BASE = 0x3347,
			CHUNK_FRAME_MOSIAC_IMAGE_TABLE = 0x3348,			// 생략?
			CHUNK_FRAME_EFFECTS = 0x3349,
			CHUNK_FRAME_IPHONE_OPTIONS = 0x334A,				// 생략? 아이폰이 아니라서?
			CHUNK_FRAME_UNKNOWN_1 = 0x334B,						// 생략?
			CHUNK_FRAME_INDEX = 0x334C,

			CHUNK_OBJECT_HEADER = 0x4444,		// 그냥 오브젝트 관련 chunk
			CHUNK_OBJECT_NAME = 0x4445,
			CHUNK_OBJECT_COMMON = 0x4446,
			CHUNK_OBJECT_UNKNOWN = 0x4447,
			CHUNK_OBJECT_EFFECTS = 0x4448,
			//
			CHUNK_OBJECT_SHAPES = 0x4450,

			CHUNK_IMAGE_OFFSETS = 0x5555,		// 오프셋 정보
			CHUNK_LOG_FONT_OFFSETS = 0x5556,
			CHUNK_SOUND_OFFSETS = 0x5557,
			CHUNK_MUSIC_OFFSETS = 0x5558,

			CHUNK_IMAGE_DATA = 0x6666,		// 데이터 정보
			CHUNK_LOG_FONT_DATA = 0x6667,
			CHUNK_SOUND_DATA = 0x6668,
			CHUNK_MUSIC_DATA = 0x6669,

			CHUNK_FUSION_3_SEED = 0x7EEE,

			CHUNK_END = 0x7F7F,

			CHUNK_UNKNOWN = 0xFFFF,

		};

		static const char* getNameAnsi(ChunkType chunkType)
		{
			switch (chunkType)
			{
				// --------------------
				// 프리뷰? 관련 chunk
				case ChunkType::CHUNK_PREVIEW:
				{
					return "preview";
				}
				// --------------------
				// app 관련 chunk
				case ChunkType::CHUNK_APP_MINI_HEADER:
				{
					return "app mini header";
				}
				case ChunkType::CHUNK_APP_HEADER:
				{
					return "app header";
				}
				case ChunkType::CHUNK_APP_NAME:
				{
					return "app name";
				}
				case ChunkType::CHUNK_APP_AUTHOR:
				{
					return "app author";
				}
				case ChunkType::CHUNK_APP_MENU:
				{
					return "app menu";
				}
				case ChunkType::CHUNK_APP_EXTENTION_PATH:
				{
					return "app extention path";
				}
				case ChunkType::CHUNK_APP_EXTENTIONS:
				{
					return "app extentions";
				}
				// 그냥 오브젝트 프로퍼티 (헤더, 이름, 커먼, unknown, 이펙트 등 전부 포함)
				case ChunkType::CHUNK_OBJECT_PROPERTIES:
				{
					return "object properties";
				}
				case ChunkType::CHUNK_APP_GLOBAL_EVENTS:
				{
					return "app global events";
				}
				case ChunkType::CHUNK_APP_FRAME_HANDLES:
				{
					return "app frame handles";
				}
				case ChunkType::CHUNK_APP_EXTENTION_DATA:
				{
					return "app extention data";
				}
				case ChunkType::CHUNK_APP_ADDITIONAL_EXTENTION:
				{
					return "app additional extention";
				}
				case ChunkType::CHUNK_APP_EDITOR_FILENAME:
				{
					return "app editor filename";
				}
				case ChunkType::CHUNK_APP_TARGET_FILENAME:
				{
					return "app target filename";
				}
				case ChunkType::CHUNK_APP_DOCS:
				{
					return "app docs";
				}
				case ChunkType::CHUNK_APP_OTHER_EXTENTIONS:
				{
					return "app other extentions";
				}
				case ChunkType::CHUNK_APP_GLOBAL_VALUES:
				{
					return "app global values";
				}
				case ChunkType::CHUNK_APP_GLOBAL_STRINGS:
				{
					return "app global strings";
				}
				case ChunkType::CHUNK_APP_EXTENTION_LIST:
				{
					return "app extention list";
				}
				case ChunkType::CHUNK_APP_ICON_16_16:
				{
					return "app icon (16*16)";
				}
				case ChunkType::CHUNK_IS_DEMO:
				{
					return "is demo";
				}
				case ChunkType::CHUNK_SERIAL_NUMBER:
				{
					return "serial number";
				}
				case ChunkType::CHUNK_BINARY_FILE_DATA:
				{
					return "binary file data";
				}
				case ChunkType::CHUNK_MENU_IMAGES:
				{
					return "menu images";
				}
				case ChunkType::CHUNK_ABOUT_TEXT:
				{
					return "about text";
				}
				case ChunkType::CHUNK_COPYRIGHT:
				{
					return "copyright";
				}
				case ChunkType::CHUNK_GLOBAL_VALUE_NAMES:
				{
					return "global value names";
				}
				case ChunkType::CHUNK_GLOBAL_STRING_NAMES:
				{
					return "global string names";
				}
				case ChunkType::CHUNK_MOVEMENT_EXTENTIONS:
				{
					return "movement extentions";
				}
				case ChunkType::CHUNK_FRAME_ITEMS_2:
				{
					return "frame items 2";
				}
				case ChunkType::CHUNK_EXE_ONLY:
				{
					return "exe only";
				}
				// --------------------
				case ChunkType::CHUNK_PROTECTION:
				{
					return "protection";
				}
				case ChunkType::CHUNK_SHADER_LIST:
				{
					return "shader list";
				}
				// --------------------
				case ChunkType::CHUNK_EXTENTED_HEADER:
				{
					return "extended header";
				}
				// 이 app의 코드페이지
				case ChunkType::CHUNK_APP_CODEPAGE:
				{
					return "codepage";
				}
				case ChunkType::CHUNK_FRAME_OFFSET:
				{
					return "frame offset";
				}
				case ChunkType::CHUNK_AD_MOB_ID:
				{
					return "ad mob id";
				}
				// --------------------
				case ChunkType::CHUNK_ANDROID_MENU:
				{
					return "android menu";
				}
				// --------------------
				// 4바이트 정수 2개, 앞의 값은 0, 뒤의 값은 product version
				case ChunkType::CHUNK_PRODUCT_VERSION_INFO:
				{
					return "product version info";
				}
				// --------------------
				// 정수가 3개 있는 값인거 같은데...
				case ChunkType::CHUNK_PRODUCT_BUILD_VERSION_INFO:
				{
					return "product build version info";
				}
				// --------------------
				// 이 app의 로케일 id
				case ChunkType::CHUNK_APP_LOCALE_ID:
				{
					return "locale id";
				}
				// --------------------
				// +2.5 버전 오브젝트 관련 chunk들
				case ChunkType::CHUNK_2_5_PLUS_OBJECT_HEADERS:
				{
					return "2.5+ object headers";
				}
				case ChunkType::CHUNK_2_5_PLUS_OBJECT_NAMES:
				{
					return "2.5+ object names";
				}
				case ChunkType::CHUNK_2_5_PLUS_OBJECT_SHADERS:
				{
					return "2.5+ object shaders";
				}
				case ChunkType::CHUNK_2_5_PLUS_OBJECT_DATA:
				{
					return "2.5+ object data";
				}
				// +2.5 버전 오프젝트의 오프셋을 담은 정보인데, 압축된게 있고 아닌게 있어서 build 버전 좀 확인해봐야 할듯
				case ChunkType::CHUNK_2_5_PLUS_OBJECT_OFFSETS:
				{
					return "2.5+ object offsets";
				}
				// 여기 껴있는거 보니까 이 폰트들은 object 2.5+ 이상에서만 쓰는건가봄...
				case ChunkType::CHUNK_2_5_TTF_INFO_LIST:
				{
					return "2.5+ object TTF info";
				}
				case ChunkType::CHUNK_2_5_TTF_DATA_LIST:
				{
					return "2.5+ object TTF data";
				}
				case ChunkType::CHUNK_2_5_PLUS_SHADER_DATA:
				{
					return "2.5+ shader data";
				}
				case ChunkType::CHUNK_2_5_UNKNOWN_1:
				{
					return "2.5+ (unknown 1)";
				}
				// 일단 이쪽은 이중문자열 묶음인 건 거의 확실해 보임
				case ChunkType::CHUNK_2_5_UNKNOWN_2:
				{
					return "2.5+ (unknown 2)";
				}
				// --------------------
				// 프레임 관련 chunk
				case ChunkType::CHUNK_FRAME:
				{
					return "frame";
				}
				case ChunkType::CHUNK_FRAME_HEADER:
				{
					return "frame header";
				}
				case ChunkType::CHUNK_FRAME_NAME:
				{
					return "frame name";
				}
				case ChunkType::CHUNK_FRAME_PASSWORD:
				{
					return "frame password";
				}
				case ChunkType::CHUNK_FRAME_PALETTE:
				{
					return "frame pallete";
				}
				case ChunkType::CHUNK_FRAME_OBJECT_INSTANCE:
				{
					return "frame object instance";
				}
				case ChunkType::CHUNK_FRAME_FADE_IN_FRAME:
				{
					return "frame fade in (frame)";
				}
				case ChunkType::CHUNK_FRAME_FADE_OUT_FRAME:
				{
					return "frame fade out (frame)";
				}
				case ChunkType::CHUNK_FRAME_FADE_IN:
				{
					return "frame fade in";
				}
				case ChunkType::CHUNK_FRAME_FADE_OUT:
				{
					return "frame fade out";
				}
				case ChunkType::CHUNK_FRAME_EVENTS:
				{
					return "frame event";
				}
				case ChunkType::CHUNK_FRAME_PLAY_HEADER:
				{
					return "frame play header";
				}
				case ChunkType::CHUNK_FRAME_ADDITIONAL_ITEM:
				{
					return "frame additional item";
				}
				case ChunkType::CHUNK_FRAME_ADDITIONAL_ITEM_INSTANCE:
				{
					return "frame additional item instance";
				}
				case ChunkType::CHUNK_FRAME_LAYERS:
				{
					return "frame layers";
				}
				case ChunkType::CHUNK_FRAME_VIRTUAL_RECT:
				{
					return "frame virtual rect";
				}
				case ChunkType::CHUNK_FRAME_DEMO_FILE_PATH:
				{
					return "frame demo file path";
				}
				case ChunkType::CHUNK_FRAME_RANDOM_SEED:
				{
					return "frame random seed";
				}
				case ChunkType::CHUNK_FRAME_LAYER_EFFECTS:
				{
					return "frame layer effects";
				}
				case ChunkType::CHUNK_FRAME_BLU_LAY_OPTIONS:
				{
					return "frame blu lay options";
				}
				case ChunkType::CHUNK_FRAME_MVT_TIMER_BASE:
				{
					return "frame mvt timer base";
				}
				case ChunkType::CHUNK_FRAME_MOSIAC_IMAGE_TABLE:
				{
					return "frame mosiac image table";
				}
				case ChunkType::CHUNK_FRAME_EFFECTS:
				{
					return "frame effects";
				}
				case ChunkType::CHUNK_FRAME_IPHONE_OPTIONS:
				{
					return "frame iphone options";
				}
				case ChunkType::CHUNK_FRAME_UNKNOWN_1:
				{
					return "frame (unknown 1)";
				}
				case ChunkType::CHUNK_FRAME_INDEX:
				{
					return "frame index";
				}
				// --------------------
				// 그냥 오브젝트 관련 chunk
				case ChunkType::CHUNK_OBJECT_HEADER:
				{
					return "object header";
				}
				case ChunkType::CHUNK_OBJECT_NAME:
				{
					return "object name";
				}
				case ChunkType::CHUNK_OBJECT_COMMON:
				{
					return "object common";
				}
				case ChunkType::CHUNK_OBJECT_UNKNOWN:
				{
					return "object unknown";
				}
				case ChunkType::CHUNK_OBJECT_EFFECTS:
				{
					return "object effects";
				}
				case ChunkType::CHUNK_OBJECT_SHAPES:
				{
					return "object shapes";
				}
				// --------------------
				// 오프셋 정보
				case ChunkType::CHUNK_IMAGE_OFFSETS:
				{
					return "image offsets";
				}
				case ChunkType::CHUNK_LOG_FONT_OFFSETS:
				{
					return "log font offsets";
				}
				case ChunkType::CHUNK_SOUND_OFFSETS:
				{
					return "sound offsets";
				}
				case ChunkType::CHUNK_MUSIC_OFFSETS:
				{
					return "music offsets";
				}
				// --------------------
				// 데이터 정보
				case ChunkType::CHUNK_IMAGE_DATA:
				{
					return "image data";
				}
				case ChunkType::CHUNK_LOG_FONT_DATA:
				{
					return "log font data";
				}
				case ChunkType::CHUNK_SOUND_DATA:
				{
					return "sound data";
				}
				case ChunkType::CHUNK_MUSIC_DATA:
				{
					return "music data";
				}
				// --------------------
				case ChunkType::CHUNK_FUSION_3_SEED:
				{
					return "fusion-3 seed";
				}
				// --------------------
				case ChunkType::CHUNK_END:
				{
					return "end";
				}
				// --------------------
				case ChunkType::CHUNK_UNKNOWN:
				{
					return "(unknown)";
				}
				// --------------------
				default: 
				{
					return "(unknown)";
				}
			}
		}


		static const wchar_t* getNameUni(ChunkType chunkType)
		{
			switch (chunkType)
			{
				// --------------------
				// 프리뷰? 관련 chunk
				case ChunkType::CHUNK_PREVIEW:
				{
					return L"preview";
				}
				// --------------------
				// app 관련 chunk
				case ChunkType::CHUNK_APP_MINI_HEADER:
				{
					return L"app mini header";
				}
				case ChunkType::CHUNK_APP_HEADER:
				{
					return L"app header";
				}
				case ChunkType::CHUNK_APP_NAME:
				{
					return L"app name";
				}
				case ChunkType::CHUNK_APP_AUTHOR:
				{
					return L"app author";
				}
				case ChunkType::CHUNK_APP_MENU:
				{
					return L"app menu";
				}
				case ChunkType::CHUNK_APP_EXTENTION_PATH:
				{
					return L"app extention path";
				}
				case ChunkType::CHUNK_APP_EXTENTIONS:
				{
					return L"app extentions";
				}
				// 그냥 오브젝트 프로퍼티 (헤더, 이름, 커먼, unknown, 이펙트 등 전부 포함)
				case ChunkType::CHUNK_OBJECT_PROPERTIES:
				{
					return L"object properties";
				}
				case ChunkType::CHUNK_APP_GLOBAL_EVENTS:
				{
					return L"app global events";
				}
				case ChunkType::CHUNK_APP_FRAME_HANDLES:
				{
					return L"app frame handles";
				}
				case ChunkType::CHUNK_APP_EXTENTION_DATA:
				{
					return L"app extention data";
				}
				case ChunkType::CHUNK_APP_ADDITIONAL_EXTENTION:
				{
					return L"app additional extention";
				}
				case ChunkType::CHUNK_APP_EDITOR_FILENAME:
				{
					return L"app editor filename";
				}
				case ChunkType::CHUNK_APP_TARGET_FILENAME:
				{
					return L"app target filename";
				}
				case ChunkType::CHUNK_APP_DOCS:
				{
					return L"app docs";
				}
				case ChunkType::CHUNK_APP_OTHER_EXTENTIONS:
				{
					return L"app other extentions";
				}
				case ChunkType::CHUNK_APP_GLOBAL_VALUES:
				{
					return L"app global values";
				}
				case ChunkType::CHUNK_APP_GLOBAL_STRINGS:
				{
					return L"app global strings";
				}
				case ChunkType::CHUNK_APP_EXTENTION_LIST:
				{
					return L"app extention list";
				}
				case ChunkType::CHUNK_APP_ICON_16_16:
				{
					return L"app icon (16*16)";
				}
				case ChunkType::CHUNK_IS_DEMO:
				{
					return L"is demo";
				}
				case ChunkType::CHUNK_SERIAL_NUMBER:
				{
					return L"serial number";
				}
				case ChunkType::CHUNK_BINARY_FILE_DATA:
				{
					return L"binary file data";
				}
				case ChunkType::CHUNK_MENU_IMAGES:
				{
					return L"menu images";
				}
				case ChunkType::CHUNK_ABOUT_TEXT:
				{
					return L"about text";
				}
				case ChunkType::CHUNK_COPYRIGHT:
				{
					return L"copyright";
				}
				case ChunkType::CHUNK_GLOBAL_VALUE_NAMES:
				{
					return L"global value names";
				}
				case ChunkType::CHUNK_GLOBAL_STRING_NAMES:
				{
					return L"global string names";
				}
				case ChunkType::CHUNK_MOVEMENT_EXTENTIONS:
				{
					return L"movement extentions";
				}
				case ChunkType::CHUNK_FRAME_ITEMS_2:
				{
					return L"frame items 2";
				}
				case ChunkType::CHUNK_EXE_ONLY:
				{
					return L"exe only";
				}
				// --------------------
				case ChunkType::CHUNK_PROTECTION:
				{
					return L"protection";
				}
				case ChunkType::CHUNK_SHADER_LIST:
				{
					return L"shader list";
				}
				// --------------------
				case ChunkType::CHUNK_EXTENTED_HEADER:
				{
					return L"extended header";
				}
				// 이 app의 코드페이지
				case ChunkType::CHUNK_APP_CODEPAGE:
				{
					return L"codepage";
				}
				case ChunkType::CHUNK_FRAME_OFFSET:
				{
					return L"frame offset";
				}
				case ChunkType::CHUNK_AD_MOB_ID:
				{
					return L"ad mob id";
				}
				// --------------------
				case ChunkType::CHUNK_ANDROID_MENU:
				{
					return L"android menu";
				}
				// --------------------
				// 4바이트 정수 2개, 앞의 값은 0, 뒤의 값은 product version
				case ChunkType::CHUNK_PRODUCT_VERSION_INFO:
				{
					return L"product version info";
				}
				// --------------------
				// 정수가 3개 있는 값인거 같은데...
				case ChunkType::CHUNK_PRODUCT_BUILD_VERSION_INFO:
				{
					return L"product build version info";
				}
				// --------------------
				// 이 app의 로케일 id
				case ChunkType::CHUNK_APP_LOCALE_ID:
				{
					return L"locale id";
				}
				// --------------------
				// +2.5 버전 오브젝트 관련 chunk들
				case ChunkType::CHUNK_2_5_PLUS_OBJECT_HEADERS:
				{
					return L"2.5+ object headers";
				}
				case ChunkType::CHUNK_2_5_PLUS_OBJECT_NAMES:
				{
					return L"2.5+ object names";
				}
				case ChunkType::CHUNK_2_5_PLUS_OBJECT_SHADERS:
				{
					return L"2.5+ object shaders";
				}
				case ChunkType::CHUNK_2_5_PLUS_OBJECT_DATA:
				{
					return L"2.5+ object data";
				}
				// +2.5 버전 오프젝트의 오프셋을 담은 정보인데, 압축된게 있고 아닌게 있어서 build 버전 좀 확인해봐야 할듯
				case ChunkType::CHUNK_2_5_PLUS_OBJECT_OFFSETS:
				{
					return L"2.5+ object offsets";
				}
				// 여기 껴있는거 보니까 이 폰트들은 object 2.5+ 이상에서만 쓰는건가봄...
				case ChunkType::CHUNK_2_5_TTF_INFO_LIST:
				{
					return L"2.5+ object TTF info";
				}
				case ChunkType::CHUNK_2_5_TTF_DATA_LIST:
				{
					return L"2.5+ object TTF data";
				}
				case ChunkType::CHUNK_2_5_PLUS_SHADER_DATA:
				{
					return L"2.5+ shader data";
				}
				case ChunkType::CHUNK_2_5_UNKNOWN_1:
				{
					return L"2.5+ (unknown 1)";
				}
				case ChunkType::CHUNK_2_5_UNKNOWN_2:
				{
					return L"2.5+ (unknown 2)";
				}
				// --------------------
				// 프레임 관련 chunk
				case ChunkType::CHUNK_FRAME:
				{
					return L"frame";
				}
				case ChunkType::CHUNK_FRAME_HEADER:
				{
					return L"frame header";
				}
				case ChunkType::CHUNK_FRAME_NAME:
				{
					return L"frame name";
				}
				case ChunkType::CHUNK_FRAME_PASSWORD:
				{
					return L"frame password";
				}
				case ChunkType::CHUNK_FRAME_PALETTE:
				{
					return L"frame pallete";
				}
				case ChunkType::CHUNK_FRAME_OBJECT_INSTANCE:
				{
					return L"frame object instance";
				}
				case ChunkType::CHUNK_FRAME_FADE_IN_FRAME:
				{
					return L"frame fade in (frame)";
				}
				case ChunkType::CHUNK_FRAME_FADE_OUT_FRAME:
				{
					return L"frame fade out (frame)";
				}
				case ChunkType::CHUNK_FRAME_FADE_IN:
				{
					return L"frame fade in";
				}
				case ChunkType::CHUNK_FRAME_FADE_OUT:
				{
					return L"frame fade out";
				}
				case ChunkType::CHUNK_FRAME_EVENTS:
				{
					return L"frame event";
				}
				case ChunkType::CHUNK_FRAME_PLAY_HEADER:
				{
					return L"frame play header";
				}
				case ChunkType::CHUNK_FRAME_ADDITIONAL_ITEM:
				{
					return L"frame additional item";
				}
				case ChunkType::CHUNK_FRAME_ADDITIONAL_ITEM_INSTANCE:
				{
					return L"frame additional item instance";
				}
				case ChunkType::CHUNK_FRAME_LAYERS:
				{
					return L"frame layers";
				}
				case ChunkType::CHUNK_FRAME_VIRTUAL_RECT:
				{
					return L"frame virtual rect";
				}
				case ChunkType::CHUNK_FRAME_DEMO_FILE_PATH:
				{
					return L"frame demo file path";
				}
				case ChunkType::CHUNK_FRAME_RANDOM_SEED:
				{
					return L"frame random seed";
				}
				case ChunkType::CHUNK_FRAME_LAYER_EFFECTS:
				{
					return L"frame layer effects";
				}
				case ChunkType::CHUNK_FRAME_BLU_LAY_OPTIONS:
				{
					return L"frame blu lay options";
				}
				case ChunkType::CHUNK_FRAME_MVT_TIMER_BASE:
				{
					return L"frame mvt timer base";
				}
				case ChunkType::CHUNK_FRAME_MOSIAC_IMAGE_TABLE:
				{
					return L"frame mosiac image table";
				}
				case ChunkType::CHUNK_FRAME_EFFECTS:
				{
					return L"frame effects";
				}
				case ChunkType::CHUNK_FRAME_IPHONE_OPTIONS:
				{
					return L"frame iphone options";
				}
				case ChunkType::CHUNK_FRAME_UNKNOWN_1:
				{
					return L"frame (unknown 1)";
				}
				case ChunkType::CHUNK_FRAME_INDEX:
				{
					return L"frame index";
				}
				// --------------------
				// 그냥 오브젝트 관련 chunk
				case ChunkType::CHUNK_OBJECT_HEADER:
				{
					return L"object header";
				}
				case ChunkType::CHUNK_OBJECT_NAME:
				{
					return L"object name";
				}
				case ChunkType::CHUNK_OBJECT_COMMON:
				{
					return L"object common";
				}
				case ChunkType::CHUNK_OBJECT_UNKNOWN:
				{
					return L"object unknown";
				}
				case ChunkType::CHUNK_OBJECT_EFFECTS:
				{
					return L"object effects";
				}
				case ChunkType::CHUNK_OBJECT_SHAPES:
				{
					return L"object shapes";
				}
				// --------------------
				// 오프셋 정보
				case ChunkType::CHUNK_IMAGE_OFFSETS:
				{
					return L"image offsets";
				}
				case ChunkType::CHUNK_LOG_FONT_OFFSETS:
				{
					return L"log font offsets";
				}
				case ChunkType::CHUNK_SOUND_OFFSETS:
				{
					return L"sound offsets";
				}
				case ChunkType::CHUNK_MUSIC_OFFSETS:
				{
					return L"music offsets";
				}
				// --------------------
				// 데이터 정보
				case ChunkType::CHUNK_IMAGE_DATA:
				{
					return L"image data";
				}
				case ChunkType::CHUNK_LOG_FONT_DATA:
				{
					return L"log font data";
				}
				case ChunkType::CHUNK_SOUND_DATA:
				{
					return L"sound data";
				}
				case ChunkType::CHUNK_MUSIC_DATA:
				{
					return L"music data";
				}
				// --------------------
				case ChunkType::CHUNK_FUSION_3_SEED:
				{
					return L"fusion-3 seed";
				}
				// --------------------
				case ChunkType::CHUNK_END:
				{
					return L"end";
				}
				// --------------------
				case ChunkType::CHUNK_UNKNOWN:
				{
					return L"(unknown)";
				}
				// --------------------
				default:
				{
					return L"(unknown)";
				}
			}
		}

	}

}