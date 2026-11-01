#include "Shader.h"
#include "../../Archive/Archive.h"
#include "../../Util/JsonUtil.h"
#include "../../Util/CommonUtil.h"

using namespace CTT::Chunk;
using namespace CTT::Archive;
using namespace CTT::JsonUtil;
using namespace CTT::CommonUtil;


Shader::Shader() {
	memset(&header, 0, sizeof(ShaderHeader));
	totalDataSize = 0;
	compiledData = NULL;
	compiledDataSize = 0;
	backgroundTexture = 0;
}


Shader::~Shader() {
	if (compiledData != NULL) {
		free(compiledData);
		compiledData = NULL;
	}
}


bool Shader::exportData(wstring& dirPath, UINT32 index, bool isFullyExport)
{
	DWORD dWrite;

	// 추출 파일 경로 생성
	wstring exportJsonPath = dirPath + L"/" + getExtentionRemovedPath(name) + L"_params.json";
	wstring exportCompiledShaderPath = dirPath + L"/" + getExtentionRemovedPath(name) + L"_compiled." + getExtention(name);

	wcout << infoPrefix << L"exporting " << exportCompiledShaderPath << L"..." << endl;

	// json 데이터 추출 (일단 패러미터 타입만 넣어서 하는걸로 하자. 어차피 교체는 할 필요가 없을 것 같고)
	Json::Value root;
	Json::Value parameters;
	for (UINT32 i = 0; i < parameterList.size();i++) {
		Json::Value parameter;
		parameter["name"] = getAnsiStr(parameterList[i].name, CodePage::CP_UTF_8);
		switch (parameterList[i].type)
		{
			case ShaderParameterType::INT: 
			{
				parameter["type"] = "int";
				break;
			}
			case ShaderParameterType::FLOAT:
			{
				parameter["type"] = "float";
				break;
			}
			case ShaderParameterType::FLOAT4_RGBA:
			{
				parameter["type"] = "float4_rgba";
				break;
			}
			case ShaderParameterType::IMAGE_HANDLE:
			{
				parameter["type"] = "image_handle";
				break;
			}
			default:
			{
				parameter["type"] = "unknown";
				break;
			}
		}
		parameters.append(parameter);
	}
	root["parameters"] = parameters;

	if (!writeIntoJson(root, exportJsonPath)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": json 데이터를 기록하지 못했습니다." << endl;
		return false;
	}

	// 컴파일된 shader 데이터 추출
	HANDLE hCompiledShader = CreateFileW(exportCompiledShaderPath.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hCompiledShader == INVALID_HANDLE_VALUE) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(exportCompiledShaderPath) << L": 컴파일된 shader를 추출하지 못했습니다." << endl;
		return false;
	}
	WriteFile(hCompiledShader, compiledData, compiledDataSize, &dWrite, NULL);
	CloseHandle(hCompiledShader);

	// fully export 모드라면 디컴파일한 shader 데이터도 추출
	if (isFullyExport) {
	}

	return true;
}


bool Shader::importData(wstring& dirPath, UINT32 index)
{
	return true;
}


bool Shader::loadFromBuffer(BYTE*& bufferPnt)
{
	// 초기화
	memcpy(&header, bufferPnt, sizeof(ShaderHeader));

	if (compiledData != NULL) {
		free(compiledData);
		compiledData = NULL;
	}

	parameterList.clear();

	// 이름 확보 (string -> wstring)
	string nameAnsi = (char*)(bufferPnt + header.nameOffset);
	name = getUniStr(nameAnsi, CodePage::CP_UTF_8);

	// 데이터 크기 및 데이터 확보, parameterOffset 값이 0이면 전체 크기로부터 추정한다
	if (header.parameterOffset != 0) {
		compiledDataSize = header.parameterOffset - header.compiledDataOffset;
	}
	else {
		compiledDataSize = totalDataSize - header.compiledDataOffset;
	}
	compiledData = (BYTE*)malloc(compiledDataSize);
	memcpy(compiledData, (bufferPnt + header.compiledDataOffset), compiledDataSize);

	// 패러미터 정보 확보
	if (header.parameterOffset != 0) {

		BYTE* parameterBufferBasePnt = (bufferPnt + header.parameterOffset);
		BYTE* parameterBufferPnt = parameterBufferBasePnt;

		UINT32 parameterCount = *(UINT32*)(parameterBufferPnt);
		parameterBufferPnt += sizeof(UINT32);

		UINT32 typeOffset = *(UINT32*)(parameterBufferPnt);
		ShaderParameterType* parameterTypeListPnt = (ShaderParameterType*)(parameterBufferBasePnt + typeOffset);
		parameterBufferPnt += sizeof(UINT32);

		UINT32 nameOffset = *(UINT32*)(parameterBufferPnt);
		char* parameterNameListPnt = (char*)(parameterBufferBasePnt + nameOffset);
		parameterBufferPnt += sizeof(UINT32);

		for (UINT32 i = 0; i < parameterCount;i++) {
			ShaderParameter parameter;
			parameter.type = *(parameterTypeListPnt);
			parameterTypeListPnt++;
			string parameterNameAnsi = parameterNameListPnt;
			parameter.name = getUniStr(parameterNameAnsi, CodePage::CP_UTF_8);
			parameterNameListPnt += (strlen(parameterNameListPnt) + 1);
			parameterList.push_back(parameter);
		}

	}

	// 엔트리포인트는 직접 디코더를 넣어야 할거 같은데...

	return true;
}


bool Shader::saveIntoBuffer(BYTE*& bufferPnt)
{
	// *** 미구현 ***

	return true;
}


// 데이터 렌더링용 함수
void Shader::renderData(UINT32 index)
{
	stringstream oss;

	// 인덱스 표현
	oss << "  <" << index << ">{";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    name: \"" << getAnsiStr(name, CodePage::CP_UTF_8) << "\",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    background_texture: " << header.backgroundTexture << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	ImGui::Text("    parameters: [");

	for (UINT32 i = 0; i < parameterList.size(); i++) {

		oss << "      <" << i << ">{";
		ImGui::Text(oss.str().c_str());
		oss.str("");

		oss << "        name: \"" << getAnsiStr(parameterList[i].name, CodePage::CP_UTF_8) << "\",";
		ImGui::Text(oss.str().c_str());
		oss.str("");

		oss << "        type: ";
		switch (parameterList[i].type)
		{
			case ShaderParameterType::INT:
			{
				oss << "int";
				break;
			}
			case ShaderParameterType::FLOAT:
			{
				oss << "float";
				break;
			}
			case ShaderParameterType::FLOAT4_RGBA:
			{
				oss << "float4_rgba";
				break;
			}
			case ShaderParameterType::IMAGE_HANDLE:
			{
				oss << "image_handle";
				break;
			}
			default:
			{
				oss << "unknown";
				break;
			}
		}
		ImGui::Text(oss.str().c_str());
		oss.str("");

		ImGui::Text("      }");

		if ((i + 1) < parameterList.size()) {
			float commaStartPosX = ImGui::GetCursorPos().x + ImGui::CalcTextSize("      }").x;
			ImGui::SameLine(commaStartPosX);
			ImGui::Text(",");
		}

	}

	ImGui::Text("    ]");

	ImGui::Text("  }");
}