#pragma once

#include "../../Util/ByteBufferUtil.h"

// 2.5+ object property container:
//   chunk 0x2256 : per object [u32 decompressed size][u32 compressed size][zlib stream]  (one element per object handle)
//   chunk 0x2257 : u32[count] start offset of every element inside the 0x2256 payload (element 0 = 0)
//
// Elements are decompressed for editing. An element that is not changed is written back with its original
// compressed bytes, so an unchanged container is reproduced byte for byte.

namespace CTT {

	namespace Chunk {

		using namespace std;
		using namespace CTT::ByteBufferUtil;

		typedef struct _object_container_element {
			ByteBuffer data;				// decompressed ObjectCommon blob (or opaque property data)
			ByteBuffer originalCompressed;	// zlib stream as found in the archive
			bool changed;
			_object_container_element() : changed(false) {}
		} ObjectContainerElement;

		class ObjectElementContainer
		{
		public:
			// payload = decoded content of chunk 0x2256
			bool parse(const ByteBuffer& payload, string& errorMessage);

			// rebuild both chunk payloads (changed elements are recompressed with the same level the games use)
			bool build(ByteBuffer& payload2256, ByteBuffer& payload2257, string& errorMessage) const;

			vector<ObjectContainerElement>& getElements() { return elements; }

			// replace the data of one element (marks it changed)
			void setElementData(size_t index, const ByteBuffer& data);

			// diagnostics: does recompressing `data` give back exactly `compressed`?
			static bool isRecompressIdentical(const ByteBuffer& data, const ByteBuffer& compressed);

		private:
			vector<ObjectContainerElement> elements;
		};

	}

}
