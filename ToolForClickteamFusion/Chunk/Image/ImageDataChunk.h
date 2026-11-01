#pragma once

#include "../DataChunk.h"
#include "../../Archive/Archive.h"
#include "NormalImage.h"
#include "TwoFivePlusImage.h"

namespace CTT {

	namespace Chunk {

		using namespace CTT::Archive;

		class ImageDataChunk : public DataChunk
		{
		public:
			// 이미지 목록(위치, 핸들, 크기, 그래픽 모드, 플래그)을 json 으로 내보낸다 (2.5+ 전용)
			// 오브젝트의 애니메이션 프레임이 가리키는 것은 위치가 아니라 핸들이다
			// json 형식: { "type": "imageList", "version": 1, "imageCount": 1700, "maxHandle": 2251, "nextHandle": 2252,
			//              "images": [ { "index": 0, "handle": 12, "width": 32, "height": 36, "graphicMode": 8, "flags": 136, "file": "000000.png" } ] }
			bool exportImageList(wstring& exportJsonPath);

			// 새 이미지를 PNG 파일들로부터 추가한다 (2.5+ 전용). PNG 경로는 baseDirPath 기준의 상대경로이다
			// json 형식: { "type": "imageAppend", "version": 1, "templateHandle": 2251,
			//              "images": [ { "handle": 2252, "file": "glyphs/0001.png" } ] }
			// templateHandle 의 이미지에서 플래그, 그래픽 모드, 핫스팟 등 헤더 값을 복제한다 (생략하면 기본값 사용)
			bool appendImages(wstring& manifestJsonPath, wstring& baseDirPath);

		protected:
			BaseData* allocateData()
			{
				BaseImage* img = NULL;
				// 이건 나중에 제보를 받으면서 확인하던가 해야겠음
				if (getArchive()->isTwoPointFiveImage()) {
					img = new TwoFivePlusImage();
				}
				else {
					img = new NormalImage();
				}
				return img;
			}
		};

	}

}
