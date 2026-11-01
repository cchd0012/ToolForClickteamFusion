#include "ObjectElementContainer.h"
#include "../../Util/ZlibUtil.h"

using namespace CTT::Chunk;
using namespace CTT::ZlibUtil;


// compress with ZlibUtil (Z_BEST_COMPRESSION, the level the games use)
static ByteBuffer deflateBuffer(const ByteBuffer& data)
{
	BYTE* encodedData = NULL;
	UINT32 encodedSize = 0;
	BYTE emptyByte = 0;
	if (!encode(data.empty() ? &emptyByte : (BYTE*)&data[0], (UINT32)data.size(), encodedData, encodedSize)) {
		throw ByteBufferError("zlib compress failed");
	}
	ByteBuffer out(encodedData, encodedData + encodedSize);
	free(encodedData);
	return out;
}


bool ObjectElementContainer::isRecompressIdentical(const ByteBuffer& data, const ByteBuffer& compressed)
{
	try {
		return deflateBuffer(data) == compressed;
	}
	catch (const exception&) {
		return false;
	}
}


bool ObjectElementContainer::parse(const ByteBuffer& payload, string& errorMessage)
{
	elements.clear();
	try {
		ByteBufferReader reader(payload);
		while (reader.remaining() > 0) {
			UINT32 orgSize = reader.readU32();
			UINT32 compSize = reader.readU32();
			if (compSize > reader.remaining()) {
				throw ByteBufferError("element exceeds container");
			}
			ObjectContainerElement element;
			element.originalCompressed = reader.readBytes(compSize);
			BYTE* decodedData = NULL;
			UINT32 decodedSize = orgSize;
			if (!decode(element.originalCompressed.empty() ? NULL : &element.originalCompressed[0], compSize, decodedData, decodedSize)) {
				throw ByteBufferError("element decompression failed");
			}
			// ZlibUtil does not report failed streams, so the size has to be checked here
			if (decodedSize != orgSize) {
				free(decodedData);
				throw ByteBufferError("element decompression failed");
			}
			element.data.assign(decodedData, decodedData + decodedSize);
			free(decodedData);
			elements.push_back(element);
		}
	}
	catch (const exception& e) {
		errorMessage = e.what();
		return false;
	}
	return true;
}


void ObjectElementContainer::setElementData(size_t index, const ByteBuffer& data)
{
	elements[index].data = data;
	elements[index].changed = true;
}


bool ObjectElementContainer::build(ByteBuffer& payload2256, ByteBuffer& payload2257, string& errorMessage) const
{
	try {
		ByteBufferWriter body;
		ByteBufferWriter offsets;
		for (size_t i = 0; i < elements.size(); i++) {
			const ObjectContainerElement& element = elements[i];
			offsets.writeU32((UINT32)body.size());
			ByteBuffer compressed;
			if (element.changed || element.originalCompressed.empty()) {
				compressed = deflateBuffer(element.data);
			}
			else {
				compressed = element.originalCompressed;
			}
			body.writeU32((UINT32)element.data.size());
			body.writeU32((UINT32)compressed.size());
			body.writeBytes(compressed);
		}
		payload2256 = body.getBuffer();
		payload2257 = offsets.getBuffer();
	}
	catch (const exception& e) {
		errorMessage = e.what();
		return false;
	}
	return true;
}
