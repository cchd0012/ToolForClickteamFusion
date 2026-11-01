#include "FrameLayerEffect.h"
#include "../../Archive/Archive.h"
#include "../../Util/ZlibUtil.h"
#include "../../Util/CommonUtil.h"

using namespace CTT::Chunk;
using namespace CTT::Archive;
using namespace CTT::ZlibUtil;
using namespace CTT::CommonUtil;


FrameLayerEffect::FrameLayerEffect() {
	memset(&inst, 0, sizeof(FrameEffectInstruction));
	baseBuffer = valuesBufferPnt = NULL;
}


FrameLayerEffect::~FrameLayerEffect() {
}


bool FrameLayerEffect::exportJson(Json::Value& exportValue, vector<UINT32>& offsetList)
{
	// value로 추출

	Json::Value instValue;

	instValue["effect"] = inst.info.effect;
	instValue["effect_parameter"] = inst.info.effectParam;
	instValue["rgb_coefficent"] = getAnsiStr(inst.info.RGBCoeff.toString(), CodePage::CP_UTF_8);

	Json::Value shaderInfoValue;

	if (inst.info.shaderIndex != -1) {

		shaderInfoValue["index"] = inst.info.shaderIndex;

		shaderInfoValue["name"] = getAnsiStr(inst.shaderName, CodePage::CP_UTF_8);

		Json::Value argumentsValue;

		for (UINT32 i = 0; i < inst.shaderArguments.size(); i++) {

			Json::Value argumentValue;

			argumentValue["name"] = getAnsiStr(inst.shaderArguments[i].info.name, CodePage::CP_UTF_8);
			switch (inst.shaderArguments[i].info.type)
			{
				case ShaderParameterType::INT:
				{
					argumentValue["type"] = "int";
					argumentValue["value"] = inst.shaderArguments[i].intValue;
					break;
				}
				case ShaderParameterType::FLOAT:
				{
					argumentValue["type"] = "float";
					argumentValue["value"] = inst.shaderArguments[i].floatValue;
					break;
				}
				case ShaderParameterType::FLOAT4_RGBA:
				{
					argumentValue["type"] = "float4_rgba";
					argumentValue["value"] = getAnsiStr(inst.shaderArguments[i].colorValue.toString(), CodePage::CP_UTF_8);
					break;
				}
				case ShaderParameterType::IMAGE_HANDLE:
				{
					argumentValue["type"] = "image_handle";
					argumentValue["value"] = inst.shaderArguments[i].imageHandle;
					break;
				}
				default:
				{
					argumentValue["type"] = "unknown";
					break;
				}	
			}
			argumentsValue.append(argumentValue);

		}

		shaderInfoValue["arguments"] = argumentsValue;

		instValue["shader_info"] = shaderInfoValue;

	}

	exportValue["inst"] = instValue;

	return true;
}


