#pragma once

#include <map>
#include "../Common.h"
#include "../Glyph/FontAtlas.h"
#include "../ImGUI/Headers.h"

// 한글화 도구 창(글자 세트 만들기 / 오브젝트에 적용 / 게임에 넣기 세 탭)의 화면 상태
//
// 작업 흐름: 추출 -> 수정 -> 교체. 이 창들은 "수정" 단계에서 쓴다.
//   - 글자 아틀라스 창: 폰트에서 아틀라스(PNG + 글자표)를 만들고, 번호를 붙여 txtblt.mfx 에 심은 모듈을 만든다
//   - 블리터 글자표 배정 창: 추출한 오브젝트 JSON(script/object)의 텍스트 블리터마다 글자표와 줄 폭 계산 방식을 고르고,
//     그 블리터가 받는 문자열(text/textFlow.json)을 배정한 글자표로 그려 미리 본다
// 작업 폴더는 <아카이브 이름>/glyph (아틀라스, 설정, 패치한 모듈)이고, 오브젝트 JSON 은 추출 폴더의 script/object 에 있다

namespace CTT {

	namespace App {

		using namespace std;

		struct HangulAtlasItem
		{
			wstring name;
			UINT32 glyphCount;
			UINT32 lineHeight;
			INT32 id;						// 글자표 번호 (0 이면 배정 안 함)
			HangulAtlasItem() : glyphCount(0), lineHeight(0), id(0) {}
		};

		struct HangulBlitterItem
		{
			INT32 index;					// 오브젝트 번호
			string name;
			string font;					// 알터러블 문자열 0 (글꼴 이름)
			string initialText;
			INT32 fontTable;
			bool pixel;						// layout == "pixel"
			bool selected;
			bool dirty;
			wstring jsonPath;
			HangulBlitterItem() : index(0), fontTable(0), pixel(false), selected(false), dirty(false) {}
		};

		// 미리보기에 쓰는 아틀라스: 글자표 + D3D 텍스처
		struct HangulPreviewAtlas
		{
			CTT::Glyph::FontAtlasData data;
			LPDIRECT3DTEXTURE9 texture;
			HangulPreviewAtlas() : texture(NULL) {}
		};

		struct HangulUiState
		{
			bool showWindow;
			int requestTab;					// 창을 열 때 먼저 보여 줄 탭 (0 글자 세트, 1 오브젝트 적용, 2 게임에 넣기, -1 요청 없음)

			// ---- 아틀라스 만들기 폼
			char atlasName[64];
			char fontPath[MAX_UTF8_PATH];
			int fontSize;
			int atlasMode;					// 0 = hangul, 1 = extendedHangul, 2 = limitedHangul
			char extraChars[512];			// limitedHangul 에서 더 넣을 글자
			float color[3];
			float outlineColor[3];
			int outlineSize;
			int letterSpacing;
			int dy;
			int atlasWidth;

			// 만드는 중인 작업 (스레드가 끝나면 buildDone 이 1 이 된다)
			volatile LONG buildRunning;
			volatile LONG buildDone;
			volatile LONG buildOk;

			// ---- 아틀라스 목록 / 번호 배정
			vector<HangulAtlasItem> atlases;
			bool atlasesLoaded;

			// ---- 모듈 패치
			char moduleInput[MAX_UTF8_PATH];
			char moduleOutput[MAX_UTF8_PATH];

			// ---- 블리터
			vector<HangulBlitterItem> blitters;
			map<string, vector<string> > textsByName;		// 블리터 이름 -> 나올 수 있는 문자열 (textFlow.json)
			map<string, INT32> tableByFont;					// 글꼴 이름 -> 글자표 번호 (규칙으로 배정)
			bool blittersLoaded;
			int currentBlitter;				// 선택해서 보고 있는 항목 (blitters 의 위치, -1 이면 없음)
			char filter[64];
			int applyTable;
			int applyLayout;				// 0 = byte, 1 = pixel

			// ---- 미리보기
			char sample[512];
			int previewWidth;
			int previewScale;
			int previewAtlas;				// atlases 의 위치 (아틀라스 창의 미리보기용)
			map<wstring, HangulPreviewAtlas> previewCache;

			// 메시지
			string message;
			bool messageIsError;

			HangulUiState();
			~HangulUiState();
			void releaseTextures();
		};

	}

}
