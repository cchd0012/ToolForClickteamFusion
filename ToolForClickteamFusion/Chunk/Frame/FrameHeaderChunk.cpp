#include "FrameHeaderChunk.h"
#include "../../Archive/Archive.h"
#include "../../Util/CommonUtil.h"
#include "../../ImGUI/Headers.h"

using namespace CTT::Chunk;
using namespace CTT::Archive;
using namespace CTT::CommonUtil;

// 비트 위치별로 포함되는 정보
wstring frameFlagStr[] = {
	L"DisplayTitle",
	L"GrabDesktop",
	L"KeepDisplay",
	L"Unk1",
	L"Unk2",
	L"HandleCollision",
	L"Unk3",
	L"Unk4",
	L"ResizeAtStart",
	L"Unk5",
	L"Unk6",
	L"Unk7",
	L"Unk8",
	L"Unk9",
	L"Unk10",
	L"TimeMovements",
	L"Unk11",
	L"Unk12",
	L"DontInclude",
	L"DontEraseBG"
};


vector<string> CTT::Chunk::getFrameFlagProperties(UINT32 frameFlag)
{
	vector<string> result;
	for (UINT32 i = 0; i < 0x14; i++) {
		if ((frameFlag & (1 << i)) != 0) {
			string flagStrAnsi = getAnsiStr(frameFlagStr[i], CodePage::CP_UTF_8);
			result.push_back(flagStrAnsi);
		}
	}
	return result;
}


FrameHeaderChunk::FrameHeaderChunk()
{
	memset(&value, 0, sizeof(FrameHeader));
}


FrameHeaderChunk::~FrameHeaderChunk()
{
}


bool FrameHeaderChunk::loadFromOrgBuffer()
{
	// chunk 데이터: [FrameHeader]
	memcpy(&value, orgBuffer, sizeof(FrameHeader));
	free(orgBuffer);
	orgBuffer = NULL;
	return true;
}


bool FrameHeaderChunk::saveIntoOrgBuffer()
{
	if (orgBuffer != NULL) {
		free(orgBuffer);
		orgBuffer = NULL;
	}
	orgSize = sizeof(FrameHeader);
	orgBuffer = (BYTE*)malloc(orgSize);
	memcpy(orgBuffer, &value, sizeof(FrameHeader));
	return true;
}


// 데이터 렌더링용
void FrameHeaderChunk::renderData()
{
	stringstream oss;

	ImGui::Text("{");

	oss << "  width: " << value.width << "\n";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "  height: " << value.height << "\n";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "  bg_color:";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	// 컬러 창은 나타내되 선택 불가능하도록
	ImGuiColorEditFlags colorEditFlags = ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel;
	ImVec4 bgColor = ImVec4((float(value.bgColor.r) / 255.0f), (float(value.bgColor.g) / 255.0f), (float(value.bgColor.b) / 255.0f), (float(value.bgColor.a) / 255.0f));
	// -- 같은 라인 --
	ImGui::SameLine();
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3.0f);
	ImGui::ColorEdit4("MyColor##1", (float*)&bgColor, colorEditFlags);
	// -- 같은 라인 --
	ImGui::SameLine(ImGui::CalcTextSize(u8"  bg_color:").x + 36.0f);
	ImGui::Text(",");
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3.0f);

	vector<string> frameFlagPropertyStrings = getFrameFlagProperties(value.flag);
	oss << "  flag: [";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	for (size_t i = 0; i < frameFlagPropertyStrings.size(); i++) {
		oss << "     " << frameFlagPropertyStrings[i];
		if ((i + 1) < frameFlagPropertyStrings.size()) {
			oss << ",";
		}
		ImGui::Text(oss.str().c_str());
		oss.str("");
	}

	ImGui::Text("  ]");

	ImGui::Text("}");
}