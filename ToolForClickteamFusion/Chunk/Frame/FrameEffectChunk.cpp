#include "FrameEffectChunk.h"
#include "../../Archive/Archive.h"
#include "../../Util/ZlibUtil.h"
#include "../../Util/CommonUtil.h"

using namespace CTT::Chunk;
using namespace CTT::Archive;
using namespace CTT::ZlibUtil;
using namespace CTT::CommonUtil;


FrameEffectChunk::FrameEffectChunk()
{
	memset(&value, 0, sizeof(FrameEffectInstruction));
}


FrameEffectChunk::~FrameEffectChunk()
{
}


bool FrameEffectChunk::loadFromOrgBuffer()
{
	// chunk 데이터: [FrameEffectInstruction]

	BYTE* orgBufferPnt = orgBuffer;

	// info 확보
	memcpy(&value.info, orgBufferPnt, sizeof(FrameEffectInfo));
	orgBufferPnt += sizeof(FrameEffectInfo);

	// paramsCount 확보
	UINT32 paramsCount = *(UINT32*)(orgBufferPnt);
	orgBufferPnt += sizeof(UINT32);

	// info.shaderIndex 값이 -1이면 그냥 리턴한다
	if (value.info.shaderIndex == -1) {
		return true;
	}

	// 사용하는 shader의 이름 및 parameter 리스트 확보
	Shader* shader = reinterpret_cast<Shader*>(getArchive()->getShaderListChunk()->getValue()[value.info.shaderIndex]);
	value.shaderName = shader->getName();

	vector<ShaderParameter> shaderParameters = shader->getParameterList();

	value.shaderArguments.clear();
	value.shaderArguments.resize(paramsCount);

	// parameter 읽어들이기
	for (UINT32 i = 0; i < paramsCount; i++) {

		memset(&value.shaderArguments[i], 0, sizeof(ShaderArgument));

		// 이름 복사가 그냥 하면 잘 안되는거 같음
		value.shaderArguments[i].info.name.resize(shaderParameters[i].name.size() + 1);
		memset((wchar_t*)value.shaderArguments[i].info.name.data(), 0, ((shaderParameters[i].name.size() + 1) * sizeof(wchar_t)));
		memcpy((wchar_t*)value.shaderArguments[i].info.name.data(), shaderParameters[i].name.data(), (shaderParameters[i].name.size() * sizeof(wchar_t)));

		value.shaderArguments[i].info.type = shaderParameters[i].type;

		switch (value.shaderArguments[i].info.type) {
			case ShaderParameterType::INT: {
				value.shaderArguments[i].intValue = *(UINT32*)orgBufferPnt;
				orgBufferPnt += sizeof(UINT32);
				break;
			}
			case ShaderParameterType::FLOAT: {
				value.shaderArguments[i].floatValue = *(float*)orgBufferPnt;
				orgBufferPnt += sizeof(float);
				break;
			}
			case ShaderParameterType::FLOAT4_RGBA: {
				value.shaderArguments[i].colorValue = *(Color*)orgBufferPnt;
				orgBufferPnt += sizeof(Color);
				break;
			}
			case ShaderParameterType::IMAGE_HANDLE: {
				value.shaderArguments[i].imageHandle = *(UINT32*)orgBufferPnt;
				orgBufferPnt += sizeof(UINT32);
				break;
			}
			default: {
				break;
			}
		}

	}

	free(orgBuffer);
	orgBuffer = NULL;
	return true;
}


bool FrameEffectChunk::saveIntoOrgBuffer()
{

	// 크기 세팅
	orgSize = sizeof(FrameEffectInfo) + sizeof(UINT32); 
	
	for (UINT32 i = 0; i < value.shaderArguments.size(); i++) {
		switch (value.shaderArguments[i].info.type) {
			case ShaderParameterType::INT: {
				orgSize += sizeof(UINT32);
				break;
			}
			case ShaderParameterType::FLOAT: {
				orgSize += sizeof(float);
				break;
			}
			case ShaderParameterType::FLOAT4_RGBA: {
				orgSize += sizeof(Color);
				break;
			}
			case ShaderParameterType::IMAGE_HANDLE: {
				orgSize += sizeof(UINT32);
				break;
			}
			default: {
				break;
			}
		}
	}

	// 버퍼 할당
	if (orgBuffer != NULL) {
		free(orgBuffer);
		orgBuffer = NULL;
	}
	orgBuffer = (BYTE*)malloc(orgSize);
	memset(orgBuffer, 0, orgSize);

	// orgBuffer에 기록, 기록을 마친 데이터는 정리
	BYTE* orgBufferPnt = orgBuffer;

	// info 기록
	memcpy(orgBufferPnt, &value.info, sizeof(FrameEffectInfo));
	orgBufferPnt += sizeof(FrameEffectInfo);

	// paramsCount 기록
	UINT32 paramsCount = (UINT32)value.shaderArguments.size();
	*(UINT32*)(orgBufferPnt) = paramsCount;
	orgBufferPnt += sizeof(UINT32);
	
	// parameter 기록하기
	for (UINT32 i = 0; i < paramsCount; i++) {
		switch (value.shaderArguments[i].info.type) {
			case ShaderParameterType::INT: {
				*(UINT32*)orgBufferPnt = value.shaderArguments[i].intValue;
				orgBufferPnt += sizeof(UINT32);
				break;
			}
			case ShaderParameterType::FLOAT: {
				*(float*)orgBufferPnt = value.shaderArguments[i].floatValue;
				orgBufferPnt += sizeof(float);
				break;
			}
			case ShaderParameterType::FLOAT4_RGBA: {
				*(Color*)orgBufferPnt = value.shaderArguments[i].colorValue;
				orgBufferPnt += sizeof(Color);
				break;
			}
			case ShaderParameterType::IMAGE_HANDLE: {
				*(UINT32*)orgBufferPnt = value.shaderArguments[i].imageHandle;
				orgBufferPnt += sizeof(UINT32);
				break;
			}
			default: {
				break;
			}
		}
	}

	return true;
}


