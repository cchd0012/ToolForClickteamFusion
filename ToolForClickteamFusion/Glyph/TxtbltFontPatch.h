#pragma once

#include "../Common.h"

namespace CTT {

	namespace Glyph {

		using namespace std;

		// txtblt.mfx(Text Blitter) 가 폰트 아틀라스 + 글자표(KFT1, Glyph/FontAtlas.h)로 글자를 그리게 한다 (Hangul 방식)
		//  - 원본 txtblt.mfx(37376바이트 판. Sally, Sonic 3D 2D, PrototypeNCSAGE 등에 같은 것이 들어 있다)에 새 섹션 .kfont 를 붙이고
		//    오브젝트가 만들어질 때 편집 데이터에 적힌 글자표 번호(오브젝트 JSON 의 textBlitter.fontTable)로 글자표를 찾아 런타임 개체(+0x418)에 연결한다
		//  - 글자표가 연결된 오브젝트는 글자를 1바이트(ASCII) 또는 2바이트(CP949)로 읽어 글자표대로 그린다 (비례폭).
		//    연결되지 않은 오브젝트는 원래 방식 그대로 그린다
		//  - 줄 폭 계산(레이아웃)은 오브젝트마다 고른다: 기본은 원래대로 바이트 수 x 셀 폭이고, 편집 데이터 꼬리의 플래그(textBlitter.layout = "pixel")가
		//    켜진 오브젝트는 글자 폭(advance)의 합으로 자동 줄바꿈과 가운데/오른쪽 정렬을 계산한다 (한글 쌍은 줄 사이에서 끊기지 않는다)
		struct TxtbltFontAssignment
		{
			UINT32 id;									// 글자표 번호 (1 이상, 오브젝트 JSON 의 fontTable 과 같다)
			vector<unsigned char> table;				// KFT1 글자표
			TxtbltFontAssignment() : id(0) {}
		};

		// module 은 원본 txtblt.mfx 의 내용. 성공하면 패치된 내용으로 바뀐다. 실패하면 error 에 이유를 둔다
		bool patchTxtbltFont(vector<unsigned char>& module, const vector<TxtbltFontAssignment>& assignments, wstring& error);

		// 개발용 스위치: ToolForClickteamFusion.exe --txtblt-font patch <입력 mfx> <출력 mfx> <설정 json>
		//   설정: { "assignments": [ { "id": 1, "atlasDir": "폴더", "atlas": "이름" }, ... ] }  (경로는 설정 파일 기준)
		int runTxtbltFontCommand(int argc, WCHAR** argv);

	}

}
