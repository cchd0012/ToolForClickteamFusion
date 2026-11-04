#include "FrameEventScript.h"
#include "../../../Util/CommonUtil.h"

#include <set>

using namespace CTT::Chunk;
using namespace CTT::CommonUtil;


typedef Json::Int64 JsonInt;

static const char* frameEventsJsonType = "frameEvents";
static const UINT32 frameEventsJsonVersion = 1;


// ------------------------------------------------------------------ 보조 함수

static string toHex(const ByteBuffer& data)
{
	static const char* digits = "0123456789ABCDEF";
	string text;
	text.reserve(data.size() * 2);
	for (size_t i = 0; i < data.size(); i++) {
		text.push_back(digits[data[i] >> 4]);
		text.push_back(digits[data[i] & 15]);
	}
	return text;
}


static bool fromHex(const string& text, ByteBuffer& data)
{
	data.clear();
	if ((text.size() % 2) != 0) {
		return false;
	}
	for (size_t i = 0; i < text.size(); i += 2) {
		int value = 0;
		for (size_t k = 0; k < 2; k++) {
			char c = text[i + k];
			int digit;
			if ((c >= '0') && (c <= '9')) { digit = c - '0'; }
			else if ((c >= 'A') && (c <= 'F')) { digit = c - 'A' + 10; }
			else if ((c >= 'a') && (c <= 'f')) { digit = c - 'a' + 10; }
			else { return false; }
			value = (value << 4) | digit;
		}
		data.push_back((BYTE)value);
	}
	return true;
}


static string toUtf8(const wstring& text)
{
	return getAnsiStr(text, CodePage::CP_UTF_8);
}


static wstring fromUtf8(const string& text)
{
	return getUniStr(text, CodePage::CP_UTF_8);
}


// 필드가 없거나 범위를 벗어나면 예외를 던진다
static JsonInt getInt(const Json::Value& value, const char* key, JsonInt minValue, JsonInt maxValue)
{
	if (!value.isObject() || !value.isMember(key) || !value[key].isIntegral()) {
		throw ByteBufferError(string("missing or invalid integer field: ") + key);
	}
	JsonInt number = value[key].asInt64();
	if ((number < minValue) || (number > maxValue)) {
		throw ByteBufferError(string("integer field out of range: ") + key);
	}
	return number;
}


static JsonInt getOptionalInt(const Json::Value& value, const char* key, JsonInt minValue, JsonInt maxValue, JsonInt defaultValue)
{
	if (!value.isObject() || !value.isMember(key)) {
		return defaultValue;
	}
	return getInt(value, key, minValue, maxValue);
}


static string getString(const Json::Value& value, const char* key)
{
	if (!value.isObject() || !value.isMember(key) || !value[key].isString()) {
		throw ByteBufferError(string("missing or invalid string field: ") + key);
	}
	return value[key].asString();
}


static ByteBuffer getHex(const Json::Value& value, const char* key)
{
	ByteBuffer data;
	if (!fromHex(getString(value, key), data)) {
		throw ByteBufferError(string("invalid hex field: ") + key);
	}
	return data;
}


static const Json::Value& getArray(const Json::Value& value, const char* key)
{
	if (!value.isObject() || !value.isMember(key) || !value[key].isArray()) {
		throw ByteBufferError(string("missing or invalid array field: ") + key);
	}
	return value[key];
}


// 오브젝트 참조(objectInfo)에 붙일 이름
static string objectNameOf(INT16 objectType, UINT16 objectInfo, const FrameEventScriptContext& context)
{
	if (objectInfo >= 0x8000) {
		ostringstream stream;
		stream << "qualifier#" << (objectInfo & 0x7FFF);
		return stream.str();
	}
	if (((objectType >= 2) || (objectType == -7)) && (objectInfo < context.objectNames.size())) {
		return context.objectNames[objectInfo];
	}
	return string();
}




// ------------------------------------------------------------------ 위치 구조

static Json::Value readPosition(ByteBufferReader& reader)
{
	Json::Value position(Json::objectValue);
	position["parent"] = (JsonInt)reader.readU16();
	position["flags"] = (JsonInt)reader.readI16();
	position["x"] = (JsonInt)reader.readI16();
	position["y"] = (JsonInt)reader.readI16();
	position["slope"] = (JsonInt)reader.readI16();
	position["angle"] = (JsonInt)reader.readI16();
	position["direction"] = (JsonInt)reader.readI32();
	position["typeParent"] = (JsonInt)reader.readI16();
	position["list"] = (JsonInt)reader.readI16();
	position["layer"] = (JsonInt)reader.readI16();
	return position;
}


static void writePosition(const Json::Value& position, ByteBufferWriter& writer)
{
	writer.writeU16((UINT16)getInt(position, "parent", 0, 65535));
	writer.writeI16((INT16)getInt(position, "flags", -32768, 32767));
	writer.writeI16((INT16)getInt(position, "x", -32768, 32767));
	writer.writeI16((INT16)getInt(position, "y", -32768, 32767));
	writer.writeI16((INT16)getInt(position, "slope", -32768, 32767));
	writer.writeI16((INT16)getInt(position, "angle", -32768, 32767));
	writer.writeI32((INT32)getInt(position, "direction", -2147483647 - 1, 2147483647));
	writer.writeI16((INT16)getInt(position, "typeParent", -32768, 32767));
	writer.writeI16((INT16)getInt(position, "list", -32768, 32767));
	writer.writeI16((INT16)getInt(position, "layer", -32768, 32767));
}




