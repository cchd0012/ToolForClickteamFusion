#include "ObjectScript.h"
#include "../../Util/CommonUtil.h"

using namespace CTT::Chunk;
using namespace CTT::CommonUtil;


static const char* objectScriptJsonType = "objectScript";
static const UINT32 objectScriptJsonVersion = 1;


// ------------------------------------------------------------------ 비교 보조 함수

static bool sameAnimations(const ObjectAnimations& a, const ObjectAnimations& b)
{
	if (a.slots.size() != b.slots.size()) {
		return false;
	}
	for (size_t i = 0; i < a.slots.size(); i++) {
		if (a.slots[i].used != b.slots[i].used) {
			return false;
		}
		const map<INT32, ObjectAnimationDirection>& da = a.slots[i].directions;
		const map<INT32, ObjectAnimationDirection>& db = b.slots[i].directions;
		if (da.size() != db.size()) {
			return false;
		}
		map<INT32, ObjectAnimationDirection>::const_iterator ia = da.begin();
		map<INT32, ObjectAnimationDirection>::const_iterator ib = db.begin();
		for (; ia != da.end(); ++ia, ++ib) {
			if ((ia->first != ib->first) || (ia->second.minSpeed != ib->second.minSpeed) || (ia->second.maxSpeed != ib->second.maxSpeed)
				|| (ia->second.repeat != ib->second.repeat) || (ia->second.backTo != ib->second.backTo) || (ia->second.frames != ib->second.frames)) {
				return false;
			}
		}
	}
	return true;
}


static bool sameText(const ObjectText& a, const ObjectText& b)
{
	if ((a.width != b.width) || (a.height != b.height) || (a.paragraphs.size() != b.paragraphs.size())) {
		return false;
	}
	for (size_t i = 0; i < a.paragraphs.size(); i++) {
		if ((a.paragraphs[i].font != b.paragraphs[i].font) || (a.paragraphs[i].flags != b.paragraphs[i].flags)
			|| (memcmp(a.paragraphs[i].color, b.paragraphs[i].color, 4) != 0) || (a.paragraphs[i].text != b.paragraphs[i].text)) {
			return false;
		}
	}
	return true;
}


// ---- Text Blitter(txtblt.mfx, 식별자 TLBT) 편집 데이터 필드
// 확장 데이터 블록의 맨 앞은 5개 dword 헤더(0x14바이트)이고, 필드 위치는 txtblt.mfx 의 CreateRunObject 가 읽는 편집 데이터(edPtr) 기준이다
static const size_t kTextBlitterCellWidth = 0x5a4;
static const size_t kTextBlitterCellHeight = 0x5a8;
static const size_t kTextBlitterImageWidth = 0x5b8;
static const size_t kTextBlitterImageHeight = 0x5bc;
static const size_t kTextBlitterImageHandle = 0x5d0;			// u16. 실행 중에는 런타임이 다른 값으로 바꿔서 쓴다
static const size_t kTextBlitterFlags = 0x5c8;					// u32 옵션 플래그. 알려진 비트는 아래 kTextBlitterFlagNames 에 있다
static const size_t kTextBlitterTabWidth = 0x5cc;				// i32 탭 문자가 차지하는 칸 수 (읽기 전용으로 내보낸다)
static const size_t kTextBlitterAlign = 0x1be4;				// i32 줄 정렬 (읽기 전용으로 내보낸다)
// 옵션 플래그 이름 (txtblt.mfx 의 그리기/줄나누기 코드에서 확인한 비트만. 나머지는 "0x4000" 같은 16진수로 표시한다)
struct TextBlitterFlagName { UINT32 bit; const char* name; };
static const TextBlitterFlagName kTextBlitterFlagNames[] = {
	{ 0x00000002, "ESCAPE" },		// 백슬래시 이스케이프 처리 (탭 칸이 0보다 크면 탭 문자도 처리)
	{ 0x00000020, "WRAP" },			// 자동 줄바꿈
	{ 0x00000040, "CRLF" },			// CR/LF 를 줄바꿈으로 처리 (글자로 그리지 않는다)
	{ 0x00000800, "CHAR_EVENT" },	// 글자 하나를 그릴 때마다 이벤트를 보낸다 (글자별 위치/모양을 이벤트가 바꿀 수 있다)
	{ 0x00020000, "BOX" }			// 상자 안에 맞춰 그린다
};
// 글자표 번호: 빈 꼬리 영역에 둔다 (원본 오브젝트는 이 영역이 전부 0이고 런타임도 건드리지 않는다)
static const size_t kTextBlitterFontTableMagic = 0x2030;		// u32 'KFNT' (4B 46 4E 54)
static const size_t kTextBlitterFontTable = 0x2034;				// u32 글자표 번호 (1 이상)
static const size_t kTextBlitterLayoutFlags = 0x2038;			// u32 레이아웃 플래그: bit0 = 줄 폭을 글자 폭의 합으로 계산 (블록 크기 0x203C 이상)
static const size_t kTextBlitterMinBlock = 0x2038;
static const size_t kTextBlitterFlagsBlock = 0x203C;
static const UINT32 kTextBlitterFontTableMagicValue = 0x544E464B;
static const size_t kTextBlitterText = 0xa0;					// 오브젝트를 만들 때 처음 들어가는 문자열 (널 종료, ANSI). 다음 필드(0x5a0) 앞까지
static const size_t kTextBlitterTextEnd = 0x5a0;
static const int kTextBlitterTextCodePage = 949;

