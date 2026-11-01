// 무손실 청크 모델(프레임 이벤트, ObjectCommon, 2.5+ 요소 컨테이너)의 헤드리스 자체 테스트
//
// 사용법: ChunkModelTest.exe <테스트 데이터 루트> [게임 이름]
// 테스트 데이터 루트에는 덤프 스크립트가 만든 게임별 폴더가 있다:
//   <game>/meta.txt               build=<n> layout=<tfp|n284|old>
//   <game>/events/f<N>.evt        복호화된 0x333D 데이터
//   <game>/objects.bin            [u8 레이아웃][i32 build][i16 타입][u32 길이][데이터] 의 반복
//   <game>/c2256.bin, c2257.bin   복호화된 2.5+ 요소 컨테이너 (선택)

#include "../ToolForClickteamFusion/Chunk/Frame/FrameEvent/FrameEventModel.h"
#include "../ToolForClickteamFusion/Chunk/Frame/FrameEvent/FrameEventText.h"
#include "../ToolForClickteamFusion/Chunk/Object/ObjectCommon.h"
#include "../ToolForClickteamFusion/Chunk/Object/ObjectText.h"
#include "../ToolForClickteamFusion/Chunk/TwoFivePlusObject/ObjectElementContainer.h"

using namespace std;
using namespace CTT::Chunk;
using namespace CTT::ByteBufferUtil;

static int failureCount = 0;

static void fail(const string& what)
{
	failureCount++;
	if (failureCount <= 40) {
		printf("  FAIL: %s\n", what.c_str());
	}
}

static bool readFile(const string& path, ByteBuffer& out)
{
	FILE* f = fopen(path.c_str(), "rb");
	if (!f) {
		return false;
	}
	fseek(f, 0, SEEK_END);
	long length = ftell(f);
	fseek(f, 0, SEEK_SET);
	out.resize((size_t)length);
	if ((length > 0) && (fread(&out[0], 1, (size_t)length, f) != (size_t)length)) {
		fclose(f);
		return false;
	}
	fclose(f);
	return true;
}

static vector<string> listFiles(const string& dir, const string& pattern)
{
	vector<string> result;
	WIN32_FIND_DATAA data;
	HANDLE h = FindFirstFileA((dir + "\\" + pattern).c_str(), &data);
	if (h == INVALID_HANDLE_VALUE) {
		return result;
	}
	do {
		if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			result.push_back(data.cFileName);
		}
	} while (FindNextFileA(h, &data));
	FindClose(h);
	return result;
}

static vector<string> listDirs(const string& dir)
{
	vector<string> result;
	WIN32_FIND_DATAA data;
	HANDLE h = FindFirstFileA((dir + "\\*").c_str(), &data);
	if (h == INVALID_HANDLE_VALUE) {
		return result;
	}
	do {
		string name = data.cFileName;
		if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (name != ".") && (name != "..")) {
			result.push_back(name);
		}
	} while (FindNextFileA(h, &data));
	FindClose(h);
	return result;
}

static size_t firstDifference(const ByteBuffer& a, const ByteBuffer& b)
{
	size_t length = (a.size() < b.size()) ? a.size() : b.size();
	for (size_t i = 0; i < length; i++) {
		if (a[i] != b[i]) {
			return i;
		}
	}
	return length;
}


// ---------------------------------------------------------------- 프레임 이벤트

static vector<INT64> pointerSignature(const FrameEventChunkModel& model)
{
	vector<INT64> signature;
	for (size_t b = 0; b < model.blocks.size(); b++) {
		const vector<FrameEventDataGroup>& groups = model.blocks[b].groups;
		for (size_t g = 0; g < groups.size(); g++) {
			for (size_t c = 0; c < groups[g].conditions.size(); c++) {
				const vector<FrameEventParameter>& params = groups[g].conditions[c].params;
				for (size_t p = 0; p < params.size(); p++) {
					if (params[p].code == frameEventGroupPointerParameterCode) {
						signature.push_back(((INT64)params[p].targetGroupId << 32) | (UINT32)params[p].targetGroupDelta);
					}
				}
			}
			for (size_t a = 0; a < groups[g].actions.size(); a++) {
				const vector<FrameEventParameter>& params = groups[g].actions[a].params;
				for (size_t p = 0; p < params.size(); p++) {
					if (params[p].code == frameEventGroupPointerParameterCode) {
						signature.push_back(((INT64)params[p].targetGroupId << 32) | (UINT32)params[p].targetGroupDelta);
					}
				}
			}
		}
	}
	return signature;
}

