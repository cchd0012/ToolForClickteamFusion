#pragma once

#include "../Common.h"
#include <map>

namespace CTT {

	namespace Glyph {

		using namespace std;

		// 폰트 아틀라스 + 글자표 (txtblt 의 Hangul / ExtendedHangul / LimitedHangul 방식에 쓰는 데이터)
		//  - 아틀라스: 모든 글자를 한 장에 모은 PNG (흰색 + 알파). 글자 하나는 줄 높이 전체를 차지하는 칸이다
		//  - 글자표 : 코드(1바이트 또는 CP949 2바이트) -> 아틀라스 위치, 크기, 그릴 위치 보정, 다음 글자까지의 폭(advance)
		// 글자표는 모듈에 내장되어 코드로 바로 찾을 수 있게 앞바이트(상위 바이트) 별 페이지로 나눠 둔다 (KFT1)
		//
		// KFT1 형식 (모두 리틀 엔디언)
		//   헤더 32바이트: 'KFT1', u16 버전(1), u16 플래그, u16 줄 높이, u16 아틀라스 폭, u16 아틀라스 높이,
		//                  u16 빠진 글자 advance, u32 글자 수, u32 예약 x3
		//   페이지 목록 : u32 x 256 (상위 바이트 -> 페이지의 파일 안 위치, 0 이면 그 페이지에 글자 없음)
		//   페이지      : 글자 x 256 (하위 바이트 순서). 글자 8바이트: u16 x, u16 y, u8 폭, u8 높이, s8 가로 보정, u8 advance
		//                  폭과 높이가 모두 0 이고 advance 만 있는 글자(예: 공백)는 그리지 않고 advance 만큼 이동한다.
		//                  advance 가 0 이고 폭이 0 이면 글자 없음
		//   코드 해석   : 상위 바이트 0 = 1바이트 글자(0x20~0x7E), 그 외 = CP949 앞바이트 + 뒷바이트
		static const UINT32 kFontAtlasMagic = 0x3154464B;		// 'KFT1'
		static const UINT32 kFontAtlasHeaderSize = 32;

		enum class FontAtlasMode
		{
			Hangul,				// 아스키 + CP949 [A1-FE][A1-FE] (KS X 1001: 한글 2350자, 기호, 한자)
			ExtendedHangul,		// Hangul + 확장 완성형(UHC) 영역까지 (한글 11172자)
			LimitedHangul,		// 아스키 + 번역문에 실제로 쓰인 글자(와 extraChars)만. 아틀라스가 작아진다. 글자는 CP949 로 표현되는 것만 담는다
		};

		struct FontAtlasGlyph
		{
			UINT16 x;
			UINT16 y;
			BYTE width;
			BYTE height;
			INT8 offsetX;
			BYTE advance;
		};

		// 메모리 안의 폰트 아틀라스: 글자표와 이미지를 함께 든다
		struct FontAtlasData
		{
			wstring name;
			UINT16 lineHeight;
			UINT16 atlasWidth;
			UINT16 atlasHeight;
			UINT16 missingAdvance;				// 글자표에 없는 코드를 만났을 때 이동할 폭
			UINT16 flags;
			map<UINT16, FontAtlasGlyph> glyphs;	// 코드 -> 글자 (상위 바이트 << 8 | 하위 바이트)
			vector<BYTE> bgra;					// atlasWidth x atlasHeight x 4 (B, G, R, A. Util/PngUtil 의 순서)

			FontAtlasData() : lineHeight(0), atlasWidth(0), atlasHeight(0), missingAdvance(0), flags(0) {}
		};

		// 글자표를 KFT1 바이너리로 직렬화 / 역직렬화한다 (이미지는 포함하지 않는다)
		bool serializeFontAtlasTable(const FontAtlasData& atlas, vector<BYTE>& out);
		bool parseFontAtlasTable(const BYTE* data, size_t size, FontAtlasData& atlas);

		// KFT1 바이너리에서 코드를 직접 찾는다 (모듈 안의 코드와 같은 방식. 검증용)
		bool lookupFontAtlasGlyph(const BYTE* data, size_t size, UINT16 code, FontAtlasGlyph& glyph);

		// 폴더(<이름>.png, <이름>.kft, <이름>.json)에 저장 / 읽기
		bool saveFontAtlas(const FontAtlasData& atlas, const wstring& directory);
		bool loadFontAtlas(FontAtlasData& atlas, const wstring& directory, const wstring& name);

		// 설정 json 대로 폰트에서 아틀라스를 만든다 (설정은 FontAtlas.cpp 위쪽 설명 참고)
		bool buildFontAtlas(const wstring& configPath);

		// 기존 txtblt_fix_kor.dll 의 FONT_INFO 리소스(글자표)와 그 글자 이미지 PNG 에서 아틀라스를 만든다
		// 같은 코드가 여러 번 나오는 항목(악센트 글자를 아스키 코드로 바꾼 것)은 BMFont .fnt(fntPath)가 있으면
		// 코드가 가리키는 원래 글자의 항목을 쓰고, 없으면 첫 항목을 쓴다.
		// lastWins 이면 원본 DLL 처럼 마지막 항목이 남는다 ('A' 가 'À' 로 그려지는 결과)
		bool importLegacyFontInfo(const wstring& resourcePath, const wstring& pngPath, const wstring& name, const wstring& directory, bool lastWins = false, const wstring& fntPath = wstring());

		// 개발용 스위치
		//   --font-atlas build  <설정 json>
		//   --font-atlas legacy <FONT_INFO 리소스 파일> <글자 이미지 PNG> <이름> <출력 폴더>
		//   --font-atlas check  <폴더> <이름>      저장한 글자표를 다시 읽어 모든 글자가 일치하는지 확인한다
		int runFontAtlasCommand(int argc, WCHAR** argv);

	}

}
