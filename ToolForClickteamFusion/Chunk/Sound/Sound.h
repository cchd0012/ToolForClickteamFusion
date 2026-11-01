#pragma once

#include "../Chunk.h"
#include "../BaseData.h"
#include "../../Util/JsonUtil.h"

namespace CTT {

	namespace Chunk {

		using namespace CTT::JsonUtil;

		typedef struct _common_sound_info
		{
			UINT32 handle;		// 추후 남는 공간의 handle 위치를 넣거나, 꽉 차면 (handle최대값 + 1) 하면 될듯
			UINT32 checksum;	// 체크섬, 0이어도 상관없는듯?
			UINT32 reference;	// 대충 1이면 될듯
			UINT32 orgSize;		// *원본크기
			BYTE flags;			// 이건 그냥 이건 0x1인거 같음
			BYTE flags2;		// 이건 그냥 이건 0x40인거 같음
			BYTE padd[6];		// 0 채우기
			UINT32 nameLength;	// *이름 길이 (wcslen, 4의 배수여야 하나?)
		} CommonSoundInfo;

		class Sound : public BaseData
		{
		public:
			Sound();
			~Sound();

			UINT32 getDataSize() { return sizeof(CommonSoundInfo) + sizeof(UINT32) + compressedSize; }

			UINT32 getHandle() { return info.handle; }
			void setHandle(UINT32 handle) { info.handle = handle; }

			BYTE getFlags() { return info.flags; }
			void clearFlags() { info.flags = 0; }

			bool exportData(wstring& dirPath, UINT32 index, bool isFullyExport);
			bool importData(wstring& dirPath, UINT32 index);
			bool exportJson(Json::Value& exportValue, vector<UINT32>& offsetList) { return true; }
			bool importJson(Json::Value& importValue, vector<UINT32>& offsetList) { return true; }

			bool loadFromBuffer(BYTE*& bufferPnt);
			bool saveIntoBuffer(BYTE*& bufferPnt);

			// 데이터 렌더링용 함수
			void renderData(UINT32 index);

		private:
			bool deriveSoundData();
			bool deriveCompressedData();

			CommonSoundInfo info;
			BYTE *compressedData, *soundData;
			UINT32 compressedSize, soundSize;
		};

	}

}