static bool isTextBlitter(ObjectCommon& common)
{
	ByteBuffer identifier = common.getIdentifier();
	return (identifier.size() == 4) && (memcmp(&identifier[0], "TLBT", 4) == 0);
}

static INT32 readBlockI32(const ByteBuffer& block, size_t offset)
{
	INT32 v;
	memcpy(&v, &block[offset], 4);
	return v;
}

static void writeBlockI32(ByteBuffer& block, size_t offset, INT32 v)
{
	memcpy(&block[offset], &v, 4);
}

// 편집 데이터의 처음 문자열(CP949)을 UTF-8 로 읽는다
static string readBlitterText(const ByteBuffer& block)
{
	size_t length = 0;
	while ((kTextBlitterText + length < kTextBlitterTextEnd) && (block[kTextBlitterText + length] != 0)) {
		length++;
	}
	if (length == 0) {
		return string();
	}
	int wideLength = MultiByteToWideChar(kTextBlitterTextCodePage, 0, (const char*)&block[kTextBlitterText], (int)length, NULL, 0);
	wstring wide(wideLength, L'\0');
	MultiByteToWideChar(kTextBlitterTextCodePage, 0, (const char*)&block[kTextBlitterText], (int)length, &wide[0], wideLength);
	return getAnsiStr(wide, CodePage::CP_UTF_8);
}

// UTF-8 문자열을 CP949 로 바꿔 편집 데이터에 넣는다. 표현할 수 없는 글자나 너무 긴 문자열이면 실패
static bool writeBlitterText(ByteBuffer& block, const string& utf8)
{
	wstring wide = getUniStr(utf8, CodePage::CP_UTF_8);
	string bytes;
	if (!wide.empty()) {
		BOOL usedDefault = FALSE;
		int length = WideCharToMultiByte(kTextBlitterTextCodePage, WC_NO_BEST_FIT_CHARS, wide.c_str(), (int)wide.size(), NULL, 0, NULL, &usedDefault);
		bytes.assign(length, '\0');
		WideCharToMultiByte(kTextBlitterTextCodePage, WC_NO_BEST_FIT_CHARS, wide.c_str(), (int)wide.size(), &bytes[0], length, NULL, &usedDefault);
		if (usedDefault) {
			return false;
		}
	}
	if (kTextBlitterText + bytes.size() >= kTextBlitterTextEnd) {
		return false;
	}
	memset(&block[kTextBlitterText], 0, kTextBlitterTextEnd - kTextBlitterText);
	memcpy(&block[kTextBlitterText], bytes.data(), bytes.size());
	return true;
}


// JSON 정수를 범위를 확인하며 읽는다
static bool readInt(const Json::Value& value, INT64 minValue, INT64 maxValue, INT64& out)
{
	if (!value.isIntegral()) {
		return false;
	}
	out = value.asInt64();
	return (out >= minValue) && (out <= maxValue);
}




// ------------------------------------------------------------------ 디컴파일

