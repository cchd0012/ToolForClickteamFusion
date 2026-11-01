#include "CommonUtil.h"

using namespace CTT::CommonUtil;

wstring J2U(string &str);
wstring K2U(string &str);
wstring A2U(string &str);
wstring Utf82U(string &str);

string U2J(wstring& wstr);
string U2K(wstring& wstr);
string U2A(wstring& wstr);
string U2Utf8(wstring& wstr);


wstring CTT::CommonUtil::getExtentionRemovedPath(wstring &path) {
	wstring result;
	size_t lastDotPos = path.find_last_of(L".");
	if (lastDotPos == wstring::npos) {
		result = path;
	}
	else {
		result = path.substr(0, lastDotPos);
	}
	return result;
}

wstring CTT::CommonUtil::getExtention(wstring& path) {
	wstring result;
	size_t lastDotPos = path.find_last_of(L".");
	if (lastDotPos == wstring::npos) {
		result = L"";
	}
	else {
		result = path.substr(lastDotPos + 1);
	}
	return result;
}

wstring CTT::CommonUtil::getOnlyName(wstring &path) {
	wstring result = getExtentionRemovedPath(path);
	size_t lastSlashPos = path.find_last_of(L"/\\");
	if (lastSlashPos == wstring::npos) {
		result = path;
	}
	else {
		result = path.substr(lastSlashPos + 1);
	}
	return result;
}

wstring CTT::CommonUtil::getOnlyDirPath(wstring &path) {
	wstring result;
	size_t lastSlashPos = path.find_last_of(L"/\\");
	if (lastSlashPos == wstring::npos) {
		result = path;
	}
	else {
		result = path.substr(0, lastSlashPos);
	}
	return result;
}

wstring CTT::CommonUtil::getUniStr(string str, CodePage cp) {
	switch (cp)
	{
		case CodePage::CP_JAP: return J2U(str);
		case CodePage::CP_KOR: return K2U(str);
		case CodePage::CP_UTF_8: return Utf82U(str);
		default: return A2U(str);
	}
}

wstring J2U(string &str)
{
	wstring result;
	int nLen = MultiByteToWideChar(932, 0, str.c_str(), -1, NULL, NULL);
	result.resize(nLen + 1);
	MultiByteToWideChar(932, 0, str.c_str(), -1, (WCHAR*)result.c_str(), nLen);
	result.resize(wcslen(result.c_str()));
	return result;
}
wstring K2U(string &str)
{
	wstring result;
	int nLen = MultiByteToWideChar(949, 0, str.c_str(), -1, NULL, NULL);
	result.resize(nLen + 1);
	MultiByteToWideChar(949, 0, str.c_str(), -1, (WCHAR*)result.c_str(), nLen);
	result.resize(wcslen(result.c_str()));
	return result;
}
wstring A2U(string &str)
{
	wstring result;
	int nLen = MultiByteToWideChar(0, 0, str.c_str(), -1, NULL, NULL);
	result.resize(nLen + 1);
	MultiByteToWideChar(0, 0, str.c_str(), -1, (WCHAR*)result.c_str(), nLen);
	result.resize(wcslen(result.c_str()));
	return result;
}
wstring Utf82U(string &str)
{
	wstring result;
	int nLen = MultiByteToWideChar(65001, 0, str.c_str(), -1, NULL, NULL);
	result.resize(nLen + 1);
	MultiByteToWideChar(65001, 0, str.c_str(), -1, (WCHAR*)result.c_str(), nLen);
	result.resize(wcslen(result.c_str()));
	return result;
}

string CTT::CommonUtil::getAnsiStr(wstring wstr, CodePage cp) {
	switch (cp)
	{
		case CodePage::CP_JAP: return U2J(wstr);
		case CodePage::CP_KOR: return U2K(wstr);
		case CodePage::CP_UTF_8: return U2Utf8(wstr);
		default: return U2A(wstr);
	}
}

