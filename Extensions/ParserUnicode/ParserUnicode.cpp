// ParserUnicode : Clickteam Fusion 의 'String Parser'(parser.mfx, 확장 식별자 'PSE3') 중 dtge 가 쓰는 기능을 유니코드(UTF-16)로 다시 만든 확장
//
// 원본 parser.mfx 는 ANSI 방식이라, 유니코드 앱에서는 런타임이 문자열을 시스템 코드페이지(ANSI)로 바꿔서 넘긴다.
// 그러면 한글이 2바이트로 세어지거나 '?' 로 바뀌어, 글자 수로 줄 끝을 계산하는 이벤트와 어긋난다.
// 이 확장은 GetInfos(8)(KGI_UNICODE)에 1 을 돌려주어 런타임이 UTF-16 문자열을 그대로 넘기게 하고,
// 문자열을 글자(wchar_t) 단위로 다룬다.
//
// 구현한 기능 (원본의 동작/식 번호 = 표 번호 + 80)
//   동작 80 : 텍스트 저장                      (표 0)
//   동작 86 : 구분자 목록에 추가               (표 6)  긴 구분자가 앞에 오도록 정렬, 중복은 넣지 않는다
//   식  80  : 저장한 텍스트 반환               (표 0)
//   식  86  : 텍스트에서 문자열이 N번째로 나타나는 위치 (표 6)  1부터 센다, 없으면 -1, 텍스트가 없으면 0
//   식  109 : N번째 토큰 (표 29)  구분자로 텍스트를 나눈다. 연속된 구분자는 하나로 보고 앞쪽 구분자는 건너뛴다. N 은 1부터, 없으면 빈 문자열
// 그 밖의 조건/동작/식은 원본과 같은 번호에 자리만 두고 아무것도 하지 않는다 (dtge 는 쓰지 않는다)
//
// 런타임과의 약속(원본 코드를 분석해서 그대로 따랐다)
//   RUNDATA 크기 0xBC. +8 = 런타임 헤더 포인터, +0x7C = 헤더 플래그(식이 문자열을 돌려주면 0x8000 을 켠다), +0xB8 = 파서 객체
//   런타임 헤더 +0x36C(식 첫 인자 읽기) / +0x370(식 다음 인자 읽기) / +0x394(식이 돌려줄 문자열 공간 할당) 은 stdcall 함수 포인터
//   편집 데이터: +0x18 에 ANSI 초기 텍스트(최대 0x400바이트), +0x41A / +0x41C 에 16비트 플래그
//                +0x41A(flagC) 가 0 이면 대소문자를 구분하지 않고 비교한다. +0x41C(flagE) 는 원본에서도 비교 결과에 영향이 없다

#include <windows.h>
#include <string>
#include <vector>
#include <wchar.h>

using namespace std;

namespace {

	// 파서 객체 (RUNDATA +0xB8 이 가리킨다)
	typedef struct _parser_data
	{
		bool hasText;				// 텍스트를 한 번이라도 저장했는지 (원본의 NULL 포인터에 해당)
		wstring text;
		vector<wstring> delimiters;	// 긴 순서로 정렬된 구분자 목록
		WORD flagC;					// 0 이면 대소문자를 구분하지 않는다
		WORD flagE;
		_parser_data() : hasText(false), flagC(0), flagE(0) {}
	} ParserData;

	const int rundataParserOffset = 0xB8;
	const int rundataHeaderOffset = 0x08;
	const int rundataFlagsOffset = 0x7C;
	const WORD returnStringFlag = 0x8000;
	const int runtimeGetFirstParamOffset = 0x36C;
	const int runtimeGetNextParamOffset = 0x370;
	const int runtimeAllocStringOffset = 0x394;

	typedef LONG_PTR(__stdcall* RuntimeFunction)(void* rdPtr, LONG_PTR arg1, LONG_PTR arg2);