// ------------------------------------------------------------------ 수식 토큰

static string operatorText(INT16 num)
{
	switch (num) {
		case 2: return "+";
		case 4: return "-";
		case 6: return "*";
		case 8: return "/";
		case 10: return " mod ";
	}
	ostringstream stream;
	stream << "op#" << num;
	return stream.str();
}


// 수식 토큰 하나를 JSON 으로 풀어낸다. 모르는 구조는 hex 로 보존한다
static Json::Value decompileToken(const FrameEventExpressionToken& token, const FrameEventScriptContext& context, string& text)
{
	Json::Value value(Json::objectValue);
	value["objectType"] = (JsonInt)token.objectType;
	value["num"] = (JsonInt)token.num;
	const ByteBuffer& body = token.body;
	bool decoded = false;
	try {
		ByteBufferReader reader(body);
		if (token.objectType == -1) {
			if ((token.num == 0) && (body.size() == 4)) {
				INT32 number = reader.readI32();
				value["kind"] = "long";
				value["value"] = (JsonInt)number;
				ostringstream stream;
				stream << number;
				text = stream.str();
				decoded = true;
			}
			else if (token.num == 3) {
				wstring content;
				if (getFrameEventExpressionString(token, content) && (body.size() == ((content.size() + 1) * 2))) {
					value["kind"] = "string";
					value["value"] = toUtf8(content);
					text = "\"" + toUtf8(content) + "\"";
					decoded = true;
				}
			}
			else if ((token.num == 23) && (body.size() == 12)) {
				double number;
				memcpy(&number, &body[0], sizeof(double));
				value["kind"] = "double";
				value["value"] = number;	// 읽기용, 다시 적용할 때는 hex 를 쓴다
				value["hex"] = toHex(body);
				text = "double";
				decoded = true;
			}
			else if (((token.num == 24) || (token.num == 50)) && (body.size() == 6)) {
				INT32 aux = reader.readI32();
				INT16 index = reader.readI16();
				value["kind"] = (token.num == 24) ? "globalValue" : "globalString";
				value["aux"] = (JsonInt)aux;
				value["index"] = (JsonInt)index;
				ostringstream stream;
				stream << ((token.num == 24) ? "global#" : "globalString#") << index;
				text = stream.str();
				decoded = true;
			}
		}
		else if ((token.objectType >= 2) || (token.objectType == -7)) {
			bool hasValue = ((token.num == 16) || (token.num == 19));
			if (body.size() == (hasValue ? 6u : 4u)) {
				UINT16 objectInfo = reader.readU16();
				INT16 objectList = reader.readI16();
				value["kind"] = "object";
				value["objectInfo"] = (JsonInt)objectInfo;
				value["objectList"] = (JsonInt)objectList;
				string name = objectNameOf(token.objectType, objectInfo, context);
				if (!name.empty()) {
					value["objectName"] = name;
				}
				ostringstream stream;
				stream << (name.empty() ? "object" : name) << ".exp#" << token.num;
				if (hasValue) {
					INT16 extra = reader.readI16();
					value["value"] = (JsonInt)extra;
					stream << "(" << extra << ")";
				}
				text = stream.str();
				decoded = true;
			}
		}
		else if (body.empty()) {
			// 연산자 등, 본문이 없는 토큰
			text = (token.objectType == 0) ? operatorText(token.num) : string();
			if (text.empty()) {
				ostringstream stream;
				stream << "sys#" << token.num;
				text = stream.str();
			}
			decoded = true;
		}
	}
	catch (const exception&) {
		decoded = false;
	}
	if (!decoded) {
		value = Json::Value(Json::objectValue);
		value["objectType"] = (JsonInt)token.objectType;
		value["num"] = (JsonInt)token.num;
		value["hex"] = toHex(body);
		ostringstream stream;
		stream << "token#" << token.objectType << ":" << token.num;
		text = stream.str();
	}
	return value;
}


static FrameEventExpressionToken recompileToken(const Json::Value& value)
{
	FrameEventExpressionToken token;
	token.objectType = (INT16)getInt(value, "objectType", -32768, 32767);
	token.num = (INT16)getInt(value, "num", -32768, 32767);
	string kind = value.isMember("kind") ? getString(value, "kind") : string();
	ByteBufferWriter writer;
	if (kind == "long") {
		writer.writeI32((INT32)getInt(value, "value", -2147483647 - 1, 2147483647));
	}
	else if (kind == "string") {
		writer.writeWString(fromUtf8(getString(value, "value")));
	}
	else if (kind == "double") {
		writer.writeBytes(getHex(value, "hex"));
	}
	else if ((kind == "globalValue") || (kind == "globalString")) {
		writer.writeI32((INT32)getInt(value, "aux", -2147483647 - 1, 2147483647));
		writer.writeI16((INT16)getInt(value, "index", -32768, 32767));
	}
	else if (kind == "object") {
		writer.writeU16((UINT16)getInt(value, "objectInfo", 0, 65535));
		writer.writeI16((INT16)getInt(value, "objectList", -32768, 32767));
		if ((token.num == 16) || (token.num == 19)) {
			writer.writeI16((INT16)getInt(value, "value", -32768, 32767));
		}
	}
	else if (value.isMember("hex")) {
		writer.writeBytes(getHex(value, "hex"));
	}
	token.body = writer.getBuffer();
	return token;
}