static void collectParams(FrameEventDataGroup& group, vector<FrameEventParameter*>& params)
{
	for (size_t c = 0; c < group.conditions.size(); c++) {
		for (size_t p = 0; p < group.conditions[c].params.size(); p++) {
			params.push_back(&group.conditions[c].params[p]);
		}
	}
	for (size_t a = 0; a < group.actions.size(); a++) {
		for (size_t p = 0; p < group.actions[a].params.size(); p++) {
			params.push_back(&group.actions[a].params[p]);
		}
	}
}

// 모든 문자열 토큰 뒤에 표식을 붙여서 모든 그룹의 크기가 바뀌고 모든 포인터가 이동해야 하게 만든다
static size_t growAllStrings(FrameEventChunkModel& model)
{
	size_t changed = 0;
	for (size_t b = 0; b < model.blocks.size(); b++) {
		vector<FrameEventDataGroup>& groups = model.blocks[b].groups;
		for (size_t g = 0; g < groups.size(); g++) {
			vector<FrameEventParameter*> params;
			collectParams(groups[g], params);
			for (size_t i = 0; i < params.size(); i++) {
				if (!isFrameEventExpressionParameter(params[i]->code)) {
					continue;
				}
				FrameEventExpression expression;
				string errorMessage;
				if (!parseFrameEventExpression(params[i]->body, expression, errorMessage)) {
					continue;
				}
				bool any = false;
				for (size_t t = 0; t < expression.tokens.size(); t++) {
					wstring text;
					if (getFrameEventExpressionString(expression.tokens[t], text)) {
						if (setFrameEventExpressionString(expression.tokens[t], text + L"\xD55C\xAE00")) {
							any = true;
							changed++;
						}
					}
				}
				if (any) {
					params[i]->body = buildFrameEventExpression(expression);
				}
			}
		}
	}
	return changed;
}

static void testEvents(const string& gameDir, UINT32 build)
{
	vector<string> files = listFiles(gameDir + "\\events", "*.evt");
	size_t frames = 0, groups = 0, pointers = 0, unresolved = 0, expressionParams = 0, expressionFailures = 0, stringTokens = 0, grown = 0, exact = 0;
	for (size_t i = 0; i < files.size(); i++) {
		ByteBuffer data;
		if (!readFile(gameDir + "\\events\\" + files[i], data)) {
			fail("read " + files[i]);
			continue;
		}
		FrameEventChunkModel model;
		string errorMessage;
		if (!parseFrameEventChunk(data, build, model, errorMessage)) {
			fail(files[i] + " parse: " + errorMessage);
			continue;
		}
		frames++;
		groups += model.getGroupCount();
		unresolved += countUnresolvedFrameEventPointers(model);
		vector<INT64> signature = pointerSignature(model);
		pointers += signature.size();

		ByteBuffer out;
		if (!writeFrameEventChunk(model, out, errorMessage)) {
			fail(files[i] + " write: " + errorMessage);
			continue;
		}
		if (out == data) {
			exact++;
		}
		else {
			char text[200];
			sprintf(text, "%s byte mismatch: orig %u bytes, new %u bytes, first difference at %u", files[i].c_str(), (unsigned)data.size(), (unsigned)out.size(), (unsigned)firstDifference(data, out));
			fail(text);
			continue;
		}

		// 통계 + 모든 수식 파라미터의 인코딩 왕복 검증
		for (size_t b = 0; b < model.blocks.size(); b++) {
			for (size_t g = 0; g < model.blocks[b].groups.size(); g++) {
				vector<FrameEventParameter*> params;
				collectParams(model.blocks[b].groups[g], params);
				for (size_t k = 0; k < params.size(); k++) {
					if (!isFrameEventExpressionParameter(params[k]->code)) {
						continue;
					}
					expressionParams++;
					FrameEventExpression expression;
					if (!parseFrameEventExpression(params[k]->body, expression, errorMessage)) {
						expressionFailures++;
						continue;
					}
					if (buildFrameEventExpression(expression) != params[k]->body) {
						fail("expression param does not re-encode identically");
					}
					for (size_t t = 0; t < expression.tokens.size(); t++) {
						wstring text;
						if (getFrameEventExpressionString(expression.tokens[t], text)) {
							stringTokens++;
						}
					}
				}
			}
		}

		// 재배치 검증: 모든 문자열을 늘려서 쓰고 다시 파싱해도 포인터는 같은 그룹을 가리켜야 한다
		FrameEventChunkModel grownModel = model;
		grown += growAllStrings(grownModel);
		ByteBuffer grownData;
		if (!writeFrameEventChunk(grownModel, grownData, errorMessage)) {
			fail(files[i] + " write(grown): " + errorMessage);
			continue;
		}
		FrameEventChunkModel reparsed;
		if (!parseFrameEventChunk(grownData, build, reparsed, errorMessage)) {
			fail(files[i] + " reparse(grown): " + errorMessage);
			continue;
		}
		if (pointerSignature(reparsed) != signature) {
			fail(files[i] + " group pointers changed target after resize");
		}
		if (reparsed.getGroupCount() != model.getGroupCount()) {
			fail(files[i] + " group count changed");
		}
	}
	printf("  events : %u frames, %u groups, byte-exact %u/%u, pointers %u (unresolved %u), expr params %u (unparsable %u), string tokens %u, resized strings %u\n",
		(unsigned)frames, (unsigned)groups, (unsigned)exact, (unsigned)frames, (unsigned)pointers, (unsigned)unresolved, (unsigned)expressionParams, (unsigned)expressionFailures, (unsigned)stringTokens, (unsigned)grown);
}


