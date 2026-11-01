#include "ObjectHeaderListChunk.h"
#include "../../ImGUI/Headers.h"

using namespace CTT::Chunk;


// 항목 하나의 바이트 크기
static const size_t objectHeaderEntrySize = 16;


bool ObjectHeaderListChunk::loadFromOrgBuffer()
{
	raw.assign(orgBuffer, orgBuffer + orgSize);
	entries.clear();
	try {
		ByteBufferReader reader(raw);
		while (reader.remaining() >= objectHeaderEntrySize) {
			ObjectHeaderEntry entry;
			entry.handle = reader.readI16();
			entry.type = reader.readI16();
			entry.flags = reader.readI16();
			reader.skip(2);
			entry.ink = reader.readU8();
			reader.skip(7);
			entries.push_back(entry);
		}
	}
	catch (const exception&) {
		entries.clear();
	}
	free(orgBuffer);
	orgBuffer = NULL;
	return true;
}


bool ObjectHeaderListChunk::saveIntoOrgBuffer()
{
	if (orgBuffer != NULL) {
		free(orgBuffer);
		orgBuffer = NULL;
	}
	orgSize = (UINT32)raw.size();
	orgBuffer = (BYTE*)malloc(orgSize > 0 ? orgSize : 1);
	if (orgSize > 0) {
		memcpy(orgBuffer, &raw[0], orgSize);
	}
	return true;
}


void ObjectHeaderListChunk::renderData()
{
	ImGui::Text(u8"오브젝트 헤더 목록: %u개", (unsigned)entries.size());
}
