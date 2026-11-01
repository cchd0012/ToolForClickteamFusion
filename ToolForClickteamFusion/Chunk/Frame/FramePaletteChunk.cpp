#include "FramePaletteChunk.h"
#include "../../Archive/Archive.h"
#include "../../ImGUI/Headers.h"

using namespace CTT::Chunk;
using namespace CTT::Archive;


FramePaletteChunk::FramePaletteChunk()
{
}


FramePaletteChunk::~FramePaletteChunk()
{
}


bool FramePaletteChunk::loadFromOrgBuffer()
{
	// chunk 데이터: vector<Color>, 0x101개로 고정인듯?
	value.clear();

	BYTE* orgBufferPnt = orgBuffer;
	for (UINT32 i = 0; i < 0x101;i++) {
		Color color = Color();
		color.r = *(orgBufferPnt++);
		color.g = *(orgBufferPnt++);
		color.b = *(orgBufferPnt++);
		color.a = *(orgBufferPnt++);
		value.push_back(color);
	}

	// 여기서 색상 초기화
	for (UINT32 i = 0; i < 0x101; i++)
	{
		colorPalette[i].x = (float)value[i].r / 255.0f;
		colorPalette[i].y = (float)value[i].g / 255.0f;
		colorPalette[i].z = (float)value[i].b / 255.0f;
		colorPalette[i].w = (float)value[i].a / 255.0f;
	}

	free(orgBuffer);
	orgBuffer = NULL;
	return true;
}


bool FramePaletteChunk::saveIntoOrgBuffer()
{
	if (orgBuffer != NULL) {
		free(orgBuffer);
		orgBuffer = NULL;
	}

	orgSize = sizeof(UINT32) * 0x101;
	orgBuffer = (BYTE*)malloc(orgSize);
	BYTE* orgBufferPnt = orgBuffer;
	
	for (UINT32 i = 0; i < 0x101; i++) {
		Color color = value[i];
		*(orgBufferPnt++) = color.r;
		*(orgBufferPnt++) = color.g;
		*(orgBufferPnt++) = color.b;
		*(orgBufferPnt++) = color.a;
	}

	return true;
}


// 데이터 렌더링용
void FramePaletteChunk::renderData()
{
	for (int n = 0; n < IM_ARRAYSIZE(colorPalette); n++)
	{
		ImGui::PushID(n);
		if ((n % 0x10) != 0) {
			ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.y);
		}
		ImGui::ColorButton("##palette", colorPalette[n], ImGuiColorEditFlags_None, ImVec2(20, 20));
		ImGui::PopID();
	}
}