// ---------------------------------------------------------------- 프레임 이벤트 텍스트

static const wchar_t* translationMarker = L"\xD55C";	// 번역문 끝에 붙이는 표식 ('한')

// 모델의 비어 있지 않은 문자열 토큰 수와, 그중 표식으로 끝나는 토큰 수를 센다
static void countStringTokens(FrameEventChunkModel& model, UINT32& total, UINT32& withMarker)
{
	total = 0;
	withMarker = 0;
	for (size_t b = 0; b < model.blocks.size(); b++) {
		for (size_t g = 0; g < model.blocks[b].groups.size(); g++) {
			vector<FrameEventParameter*> params;
			collectParams(model.blocks[b].groups[g], params);
			for (size_t k = 0; k < params.size(); k++) {
				if (!isFrameEventExpressionParameter(params[k]->code)) {
					continue;
				}
				FrameEventExpression expression;
				string errorMessage;
				if (!parseFrameEventExpression(params[k]->body, expression, errorMessage)) {
					continue;
				}
				for (size_t t = 0; t < expression.tokens.size(); t++) {
					wstring text;
					if (getFrameEventExpressionString(expression.tokens[t], text) && !text.empty()) {
						total++;
						if (text.size() > 0 && text.substr(text.size() - 1) == translationMarker) {
							withMarker++;
						}
					}
				}
			}
		}
	}
}

