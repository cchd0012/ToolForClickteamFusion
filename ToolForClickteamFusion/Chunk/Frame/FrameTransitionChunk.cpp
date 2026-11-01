#include "FrameTransitionChunk.h"
#include "../../Archive/Archive.h"
#include "../../Util/ZlibUtil.h"
#include "../../Util/CommonUtil.h"

using namespace CTT::Chunk;
using namespace CTT::Archive;
using namespace CTT::ZlibUtil;
using namespace CTT::CommonUtil;


FrameTransitionChunk::FrameTransitionChunk()
{
	memset(&value, 0, sizeof(FrameTransitionInfo));
}


FrameTransitionChunk::~FrameTransitionChunk()
{
}


bool FrameTransitionChunk::loadFromOrgBuffer()
{
	// chunk 데이터: [FrameTransitionInfo]

	BYTE* orgBufferPnt = orgBuffer;
	BYTE* baseBuffer = orgBuffer;

	char tmpStr[5];
	memset(tmpStr, 0, 5);

	memcpy(tmpStr, orgBufferPnt, 4);
	orgBufferPnt += 4;
	value.moduleType = getUniStr(tmpStr, CodePage::CP_ANSI);

	memcpy(tmpStr, orgBufferPnt, 4);
	orgBufferPnt += 4;
	value.name = getUniStr(tmpStr, CodePage::CP_ANSI);

	value.duration = *(UINT32*)orgBufferPnt;
	orgBufferPnt += sizeof(UINT32);

	value.flags = *(UINT32*)orgBufferPnt;
	orgBufferPnt += sizeof(UINT32);

	value.color = *(Color*)orgBufferPnt;
	orgBufferPnt += sizeof(Color);

	// 실제 모듈명의 이름을 가리키는 오프셋인가?
	UINT32 moduleFileNameOffset = *(UINT32*)orgBufferPnt;
	orgBufferPnt += sizeof(UINT32);

	UINT32 parameterOffset = *(UINT32*)orgBufferPnt;
	orgBufferPnt += sizeof(UINT32);

	UINT32 parameterSize = *(UINT32*)orgBufferPnt;
	orgBufferPnt += sizeof(UINT32);

	wchar_t* moduleFileNamePointer = (wchar_t*)(baseBuffer + moduleFileNameOffset);
	value.moduleFileName = moduleFileNamePointer;

	BYTE* parameterDataPointer = baseBuffer + parameterOffset;
	value.parameterData.resize(parameterSize, 0);
	memcpy(value.parameterData.data(), parameterDataPointer, parameterSize);

	free(orgBuffer);
	orgBuffer = NULL;
	return true;
}


bool FrameTransitionChunk::saveIntoOrgBuffer()
{
	// parameterOffset = 4 + 4 + (sizeof(UINT32) * 2) + sizeof(Color) + sizeof(wchar_t) * (value.moduleFileName 길이 + 1)
	// 크기 계산: parameterOffset + (value.parameterData의 크기)
	// parameterOffset은 parameterSize가 0이라도 정확히 기록해야 한다

	if (orgBuffer != NULL) {
		free(orgBuffer);
		orgBuffer = NULL;
	}

	UINT32 moduleFileNameOffset = 4 + 4 + (sizeof(UINT32) * 2) + sizeof(Color);
	UINT32 parameterOffset = moduleFileNameOffset + (sizeof(wchar_t) * ((UINT32)value.moduleFileName.length() + 1));
	UINT32 parameterSize = (UINT32)value.parameterData.size();
	orgSize = parameterOffset + parameterSize;

	orgBuffer = (BYTE*)malloc(orgSize);
	memset(orgBuffer, 0, orgSize);
	BYTE* orgBufferPnt = orgBuffer;
	BYTE* baseBuffer = orgBuffer;

	string moduleTypeAnsi = getAnsiStr(value.moduleType, CodePage::CP_ANSI);
	memcpy(orgBufferPnt, moduleTypeAnsi.data(), 4);
	orgBufferPnt += 4;

	string nameAnsi = getAnsiStr(value.name, CodePage::CP_ANSI);
	memcpy(orgBufferPnt, nameAnsi.data(), 4);
	orgBufferPnt += 4;

	value.duration = *(UINT32*)orgBufferPnt;
	orgBufferPnt += sizeof(UINT32);

	value.flags = *(UINT32*)orgBufferPnt;
	orgBufferPnt += sizeof(UINT32);

	value.color = *(Color*)orgBufferPnt;
	orgBufferPnt += sizeof(Color);

	*(UINT32*)orgBufferPnt = moduleFileNameOffset;
	orgBufferPnt += sizeof(UINT32);

	*(UINT32*)orgBufferPnt = parameterOffset;
	orgBufferPnt += sizeof(UINT32);

	*(UINT32*)orgBufferPnt = parameterSize;
	orgBufferPnt += sizeof(UINT32);

	wchar_t* moduleFileNamePointer = (wchar_t*)(baseBuffer + moduleFileNameOffset);
	memcpy(moduleFileNamePointer, value.moduleFileName.data(), (sizeof(wchar_t) * value.moduleFileName.length()));

	BYTE* parameterDataPointer = baseBuffer + parameterOffset;
	memcpy(parameterDataPointer, value.parameterData.data(), parameterSize);

	return true;
}


// 데이터 렌더링용
void FrameTransitionChunk::renderData()
{
	stringstream oss;

	ImGui::Text("{");

	oss << u8"  module_type: \"" << getAnsiStr(value.moduleType, CodePage::CP_UTF_8) << "\",\n";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << u8"  name: \"" << getAnsiStr(value.name, CodePage::CP_UTF_8) << "\",\n";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << u8"  duration: " << value.duration << ",\n";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << u8"  flags: " << value.flags << ",\n";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "  color:";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	// 컬러 창은 나타내되 선택 불가능하도록
	ImGuiColorEditFlags colorEditFlags = ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel;
	ImVec4 bgColor = ImVec4((float(value.color.r) / 255.0f), (float(value.color.g) / 255.0f), (float(value.color.b) / 255.0f), (float(value.color.a) / 255.0f));
	// -- 같은 라인 --
	ImGui::SameLine();
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3.0f);
	ImGui::ColorEdit4("MyColor##1", (float*)&bgColor, colorEditFlags);
	// -- 같은 라인 --
	ImGui::SameLine(ImGui::CalcTextSize(u8"  color:").x + 36.0f);
	ImGui::Text(",");
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3.0f);

	oss << u8"  module_file_name: \"" << getAnsiStr(value.moduleFileName, CodePage::CP_UTF_8) << "\",\n";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "  parameter_data: ";
	UINT32 maxCount = ((UINT32)value.parameterData.size() > 60) ? 60 : (UINT32)value.parameterData.size();
	oss << "[";
	for (UINT32 i = 0; i < maxCount; i++) {
		if (i % 0x10 == 0) {
			oss << "\n    ";
		}
		oss << setw(2) << setfill('0') << hex << uppercase << int(value.parameterData[i]) << " ";
	}
	if (maxCount != (UINT32)value.parameterData.size()) {
		oss << "... ";
	}
	oss << "\n";
	oss << "  ]";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	ImGui::Text("}");

}