string U2J(wstring& wstr)
{
	string result;
	int nLen = WideCharToMultiByte(932, 0, wstr.c_str(), -1, NULL, NULL, NULL, NULL);
	result.resize(nLen + 1);
	WideCharToMultiByte(932, 0, wstr.c_str(), -1, (char*)result.c_str(), nLen, NULL, NULL);
	result.resize(strlen(result.c_str()));
	return result;
}
string U2K (wstring &wstr)
{
	string result;
	int nLen = WideCharToMultiByte(949, 0, wstr.c_str(), -1, NULL, NULL, NULL, NULL);
	result.resize(nLen + 1);
	WideCharToMultiByte(949, 0, wstr.c_str(), -1, (char*)result.c_str(), nLen, NULL, NULL);
	result.resize(strlen(result.c_str()));
	return result;
}
string U2A(wstring& wstr)
{
	string result;
	int nLen = WideCharToMultiByte(0, 0, wstr.c_str(), -1, NULL, NULL, NULL, NULL);
	result.resize(nLen + 1);
	WideCharToMultiByte(0, 0, wstr.c_str(), -1, (char*)result.c_str(), nLen, NULL, NULL);
	result.resize(strlen(result.c_str()));
	return result;
}
string U2Utf8(wstring& wstr)
{
	string result;
	int nLen = WideCharToMultiByte(65001, 0, wstr.c_str(), -1, NULL, NULL, NULL, NULL);
	result.resize(nLen + 1);
	WideCharToMultiByte(65001, 0, wstr.c_str(), -1, (char*)result.c_str(), nLen, NULL, NULL);
	result.resize(strlen(result.c_str()));
	return result;
}


wstring CTT::CommonUtil::byteToHexStr(BYTE b)
{
	wstringstream ss;
	wstring result;
	ss << uppercase << setw(2) << setfill(L'0') << hex << b;
	result = ss.str();
	return result;
}


bool CTT::CommonUtil::contains(const vector<wstring>& valueList, const wstring value)
{
	return find(valueList.begin(), valueList.end(), value) != valueList.end();
}

bool CTT::CommonUtil::contains(const vector<wstring>& valueList, const wstring value, INT32& index)
{
	if (find(valueList.begin(), valueList.end(), value) == valueList.end()) {
		index = -1;
		return false;
	}
	else {
		index = find(valueList.begin(), valueList.end(), value) - valueList.begin();
		return true;
	}
}


bool CTT::CommonUtil::contains(const vector<UINT32>& valueList, const UINT32 value)
{
	return find(valueList.begin(), valueList.end(), value) != valueList.end();
}

bool CTT::CommonUtil::contains(const vector<UINT32>& valueList, const UINT32 value, INT32& index)
{
	if (find(valueList.begin(), valueList.end(), value) == valueList.end()) {
		index = -1;
		return false;
	}
	else {
		index = find(valueList.begin(), valueList.end(), value) - valueList.begin();
		return true;
	}
}


// 문자열 양쪽의 공백을 제거하는 헬퍼 함수
wstring CTT::CommonUtil::trim(const wstring& str) 
{
	if (str.empty()) { return str; }
	size_t first = str.find_first_not_of(L" \t\n\r\f\v");
	size_t last = str.find_last_not_of(L" \t\n\r\f\v");
	return str.substr(first, last - first + 1);
}

vector<wstring> CTT::CommonUtil::split(const wstring& str)
{
	return CTT::CommonUtil::split(str, L",");
}

// 구분자를 이용해 문자열을 분할하는 함수
vector<wstring> CTT::CommonUtil::split(const wstring& str, const wstring& delimiters) {
	vector<wstring> tokens;
	wstring token;
	size_t start = 0;
	// 루프 돌기
	while (start < str.length()) {
		// 구분자를 찾아서 그 위치를 end에 저장
		size_t end = str.find_first_of(delimiters, start);
		// 구분자를 찾지 못하면 문자열의 끝
		if (end == wstring::npos) {
			token = str.substr(start);
			token = trim(token); // 양쪽 공백 제거
			if (!token.empty()) {
				tokens.push_back(token);
			}
			break; // 루프 종료
		}
		// 구분자 전의 문자열을 token에 저장
		token = str.substr(start, end - start);
		token = trim(token); // 양쪽 공백 제거
		// 길이가 0이 아닐때만 넣기
		if (!token.empty()) {
			tokens.push_back(token);
		}
		// 다음 검색 시작 위치를 구분자 다음으로 설정
		start = end + 1;
	}
	return tokens;
}