static void testEventText(const string& gameDir, UINT32 build)
{
	vector<string> files = listFiles(gameDir + "\\events", "*.evt");

	// 1) 모든 프레임의 문자열을 표에 모은다
	FrameEventTextTable table;
	UINT32 collected = 0;
	for (size_t i = 0; i < files.size(); i++) {
		ByteBuffer data;
		FrameEventChunkModel model;
		string errorMessage;
		if (!readFile(gameDir + "\\events\\" + files[i], data) || !parseFrameEventChunk(data, build, model, errorMessage)) {
			fail(files[i] + " text collect: " + errorMessage);
			return;
		}
		collected += table.addFrameEventText(model, (UINT32)i);
	}

	// 2) JSON 으로 내보냈다가 다시 읽는다. 번역 없는 표와, 모든 항목에 번역을 단 표를 만든다
	Json::Value untranslatedJson;
	table.exportJson(untranslatedJson);
	Json::Value translatedJson = untranslatedJson;
	for (Json::ArrayIndex i = 0; i < translatedJson["texts"].size(); i++) {
		Json::Value& item = translatedJson["texts"][i];
		item["translation"] = item["original"].asString() + "\xED\x95\x9C";	// UTF-8 '한'
	}
	FrameEventTextTable emptyTable;
	FrameEventTextTable translatedTable;
	if (!emptyTable.importJson(untranslatedJson) || !translatedTable.importJson(translatedJson)) {
		fail("text table json import");
		return;
	}
	if ((emptyTable.getEntryCount() != table.getEntryCount()) || (translatedTable.getEntryCount() != table.getEntryCount())) {
		fail("text table entry count changed by the json round trip");
	}

	// 3) 번역이 없으면 바이트가 그대로여야 하고, 모두 번역하면 모든 토큰이 바뀌어야 하며 포인터는 그대로여야 한다
	UINT32 replacedTotal = 0, tokenTotal = 0;
	for (size_t i = 0; i < files.size(); i++) {
		ByteBuffer data;
		FrameEventChunkModel model;
		string errorMessage;
		readFile(gameDir + "\\events\\" + files[i], data);
		parseFrameEventChunk(data, build, model, errorMessage);
		vector<INT64> signature = pointerSignature(model);

		FrameEventChunkModel untouched = model;
		UINT32 replaced = 0;
		ByteBuffer untouchedData;
		if (!replaceFrameEventText(untouched, emptyTable, replaced, errorMessage) || (replaced != 0)
			|| !writeFrameEventChunk(untouched, untouchedData, errorMessage) || (untouchedData != data)) {
			fail(files[i] + " empty translation table changed the events");
			continue;
		}

		UINT32 expectedTokens, markerBefore;
		countStringTokens(model, expectedTokens, markerBefore);
		if (!replaceFrameEventText(model, translatedTable, replaced, errorMessage)) {
			fail(files[i] + " replace: " + errorMessage);
			continue;
		}
		ByteBuffer translatedData;
		if (!writeFrameEventChunk(model, translatedData, errorMessage)) {
			fail(files[i] + " write(translated): " + errorMessage);
			continue;
		}
		FrameEventChunkModel reparsed;
		if (!parseFrameEventChunk(translatedData, build, reparsed, errorMessage)) {
			fail(files[i] + " reparse(translated): " + errorMessage);
			continue;
		}
		UINT32 totalAfter, markerAfter;
		countStringTokens(reparsed, totalAfter, markerAfter);
		replacedTotal += replaced;
		tokenTotal += expectedTokens;
		if ((replaced != expectedTokens) || (totalAfter != expectedTokens) || (markerAfter != expectedTokens)) {
			fail(files[i] + " not every string token was translated");
		}
		if (pointerSignature(reparsed) != signature) {
			fail(files[i] + " group pointers changed target after text replacement");
		}
	}
	printf("  text   : %u string tokens, %u unique texts, translated %u/%u tokens, empty table keeps bytes, pointers kept\n",
		(unsigned)collected, (unsigned)table.getEntryCount(), (unsigned)replacedTotal, (unsigned)tokenTotal);
}


// ---------------------------------------------------------------- ObjectCommon