	ParserData* getParser(void* rdPtr)
	{
		return *(ParserData**)((BYTE*)rdPtr + rundataParserOffset);
	}


	RuntimeFunction getRuntimeFunction(void* rdPtr, int offset)
	{
		BYTE* header = *(BYTE**)((BYTE*)rdPtr + rundataHeaderOffset);
		return *(RuntimeFunction*)(header + offset);
	}


	// 식이 돌려줄 문자열을 런타임에서 할당받아 채운다. 크기 단위(글자/바이트)를 확신할 수 없어 바이트 수로 넉넉히 요청한다
	LONG_PTR returnString(void* rdPtr, const wstring& value)
	{
		LONG_PTR size = (LONG_PTR)((value.size() + 1) * sizeof(wchar_t));
		wchar_t* buffer = (wchar_t*)getRuntimeFunction(rdPtr, runtimeAllocStringOffset)(rdPtr, size, 0);
		*(WORD*)((BYTE*)rdPtr + rundataFlagsOffset) |= returnStringFlag;
		if (buffer != NULL) {
			wmemcpy(buffer, value.c_str(), value.size() + 1);
		}
		return (LONG_PTR)buffer;
	}


	bool sameText(const ParserData* parser, const wchar_t* a, const wchar_t* b, size_t count)
	{
		if (parser->flagC == 0) {
			return _wcsnicmp(a, b, count) == 0;
		}
		return wcsncmp(a, b, count) == 0;
	}


	// 텍스트의 position 에서 구분자가 시작하면 그 구분자의 길이를, 아니면 0 을 돌려준다 (긴 구분자부터 검사한다)
	size_t delimiterLengthAt(const ParserData* parser, size_t position)
	{
		size_t remaining = parser->text.size() - position;
		for (size_t i = 0; i < parser->delimiters.size(); i++) {
			const wstring& delimiter = parser->delimiters[i];
			if ((delimiter.size() <= remaining) && sameText(parser, delimiter.c_str(), parser->text.c_str() + position, delimiter.size())) {
				return delimiter.size();
			}
		}
		return 0;
	}


	// n 번째(1부터) 토큰이 시작하는 위치. 없으면 -1
	LONG_PTR tokenStart(const ParserData* parser, LONG_PTR n)
	{
		size_t length = parser->text.size();
		bool atStart = true;
		LONG_PTR counted = 0;
		for (size_t position = 0; position < length; position++) {
			size_t delimiter = delimiterLengthAt(parser, position);
			if (delimiter > 0) {
				position += delimiter - 1;
				atStart = true;
			}
			else if (atStart) {
				atStart = false;
				counted++;
				if (counted == n) {
					return (LONG_PTR)position;
				}
			}
		}
		return -1;
	}


	// start 부터 처음 만나는 구분자의 위치 (없으면 텍스트 끝)
	size_t tokenEnd(const ParserData* parser, size_t start)
	{
		size_t length = parser->text.size();
		for (size_t position = start; position < length; position++) {
			if (delimiterLengthAt(parser, position) > 0) {
				return position;
			}
		}
		return length;
	}


	// ---- 동작 80: 텍스트 저장
	LONG_PTR __stdcall actionSetText(void* rdPtr, LONG_PTR param1, LONG_PTR param2)
	{
		ParserData* parser = getParser(rdPtr);
		const wchar_t* text = (const wchar_t*)param1;
		if ((parser != NULL) && (text != NULL)) {
			parser->text = text;
			parser->hasText = true;
		}
		return 0;
	}