// ------------------------------------------------------------------ 파라미터

// 파라미터 코드에 해당하는 종류 이름. "raw" 는 구조를 풀지 않는다
static const char* paramKindOf(INT16 code)
{
	if (isFrameEventExpressionParameter(code)) {
		return "expression";
	}
	switch (code) {
		case 1: return "object";
		case 2: case 42: return "time";
		case 3: case 4: case 10: case 11: case 12: case 17: case 26: case 31: case 43: case 57: case 58: case 60: case 61: return "short";
		case 5: case 25: case 29: case 34: case 48: case 56: case 67: case 70: return "int";
		case 6: case 7: case 35: case 36: return "sample";
		case 9: case 21: return "create";
		case 13: return "every";
		case 14: case 44: return "key";
		case 16: return "position";
		case 18: return "shoot";
		case 19: return "zone";
		case 24: return "color";
		case 40: return "filename";
		case 50: return "altValue";
		case 32: return "click";
		case 33: return "program";
		case 55: return "extension";
		case 38: return "group";
		case 39: return "groupPointer";
		case 49: return "globalValue";
		case 41: case 64: return "string";
		case 47: case 51: return "twoShorts";
		case 68: return "multiVariables";
		case 69: return "childEvent";
	}
	return "raw";
}


// 파라미터 본문을 종류별 필드로 풀어낸다. 본문을 남김없이 해석하지 못하면 false
static bool decodeParamFields(INT16 code, const string& kind, const ByteBuffer& body, const FrameEventScriptContext& context, Json::Value& out)
{
	try {
		ByteBufferReader reader(body);
		if (kind == "object") {
			out["list"] = (JsonInt)reader.readI16();
			UINT16 objectInfo = reader.readU16();
			INT16 objectType = reader.readI16();
			out["objectInfo"] = (JsonInt)objectInfo;
			out["objectType"] = (JsonInt)objectType;
			string name = objectNameOf(objectType, objectInfo, context);
			if (!name.empty()) {
				out["objectName"] = name;
			}
		}
		else if (kind == "time") {
			out["timer"] = (JsonInt)reader.readI32();
			out["loops"] = (JsonInt)reader.readI32();
			out["comparison"] = (JsonInt)reader.readI16();
		}
		else if (kind == "short") {
			out["value"] = (JsonInt)reader.readI16();
		}
		else if (kind == "int") {
			out["value"] = (JsonInt)reader.readI32();
		}
		else if (kind == "sample") {
			out["handle"] = (JsonInt)reader.readI16();
			out["flags"] = (JsonInt)reader.readU16();
			out["name"] = toUtf8(reader.readWString());
		}
		else if (kind == "create") {
			out["position"] = readPosition(reader);
			out["instance"] = (JsonInt)reader.readU16();
			out["objectInfo"] = (JsonInt)reader.readU16();
		}
		else if (kind == "every") {
			out["delay"] = (JsonInt)reader.readI32();
			out["counter"] = (JsonInt)reader.readI32();
		}
		else if (kind == "key") {
			out["value"] = (JsonInt)reader.readU16();
		}
		else if (kind == "expression") {
			FrameEventExpression expression;
			string errorMessage;
			if (!parseFrameEventExpression(body, expression, errorMessage)) {
				return false;
			}
			out["comparison"] = (JsonInt)expression.cmp;
			Json::Value tokens(Json::arrayValue);
			string summary;
			for (size_t i = 0; i < expression.tokens.size(); i++) {
				string tokenText;
				tokens.append(decompileToken(expression.tokens[i], context, tokenText));
				if (!summary.empty()) {
					summary += " ";
				}
				summary += tokenText;
			}
			out["tokens"] = tokens;
			if (!expression.tail.empty()) {
				out["tail"] = toHex(expression.tail);
			}
			out["text"] = summary;
			return true;
		}
		else if (kind == "position") {
			out["position"] = readPosition(reader);
		}
		else if (kind == "shoot") {
			out["position"] = readPosition(reader);
			out["instance"] = (JsonInt)reader.readU16();
			out["objectInfo"] = (JsonInt)reader.readU16();
			out["unknown"] = toHex(reader.readBytes(4));
			out["speed"] = (JsonInt)reader.readI16();
		}
		else if (kind == "zone") {
			Json::Value values(Json::arrayValue);
			for (int i = 0; i < 4; i++) {
				values.append((JsonInt)reader.readI16());
			}
			out["values"] = values;
		}
		else if (kind == "color") {
			Json::Value values(Json::arrayValue);
			for (int i = 0; i < 4; i++) {
				values.append((JsonInt)reader.readU8());
			}
			out["values"] = values;
		}
		else if (kind == "filename") {
			// 널로 끝나는 ASCII 문자열만 풀어낸다 (그 밖의 바이트가 있으면 raw)
			string text;
			while (true) {
				BYTE c = reader.readU8();
				if (c == 0) {
					break;
				}
				if (c >= 0x80) {
					return false;
				}
				text.push_back((char)c);
			}
			out["value"] = text;
		}
		else if (kind == "altValue") {
			out["value"] = (JsonInt)reader.readI16();
		}
		else if (kind == "click") {
			out["button"] = (JsonInt)reader.readU8();
			out["double"] = (JsonInt)reader.readU8();
		}
		else if (kind == "program") {
			// 데이터가 없다
		}
		else if (kind == "extension") {
			INT16 size = reader.readI16();
			out["type"] = (JsonInt)reader.readI16();
			out["extensionCode"] = (JsonInt)reader.readI16();
			out["size"] = (JsonInt)size;
			out["data"] = toHex(reader.readBytes(reader.remaining()));
		}
		else if (kind == "group") {
			out["flags"] = (JsonInt)reader.readU16();
			out["id"] = (JsonInt)reader.readU16();
			// build 293 이상은 이름 없이 2바이트 꼬리만 있다. 이름이 있으면 널로 끝나는 UTF-16 문자열 + 채움 바이트이다
			if (reader.remaining() > 4) {
				out["name"] = toUtf8(reader.readWString());
			}
			out["padding"] = toHex(reader.readBytes(reader.remaining()));
		}
		else if (kind == "groupPointer") {
			out["pointer"] = (JsonInt)reader.readI32();	// 읽기용, 쓸 때 다시 계산한다
			out["id"] = (JsonInt)reader.readI16();
		}
		else if (kind == "globalValue") {
			out["value"] = (JsonInt)reader.readI16();
		}
		else if (kind == "string") {
			// 널로 끝나는 UTF-16 문자열 (뒤따르는 바이트는 보존한다)
			if ((body.size() < 2) || ((body.size() % 2) != 0)) {
				return false;
			}
			out["value"] = toUtf8(reader.readWString());
			out["tail"] = toHex(reader.readBytes(reader.remaining()));
		}
		else if (kind == "twoShorts") {
			out["first"] = (JsonInt)reader.readI16();
			out["second"] = (JsonInt)reader.readI16();
		}
		else if (kind == "multiVariables") {
			// 알터러블/전역 값 여러 개를 한 번에 정하는 파라미터. 12바이트 머리(flags, flagMasks, flagValues) 뒤에 항목 4칸(각 16바이트)이 있고,
			// flags 의 4비트씩이 항목 하나를 가리킨다: 1 = 쓰는 항목, 2 = 전역 값, 4 = 실수(double) 값
			const INT32 flags = reader.readI32();
			out["flags"] = (JsonInt)flags;
			out["flagMasks"] = (JsonInt)reader.readI32();
			out["flagValues"] = (JsonInt)reader.readI32();
			Json::Value values(Json::arrayValue);
			for (int i = 0; i < 4 && ((flags >> (4 * i)) & 1) != 0; i++) {
				Json::Value item(Json::objectValue);
				item["index"] = (JsonInt)reader.readI32();
				item["op"] = (JsonInt)reader.readI32();
				if (((flags >> (4 * i)) & 4) != 0) {
					ByteBuffer bits = reader.readBytes(8);
					double d = 0.0;
					memcpy(&d, &bits[0], 8);
					item["value"] = d;
				}
				else {
					item["value"] = (JsonInt)reader.readI32();
					if (reader.readI32() != 0) {
						return false;		// 값 뒤 4바이트가 0 이 아니면 구조를 모른다
					}
				}
				values.append(item);
			}
			out["values"] = values;
		}
		else if (kind == "childEvent") {
			// 개수 + (오브젝트 정보, 값) 짝 + 끝 표지 4바이트
			const INT32 count = reader.readI32();
			if (count < 0 || (size_t)count * 4 + 4 > reader.remaining()) {
				return false;
			}
			Json::Value list(Json::arrayValue);
			for (INT32 i = 0; i < count; i++) {
				Json::Value item(Json::objectValue);
				item["objectInfo"] = (JsonInt)reader.readI16();
				item["extra"] = (JsonInt)reader.readI16();
				list.append(item);
			}
			out["list"] = list;
			out["terminator"] = (JsonInt)reader.readI32();
		}
		else {
			return false;
		}
		// 필드 뒤에 남은 바이트: 모두 0 이면 채움 길이(padTo)로, 아니면 그대로(tail) 보존한다
		if (reader.remaining() > 0) {
			ByteBuffer rest = reader.readBytes(reader.remaining());
			bool allZero = true;
			for (size_t i = 0; i < rest.size(); i++) {
				if (rest[i] != 0) {
					allZero = false;
					break;
				}
			}
			if (allZero) {
				out["padTo"] = (JsonInt)body.size();
			}
			else {
				out["tail"] = toHex(rest);
			}
		}
		return true;
	}
	catch (const exception&) {
		return false;
	}
}


