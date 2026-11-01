#include "FrameVirtualRectChunk.h"
#include "../../Archive/Archive.h"
#include "../../ImGUI/Headers.h"

using namespace CTT::Chunk;
using namespace CTT::Archive;


FrameVirtualRectChunk::FrameVirtualRectChunk()
{
	memset(&value, 0, sizeof(FrameVirtualRect));
}


FrameVirtualRectChunk::~FrameVirtualRectChunk()
{
}


bool FrameVirtualRectChunk::loadFromOrgBuffer()
{
	// chunk 데이터: [FrameVirtualRect]
	memcpy(&value, orgBuffer, sizeof(FrameVirtualRect));
	free(orgBuffer);
	orgBuffer = NULL;
	return true;
}


bool FrameVirtualRectChunk::saveIntoOrgBuffer()
{
	if (orgBuffer != NULL) {
		free(orgBuffer);
		orgBuffer = NULL;
	}
	orgSize = sizeof(FrameVirtualRect);
	orgBuffer = (BYTE*)malloc(orgSize);
	memcpy(orgBuffer, &value, sizeof(FrameVirtualRect));
	return true;
}


// 데이터 렌더링용
void FrameVirtualRectChunk::renderData()
{
	stringstream oss;

	ImGui::Text("{");

	oss << u8"  left: " << value.left << ",\n";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << u8"  top: " << value.top << ",\n";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << u8"  right: " << value.right << ",\n";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << u8"  bottom: " << value.bottom << "\n";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	ImGui::Text("}");

}