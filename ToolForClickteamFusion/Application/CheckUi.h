#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>
#include "../Common.h"
#include "../Util/JsonUtil.h"
#include "FileWatch.h"

// 확인 단계(프레임 배치도)의 화면 상태
//
// 추출한 폴더의 파일만 읽는다: script/frame/*.json 의 layout(프레임 크기, 배경색, 오브젝트 인스턴스 위치),
// script/object/<번호>.json(이름, 종류, 애니메이션, 글자 오브젝트 문자열), image/imageList.json + image/*.png.
// 이벤트는 실행하지 않으므로 이벤트가 만들거나 옮기거나 숨기는 오브젝트는 반영되지 않는다 (처음 배치도다).

namespace CTT {

	namespace App {

		using namespace std;

		struct CheckFrameItem
		{
			int index;
			string name;				// UTF-8
			wstring path;
			CheckFrameItem() : index(0) {}
		};

		struct CheckImageInfo
		{
			wstring file;				// image 폴더 안의 PNG 이름
			int width;
			int height;
			int hotX;
			int hotY;
			bool triedLoad;
			LPDIRECT3DTEXTURE9 texture;
			CheckImageInfo() : width(0), height(0), hotX(0), hotY(0), triedLoad(false), texture(NULL) {}
		};

		struct CheckObjectInfo
		{
			bool found;					// 오브젝트 JSON 이 있다 (배경 오브젝트는 JSON 이 없다)
			string name;
			string identifier;
			int objectType;
			int imageHandle;			// 활성 오브젝트: 첫 애니메이션의 첫 그림 (0 이면 없음)
			int width;					// 텍스트 오브젝트의 크기
			int height;
			bool isBlitter;
			string blitterText;
			int fontTable;
			vector<string> flagNames;
			CheckObjectInfo() : found(false), objectType(0), imageHandle(0), width(0), height(0), isBlitter(false), fontTable(0) {}
		};

		// 지켜보는 폴더들에서 달라진 파일
		struct CheckChanges
		{
			WatchResult frames;
			WatchResult objects;
			WatchResult images;
			bool any() const { return frames.any() || objects.any() || images.any(); }
			wstring fingerprint() const { return fingerprintOf(frames) + L"#" + fingerprintOf(objects) + L"#" + fingerprintOf(images); }
		};

		struct CheckUiState
		{
			bool loaded;				// 프레임 목록을 읽었다
			string message;
			vector<CheckFrameItem> frames;
			int selectedFrame;			// frames 의 위치, -1 이면 없음
			bool layoutLoaded;
			Json::Value layout;			// 선택한 프레임 JSON 의 layout
			int frameWidth;
			int frameHeight;
			int background[3];
			map<int, CheckImageInfo> images;		// 그림 핸들 -> 정보
			map<int, CheckObjectInfo> objects;		// 오브젝트 번호 -> 정보
			int selectedInstance;		// layout.instances 의 위치, -1 이면 없음
			int zoom;					// 0 = 화면에 맞춤, 1 이상 = 배율
			bool showNames;
			bool showImages;
			bool useGlyphs;				// 글자 오브젝트를 배정한 폰트 아틀라스 글자로 그린다
			bool hideOverlays;			// 맨 위에 그려지는 전체 화면 그림(페이드 덮개 등)을 숨긴다
			set<int> hiddenObjects;		// 사용자가 숨긴 오브젝트 번호

			// 도구 밖에서 파일이 바뀌었는지 알아내는 기준 (읽을 때의 상태)
			DirSnapshot frameSnap;		// script/frame
			DirSnapshot objectSnap;		// script/object
			DirSnapshot imageSnap;		// image

			CheckUiState();
			~CheckUiState();
			void releaseTextures();
			void reset();
		};

	}

}
