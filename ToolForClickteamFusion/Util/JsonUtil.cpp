#include "JsonUtil.h"
#include "CommonUtil.h"

using namespace CTT::JsonUtil;
using namespace CTT::CommonUtil;


bool CTT::JsonUtil::readFromJson(Json::Value& value, wstring& path)
{
	DWORD dRead;
	
	wstring jsonPath = path;

	// 디렉토리가 존재하는지 파악하기
	wstring dirPath = getOnlyDirPath(path);
	if (path != dirPath) {
		if (isFileOrDir(dirPath) != FSOType::FSO_DIRECTORY) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << dirPath << L": 디렉토리가 존재하지 않습니다" << endl;
			return false;
		}
	}

	// json 확장자 없으면 붙여주기
	if (jsonPath.rfind(L".json") == wstring::npos) {
		jsonPath += L".json";
	}

	// 핸들 만들기
	HANDLE hJson = CreateFileW(jsonPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	// 파일 크기만큼 읽어들이기
	DWORD jsonSize = GetFileSize(hJson, NULL);
	std::string input(jsonSize, 0);
	if (!ReadFile(hJson, &input[0], jsonSize, &dRead, NULL)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << jsonPath << L": json 데이터를 읽어들이지 못했습니다" << endl;
		return false;
	}
	CloseHandle(hJson);

	// json 파싱
	Json::CharReaderBuilder reader;
	std::stringstream ss(input);
	std::string errs;
	if (!Json::parseFromStream(reader, ss, &value, &errs)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << jsonPath << L": json 데이터 파싱 실패: " << getUniStr(errs, CodePage::CP_UTF_8) << endl;
		return false;
	}

	return true;
}


namespace {

	// 압축했을 때의 길이를 대략 센다. budget 을 다 쓰면 바로 멈춘다 (큰 노드를 끝까지 세지 않기 위해)
	bool fitsInBudget(const Json::Value& value, int& budget)
	{
		if (value.isObject()) {
			budget -= 2;
			for (Json::Value::const_iterator it = value.begin(); it != value.end(); ++it) {
				budget -= (int)it.name().size() + 4;
				if (budget < 0 || !fitsInBudget(*it, budget)) {
					return false;
				}
			}
		}
		else if (value.isArray()) {
			budget -= 2;
			for (Json::ArrayIndex i = 0; i < value.size(); i++) {
				budget -= 1;
				if (budget < 0 || !fitsInBudget(value[i], budget)) {
					return false;
				}
			}
		}
		else if (value.isString()) {
			budget -= (int)value.asString().size() + 2;
		}
		else {
			budget -= 8;
		}
		return budget >= 0;
	}


	// 줄 단위로 압축해서 쓴다. 작은 노드는 한 줄로, 큰 노드는 자식을 한 줄씩 펼친다
	void writeInlineNode(Json::StreamWriter& compactWriter, const Json::Value& value, int depth, int inlineLimit, string& out)
	{
		bool isContainer = (value.isObject() || value.isArray()) && value.size() > 0;
		int budget = inlineLimit;
		if (!isContainer || fitsInBudget(value, budget)) {
			ostringstream stream;
			compactWriter.write(value, &stream);
			out += stream.str();
			return;
		}
		string childIndent((size_t)(depth + 1), ' ');
		string closeIndent((size_t)depth, ' ');
		if (value.isObject()) {
			out += "{\r\n";
			bool first = true;
			for (Json::Value::const_iterator it = value.begin(); it != value.end(); ++it) {
				if (!first) {
					out += ",\r\n";
				}
				first = false;
				ostringstream keyStream;
				compactWriter.write(Json::Value(it.name()), &keyStream);
				out += childIndent + keyStream.str() + ": ";
				writeInlineNode(compactWriter, *it, depth + 1, inlineLimit, out);
			}
			out += "\r\n" + closeIndent + "}";
		}
		else {
			out += "[\r\n";
			for (Json::ArrayIndex i = 0; i < value.size(); i++) {
				if (i > 0) {
					out += ",\r\n";
				}
				out += childIndent;
				writeInlineNode(compactWriter, value[i], depth + 1, inlineLimit, out);
			}
			out += "\r\n" + closeIndent + "]";
		}
	}

}


bool CTT::JsonUtil::writeIntoJson(Json::Value& value, wstring &path, UINT32 inlineLimit)
{
	DWORD dWrite;

	wstring jsonPath = path;

	// 먼저 디렉토리를 포함한 경로라면 디렉토리를 만들어야 한다
	wstring dirPath = getOnlyDirPath(path);
	if (path != dirPath) {
		if (!createDirectoryRecursively(dirPath)) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << dirPath << L": 디렉토리를 생성하지 못했습니다" << endl;
			return false;
		}
	}

	// json 확장자 없으면 붙여주기
	if (jsonPath.rfind(L".json") == wstring::npos) {
		jsonPath += L".json";
	}

	// 핸들 만들기
	HANDLE hJson = CreateFileW(jsonPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	// 핸들에 기록
	Json::StreamWriterBuilder builder;
	builder.settings_["emitUTF8"] = true;
	string output;
	if (inlineLimit == 0) {
		output = Json::writeString(builder, value);
	}
	else {
		builder.settings_["indentation"] = "";
		unique_ptr<Json::StreamWriter> compactWriter(builder.newStreamWriter());
		writeInlineNode(*compactWriter, value, 0, (int)inlineLimit, output);
	}
	WriteFile(hJson, output.c_str(), output.size(), &dWrite, NULL);
	CloseHandle(hJson);

	return true;
}