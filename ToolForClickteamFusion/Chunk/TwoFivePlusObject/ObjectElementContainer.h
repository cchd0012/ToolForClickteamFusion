#pragma once

#include "../../Util/ByteBufferUtil.h"

// 2.5+ 오브젝트 속성 컨테이너:
//   청크 0x2256 : 오브젝트별 [u32 압축 해제 크기][u32 압축 크기][zlib 스트림]  (오브젝트 핸들 하나당 요소 하나)
//   청크 0x2257 : u32[개수], 0x2256 데이터 안에서 각 요소가 시작하는 오프셋 (첫 요소 = 0)
//
// 요소는 편집을 위해 압축을 풀어 둔다. 변경하지 않은 요소는 원본 압축 바이트를 그대로 다시 쓰므로,
// 변경이 없는 컨테이너는 바이트 단위로 똑같이 재현된다.

namespace CTT {

	namespace Chunk {

		using namespace std;
		using namespace CTT::ByteBufferUtil;

		typedef struct _object_container_element {
			ByteBuffer data;				// 압축을 푼 ObjectCommon 데이터 (또는 해석하지 않는 속성 데이터)
			ByteBuffer originalCompressed;	// 아카이브에 들어 있던 zlib 스트림
			bool changed;
			_object_container_element() : changed(false) {}
		} ObjectContainerElement;

		class ObjectElementContainer
		{
		public:
			// payload = 복호화된 0x2256 청크의 내용
			bool parse(const ByteBuffer& payload, string& errorMessage);

			// 두 청크의 데이터를 다시 만든다 (변경한 요소는 게임이 사용하는 것과 같은 압축 레벨로 다시 압축한다)
			bool build(ByteBuffer& payload2256, ByteBuffer& payload2257, string& errorMessage) const;

			vector<ObjectContainerElement>& getElements() { return elements; }

			// 요소 하나의 데이터를 교체한다 (변경됨으로 표시한다)
			void setElementData(size_t index, const ByteBuffer& data);

			// 진단용: `data` 를 다시 압축하면 `compressed` 와 정확히 같아지는지 확인한다
			static bool isRecompressIdentical(const ByteBuffer& data, const ByteBuffer& compressed);

		private:
			vector<ObjectContainerElement> elements;
		};

	}

}