bool CTT::Chunk::decompileObjectScript(INT32 index, const string& name, INT16 objectType, ObjectCommon& common, Json::Value& value)
{
	value = Json::Value(Json::objectValue);
	value["type"] = objectScriptJsonType;
	value["version"] = objectScriptJsonVersion;
	value["index"] = index;
	value["name"] = name;
	value["objectType"] = objectType;
	ByteBuffer identifier = common.getIdentifier();
	// 식별자는 확장 오브젝트에서만 'TLBT' 같은 글자이고, 그 밖에는 의미 없는 바이트일 수 있다. 글자가 아니면 16진수로 적는다 (JSON 문자열은 UTF-8 이어야 한다)
	bool identifierPrintable = true;
	for (size_t i = 0; i < identifier.size(); i++) {
		if (identifier[i] < 0x20 || identifier[i] > 0x7E) {
			identifierPrintable = false;
		}
	}
	if (identifierPrintable) {
		value["identifier"] = string(identifier.begin(), identifier.end());
	}
	else {
		string hexText = "0x";
		const char* digits = "0123456789ABCDEF";
		for (size_t i = 0; i < identifier.size(); i++) {
			hexText += digits[identifier[i] >> 4];
			hexText += digits[identifier[i] & 15];
		}
		value["identifier"] = hexText;
	}

	if (common.hasAnimations()) {
		ObjectAnimations& animations = common.getAnimations();
		value["animationSlotCount"] = (Json::UInt)animations.slots.size();
		Json::Value slots(Json::arrayValue);
		for (size_t s = 0; s < animations.slots.size(); s++) {
			if (!animations.slots[s].used) {
				continue;
			}
			Json::Value slot(Json::objectValue);
			slot["slot"] = (Json::UInt)s;
			Json::Value directions(Json::arrayValue);
			for (map<INT32, ObjectAnimationDirection>::const_iterator it = animations.slots[s].directions.begin(); it != animations.slots[s].directions.end(); ++it) {
				Json::Value direction(Json::objectValue);
				direction["direction"] = it->first;
				direction["minSpeed"] = (int)it->second.minSpeed;
				direction["maxSpeed"] = (int)it->second.maxSpeed;
				direction["repeat"] = (int)it->second.repeat;
				direction["backTo"] = (int)it->second.backTo;
				Json::Value frames(Json::arrayValue);
				for (size_t f = 0; f < it->second.frames.size(); f++) {
					frames.append((int)it->second.frames[f]);
				}
				direction["frames"] = frames;
				directions.append(direction);
			}
			slot["directions"] = directions;
			slots.append(slot);
		}
		value["animations"] = slots;
	}

	if (common.hasValues()) {
		Json::Value values(Json::arrayValue);
		for (size_t i = 0; i < common.getValues().size(); i++) {
			values.append((int)common.getValues()[i]);
		}
		value["alterableValues"] = values;
	}

	if (common.hasStrings()) {
		Json::Value strings(Json::arrayValue);
		for (size_t i = 0; i < common.getStrings().size(); i++) {
			strings.append(getAnsiStr(common.getStrings()[i], CodePage::CP_UTF_8));
		}
		value["alterableStrings"] = strings;
	}

	if (common.hasText()) {
		ObjectText& text = common.getText();
		Json::Value textValue(Json::objectValue);
		textValue["width"] = text.width;
		textValue["height"] = text.height;
		Json::Value paragraphs(Json::arrayValue);
		for (size_t i = 0; i < text.paragraphs.size(); i++) {
			Json::Value paragraph(Json::objectValue);
			paragraph["font"] = text.paragraphs[i].font;
			paragraph["flags"] = text.paragraphs[i].flags;
			Json::Value color(Json::arrayValue);
			for (int c = 0; c < 4; c++) {
				color.append((int)text.paragraphs[i].color[c]);
			}
			paragraph["color"] = color;
			paragraph["text"] = getAnsiStr(text.paragraphs[i].text, CodePage::CP_UTF_8);
			paragraphs.append(paragraph);
		}
		textValue["paragraphs"] = paragraphs;
		value["text"] = textValue;
	}
	// Text Blitter 의 편집 데이터 필드 (셀/이미지 크기, 이미지 핸들, 글자표 번호)
	if (isTextBlitter(common) && common.hasExtensionBlock()) {
		ByteBuffer block = common.getExtensionBlock();
		if (block.size() >= kTextBlitterMinBlock) {
			Json::Value blitter(Json::objectValue);
			blitter["cellWidth"] = readBlockI32(block, kTextBlitterCellWidth);
			blitter["cellHeight"] = readBlockI32(block, kTextBlitterCellHeight);
			blitter["imageWidth"] = readBlockI32(block, kTextBlitterImageWidth);
			blitter["imageHeight"] = readBlockI32(block, kTextBlitterImageHeight);
			blitter["imageHandle"] = (int)(block[kTextBlitterImageHandle] | (block[kTextBlitterImageHandle + 1] << 8));
			bool hasFontTable = ((UINT32)readBlockI32(block, kTextBlitterFontTableMagic) == kTextBlitterFontTableMagicValue);
			blitter["fontTable"] = hasFontTable ? readBlockI32(block, kTextBlitterFontTable) : 0;
			bool pixelLayout = hasFontTable && (block.size() >= kTextBlitterFlagsBlock) && ((readBlockI32(block, kTextBlitterLayoutFlags) & 1) != 0);
			blitter["layout"] = pixelLayout ? "pixel" : "byte";
			if (block.size() >= kTextBlitterFlags + 4) {
				const UINT32 flagsValue = (UINT32)readBlockI32(block, kTextBlitterFlags);
				blitter["flags"] = (Json::UInt)flagsValue;
				Json::Value names(Json::arrayValue);
				UINT32 rest = flagsValue;
				for (size_t f = 0; f < sizeof(kTextBlitterFlagNames) / sizeof(kTextBlitterFlagNames[0]); f++) {
					if ((flagsValue & kTextBlitterFlagNames[f].bit) != 0) {
						names.append(kTextBlitterFlagNames[f].name);
						rest &= ~kTextBlitterFlagNames[f].bit;
					}
				}
				for (int bit = 0; bit < 32; bit++) {
					if ((rest & (1u << bit)) != 0) {
						char hex[16];
						sprintf_s(hex, "0x%X", 1u << bit);
						names.append(hex);
					}
				}
				blitter["flagNames"] = names;		// 읽기 전용 설명. 고칠 때는 flags(숫자)를 고친다
			}
			if (block.size() >= kTextBlitterAlign + 4) {
				blitter["tabWidth"] = readBlockI32(block, kTextBlitterTabWidth);
				blitter["align"] = readBlockI32(block, kTextBlitterAlign);		// 읽기 전용
			}
			if (block.size() >= kTextBlitterTextEnd) {
				blitter["text"] = readBlitterText(block);
			}
			value["textBlitter"] = blitter;
		}
	}
	return true;
}