wstring CTT::CommonUtil::join(vector<wstring> const& strings, wstring delimiter)
{
	if (strings.empty()) {
		return wstring();
	}
	return accumulate(strings.begin() + 1, strings.end(), strings[0],
		[&delimiter](wstring x, wstring y) {
			return x + delimiter + y;
		}
	);
}


void CTT::CommonUtil::splitTwice(const wstring& str, wstring& first, wstring& second, wstring& third) {

	size_t firstPos = str.find(L'_');
	if (firstPos == wstring::npos) {
		first = str;
		second = L"";
		third = L"";
		return;
		first = str.substr(0, firstPos);
		second = str.substr(firstPos + 1);
	}

	size_t secondPos = str.find(L'_', firstPos + 1);
	if (secondPos == wstring::npos) {
		first = str.substr(0, firstPos);
		second = str.substr(firstPos + 1);
		third = L"";
		return;
	}

	first = str.substr(0, firstPos);
	second = str.substr(firstPos + 1, secondPos - firstPos - 1);
	third = str.substr(secondPos + 1);
	return;
}


// 모든 텍스트를 교체해주는 함수
wstring CTT::CommonUtil::replaceAll(wstring& str, const wstring& from, const wstring& to) {
	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != std::wstring::npos)
	{
		// 중복검사를 피하고 from.length() > to.length()인 경우를 위해서
		str.replace(start_pos, from.length(), to);
		start_pos += to.length();
	}
	return str;
}


//*파일이면 1, 디렉토리면 0, 이외에는 -1 반환 (유니코드)
FSOType CTT::CommonUtil::isFileOrDir(wstring& path) {
	_wfinddatai64_t c_file;
	intptr_t hFile;
	FSOType result;

	// 파일 또는 디렉토리가 없으면 -1 반환
	if ((hFile = _wfindfirsti64(path.c_str(), &c_file)) == -1L) {
		result = FSOType::FSO_NONE;
	}
	// 디렉토리면 0 반환
	else if (c_file.attrib & _A_SUBDIR) {
		result = FSOType::FSO_DIRECTORY;
	}
	// 그밖의 경우는 "존재하는 파일"이기에 1 반환
	else {
		result = FSOType::FSO_FILE;
	}

	_findclose(hFile);
	return result;
}