bool FrameLayerEffect::importJson(Json::Value& importValue, vector<UINT32>& offsetList)
{
	// value로부터 읽어들이기

	Json::Value instValue = importValue["inst"];

	Json::Value shaderInfoValue = instValue["shader_info"];

	inst.info.effect = (INT16)instValue["effect"].asInt();
	inst.info.effectParam = (INT16)instValue["effect_parameter"].asInt();
	inst.info.RGBCoeff.fromString(getUniStr(instValue["rgb_coefficent"].asCString(), CodePage::CP_UTF_8));

	if (instValue.isMember("shader_info")) {

		inst.info.shaderIndex = instValue["shader_index"].asInt();

		inst.shaderName = getUniStr(instValue["shader_name"].asCString(), CodePage::CP_UTF_8);

		Json::Value shaderArgumentsValue = instValue["arguments"];
		inst.shaderArguments.clear();

		for (UINT32 i = 0; i < shaderArgumentsValue.size(); i++) {

			ShaderArgument shaderArgument;
			memset(&shaderArgument, 0, sizeof(ShaderArgument));

			// 이름 복사가 그냥 하면 잘 안되는거 같음
			wstring shaderArgName = getUniStr(shaderArgumentsValue[i]["name"].asCString(), CodePage::CP_UTF_8);
			shaderArgument.info.name.resize(shaderArgName.size() + 1);
			memset((wchar_t*)shaderArgument.info.name.data(), 0, ((shaderArgName.size() + 1) * sizeof(wchar_t)));
			memcpy((wchar_t*)shaderArgument.info.name.data(), shaderArgName.data(), (shaderArgName.size() * sizeof(wchar_t)));

			wstring shaderArgType = getUniStr(shaderArgumentsValue[i]["type"].asCString(), CodePage::CP_UTF_8);

			if (shaderArgType == L"int") {
				shaderArgument.info.type = ShaderParameterType::INT;
				shaderArgument.intValue = shaderArgumentsValue[i]["value"].asUInt();
			}
			else if (shaderArgType == L"float") {
				shaderArgument.info.type = ShaderParameterType::FLOAT;
				shaderArgument.floatValue = shaderArgumentsValue[i]["value"].asFloat();
			}
			else if (shaderArgType == L"float4_rgba") {
				shaderArgument.info.type = ShaderParameterType::FLOAT4_RGBA;
				shaderArgument.colorValue.fromString(getUniStr(shaderArgumentsValue[i]["value"].asCString(), CodePage::CP_UTF_8));
			}
			else if (shaderArgType == L"image_handle") {
				shaderArgument.info.type = ShaderParameterType::IMAGE_HANDLE;
				shaderArgument.imageHandle = shaderArgumentsValue[i]["value"].asUInt();
			}

			inst.shaderArguments.push_back(shaderArgument);

		}

	}

	else {
		inst.info.shaderIndex = -1;
		inst.shaderName.clear();
		inst.shaderArguments.clear();
	}

	return true;
}


bool FrameLayerEffect::loadFromBuffer(BYTE*& bufferPnt)
{
	// baseBuffer가 NULL이면 그냥 리턴
	if (baseBuffer == NULL) {
		return false;
	}

	// info 확보
	memcpy(&inst.info, bufferPnt, sizeof(FrameEffectInfo));
	bufferPnt += sizeof(FrameEffectInfo);

	// paramsCount, valuesOffset 확보
	UINT32 paramsCount = *(UINT32*)(bufferPnt);
	bufferPnt += sizeof(UINT32);
	UINT32 valuesOffset = *(UINT32*)(bufferPnt);
	bufferPnt += sizeof(UINT32);

	// info.shaderIndex 값이 -1이면 그냥 리턴한다
	if (inst.info.shaderIndex == -1) {
		return true;
	}

	// baseBuffer, valuesOffset로부터 valuesBufferPnt 세팅하기
	valuesBufferPnt = baseBuffer + valuesOffset;

	// 사용하는 shader의 이름 및 parameter 리스트 확보
	Shader* shader = reinterpret_cast<Shader*>(getArchive()->getShaderListChunk()->getValue()[inst.info.shaderIndex]);
	inst.shaderName = shader->getName();

	vector<ShaderParameter> shaderParameters = shader->getParameterList();

	inst.shaderArguments.clear();
	inst.shaderArguments.resize(paramsCount);

	// parameter 읽어들이기
	for (UINT32 i = 0; i < paramsCount;i++) {

		memset(&inst.shaderArguments[i], 0, sizeof(ShaderArgument));

		// 이름 복사가 그냥 하면 잘 안되는거 같음
		inst.shaderArguments[i].info.name.resize(shaderParameters[i].name.size() + 1);
		memset((wchar_t*)inst.shaderArguments[i].info.name.data(), 0, ((shaderParameters[i].name.size() + 1) * sizeof(wchar_t)));
		memcpy((wchar_t*)inst.shaderArguments[i].info.name.data(), shaderParameters[i].name.data(), (shaderParameters[i].name.size() * sizeof(wchar_t)));

		inst.shaderArguments[i].info.type = shaderParameters[i].type;

		switch (inst.shaderArguments[i].info.type) {
			case ShaderParameterType::INT: {
				inst.shaderArguments[i].intValue = *(UINT32*)valuesBufferPnt;
				valuesBufferPnt += sizeof(UINT32);
				break;
			}
			case ShaderParameterType::FLOAT: {
				inst.shaderArguments[i].floatValue = *(float*)valuesBufferPnt;
				valuesBufferPnt += sizeof(float);
				break;
			}
			case ShaderParameterType::FLOAT4_RGBA: {
				inst.shaderArguments[i].colorValue = *(Color*)valuesBufferPnt;
				valuesBufferPnt += sizeof(Color);
				break;
			}
			case ShaderParameterType::IMAGE_HANDLE: {
				inst.shaderArguments[i].imageHandle = *(UINT32*)valuesBufferPnt;
				valuesBufferPnt += sizeof(UINT32);
				break;
			}
			default: {
				break;
			}
		}
	}

	return true;
}