static void testObjects(const string& gameDir)
{
	ByteBuffer all;
	if (!readFile(gameDir + "\\objects.bin", all)) {
		printf("  objects: (no objects.bin)\n");
		return;
	}
	ByteBufferReader reader(all);
	size_t total = 0, parseFailures = 0, canonicalOk = 0, untouchedOk = 0, withAnimations = 0, withValues = 0, withStrings = 0, withText = 0, mutated = 0, animationEdits = 0;
	while (reader.remaining() > 0) {
		BYTE layoutId = reader.readU8();
		INT32 build = reader.readI32();
		INT16 type = reader.readI16();
		UINT32 length = reader.readU32();
		ByteBuffer blob = reader.readBytes(length);
		total++;
		ObjectCommon common;
		string errorMessage;
		if (!common.parse(blob, (ObjectCommonLayout)layoutId, (UINT32)build, errorMessage)) {
			parseFailures++;
			if (parseFailures <= 5) {
				printf("  object parse failure (type %d, length %u): %s\n", type, length, errorMessage.c_str());
			}
			continue;
		}
		if (common.hasAnimations()) { withAnimations++; }
		if (common.hasValues()) { withValues++; }
		if (common.hasStrings()) { withStrings++; }
		if (common.hasText()) { withText++; }
		if (common.getIdentifier().empty()) {
			fail("no identifier");
		}

		ByteBuffer out;
		if (!common.serialize(out, errorMessage)) {
			fail("serialize: " + errorMessage);
			continue;
		}
		if (out == blob) {
			untouchedOk++;
		}
		else {
			char text[120];
			sprintf(text, "untouched serialize differs (type %d, first difference %u)", type, (unsigned)firstDifference(blob, out));
			fail(text);
		}

		if (!common.serializeCanonical(out, errorMessage)) {
			fail("serializeCanonical: " + errorMessage);
			continue;
		}
		if (out == blob) {
			canonicalOk++;
		}
		else {
			char text[160];
			sprintf(text, "canonical re-encode differs (type %d, length %u -> %u, first difference %u)", type, length, (unsigned)out.size(), (unsigned)firstDifference(blob, out));
			fail(text);
			continue;
		}

		// 변형 검증: 모든 항목을 늘려서 다시 만들고 다시 파싱한다
		ObjectCommon changed = common;
		bool animationEdited = false;
		if (changed.hasAnimations()) {
			vector<ObjectAnimation>& slots = changed.getAnimations().slots;
			for (size_t s = 0; (s < slots.size()) && !animationEdited; s++) {
				if (!slots[s].used || slots[s].directions.empty()) {
					continue;
				}
				slots[s].directions.begin()->second.frames.push_back(1234);
				changed.markAnimationsChanged();
				animationEdited = true;
			}
		}
		if (changed.hasValues()) {
			changed.getValues().push_back(77);
			changed.markValuesChanged();
		}
		if (changed.hasStrings()) {
			changed.getStrings().push_back(L"\xD55C\xAE00");
			changed.markStringsChanged();
		}
		ByteBuffer grown;
		if (!changed.serialize(grown, errorMessage)) {
			// 매우 큰 오브젝트에서 i16 오프셋이 넘치는 경우는 정상적인 거부이다
			if ((errorMessage.find("exceeds 32767") == string::npos) && (errorMessage.find("too large") == string::npos) && (errorMessage.find("overflow") == string::npos)) {
				fail("mutated serialize: " + errorMessage);
			}
			continue;
		}
		ObjectCommon reparsed;
		if (!reparsed.parse(grown, (ObjectCommonLayout)layoutId, (UINT32)build, errorMessage)) {
			fail("mutated reparse: " + errorMessage);
			continue;
		}
		mutated++;
		if (animationEdited) {
			animationEdits++;
		}
		bool ok = true;
		if (reparsed.hasValues() != common.hasValues() || (common.hasValues() && (reparsed.getValues().size() != common.getValues().size() + 1))) { ok = false; }
		if (reparsed.hasStrings() != common.hasStrings() || (common.hasStrings() && ((reparsed.getStrings().size() != common.getStrings().size() + 1) || (reparsed.getStrings().back() != L"\xD55C\xAE00")))) { ok = false; }
		if (reparsed.hasAnimations() != common.hasAnimations()) { ok = false; }
		if ((reparsed.getMovementBlock() != common.getMovementBlock()) || (reparsed.getExtensionBlock() != common.getExtensionBlock())
			|| (reparsed.getCounterBlock() != common.getCounterBlock()) || (reparsed.getSystemBlock() != common.getSystemBlock())) { ok = false; }
		if (!ok) {
			fail("mutated object lost data (type " + to_string((long long)type) + ")");
		}
	}
	printf("  objects: %u blobs, parse failures %u, untouched-exact %u, canonical-exact %u, anim/val/str/text views %u/%u/%u/%u, mutation round trips %u (anim edits %u)\n",
		(unsigned)total, (unsigned)parseFailures, (unsigned)untouchedOk, (unsigned)canonicalOk, (unsigned)withAnimations, (unsigned)withValues, (unsigned)withStrings, (unsigned)withText, (unsigned)mutated, (unsigned)animationEdits);
	failureCount += (int)parseFailures;
}


// ---------------------------------------------------------------- 오브젝트 텍스트