// 디렉토리 경로 재귀 생성
bool CTT::CommonUtil::createDirectoryRecursively(const wstring& path) {
	size_t pos = 0;
	wstring currentPath;

	while ((pos = path.find_first_of(L"\\/", pos)) != wstring::npos) {
		currentPath = path.substr(0, pos);
		// 디렉토리가 존재하지 않으면 생성
		if (!currentPath.empty()) {
			if (CreateDirectoryW(currentPath.c_str(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS) {
				// 디렉토리가 성공적으로 생성되었거나 이미 존재함
			}
			else {
				wcout << errPrefix << L"경로 생성을 실패했습니다: " << path << L": " << GetLastError() << endl;
				return false;
			}
		}
		pos++;
	}

	// 마지막 경로 생성
	if (!CreateDirectoryW(path.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
		wcout << errPrefix << L"경로 생성을 실패했습니다: " << path << L": " << GetLastError() << endl;
		return false;
	}

	return true;
}


bool CTT::CommonUtil::segmentedReadFile(HANDLE hRead, BYTE* buffer, UINT32 size)
{
	DWORD dRead;

	BYTE* tmpPartBuffer = (BYTE*)malloc(SEGMENT_SIZE);
	BYTE* bufferPnt = buffer;
	memset(tmpPartBuffer, 0, SEGMENT_SIZE);

	UINT32 leftSize = size;
	while (leftSize > SEGMENT_SIZE) {
		ReadFile(hRead, tmpPartBuffer, SEGMENT_SIZE, &dRead, NULL);
		if (dRead != SEGMENT_SIZE) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 데이터를 전부 읽어오지 못했습니다" << endl;
			return false;
		}
		memcpy(bufferPnt, tmpPartBuffer, SEGMENT_SIZE);
		bufferPnt += SEGMENT_SIZE;
		leftSize -= SEGMENT_SIZE;
	}

	ReadFile(hRead, tmpPartBuffer, leftSize, &dRead, NULL);
	if (dRead != leftSize) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 데이터를 전부 읽어오지 못했습니다" << endl;
		return false;
	}
	memcpy(bufferPnt, tmpPartBuffer, leftSize);
	free(tmpPartBuffer);
	
	return true;
}

void CTT::CommonUtil::segmentedWriteFile(HANDLE hWrite, BYTE* buffer, UINT32 size)
{
	DWORD dWrite;

	BYTE* tmpPartBuffer = (BYTE*)malloc(SEGMENT_SIZE);
	BYTE* bufferPnt = buffer;
	memset(tmpPartBuffer, 0, SEGMENT_SIZE);

	UINT32 leftSize = size;
	while (leftSize > SEGMENT_SIZE) {
		memcpy(tmpPartBuffer, bufferPnt, SEGMENT_SIZE);
		WriteFile(hWrite, tmpPartBuffer, SEGMENT_SIZE, &dWrite, NULL);
		bufferPnt += SEGMENT_SIZE;
		leftSize -= SEGMENT_SIZE;
	}

	memcpy(tmpPartBuffer, bufferPnt, leftSize);
	WriteFile(hWrite, tmpPartBuffer, leftSize, &dWrite, NULL);
	free(tmpPartBuffer);
}

bool CTT::CommonUtil::segmentedTransferData(HANDLE hSrc, HANDLE hDest, UINT32 size)
{
	DWORD dRead, dWrite;

	BYTE* tmpPartBuffer = (BYTE*)malloc(SEGMENT_SIZE);
	memset(tmpPartBuffer, 0, SEGMENT_SIZE);

	UINT32 leftSize = size;
	while (leftSize > SEGMENT_SIZE) {
		ReadFile(hSrc, tmpPartBuffer, SEGMENT_SIZE, &dRead, NULL);
		if (dRead != SEGMENT_SIZE) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 원본 파일에서 데이터를 전부 읽어오지 못했습니다" << endl;
			free(tmpPartBuffer);
			return false;
		}
		WriteFile(hDest, tmpPartBuffer, SEGMENT_SIZE, &dWrite, NULL);
		leftSize -= SEGMENT_SIZE;
	}

	ReadFile(hSrc, tmpPartBuffer, leftSize, &dRead, NULL);
	if (dRead != leftSize) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 원본 파일에서 데이터를 전부 읽어오지 못했습니다" << endl;
		free(tmpPartBuffer);
		return false;
	}
	WriteFile(hDest, tmpPartBuffer, leftSize, &dWrite, NULL);
	leftSize = 0;

	return true;
}


UINT32 CTT::CommonUtil::getChecksum(BYTE* buffer, UINT32 size)
{
	UINT32 alignedSize = size / sizeof(UINT32);
	BYTE* bufferPnt = buffer;
	UINT32 result = 0;

	if ((buffer == NULL) || (size == 0)) { 
		return result;
	}

	for (UINT32 i = 0; i < alignedSize; i++) {
		UINT32 tmpVal = *(UINT32*)bufferPnt;
		result = (result << 1) + (result >> 31) + tmpVal;
		bufferPnt += sizeof(UINT32);
	}
	for (UINT32 i = 0; i < (size & 0x3); i++) {
		BYTE tmpVal = *bufferPnt;
		result = (result << 1) + (result >> 31) + tmpVal;
		bufferPnt++;
	}

	return result;
}