bool FrameLayerEffect::saveIntoBuffer(BYTE*& bufferPnt)
{
	// valuesBufferPnt, baseBuffer가 NULL이면 그냥 리턴
	if ((valuesBufferPnt == NULL) || (baseBuffer == NULL)) {
		return false;
	}

	// info 기록
	memcpy(bufferPnt, &inst.info, sizeof(FrameEffectInfo));
	bufferPnt += sizeof(FrameEffectInfo);

	// paramsCount는 그냥 기록해도 되지만,
	// paramsOffset은 FrameLayerEffectDataChunk 에서 valuesBufferPnt 값을 계속 가지고
	// 버퍼에 기록하기 전에 paramsBuffer를 세팅해줘야 한다

	// paramsCount 세팅하기
	UINT32 paramsCount = (UINT32)inst.shaderArguments.size();

	// baseBuffer, paramBuffer로부터 valuesOffset 세팅하기
	UINT32 valuesOffset = (UINT32)(valuesBufferPnt - baseBuffer);

	// paramsCount가 0이면 paramsOffset도 0으로 세팅한다
	if (paramsCount == 0) {
		valuesOffset = 0;
	}

	// paramsCount, valuesOffset 기록
	*(UINT32*)(bufferPnt) = paramsCount;
	bufferPnt += sizeof(UINT32);
	*(UINT32*)(bufferPnt) = valuesOffset;
	bufferPnt += sizeof(UINT32);

	// parameter 기록하기
	for (UINT32 i = 0; i < paramsCount; i++) {
		switch (inst.shaderArguments[i].info.type) {
			case ShaderParameterType::INT: {
				*(UINT32*)valuesBufferPnt = inst.shaderArguments[i].intValue;
				valuesBufferPnt += sizeof(UINT32);
				break;
			}
			case ShaderParameterType::FLOAT: {
				*(float*)valuesBufferPnt = inst.shaderArguments[i].floatValue;
				valuesBufferPnt += sizeof(float);
				break;
			}
			case ShaderParameterType::FLOAT4_RGBA: {
				*(Color*)valuesBufferPnt = inst.shaderArguments[i].colorValue;
				valuesBufferPnt += sizeof(Color);
				break;
			}
			case ShaderParameterType::IMAGE_HANDLE: {
				*(UINT32*)valuesBufferPnt = inst.shaderArguments[i].imageHandle;
				valuesBufferPnt += sizeof(UINT32);
				break;
			}
			default: {
				break;
			}
		}
	}

	return true;
}