// 종류별 필드로부터 파라미터 본문을 만든다. 필드가 잘못되었으면 예외를 던진다
static ByteBuffer encodeParamFields(const string& kind, const Json::Value& fields)
{
	ByteBufferWriter writer;
	if (kind == "object") {
		writer.writeI16((INT16)getInt(fields, "list", -32768, 32767));
		writer.writeU16((UINT16)getInt(fields, "objectInfo", 0, 65535));
		writer.writeI16((INT16)getInt(fields, "objectType", -32768, 32767));
	}
	else if (kind == "time") {
		writer.writeI32((INT32)getInt(fields, "timer", -2147483647 - 1, 2147483647));
		writer.writeI32((INT32)getInt(fields, "loops", -2147483647 - 1, 2147483647));
		writer.writeI16((INT16)getInt(fields, "comparison", -32768, 32767));
	}
	else if (kind == "short") {
		writer.writeI16((INT16)getInt(fields, "value", -32768, 32767));
	}
	else if (kind == "int") {
		writer.writeI32((INT32)getInt(fields, "value", -2147483647 - 1, 2147483647));
	}
	else if (kind == "sample") {
		writer.writeI16((INT16)getInt(fields, "handle", -32768, 32767));
		writer.writeU16((UINT16)getInt(fields, "flags", 0, 65535));
		writer.writeWString(fromUtf8(getString(fields, "name")));
	}
	else if (kind == "create") {
		if (!fields.isMember("position")) {
			throw ByteBufferError("missing position");
		}
		writePosition(fields["position"], writer);
		writer.writeU16((UINT16)getInt(fields, "instance", 0, 65535));
		writer.writeU16((UINT16)getInt(fields, "objectInfo", 0, 65535));
	}
	else if (kind == "every") {
		writer.writeI32((INT32)getInt(fields, "delay", -2147483647 - 1, 2147483647));
		writer.writeI32((INT32)getInt(fields, "counter", -2147483647 - 1, 2147483647));
	}
	else if (kind == "key") {
		writer.writeU16((UINT16)getInt(fields, "value", 0, 65535));
	}
	else if (kind == "expression") {
		FrameEventExpression expression;
		expression.cmp = (INT16)getInt(fields, "comparison", -32768, 32767);
		const Json::Value& tokens = getArray(fields, "tokens");
		for (Json::ArrayIndex i = 0; i < tokens.size(); i++) {
			expression.tokens.push_back(recompileToken(tokens[i]));
		}
		if (fields.isMember("tail")) {
			expression.tail = getHex(fields, "tail");
		}
		return buildFrameEventExpression(expression);
	}
	else if (kind == "position") {
		if (!fields.isMember("position")) {
			throw ByteBufferError("missing position");
		}
		writePosition(fields["position"], writer);
	}
	else if (kind == "shoot") {
		if (!fields.isMember("position")) {
			throw ByteBufferError("missing position");
		}
		writePosition(fields["position"], writer);
		writer.writeU16((UINT16)getInt(fields, "instance", 0, 65535));
		writer.writeU16((UINT16)getInt(fields, "objectInfo", 0, 65535));
		ByteBuffer unknown = getHex(fields, "unknown");
		if (unknown.size() != 4) {
			throw ByteBufferError("shoot unknown must be 4 bytes");
		}
		writer.writeBytes(unknown);
		writer.writeI16((INT16)getInt(fields, "speed", -32768, 32767));
	}
	else if (kind == "zone") {
		const Json::Value& values = getArray(fields, "values");
		if (values.size() != 4) {
			throw ByteBufferError("zone needs 4 values");
		}
		for (Json::ArrayIndex i = 0; i < 4; i++) {
			if (!values[i].isIntegral() || (values[i].asInt64() < -32768) || (values[i].asInt64() > 32767)) {
				throw ByteBufferError("bad zone value");
			}
			writer.writeI16((INT16)values[i].asInt64());
		}
	}
	else if (kind == "color") {
		const Json::Value& values = getArray(fields, "values");
		if (values.size() != 4) {
			throw ByteBufferError("color needs 4 values");
		}
		for (Json::ArrayIndex i = 0; i < 4; i++) {
			if (!values[i].isIntegral() || (values[i].asInt64() < 0) || (values[i].asInt64() > 255)) {
				throw ByteBufferError("bad color value");
			}
			writer.writeU8((BYTE)values[i].asInt64());
		}
	}
	else if (kind == "filename") {
		string text = getString(fields, "value");
		for (size_t i = 0; i < text.size(); i++) {
			if (((BYTE)text[i] >= 0x80) || (text[i] == 0)) {
				throw ByteBufferError("filename must be ASCII");
			}
			writer.writeU8((BYTE)text[i]);
		}
		writer.writeU8(0);
	}
	else if (kind == "altValue") {
		writer.writeI16((INT16)getInt(fields, "value", -32768, 32767));
	}
	else if (kind == "click") {
		writer.writeU8((BYTE)getInt(fields, "button", 0, 255));
		writer.writeU8((BYTE)getInt(fields, "double", 0, 255));
	}
	else if (kind == "program") {
		// 데이터가 없다
	}
	else if (kind == "extension") {
		writer.writeI16((INT16)getInt(fields, "size", -32768, 32767));
		writer.writeI16((INT16)getInt(fields, "type", -32768, 32767));
		writer.writeI16((INT16)getInt(fields, "extensionCode", -32768, 32767));
		writer.writeBytes(getHex(fields, "data"));
	}
	else if (kind == "group") {
		writer.writeU16((UINT16)getInt(fields, "flags", 0, 65535));
		writer.writeU16((UINT16)getInt(fields, "id", 0, 65535));
		if (fields.isMember("name")) {
			writer.writeWString(fromUtf8(getString(fields, "name")));
		}
		writer.writeBytes(getHex(fields, "padding"));
	}
	else if (kind == "groupPointer") {
		// 포인터 값은 저장할 때 대상 그룹의 위치로 다시 계산된다
		writer.writeI32((INT32)getOptionalInt(fields, "pointer", -2147483647 - 1, 2147483647, 0));
		writer.writeI16((INT16)getInt(fields, "id", -32768, 32767));
	}
	else if (kind == "globalValue") {
		writer.writeI16((INT16)getInt(fields, "value", -32768, 32767));
	}
	else if (kind == "string") {
		writer.writeWString(fromUtf8(getString(fields, "value")));
		if (fields.isMember("tail")) {
			writer.writeBytes(getHex(fields, "tail"));
		}
	}
	else if (kind == "twoShorts") {
		writer.writeI16((INT16)getInt(fields, "first", -32768, 32767));
		writer.writeI16((INT16)getInt(fields, "second", -32768, 32767));
	}
	else if (kind == "multiVariables") {
		const INT32 flags = (INT32)getInt(fields, "flags", -2147483647 - 1, 2147483647);
		writer.writeI32(flags);
		writer.writeI32((INT32)getInt(fields, "flagMasks", -2147483647 - 1, 2147483647));
		writer.writeI32((INT32)getInt(fields, "flagValues", -2147483647 - 1, 2147483647));
		const Json::Value& values = getArray(fields, "values");
		int used = 0;
		while (used < 4 && ((flags >> (4 * used)) & 1) != 0) {
			used++;
		}
		if ((int)values.size() != used) {
			throw ByteBufferError("multiVariables values count does not match flags");
		}
		for (int i = 0; i < used; i++) {
			const Json::Value& item = values[(Json::ArrayIndex)i];
			writer.writeI32((INT32)getInt(item, "index", -2147483647 - 1, 2147483647));
			writer.writeI32((INT32)getInt(item, "op", -2147483647 - 1, 2147483647));
			if (((flags >> (4 * i)) & 4) != 0) {
				if (!item["value"].isNumeric()) {
					throw ByteBufferError("multiVariables value must be a number");
				}
				double d = item["value"].asDouble();
				ByteBuffer bits(8);
				memcpy(&bits[0], &d, 8);
				writer.writeBytes(bits);
			}
			else {
				writer.writeI32((INT32)getInt(item, "value", -2147483647 - 1, 2147483647));
				writer.writeI32(0);
			}
		}
	}
	else if (kind == "childEvent") {
		const Json::Value& list = getArray(fields, "list");
		writer.writeI32((INT32)list.size());
		for (Json::ArrayIndex i = 0; i < list.size(); i++) {
			writer.writeI16((INT16)getInt(list[i], "objectInfo", -32768, 32767));
			writer.writeI16((INT16)getInt(list[i], "extra", -32768, 32767));
		}
		writer.writeI32((INT32)getInt(fields, "terminator", -2147483647 - 1, 2147483647));
	}
	else {
		throw ByteBufferError("unknown parameter kind: " + kind);
	}
	// 필드 뒤에 붙는 꼬리 바이트와 0 채움 (문자열 종류는 자체 꼬리를 이미 기록했다)
	if ((kind != "string") && fields.isMember("tail")) {
		writer.writeBytes(getHex(fields, "tail"));
	}
	ByteBuffer body = writer.getBuffer();
	if (fields.isMember("padTo")) {
		size_t padTo = (size_t)getInt(fields, "padTo", 0, 1 << 20);
		while (body.size() < padTo) {
			body.push_back(0);
		}
	}
	return body;
}