	// ---- 동작 86: 구분자 목록에 추가 (긴 구분자가 앞, 같은 길이끼리는 먼저 넣은 것이 앞, 같은 문자열은 한 번만)
	LONG_PTR __stdcall actionAddDelimiter(void* rdPtr, LONG_PTR param1, LONG_PTR param2)
	{
		ParserData* parser = getParser(rdPtr);
		const wchar_t* value = (const wchar_t*)param1;
		if ((parser == NULL) || (value == NULL)) {
			return 0;
		}
		size_t length = wcslen(value);
		if (length < 1) {
			return 0;
		}
		size_t index = 0;
		for (; index < parser->delimiters.size(); index++) {
			const wstring& other = parser->delimiters[index];
			if (length > other.size()) {
				break;
			}
			if ((length == other.size()) && sameText(parser, value, other.c_str(), length)) {
				return 0;	// 이미 있다
			}
		}
		parser->delimiters.insert(parser->delimiters.begin() + index, wstring(value, length));
		return 0;
	}


	// ---- 식 80: 저장한 텍스트 반환 (없으면 빈 문자열)
	LONG_PTR __stdcall expressionGetText(void* rdPtr, LONG_PTR param1)
	{
		ParserData* parser = getParser(rdPtr);
		if ((parser == NULL) || !parser->hasText) {
			return returnString(rdPtr, wstring());
		}
		return returnString(rdPtr, parser->text);
	}


	// ---- 식 86: 텍스트에서 문자열(첫 인자)이 N번째(둘째 인자)로 나타나는 위치. 겹치지 않게 세고, 1부터 센다
	LONG_PTR __stdcall expressionFindOccurrence(void* rdPtr, LONG_PTR param1)
	{
		const wchar_t* needle = (const wchar_t*)getRuntimeFunction(rdPtr, runtimeGetFirstParamOffset)(rdPtr, 1, param1);
		LONG_PTR occurrence = getRuntimeFunction(rdPtr, runtimeGetNextParamOffset)(rdPtr, 0, param1);

		ParserData* parser = getParser(rdPtr);
		if ((parser == NULL) || !parser->hasText || (needle == NULL)) {
			return 0;
		}
		const wstring& text = parser->text;
		size_t needleLength = wcslen(needle);
		if (needleLength == 0) {
			return 0;
		}
		if (text.size() < needleLength) {
			return -1;
		}
		size_t last = text.size() - needleLength;
		LONG_PTR counted = 1;
		for (size_t position = 0; position <= last; position++) {
			if (sameText(parser, needle, text.c_str() + position, needleLength)) {
				if (counted == occurrence) {
					return (LONG_PTR)(position + 1);
				}
				counted++;
				position += needleLength - 1;
			}
		}
		return -1;
	}


	// ---- 식 109: N번째 토큰 (없으면 빈 문자열)
	LONG_PTR __stdcall expressionGetToken(void* rdPtr, LONG_PTR param1)
	{
		LONG_PTR n = getRuntimeFunction(rdPtr, runtimeGetFirstParamOffset)(rdPtr, 0, param1);
		ParserData* parser = getParser(rdPtr);
		if ((parser == NULL) || !parser->hasText) {
			return returnString(rdPtr, wstring());
		}
		LONG_PTR start = tokenStart(parser, n);
		if (start < 0) {
			return returnString(rdPtr, wstring());
		}
		size_t end = tokenEnd(parser, (size_t)start);
		return returnString(rdPtr, parser->text.substr((size_t)start, end - (size_t)start));
	}


	// ---- 쓰지 않는 항목의 자리
	LONG_PTR __stdcall unusedCondition(void* rdPtr, LONG_PTR param1, LONG_PTR param2) { return 0; }
	LONG_PTR __stdcall unusedAction(void* rdPtr, LONG_PTR param1, LONG_PTR param2) { return 0; }
	LONG_PTR __stdcall unusedExpression(void* rdPtr, LONG_PTR param1) { return 0; }


	const int conditionCount = 1;
	const int actionCount = 0x18;
	const int expressionCount = 0x2A;

	void* conditionTable[conditionCount + 1];
	void* actionTable[actionCount + 2];
	void* expressionTable[expressionCount + 1];