// 데이터 렌더링용 함수
void FrameLayerEffect::renderData(UINT32 index)
{
	stringstream oss;

	// 인덱스 표현
	oss << "  <" << index << ">{";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    inst: {";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "      effect: " << inst.info.effect << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "      effect_parameter: " << inst.info.effectParam << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "      rgb_coefficent:";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	// 컬러 창은 나타내되 선택 불가능하도록
	ImGuiColorEditFlags colorEditFlags =ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel;
	Color rgbCoeff = inst.info.RGBCoeff;
	ImVec4 rgbCoeffNm = ImVec4((float(rgbCoeff.r) / 255.0f), (float(rgbCoeff.g) / 255.0f), (float(rgbCoeff.b) / 255.0f), (float(rgbCoeff.a) / 255.0f));
	// -- 같은 라인 --
	ImGui::SameLine();
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3.0f);
	ImGui::PushID(index);
	ImGui::ColorEdit4("MyColor##1", (float*)&rgbCoeffNm, colorEditFlags);
	ImGui::PopID();
	// -- 같은 라인 --
	if (inst.info.shaderIndex != -1) {
		ImGui::SameLine(ImGui::CalcTextSize(u8"      rgb_coefficent:").x + 36.0f);
		ImGui::Text(",");
	}
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3.0f);

	if (inst.info.shaderIndex != -1) {

		oss << "      shader_info: {";
		ImGui::Text(oss.str().c_str());
		oss.str("");

		oss << "        name: \"" << getAnsiStr(inst.shaderName, CodePage::CP_UTF_8) << "\",";
		ImGui::Text(oss.str().c_str());
		oss.str("");

		oss << "        arguments: [";
		ImGui::Text(oss.str().c_str());
		oss.str("");

		for (UINT32 i = 0; i < inst.shaderArguments.size(); i++) {

			oss << "          <" << i << ">{";
			ImGui::Text(oss.str().c_str());
			oss.str("");

			oss << "            name: \"" << getAnsiStr(inst.shaderArguments[i].info.name, CodePage::CP_UTF_8) << "\",";
			ImGui::Text(oss.str().c_str());
			oss.str("");

			oss << "            type: ";
			switch (inst.shaderArguments[i].info.type)
			{
				case ShaderParameterType::INT:
				{
					oss << "int,";
					ImGui::Text(oss.str().c_str());
					oss.str("");
					oss << "            value: " << inst.shaderArguments[i].intValue;
					ImGui::Text(oss.str().c_str());
					oss.str("");
					break;
				}
				case ShaderParameterType::FLOAT:
				{
					oss << "float,";
					ImGui::Text(oss.str().c_str());
					oss.str("");
					oss << "            value: " << fixed << setprecision(5) << inst.shaderArguments[i].floatValue;
					ImGui::Text(oss.str().c_str());
					oss.str("");
					break;
				}
				case ShaderParameterType::FLOAT4_RGBA:
				{
					oss << "float4_rgba,";
					ImGui::Text(oss.str().c_str());
					oss.str("");
					oss << "            value:";
					ImGui::Text(oss.str().c_str());
					oss.str("");
					// 컬러 창은 나타내되 선택 불가능하도록
					ImGuiColorEditFlags colorEditFlags = ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel;
					Color bgColor = inst.shaderArguments[i].colorValue;
					ImVec4 bgColorNm = ImVec4((float(bgColor.r) / 255.0f), (float(bgColor.g) / 255.0f), (float(bgColor.b) / 255.0f), (float(bgColor.a) / 255.0f));
					// -- 같은 라인 --
					ImGui::SameLine();
					ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3.0f);
					ImGui::PushID(index * 0x100 + i);
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
					oss << "            vaule: " << inst.shaderArguments[i].imageHandle;
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

			if ((i + 1) < inst.shaderArguments.size()) {
				ImGui::Text("          },");
			}
			else {
				ImGui::Text("          }");
			}

		}

		oss << "        ]\n";
		ImGui::Text(oss.str().c_str());
		oss.str("");

		ImGui::Text("      }");

	}

	ImGui::Text("    }");

	ImGui::Text("  }");
}