static Json::Value decompileParam(const FrameEventParameter& param, const FrameEventScriptContext& context)
{
	Json::Value value(Json::objectValue);
	value["code"] = (JsonInt)param.code;
	string kind = paramKindOf(param.code);
	bool structured = false;
	if (kind != "raw") {
		Json::Value fields(Json::objectValue);
		if (decodeParamFields(param.code, kind, param.body, context, fields)) {
			// 다시 인코딩해서 원본과 같을 때에만 구조화된 형태를 쓴다
			try {
				if (encodeParamFields(kind, fields) == param.body) {
					value["kind"] = kind;
					for (Json::Value::const_iterator it = fields.begin(); it != fields.end(); ++it) {
						value[it.name()] = *it;
					}
					structured = true;
				}
			}
			catch (const exception&) {
				structured = false;
			}
		}
	}
	if (!structured) {
		value["kind"] = "raw";
		value["hex"] = toHex(param.body);
	}
	if ((param.code == frameEventGroupPointerParameterCode) && (param.targetGroupId >= 0)) {
		value["targetGroup"] = (JsonInt)param.targetGroupId;
		value["delta"] = (JsonInt)param.targetGroupDelta;
	}
	return value;
}


static FrameEventParameter recompileParam(const Json::Value& value)
{
	FrameEventParameter param;
	param.code = (INT16)getInt(value, "code", -32768, 32767);
	string kind = getString(value, "kind");
	if (kind == "raw") {
		param.body = getHex(value, "hex");
	}
	else {
		param.body = encodeParamFields(kind, value);
	}
	if (param.code == frameEventGroupPointerParameterCode) {
		if (value.isMember("targetGroup")) {
			param.targetGroupId = (INT32)getInt(value, "targetGroup", 0, 2147483647);
			param.targetGroupDelta = (INT32)getOptionalInt(value, "delta", 0, 2147483647, 0);
		}
	}
	return param;
}