static void testObjectText(const string& gameDir)
{
	ByteBuffer all;
	if (!readFile(gameDir + "\\objects.bin", all)) {
		return;
	}

	// 1) 모든 오브젝트의 텍스트를 표에 모은다
	struct Blob { BYTE layout; INT32 build; ByteBuffer data; };
	vector<Blob> blobs;
	ByteBufferReader reader(all);
	while (reader.remaining() > 0) {
		Blob blob;
		blob.layout = reader.readU8();
		blob.build = reader.readI32();
		reader.readI16();	// 타입
		UINT32 length = reader.readU32();
		blob.data = reader.readBytes(length);
		blobs.push_back(blob);
	}
	ObjectTextTable table;
	size_t stringCount = 0, paragraphCount = 0;
	for (size_t i = 0; i < blobs.size(); i++) {
		ObjectCommon common;
		string errorMessage;
		if (!common.parse(blobs[i].data, (ObjectCommonLayout)blobs[i].layout, (UINT32)blobs[i].build, errorMessage)) {
			continue;
		}
		table.addObject((INT32)i, "object", common);
	}
	for (size_t i = 0; i < table.getEntries().size(); i++) {
		stringCount += table.getEntries()[i].strings.size();
		paragraphCount += table.getEntries()[i].paragraphs.size();
	}

	// 2) JSON 으로 내보냈다가 다시 읽는다. 번역 없는 표와, 모든 항목에 번역을 단 표를 만든다
	Json::Value untranslatedJson;
	table.exportJson(untranslatedJson);
	Json::Value translatedJson = untranslatedJson;
	for (Json::ArrayIndex o = 0; o < translatedJson["objects"].size(); o++) {
		Json::Value& object = translatedJson["objects"][o];
		for (Json::ArrayIndex s = 0; s < object["strings"].size(); s++) {
			object["strings"][s]["translation"] = object["strings"][s]["original"].asString() + "\xED\x95\x9C";
		}
		for (Json::ArrayIndex p = 0; p < object["paragraphs"].size(); p++) {
			object["paragraphs"][p]["translation"] = object["paragraphs"][p]["original"].asString() + "\xED\x95\x9C";
		}
	}
	ObjectTextTable emptyTable, translatedTable;
	if (!emptyTable.importJson(untranslatedJson) || !translatedTable.importJson(translatedJson)) {
		fail("object text table json import");
		return;
	}
	if ((emptyTable.getEntryCount() != table.getEntryCount()) || (translatedTable.getEntryCount() != table.getEntryCount())) {
		fail("object text table entry count changed by the json round trip");
	}

	// 3) 번역이 없으면 바이트가 그대로여야 하고, 모두 번역하면 모든 문자열이 바뀌고 다른 하위 블록은 그대로여야 한다
	size_t replacedTotal = 0, expectedTotal = stringCount + paragraphCount, skippedTotal = 0;
	for (size_t i = 0; i < blobs.size(); i++) {
		const ObjectTextEntry* translatedEntry = translatedTable.findEntry((INT32)i);
		const ObjectTextEntry* emptyEntry = emptyTable.findEntry((INT32)i);
		if (translatedEntry == NULL) {
			continue;
		}
		ObjectCommon common;
		string errorMessage;
		common.parse(blobs[i].data, (ObjectCommonLayout)blobs[i].layout, (UINT32)blobs[i].build, errorMessage);

		ObjectCommon untouched = common;
		UINT32 replaced = 0, skipped = 0;
		ByteBuffer untouchedData;
		if (!replaceObjectText(untouched, *emptyEntry, replaced, skipped) || (replaced != 0) || !untouched.serialize(untouchedData, errorMessage) || (untouchedData != blobs[i].data)) {
			fail("empty object text table changed an object");
			continue;
		}

		replaceObjectText(common, *translatedEntry, replaced, skipped);
		replacedTotal += replaced;
		skippedTotal += skipped;
		ByteBuffer translatedData;
		if (!common.serialize(translatedData, errorMessage)) {
			// i16 오프셋이 넘치는 큰 오브젝트는 정상적인 거부이다
			if ((errorMessage.find("exceeds 32767") == string::npos) && (errorMessage.find("too large") == string::npos) && (errorMessage.find("overflow") == string::npos)) {
				fail("object text serialize: " + errorMessage);
			}
			continue;
		}
		ObjectCommon reparsed;
		if (!reparsed.parse(translatedData, (ObjectCommonLayout)blobs[i].layout, (UINT32)blobs[i].build, errorMessage)) {
			fail("object text reparse: " + errorMessage);
			continue;
		}
		bool ok = true;
		for (size_t s = 0; s < translatedEntry->strings.size(); s++) {
			const ObjectTextString& item = translatedEntry->strings[s];
			if (!reparsed.hasStrings() || (reparsed.getStrings()[item.index] != item.translation)) { ok = false; }
		}
		for (size_t p = 0; p < translatedEntry->paragraphs.size(); p++) {
			const ObjectTextParagraphEntry& item = translatedEntry->paragraphs[p];
			if (!reparsed.hasText() || (reparsed.getText().paragraphs[item.index].text != item.translation)) { ok = false; }
		}
		if ((reparsed.getMovementBlock() != common.getMovementBlock()) || (reparsed.getExtensionBlock() != common.getExtensionBlock())
			|| (reparsed.getCounterBlock() != common.getCounterBlock())) { ok = false; }
		if (!ok) {
			fail("object text lost data (object " + to_string((long long)i) + ")");
		}
	}
	if (skippedTotal != 0) {
		fail("original text mismatch while applying the object text table");
	}
	printf("  objtext: %u objects with text, %u strings + %u paragraphs, translated %u/%u, empty table keeps bytes\n",
		(unsigned)table.getEntryCount(), (unsigned)stringCount, (unsigned)paragraphCount, (unsigned)replacedTotal, (unsigned)expectedTotal);
}