// 데이터 렌더링용
void FrameEffectChunk::renderData()
{
	stringstream oss;

	ImGui::Text("{");

	oss << "  effect: " << value.info.effect << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "  effect_parameter: " << value.info.effectParam << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "  rgb_coefficent:";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	// 컬러 창은 나타내되 선택 불가능하도록
	ImGuiColorEditFlags colorEditFlags = ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel;
	Color rgbCoeff = value.info.RGBCoeff;
	ImVec4 rgbCoeffNm = ImVec4((float(rgbCoeff.r) / 255.0f), (float(rgbCoeff.g) / 255.0f), (float(rgbCoeff.b) / 255.0f), (float(rgbCoeff.a) / 255.0f));
	// -- 같은 라인 --
	ImGui::SameLine();
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3.0f);
	ImGui::ColorEdit4("MyColor##1", (float*)&rgbCoeffNm, colorEditFlags);
	// -- 같은 라인 --
	if (value.info.shaderIndex != -1) {
		ImGui::SameLine(ImGui::CalcTextSize(u8"  rgb_coefficent:").x + 36.0f);
		ImGui::Text(",");
	}
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3.0f);

	if (value.info.shaderIndex != -1) {

		ImGui::Text("  shader_info: {");

		oss << "    name: \"" << getAnsiStr(value.shaderName, CodePage::CP_UTF_8) << "\",";
		ImGui::Text(oss.str().c_str());
		oss.str("");

		ImGui::Text("    arguments: [");

		for (UINT32 i = 0; i < value.shaderArguments.size(); i++) {

			oss << "      <" << i << ">{";
			ImGui::Text(oss.str().c_str());
			oss.str("");

			oss << "        name: \"" << getAnsiStr(value.shaderArguments[i].info.name, CodePage::CP_UTF_8) << "\",";
			ImGui::Text(oss.str().c_str());
			oss.str("");

			oss << "        type: ";
			switch (value.shaderArguments[i].info.type)
			{
				case ShaderParameterType::INT:
				{
					oss << "int,";
					ImGui::Text(oss.str().c_str());
					oss.str("");
					oss << "        value: " << value.shaderArguments[i].intValue;
					ImGui::Text(oss.str().c_str());
					oss.str("");
					break;
				}
				case ShaderParameterType::FLOAT:
				{
					oss << "float,";
					ImGui::Text(oss.str().c_str());
					oss.str("");
					oss << "        value: " << fixed << setprecision(5) << value.shaderArguments[i].floatValue;
					ImGui::Text(oss.str().c_str());
					oss.str("");
					break;
				}
				case ShaderParameterType::FLOAT4_RGBA:
				{
					oss << "float4_rgba,";
					ImGui::Text(oss.str().c_str());
					oss.str("");
					oss << "        value:";
					ImGui::Text(oss.str().c_str());
					oss.str("");
					// 컬러 창은 나타내되 선택 불가능하도록
					ImGuiColorEditFlags colorEditFlags = ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel;
					Color bgColor = value.shaderArguments[i].colorValue;
					ImVec4 bgColorNm = ImVec4((float(bgColor.r) / 255.0f), (float(bgColor.g) / 255.0f), (float(bgColor.b) / 255.0f), (float(bgColor.a) / 255.0f));
					// -- 같은 라인 --
					ImGui::SameLine();
					ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3.0f);
					ImGui::PushID(0x100);
					ImGui::ColorEdit4("MyColor2##1", (float*)&bgColorNm, colorEditFlags);
					ImGui::PopID();
					ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3.0f);
					break;
				}
				case ShaderParameterType::IMAGE_HANDLE:
				{
					oss << "image_handle,";
					ImGui::Text(oss.str().c_str());
					oss.str("");
					oss << "        vaule: " << value.shaderArguments[i].imageHandle;
					ImGui::Text(oss.str().c_str());
					oss.str("");
					break;
				}
				default:
				{
					oss << "unknown";
					ImGui::Text(oss.str().c_str());
					oss.str("");
					break;
				}
			}

			if ((i + 1) < value.shaderArguments.size()) {
				ImGui::Text("      },");
			}
			else {
				ImGui::Text("      }");
			}

		}

		ImGui::Text("    ]");

		ImGui::Text("  }");

	}

	ImGui::Text("}");

}