// ------------------------------------------------------------------ 조건 / 액션 / 그룹

static void decompileObjectFields(Json::Value& value, INT16 objectType, INT16 num, UINT16 objectInfo, INT16 objectList, INT8 flags, INT8 otherFlags, BYTE defType, const FrameEventScriptContext& context)
{
	value["objectType"] = (JsonInt)objectType;
	value["objectInfo"] = (JsonInt)objectInfo;
	value["objectList"] = (JsonInt)objectList;
	string name = objectNameOf(objectType, objectInfo, context);
	if (!name.empty()) {
		value["objectName"] = name;
	}
	value["num"] = (JsonInt)num;
	value["flags"] = (JsonInt)flags;
	value["otherFlags"] = (JsonInt)otherFlags;
	value["defType"] = (JsonInt)defType;
}


static Json::Value decompileCondition(const FrameEventCondition& condition, const FrameEventScriptContext& context)
{
	Json::Value value(Json::objectValue);
	decompileObjectFields(value, condition.objectType, condition.num, condition.objectInfo, condition.objectList, condition.flags, condition.otherFlags, condition.defType, context);
	value["ident"] = (JsonInt)condition.ident;
	Json::Value params(Json::arrayValue);
	for (size_t i = 0; i < condition.params.size(); i++) {
		params.append(decompileParam(condition.params[i], context));
	}
	value["params"] = params;
	if (!condition.slack.empty()) {
		value["slack"] = toHex(condition.slack);
	}
	return value;
}