// ------------------------------------------------------------------ 리컴파일

bool CTT::Chunk::recompileObjectScript(ObjectCommon& common, const Json::Value& value, bool& changed, string& errorMessage)
{
	changed = false;
	if (!value.isObject() || (value["type"].asString() != objectScriptJsonType)) {
		errorMessage = "not an objectScript json";
		return false;
	}

	// ---- 애니메이션 ----
	if (value.isMember("animations") && common.hasAnimations()) {
		ObjectAnimations parsed;
		INT64 slotCount = 0;
		if (value.isMember("animationSlotCount") && !readInt(value["animationSlotCount"], 0, 32767, slotCount)) {
			errorMessage = "bad animationSlotCount";
			return false;
		}
		if (!value["animations"].isArray()) {
			errorMessage = "animations must be an array";
			return false;
		}
		const Json::Value& slots = value["animations"];
		// 슬롯 수는 지정한 값과 가장 큰 슬롯 번호 + 1 중 큰 값이다
		INT64 neededCount = slotCount;
		for (Json::ArrayIndex s = 0; s < slots.size(); s++) {
			INT64 slotIndex;
			if (!readInt(slots[s]["slot"], 0, 32766, slotIndex)) {
				errorMessage = "bad animation slot";
				return false;
			}
			if (slotIndex + 1 > neededCount) {
				neededCount = slotIndex + 1;
			}
		}
		parsed.slots.resize((size_t)neededCount);
		for (Json::ArrayIndex s = 0; s < slots.size(); s++) {
			INT64 slotIndex;
			readInt(slots[s]["slot"], 0, 32766, slotIndex);
			ObjectAnimation& animation = parsed.slots[(size_t)slotIndex];
			animation.used = true;
			const Json::Value& directions = slots[s]["directions"];
			if (!directions.isArray()) {
				errorMessage = "animation directions must be an array";
				return false;
			}
			for (Json::ArrayIndex d = 0; d < directions.size(); d++) {
				const Json::Value& item = directions[d];
				INT64 directionIndex, minSpeed, maxSpeed, repeat, backTo;
				if (!readInt(item["direction"], 0, 31, directionIndex) || !readInt(item["minSpeed"], -128, 127, minSpeed)
					|| !readInt(item["maxSpeed"], -128, 127, maxSpeed) || !readInt(item["repeat"], -32768, 32767, repeat)
					|| !readInt(item["backTo"], -32768, 32767, backTo) || !item["frames"].isArray()) {
					errorMessage = "bad animation direction";
					return false;
				}
				ObjectAnimationDirection direction;
				direction.minSpeed = (INT8)minSpeed;
				direction.maxSpeed = (INT8)maxSpeed;
				direction.repeat = (INT16)repeat;
				direction.backTo = (INT16)backTo;
				for (Json::ArrayIndex f = 0; f < item["frames"].size(); f++) {
					INT64 handle;
					if (!readInt(item["frames"][f], -32768, 32767, handle)) {
						errorMessage = "bad animation frame handle";
						return false;
					}
					direction.frames.push_back((INT16)handle);
				}
				if (animation.directions.find((INT32)directionIndex) != animation.directions.end()) {
					errorMessage = "duplicate animation direction";
					return false;
				}
				animation.directions[(INT32)directionIndex] = direction;
			}
		}
		if (!sameAnimations(common.getAnimations(), parsed)) {
			common.getAnimations() = parsed;
			common.markAnimationsChanged();
			changed = true;
		}
	}

	// ---- Alterable 값 ----
	if (value.isMember("alterableValues") && common.hasValues()) {
		if (!value["alterableValues"].isArray()) {
			errorMessage = "alterableValues must be an array";
			return false;
		}
		vector<INT32> parsed;
		for (Json::ArrayIndex i = 0; i < value["alterableValues"].size(); i++) {
			INT64 number;
			if (!readInt(value["alterableValues"][i], -2147483647 - 1, 2147483647, number)) {
				errorMessage = "bad alterable value";
				return false;
			}
			parsed.push_back((INT32)number);
		}
		if (parsed != common.getValues()) {
			common.getValues() = parsed;
			common.markValuesChanged();
			changed = true;
		}
	}

	// ---- Alterable 문자열 ----
	if (value.isMember("alterableStrings") && common.hasStrings()) {
		if (!value["alterableStrings"].isArray()) {
			errorMessage = "alterableStrings must be an array";
			return false;
		}
		vector<wstring> parsed;
		for (Json::ArrayIndex i = 0; i < value["alterableStrings"].size(); i++) {
			if (!value["alterableStrings"][i].isString()) {
				errorMessage = "alterable string must be a string";
				return false;
			}
			parsed.push_back(getUniStr(value["alterableStrings"][i].asString(), CodePage::CP_UTF_8));
		}
		if (parsed != common.getStrings()) {
			common.getStrings() = parsed;
			common.markStringsChanged();
			changed = true;
		}
	}

	// ---- Text 문단 ----
	if (value.isMember("text") && common.hasText()) {
		const Json::Value& textValue = value["text"];
		ObjectText parsed;
		INT64 width, height;
		if (!readInt(textValue["width"], -2147483647 - 1, 2147483647, width) || !readInt(textValue["height"], -2147483647 - 1, 2147483647, height) || !textValue["paragraphs"].isArray()) {
			errorMessage = "bad text object";
			return false;
		}
		parsed.width = (INT32)width;
		parsed.height = (INT32)height;
		for (Json::ArrayIndex i = 0; i < textValue["paragraphs"].size(); i++) {
			const Json::Value& item = textValue["paragraphs"][i];
			INT64 font, flags;
			if (!readInt(item["font"], 0, 65535, font) || !readInt(item["flags"], 0, 65535, flags) || !item["color"].isArray() || (item["color"].size() != 4) || !item["text"].isString()) {
				errorMessage = "bad text paragraph";
				return false;
			}
			ObjectTextParagraph paragraph;
			paragraph.font = (UINT16)font;
			paragraph.flags = (UINT16)flags;
			for (int c = 0; c < 4; c++) {
				INT64 channel;
				if (!readInt(item["color"][c], 0, 255, channel)) {
					errorMessage = "bad text color";
					return false;
				}
				paragraph.color[c] = (BYTE)channel;
			}
			paragraph.text = getUniStr(item["text"].asString(), CodePage::CP_UTF_8);
			parsed.paragraphs.push_back(paragraph);
		}
		if (!sameText(common.getText(), parsed)) {
			common.getText() = parsed;
			common.markTextChanged();
			changed = true;
		}
	}
	// ---- Text Blitter 편집 데이터 필드 ----
	if (value.isMember("textBlitter") && isTextBlitter(common) && common.hasExtensionBlock()) {
		const Json::Value& blitter = value["textBlitter"];
		ByteBuffer block = common.getExtensionBlock();
		if (!blitter.isObject() || (block.size() < kTextBlitterMinBlock)) {
			errorMessage = "bad textBlitter";
			return false;
		}
		ByteBuffer updated = block;
		struct Field { const char* name; size_t offset; INT64 minValue; INT64 maxValue; };
		const Field fields[4] = {
			{ "cellWidth", kTextBlitterCellWidth, 1, 4096 },
			{ "cellHeight", kTextBlitterCellHeight, 1, 4096 },
			{ "imageWidth", kTextBlitterImageWidth, 1, 65535 },
			{ "imageHeight", kTextBlitterImageHeight, 1, 65535 }
		};
		for (int i = 0; i < 4; i++) {
			if (blitter.isMember(fields[i].name)) {
				INT64 v;
				if (!readInt(blitter[fields[i].name], fields[i].minValue, fields[i].maxValue, v)) {
					errorMessage = string("bad textBlitter.") + fields[i].name;
					return false;
				}
				writeBlockI32(updated, fields[i].offset, (INT32)v);
			}
		}
		if (blitter.isMember("imageHandle")) {
			INT64 v;
			if (!readInt(blitter["imageHandle"], 0, 65535, v)) {
				errorMessage = "bad textBlitter.imageHandle";
				return false;
			}
			updated[kTextBlitterImageHandle] = (BYTE)(v & 0xFF);
			updated[kTextBlitterImageHandle + 1] = (BYTE)((v >> 8) & 0xFF);
		}
		if (blitter.isMember("fontTable")) {
			INT64 v;
			if (!readInt(blitter["fontTable"], 0, 0x7FFFFFFF, v)) {
				errorMessage = "bad textBlitter.fontTable";
				return false;
			}
			writeBlockI32(updated, kTextBlitterFontTableMagic, (v > 0) ? (INT32)kTextBlitterFontTableMagicValue : 0);
			writeBlockI32(updated, kTextBlitterFontTable, (INT32)v);
		}
		if (blitter.isMember("flags")) {
			INT64 v;
			if (!readInt(blitter["flags"], 0, 0xFFFFFFFFLL, v) || (block.size() < kTextBlitterFlags + 4)) {
				errorMessage = "bad textBlitter.flags";
				return false;
			}
			writeBlockI32(updated, kTextBlitterFlags, (INT32)(UINT32)v);
		}
		if (blitter.isMember("layout")) {
			const string layout = blitter["layout"].isString() ? blitter["layout"].asString() : string();
			if ((layout != "byte" && layout != "pixel") || (block.size() < kTextBlitterFlagsBlock)) {
				errorMessage = "bad textBlitter.layout (byte or pixel)";
				return false;
			}
			INT32 flagsValue = readBlockI32(updated, kTextBlitterLayoutFlags);
			flagsValue = (layout == "pixel") ? (flagsValue | 1) : (flagsValue & ~1);
			writeBlockI32(updated, kTextBlitterLayoutFlags, flagsValue);
		}
		if (blitter.isMember("text")) {
			if (!blitter["text"].isString() || (block.size() < kTextBlitterTextEnd)) {
				errorMessage = "bad textBlitter.text";
				return false;
			}
			// 값이 그대로면 원래 바이트를 건드리지 않는다 (널 뒤의 찌꺼기까지 같게 유지)
			if (readBlitterText(updated) != blitter["text"].asString()) {
				if (!writeBlitterText(updated, blitter["text"].asString())) {
					errorMessage = "bad textBlitter.text (CP949 로 표현할 수 없거나 너무 깁니다)";
					return false;
				}
			}
		}
		if (updated != block) {
			common.setExtensionBlock(updated);
			changed = true;
		}
	}
	return true;
}