// ---------------------------------------------------------------- 2.5+ 요소 컨테이너

static void testContainer(const string& gameDir)
{
	ByteBuffer original2256, original2257;
	if (!readFile(gameDir + "\\c2256.bin", original2256) || !readFile(gameDir + "\\c2257.bin", original2257)) {
		return;
	}
	ObjectElementContainer container;
	string errorMessage;
	if (!container.parse(original2256, errorMessage)) {
		fail("container parse: " + errorMessage);
		return;
	}
	ByteBuffer rebuilt2256, rebuilt2257;
	if (!container.build(rebuilt2256, rebuilt2257, errorMessage)) {
		fail("container build: " + errorMessage);
		return;
	}
	bool same2256 = (rebuilt2256 == original2256);
	bool same2257 = (rebuilt2257 == original2257);
	if (!same2256) { fail("unchanged 0x2256 not byte-identical"); }
	if (!same2257) { fail("regenerated 0x2257 offsets differ from the original"); }

	// 전부 다시 압축한 뒤에도 압축을 푼 내용이 유지되는지 확인한다
	ObjectElementContainer changed = container;
	size_t recompressMatches = 0;
	vector<ObjectContainerElement>& elements = changed.getElements();
	for (size_t i = 0; i < elements.size(); i++) {
		if (ObjectElementContainer::isRecompressIdentical(elements[i].data, elements[i].originalCompressed)) {
			recompressMatches++;
		}
		changed.setElementData(i, elements[i].data);
	}
	ByteBuffer changed2256, changed2257;
	if (!changed.build(changed2256, changed2257, errorMessage)) {
		fail("changed container build: " + errorMessage);
		return;
	}
	ObjectElementContainer again;
	if (!again.parse(changed2256, errorMessage)) {
		fail("changed container reparse: " + errorMessage);
		return;
	}
	bool dataOk = (again.getElements().size() == container.getElements().size());
	for (size_t i = 0; dataOk && (i < again.getElements().size()); i++) {
		if (again.getElements()[i].data != container.getElements()[i].data) {
			dataOk = false;
		}
	}
	if (!dataOk) {
		fail("recompressed container content differs");
	}
	printf("  container: %u elements (0x2257 entries %u), unchanged 0x2256 exact %s, offsets exact %s, recompress identical to original %u, content after recompress %s\n",
		(unsigned)container.getElements().size(), (unsigned)(original2257.size() / 4), same2256 ? "yes" : "NO", same2257 ? "yes" : "NO", (unsigned)recompressMatches, dataOk ? "ok" : "BAD");
}


int main(int argc, char** argv)
{
	if (argc < 2) {
		printf("usage: ChunkModelTest <testdata root> [game name]\n");
		return 2;
	}
	setvbuf(stdout, NULL, _IONBF, 0);
	string root = argv[1];
	vector<string> games = listDirs(root);
	for (size_t i = 0; i < games.size(); i++) {
		if ((argc >= 3) && (games[i] != argv[2])) {
			continue;
		}
		string dir = root + "\\" + games[i];
		ByteBuffer meta;
		if (!readFile(dir + "\\meta.txt", meta)) {
			continue;
		}
		string metaText((const char*)&meta[0], meta.size());
		UINT32 build = 0;
		size_t at = metaText.find("build=");
		if (at != string::npos) {
			build = (UINT32)atoi(metaText.c_str() + at + 6);
		}
		printf("== %s (build %u)\n", games[i].c_str(), build);
		testEvents(dir, build);
		testEventText(dir, build);
		testObjects(dir);
		testObjectText(dir);
		testContainer(dir);
	}
	printf("\n%s (%d failures)\n", failureCount ? "FAILED" : "ALL PASS", failureCount);
	return failureCount ? 1 : 0;
}