static Json::Value decompileAction(const FrameEventAction& action, const FrameEventScriptContext& context)
{
	Json::Value value(Json::objectValue);
	decompileObjectFields(value, action.objectType, action.num, action.objectInfo, action.objectList, action.flags, action.otherFlags, action.defType, context);
	Json::Value params(Json::arrayValue);
	for (size_t i = 0; i < action.params.size(); i++) {
		params.append(decompileParam(action.params[i], context));
	}
	value["params"] = params;
	if (!action.slack.empty()) {
		value["slack"] = toHex(action.slack);
	}
	return value;
}


static Json::Value decompileGroup(const FrameEventDataGroup& group, bool newGroupHeader, const FrameEventScriptContext& context)
{
	Json::Value value(Json::objectValue);
	value["uid"] = (JsonInt)group.groupId;
	value["line"] = (JsonInt)group.line;
	value["flags"] = (JsonInt)group.flags;
	value["restricted"] = (JsonInt)group.restricted;
	value["cpt"] = (JsonInt)group.cpt;
	if (!newGroupHeader) {
		value["undo"] = (JsonInt)group.undo;
	}
	Json::Value conditions(Json::arrayValue);
	for (size_t i = 0; i < group.conditions.size(); i++) {
		conditions.append(decompileCondition(group.conditions[i], context));
	}
	value["conditions"] = conditions;
	Json::Value actions(Json::arrayValue);
	for (size_t i = 0; i < group.actions.size(); i++) {
		actions.append(decompileAction(group.actions[i], context));
	}
	value["actions"] = actions;
	if (!group.slack.empty()) {
		value["slack"] = toHex(group.slack);
	}
	return value;
}


static void recompileObjectFields(const Json::Value& value, INT16& objectType, INT16& num, UINT16& objectInfo, INT16& objectList, INT8& flags, INT8& otherFlags, BYTE& defType)
{
	objectType = (INT16)getInt(value, "objectType", -32768, 32767);
	num = (INT16)getInt(value, "num", -32768, 32767);
	objectInfo = (UINT16)getInt(value, "objectInfo", 0, 65535);
	objectList = (INT16)getInt(value, "objectList", -32768, 32767);
	flags = (INT8)getInt(value, "flags", -128, 127);
	otherFlags = (INT8)getInt(value, "otherFlags", -128, 127);
	defType = (BYTE)getInt(value, "defType", 0, 255);
}


static FrameEventCondition recompileCondition(const Json::Value& value)
{
	FrameEventCondition condition;
	recompileObjectFields(value, condition.objectType, condition.num, condition.objectInfo, condition.objectList, condition.flags, condition.otherFlags, condition.defType);
	condition.ident = (INT16)getInt(value, "ident", -32768, 32767);
	const Json::Value& params = getArray(value, "params");
	for (Json::ArrayIndex i = 0; i < params.size(); i++) {
		condition.params.push_back(recompileParam(params[i]));
	}
	if (value.isMember("slack")) {
		condition.slack = getHex(value, "slack");
	}
	return condition;
}


static FrameEventAction recompileAction(const Json::Value& value)
{
	FrameEventAction action;
	recompileObjectFields(value, action.objectType, action.num, action.objectInfo, action.objectList, action.flags, action.otherFlags, action.defType);
	const Json::Value& params = getArray(value, "params");
	for (Json::ArrayIndex i = 0; i < params.size(); i++) {
		action.params.push_back(recompileParam(params[i]));
	}
	if (value.isMember("slack")) {
		action.slack = getHex(value, "slack");
	}
	return action;
}


static FrameEventDataGroup recompileGroup(const Json::Value& value, bool newGroupHeader)
{
	FrameEventDataGroup group;
	group.groupId = (INT32)getInt(value, "uid", 0, 2147483647);
	group.line = (INT16)getInt(value, "line", -32768, 32767);
	group.flags = (UINT16)getInt(value, "flags", 0, 65535);
	if (newGroupHeader) {
		group.restricted = (INT32)getInt(value, "restricted", -2147483647 - 1, 2147483647);
		group.cpt = (INT32)getInt(value, "cpt", -2147483647 - 1, 2147483647);
	}
	else {
		group.restricted = (INT16)getInt(value, "restricted", -32768, 32767);
		group.cpt = (INT16)getInt(value, "cpt", -32768, 32767);
		group.undo = (INT16)getInt(value, "undo", -32768, 32767);
	}
	const Json::Value& conditions = getArray(value, "conditions");
	for (Json::ArrayIndex i = 0; i < conditions.size(); i++) {
		group.conditions.push_back(recompileCondition(conditions[i]));
	}
	const Json::Value& actions = getArray(value, "actions");
	for (Json::ArrayIndex i = 0; i < actions.size(); i++) {
		group.actions.push_back(recompileAction(actions[i]));
	}
	if (value.isMember("slack")) {
		group.slack = getHex(value, "slack");
	}
	return group;
}