	void buildTables()
	{
		conditionTable[0] = (void*)unusedCondition;
		conditionTable[1] = NULL;
		for (int i = 0; i < actionCount; i++) {
			actionTable[i] = (void*)unusedAction;
		}
		actionTable[0] = (void*)actionSetText;
		actionTable[6] = (void*)actionAddDelimiter;
		actionTable[actionCount] = NULL;
		actionTable[actionCount + 1] = NULL;
		for (int i = 0; i < expressionCount; i++) {
			expressionTable[i] = (void*)unusedExpression;
		}
		expressionTable[0] = (void*)expressionGetText;
		expressionTable[6] = (void*)expressionFindOccurrence;
		expressionTable[29] = (void*)expressionGetToken;
		expressionTable[expressionCount] = NULL;
	}

}


extern "C" {

	// 확장 정보. 0 = 버전, 6 = 제품, 7 = 빌드, 8 = 유니코드 여부(KGI_UNICODE)
	LONG_PTR __stdcall GetInfos(LONG_PTR which)
	{
		switch (which) {
			case 0: return 0x200;
			case 6: return 2;
			case 7: return 0xF3;
			case 8: return 1;
		}
		return 0;
	}


	// 런타임이 읽어 가는 확장 정보 구조체를 채운다
	short __stdcall GetRunObjectInfos(void* mV, void* info)
	{
		static bool built = false;
		if (!built) {
			buildTables();
			built = true;
		}
		BYTE* base = (BYTE*)info;
		*(void**)(base + 0x00) = conditionTable;
		*(void**)(base + 0x04) = actionTable;
		*(void**)(base + 0x08) = expressionTable;
		*(WORD*)(base + 0x0C) = conditionCount;
		*(WORD*)(base + 0x0E) = actionCount;
		*(WORD*)(base + 0x10) = expressionCount;
		*(WORD*)(base + 0x12) = 0x41E;			// 편집 데이터 크기
		*(DWORD*)(base + 0x14) = 0;
		*(BYTE*)(base + 0x18) = 10;
		*(WORD*)(base + 0x1A) = 0;
		*(DWORD*)(base + 0x1C) = 0x33455350;	// 'PSE3'
		*(WORD*)(base + 0x20) = 0;
		return 1;
	}


	short __stdcall GetRunObjectDataSize(void* rh, void* edPtr)
	{
		return 0xBC;
	}


	short __stdcall CreateRunObject(void* rdPtr, void* edPtr, void* cobPtr)
	{
		ParserData* parser = new ParserData();
		*(ParserData**)((BYTE*)rdPtr + rundataParserOffset) = parser;

		// 편집 데이터의 초기 텍스트는 ANSI 로 저장되어 있다
		const char* initial = (const char*)edPtr + 0x18;
		int bytes = 0;
		while ((bytes < 0x400) && (initial[bytes] != 0)) {
			bytes++;
		}
		if (bytes > 0) {
			int length = MultiByteToWideChar(CP_ACP, 0, initial, bytes, NULL, 0);
			parser->text.resize(length);
			MultiByteToWideChar(CP_ACP, 0, initial, bytes, &parser->text[0], length);
		}
		parser->hasText = true;
		parser->flagC = (*(WORD*)((BYTE*)edPtr + 0x41A) != 0) ? 1 : 0;
		parser->flagE = (*(WORD*)((BYTE*)edPtr + 0x41C) != 0) ? 1 : 0;
		return 0;
	}


	short __stdcall DestroyRunObject(void* rdPtr, long fast)
	{
		ParserData** slot = (ParserData**)((BYTE*)rdPtr + rundataParserOffset);
		delete *slot;
		*slot = NULL;
		return 0;
	}


	short __stdcall HandleRunObject(void* rdPtr) { return 1; }
	short __stdcall DisplayRunObject(void* rdPtr) { return 0; }
	short __stdcall ReInitRunObject(void* rdPtr) { return 0; }
	short __stdcall PauseRunObject(void* rdPtr) { return 0; }
	short __stdcall ContinueRunObject(void* rdPtr) { return 0; }

}
