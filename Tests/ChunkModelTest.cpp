// Headless self-test for the lossless chunk models (frame events, ObjectCommon, 2.5+ element container).
//
// usage: ChunkModelTest.exe <testdata root> [game name]
// The testdata root holds one folder per game, produced by a dump script:
//   <game>/meta.txt               build=<n> layout=<tfp|n284|old>
//   <game>/events/f<N>.evt        decoded 0x333D payloads
//   <game>/objects.bin            repeated [u8 layout][i32 build][i16 type][u32 length][blob]
//   <game>/c2256.bin, c2257.bin   decoded 2.5+ element container (optional)

#include "../ToolForClickteamFusion/Chunk/Frame/FrameEvent/FrameEventModel.h"
#include "../ToolForClickteamFusion/Chunk/Object/ObjectCommon.h"
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


// ---------------------------------------------------------------- frame events

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

// append a marker to every string token so that every group changes size and every pointer has to move
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

		// statistics + expression round trip on every expression param
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

		// relocation: grow every string, write, parse again, the pointers must still hit the same groups
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


// ---------------------------------------------------------------- object common

static void testObjects(const string& gameDir)
{
	ByteBuffer all;
	if (!readFile(gameDir + "\\objects.bin", all)) {
		printf("  objects: (no objects.bin)\n");
		return;
	}
	ByteBufferReader reader(all);
	size_t total = 0, parseFailures = 0, canonicalOk = 0, untouchedOk = 0, withAnimations = 0, withValues = 0, withStrings = 0, mutated = 0, animationEdits = 0;
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

		// mutation: grow every view, rebuild, parse again
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
			// an i16 offset overflow on very large objects is a legitimate refusal
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
	printf("  objects: %u blobs, parse failures %u, untouched-exact %u, canonical-exact %u, anim/val/str views %u/%u/%u, mutation round trips %u (anim edits %u)\n",
		(unsigned)total, (unsigned)parseFailures, (unsigned)untouchedOk, (unsigned)canonicalOk, (unsigned)withAnimations, (unsigned)withValues, (unsigned)withStrings, (unsigned)mutated, (unsigned)animationEdits);
	failureCount += (int)parseFailures;
}


// ---------------------------------------------------------------- 2.5+ element container

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

	// recompress everything and check that the decompressed content survives
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
		testObjects(dir);
		testContainer(dir);
	}
	printf("\n%s (%d failures)\n", failureCount ? "FAILED" : "ALL PASS", failureCount);
	return failureCount ? 1 : 0;
}