// ------------------------------------------------------------------ 공개 함수

bool CTT::Chunk::decompileFrameEvents(const FrameEventChunkModel& model, const FrameEventScriptContext& context, Json::Value& value)
{
	value = Json::Value(Json::objectValue);
	value["type"] = frameEventsJsonType;
	value["version"] = frameEventsJsonVersion;
	value["build"] = (JsonInt)model.build;
	Json::Value blocks(Json::arrayValue);
	for (size_t b = 0; b < model.blocks.size(); b++) {
		const FrameEventBlock& block = model.blocks[b];
		Json::Value item(Json::objectValue);
		if (block.type == FrameEventBlockType::RAW) {
			item["kind"] = "raw";
			item["hex"] = toHex(block.raw);
			// 헤더 블록의 한정자 목록은 읽기 위해 풀어 둔다 (다시 적용할 때는 hex 를 쓴다)
			if ((block.raw.size() >= 4) && (memcmp(&block.raw[0], "ER>>", 4) == 0) && (block.raw.size() >= (4 + 6 + 34 + 2))) {
				ByteBufferReader reader(block.raw, 4 + 6 + 34);
				INT16 count = reader.readI16();
				Json::Value qualifiers(Json::arrayValue);
				for (INT16 q = 0; (q < count) && (reader.remaining() >= 4); q++) {
					Json::Value qualifier(Json::objectValue);
					qualifier["objectInfo"] = (JsonInt)reader.readU16();
					qualifier["type"] = (JsonInt)reader.readI16();
					qualifiers.append(qualifier);
				}
				item["qualifiers"] = qualifiers;
			}
		}
		else if (block.type == FrameEventBlockType::SIZE) {
			item["kind"] = "size";
		}
		else {
			item["kind"] = "groups";
			Json::Value groups(Json::arrayValue);
			for (size_t g = 0; g < block.groups.size(); g++) {
				groups.append(decompileGroup(block.groups[g], model.newGroupHeader, context));
			}
			item["groups"] = groups;
		}
		blocks.append(item);
	}
	value["blocks"] = blocks;
	return true;
}


bool CTT::Chunk::recompileFrameEvents(FrameEventChunkModel& model, const Json::Value& value, string& errorMessage)
{
	try {
		if (!value.isObject() || (getString(value, "type") != frameEventsJsonType)) {
			throw ByteBufferError("not a frameEvents json");
		}
		if ((UINT32)getInt(value, "build", 0, 2147483647) != model.build) {
			throw ByteBufferError("the json was made from another build");
		}

		FrameEventChunkModel result;
		result.build = model.build;
		result.newGroupHeader = model.newGroupHeader;
		std::set<INT32> uids;
		INT32 maxUid = -1;
		const Json::Value& blocks = getArray(value, "blocks");
		for (Json::ArrayIndex b = 0; b < blocks.size(); b++) {
			FrameEventBlock block;
			string kind = getString(blocks[b], "kind");
			if (kind == "raw") {
				block.type = FrameEventBlockType::RAW;
				block.raw = getHex(blocks[b], "hex");
			}
			else if (kind == "size") {
				block.type = FrameEventBlockType::SIZE;
			}
			else if (kind == "groups") {
				block.type = FrameEventBlockType::GROUPS;
				const Json::Value& groups = getArray(blocks[b], "groups");
				for (Json::ArrayIndex g = 0; g < groups.size(); g++) {
					FrameEventDataGroup group = recompileGroup(groups[g], result.newGroupHeader);
					if (!uids.insert(group.groupId).second) {
						throw ByteBufferError("duplicate group uid");
					}
					if (group.groupId > maxUid) {
						maxUid = group.groupId;
					}
					block.groups.push_back(group);
				}
			}
			else {
				throw ByteBufferError("unknown block kind: " + kind);
			}
			result.blocks.push_back(block);
		}
		result.nextGroupId = maxUid + 1;

		// 모든 그룹 포인터의 대상 그룹이 있는지 확인한다
		for (size_t b = 0; b < result.blocks.size(); b++) {
			for (size_t g = 0; g < result.blocks[b].groups.size(); g++) {
				FrameEventDataGroup& group = result.blocks[b].groups[g];
				for (size_t c = 0; c < group.conditions.size(); c++) {
					for (size_t p = 0; p < group.conditions[c].params.size(); p++) {
						const FrameEventParameter& param = group.conditions[c].params[p];
						if ((param.targetGroupId >= 0) && (uids.find(param.targetGroupId) == uids.end())) {
							throw ByteBufferError("group pointer refers to a group that does not exist");
						}
					}
				}
				for (size_t a = 0; a < group.actions.size(); a++) {
					for (size_t p = 0; p < group.actions[a].params.size(); p++) {
						const FrameEventParameter& param = group.actions[a].params[p];
						if ((param.targetGroupId >= 0) && (uids.find(param.targetGroupId) == uids.end())) {
							throw ByteBufferError("group pointer refers to a group that does not exist");
						}
					}
				}
			}
		}
		model = result;
	}
	catch (const exception& e) {
		errorMessage = e.what();
		return false;
	}
	return true;
}
