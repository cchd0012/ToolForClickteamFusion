#include "FrameEventModel.h"
#include <map>

using namespace CTT::Chunk;


bool CTT::Chunk::isFrameEventExpressionParameter(INT16 code)
{
	switch (code) {
		case 15: case 22: case 23: case 27: case 28: case 45:
		case 46: case 52: case 53: case 54: case 59: case 62:
			return true;
	}
	return false;
}


size_t CTT::Chunk::countUnresolvedFrameEventPointers(const FrameEventChunkModel& model)
{
	size_t count = 0;
	for (size_t b = 0; b < model.blocks.size(); b++) {
		const vector<FrameEventDataGroup>& groups = model.blocks[b].groups;
		for (size_t g = 0; g < groups.size(); g++) {
			for (size_t c = 0; c < groups[g].conditions.size(); c++) {
				const vector<FrameEventParameter>& params = groups[g].conditions[c].params;
				for (size_t p = 0; p < params.size(); p++) {
					if ((params[p].code == frameEventGroupPointerParameterCode) && (params[p].targetGroupId < 0)) {
						count++;
					}
				}
			}
			for (size_t a = 0; a < groups[g].actions.size(); a++) {
				const vector<FrameEventParameter>& params = groups[g].actions[a].params;
				for (size_t p = 0; p < params.size(); p++) {
					if ((params[p].code == frameEventGroupPointerParameterCode) && (params[p].targetGroupId < 0)) {
						count++;
					}
				}
			}
		}
	}
	return count;
}




// ------------------------------------------------------------------ parse

namespace {

	// where a group pointer was found while parsing
	typedef struct _pointer_reference {
		size_t block;
		size_t group;
		bool isCondition;
		size_t item;
		size_t param;
		INT64 targetOffset;
	} PointerReference;

	typedef struct _group_range {
		INT64 end;
		INT32 groupId;
	} GroupRange;

	class FrameEventParser
	{
	public:
		FrameEventParser(const BYTE* data, size_t size, bool newHeader, FrameEventChunkModel* chunkModel)
			: reader(data, size), newGroupHeader(newHeader), model(chunkModel) {}

		void run()
		{
			const size_t total = reader.size();
			while ((reader.tell() + 4) <= total) {
				size_t at = reader.tell();
				ByteBuffer magic = reader.readBytes(4);
				string id((const char*)&magic[0], 4);
				if (id == frameEventHeaderMagic) {
					reader.skip(2 + 2 + 2 + 17 * 2);
					INT16 qualifierCount = reader.readI16();
					if (qualifierCount < 0) {
						throw ByteBufferError("bad qualifier count");
					}
					reader.skip((size_t)qualifierCount * 4);
					model->blocks.push_back(makeRawBlock(at, reader.tell() - at));
				}
				else if (id == frameEventOptionMagic) {
					reader.skip(4);
					model->blocks.push_back(makeRawBlock(at, 8));
				}
				else if (id == frameEventSizeInfoMagic) {
					reader.skip(4);
					FrameEventBlock block;
					block.type = FrameEventBlockType::SIZE;
					model->blocks.push_back(block);
				}
				else if (id == frameEventDataGroupMagic) {
					INT32 size = reader.readI32();
					if (size < 0) {
						throw ByteBufferError("bad ERev size");
					}
					size_t end = reader.tell() + (size_t)size;
					if (end > total) {
						throw ByteBufferError("ERev exceeds chunk");
					}
					model->blocks.push_back(FrameEventBlock());
					size_t blockIndex = model->blocks.size() - 1;
					model->blocks[blockIndex].type = FrameEventBlockType::GROUPS;
					while (reader.tell() < end) {
						vector<FrameEventDataGroup>& groups = model->blocks[blockIndex].groups;
						groups.push_back(FrameEventDataGroup());
						readGroup(groups.back(), blockIndex, groups.size() - 1);
					}
					if (reader.tell() != end) {
						throw ByteBufferError("ERev group overrun");
					}
				}
				else if (id == frameEventEndMagic) {
					// keep the end magic and anything that follows it
					model->blocks.push_back(makeRawBlock(at, total - at));
					return;
				}
				else {
					throw ByteBufferError("unknown event block magic");
				}
			}
			if (reader.tell() < total) {
				model->blocks.push_back(makeRawBlock(reader.tell(), total - reader.tell()));
			}
		}

		void resolvePointers()
		{
			for (size_t i = 0; i < pointers.size(); i++) {
				const PointerReference& ref = pointers[i];
				FrameEventDataGroup& group = model->blocks[ref.block].groups[ref.group];
				FrameEventParameter& param = ref.isCondition ? group.conditions[ref.item].params[ref.param] : group.actions[ref.item].params[ref.param];
				param.targetGroupId = -1;
				param.targetGroupDelta = 0;
				// the group that contains the target offset (normally the target is exactly a group start)
				map<INT64, GroupRange>::const_iterator it = groupByStart.upper_bound(ref.targetOffset);
				if (it != groupByStart.begin()) {
					--it;
					if (ref.targetOffset < it->second.end) {
						param.targetGroupId = it->second.groupId;
						param.targetGroupDelta = (INT32)(ref.targetOffset - it->first);
					}
				}
			}
		}

	private:
		FrameEventBlock makeRawBlock(size_t from, size_t length)
		{
			FrameEventBlock block;
			block.type = FrameEventBlockType::RAW;
			size_t backup = reader.tell();
			reader.seek(from);
			block.raw = reader.readBytes(length);
			reader.seek(backup);
			return block;
		}

		void readParams(vector<FrameEventParameter>& params, size_t count, size_t blockIndex, size_t groupIndex, bool isCondition, size_t itemIndex)
		{
			params.resize(count);
			for (size_t i = 0; i < count; i++) {
				size_t start = reader.tell();
				INT16 size = reader.readI16();
				if (size < 4) {
					throw ByteBufferError("bad param size");
				}
				FrameEventParameter& param = params[i];
				param.code = reader.readI16();
				param.body = reader.readBytes((size_t)size - 4);
				if ((param.code == frameEventGroupPointerParameterCode) && (param.body.size() >= 4)) {
					ByteBufferReader pointerReader(param.body);
					PointerReference ref;
					ref.block = blockIndex;
					ref.group = groupIndex;
					ref.isCondition = isCondition;
					ref.item = itemIndex;
					ref.param = i;
					ref.targetOffset = (INT64)start + (INT64)pointerReader.readI32();
					pointers.push_back(ref);
				}
			}
		}

		void readCondition(FrameEventCondition& condition, size_t blockIndex, size_t groupIndex, size_t itemIndex)
		{
			size_t start = reader.tell();
			UINT16 size = reader.readU16();
			if (size < 16) {
				throw ByteBufferError("bad condition size");
			}
			condition.objectType = reader.readI16();
			condition.num = reader.readI16();
			condition.objectInfo = reader.readU16();
			condition.objectList = reader.readI16();
			condition.flags = reader.readI8();
			condition.otherFlags = reader.readI8();
			BYTE paramCount = reader.readU8();
			condition.defType = reader.readU8();
			condition.ident = reader.readI16();
			readParams(condition.params, paramCount, blockIndex, groupIndex, true, itemIndex);
			size_t end = start + size;
			if (reader.tell() > end) {
				throw ByteBufferError("condition params overrun");
			}
			condition.slack = reader.readBytes(end - reader.tell());
		}

		void readAction(FrameEventAction& action, size_t blockIndex, size_t groupIndex, size_t itemIndex)
		{
			size_t start = reader.tell();
			UINT16 size = reader.readU16();
			if (size < 14) {
				throw ByteBufferError("bad action size");
			}
			action.objectType = reader.readI16();
			action.num = reader.readI16();
			action.objectInfo = reader.readU16();
			action.objectList = reader.readI16();
			action.flags = reader.readI8();
			action.otherFlags = reader.readI8();
			BYTE paramCount = reader.readU8();
			action.defType = reader.readU8();
			readParams(action.params, paramCount, blockIndex, groupIndex, false, itemIndex);
			size_t end = start + size;
			if (reader.tell() > end) {
				throw ByteBufferError("action params overrun");
			}
			action.slack = reader.readBytes(end - reader.tell());
		}

		void readGroup(FrameEventDataGroup& group, size_t blockIndex, size_t groupIndex)
		{
			size_t start = reader.tell();
			INT16 size = reader.readI16();
			if (size >= 0) {
				throw ByteBufferError("bad group size");
			}
			size_t groupSize = (size_t)(-(INT32)size);
			if ((groupSize < 14) || ((start + groupSize) > reader.size())) {
				throw ByteBufferError("bad group size");
			}
			BYTE conditionCount = reader.readU8();
			BYTE actionCount = reader.readU8();
			group.flags = reader.readU16();
			if (newGroupHeader) {
				group.line = reader.readI16();
				group.restricted = reader.readI32();
				group.cpt = reader.readI32();
			}
			else {
				group.restricted = reader.readI16();
				group.cpt = reader.readI16();
				group.line = reader.readI16();
				group.undo = reader.readI16();
			}
			group.groupId = model->nextGroupId++;
			GroupRange range;
			range.end = (INT64)(start + groupSize);
			range.groupId = group.groupId;
			groupByStart[(INT64)start] = range;
			group.conditions.resize(conditionCount);
			for (size_t i = 0; i < conditionCount; i++) {
				readCondition(group.conditions[i], blockIndex, groupIndex, i);
			}
			group.actions.resize(actionCount);
			for (size_t i = 0; i < actionCount; i++) {
				readAction(group.actions[i], blockIndex, groupIndex, i);
			}
			size_t end = start + groupSize;
			if (reader.tell() > end) {
				throw ByteBufferError("group content overrun");
			}
			group.slack = reader.readBytes(end - reader.tell());
		}

		ByteBufferReader reader;
		bool newGroupHeader;
		FrameEventChunkModel* model;
		vector<PointerReference> pointers;
		map<INT64, GroupRange> groupByStart;
	};

}


bool CTT::Chunk::parseFrameEventChunk(const BYTE* data, size_t size, UINT32 build, FrameEventChunkModel& model, string& errorMessage)
{
	model = FrameEventChunkModel();
	model.build = build;
	model.newGroupHeader = (build >= 0x11C);
	try {
		FrameEventParser parser(data, size, model.newGroupHeader, &model);
		parser.run();
		parser.resolvePointers();
	}
	catch (const exception& e) {
		errorMessage = e.what();
		return false;
	}
	return true;
}


bool CTT::Chunk::parseFrameEventChunk(const ByteBuffer& data, UINT32 build, FrameEventChunkModel& model, string& errorMessage)
{
	return parseFrameEventChunk(data.empty() ? NULL : &data[0], data.size(), build, model, errorMessage);
}




// ------------------------------------------------------------------ write

namespace {

	typedef struct _pointer_patch {
		size_t bodyPosition;
		size_t paramStart;
		INT32 targetGroupId;
		INT32 targetGroupDelta;
	} PointerPatch;

	class FrameEventWriter
	{
	public:
		explicit FrameEventWriter(bool newHeader) : newGroupHeader(newHeader) {}

		ByteBufferWriter writer;
		vector<PointerPatch> patches;
		map<INT32, size_t> groupStart;

		void writeParams(const vector<FrameEventParameter>& params)
		{
			for (size_t i = 0; i < params.size(); i++) {
				const FrameEventParameter& param = params[i];
				if ((param.body.size() + 4) > 32767) {
					throw ByteBufferError("param too large");
				}
				size_t start = writer.size();
				writer.writeI16((INT16)(4 + param.body.size()));
				writer.writeI16(param.code);
				if ((param.code == frameEventGroupPointerParameterCode) && (param.body.size() >= 4) && (param.targetGroupId >= 0)) {
					PointerPatch patch;
					patch.bodyPosition = writer.size();
					patch.paramStart = start;
					patch.targetGroupId = param.targetGroupId;
					patch.targetGroupDelta = param.targetGroupDelta;
					patches.push_back(patch);
				}
				writer.writeBytes(param.body);
			}
		}

		void writeCondition(const FrameEventCondition& condition)
		{
			if (condition.params.size() > 255) {
				throw ByteBufferError("too many condition params");
			}
			size_t start = writer.size();
			writer.writeU16(0);
			writer.writeI16(condition.objectType);
			writer.writeI16(condition.num);
			writer.writeU16(condition.objectInfo);
			writer.writeI16(condition.objectList);
			writer.writeI8(condition.flags);
			writer.writeI8(condition.otherFlags);
			writer.writeU8((BYTE)condition.params.size());
			writer.writeU8(condition.defType);
			writer.writeI16(condition.ident);
			writeParams(condition.params);
			writer.writeBytes(condition.slack);
			size_t size = writer.size() - start;
			if (size > 65535) {
				throw ByteBufferError("condition too large");
			}
			writer.patchU16(start, (UINT16)size);
		}

		void writeAction(const FrameEventAction& action)
		{
			if (action.params.size() > 255) {
				throw ByteBufferError("too many action params");
			}
			size_t start = writer.size();
			writer.writeU16(0);
			writer.writeI16(action.objectType);
			writer.writeI16(action.num);
			writer.writeU16(action.objectInfo);
			writer.writeI16(action.objectList);
			writer.writeI8(action.flags);
			writer.writeI8(action.otherFlags);
			writer.writeU8((BYTE)action.params.size());
			writer.writeU8(action.defType);
			writeParams(action.params);
			writer.writeBytes(action.slack);
			size_t size = writer.size() - start;
			if (size > 65535) {
				throw ByteBufferError("action too large");
			}
			writer.patchU16(start, (UINT16)size);
		}

		// returns the byte size of the group
		size_t writeGroup(const FrameEventDataGroup& group)
		{
			if ((group.conditions.size() > 255) || (group.actions.size() > 255)) {
				throw ByteBufferError("too many conditions / actions");
			}
			size_t start = writer.size();
			groupStart[group.groupId] = start;
			writer.writeI16(0);
			writer.writeU8((BYTE)group.conditions.size());
			writer.writeU8((BYTE)group.actions.size());
			writer.writeU16(group.flags);
			if (newGroupHeader) {
				writer.writeI16(group.line);
				writer.writeI32(group.restricted);
				writer.writeI32(group.cpt);
			}
			else {
				writer.writeI16((INT16)group.restricted);
				writer.writeI16((INT16)group.cpt);
				writer.writeI16(group.line);
				writer.writeI16(group.undo);
			}
			for (size_t i = 0; i < group.conditions.size(); i++) {
				writeCondition(group.conditions[i]);
			}
			for (size_t i = 0; i < group.actions.size(); i++) {
				writeAction(group.actions[i]);
			}
			writer.writeBytes(group.slack);
			size_t size = writer.size() - start;
			if (size > 32767) {
				throw ByteBufferError("group too large");
			}
			writer.patchI16(start, (INT16)(-(INT32)size));
			return size;
		}

	private:
		bool newGroupHeader;
	};

}


bool CTT::Chunk::writeFrameEventChunk(const FrameEventChunkModel& model, ByteBuffer& data, string& errorMessage)
{
	try {
		FrameEventWriter eventWriter(model.build >= 0x11C);
		ByteBufferWriter& writer = eventWriter.writer;
		vector<size_t> sizePatches;
		size_t totalGroupBytes = 0;
		for (size_t b = 0; b < model.blocks.size(); b++) {
			const FrameEventBlock& block = model.blocks[b];
			if (block.type == FrameEventBlockType::RAW) {
				writer.writeBytes(block.raw);
			}
			else if (block.type == FrameEventBlockType::SIZE) {
				writer.writeBytes((const BYTE*)frameEventSizeInfoMagic.c_str(), 4);
				sizePatches.push_back(writer.size());
				writer.writeI32(0);
			}
			else {
				writer.writeBytes((const BYTE*)frameEventDataGroupMagic.c_str(), 4);
				size_t sizePosition = writer.size();
				writer.writeI32(0);
				size_t start = writer.size();
				for (size_t g = 0; g < block.groups.size(); g++) {
					totalGroupBytes += eventWriter.writeGroup(block.groups[g]);
				}
				writer.patchI32(sizePosition, (INT32)(writer.size() - start));
			}
		}
		for (size_t i = 0; i < sizePatches.size(); i++) {
			writer.patchI32(sizePatches[i], (INT32)totalGroupBytes);
		}
		for (size_t i = 0; i < eventWriter.patches.size(); i++) {
			const PointerPatch& patch = eventWriter.patches[i];
			map<INT32, size_t>::const_iterator it = eventWriter.groupStart.find(patch.targetGroupId);
			if (it == eventWriter.groupStart.end()) {
				throw ByteBufferError("group pointer target no longer exists");
			}
			writer.patchI32(patch.bodyPosition, (INT32)((INT64)it->second + (INT64)patch.targetGroupDelta - (INT64)patch.paramStart));
		}
		data = writer.getBuffer();
	}
	catch (const exception& e) {
		errorMessage = e.what();
		return false;
	}
	return true;
}




// ------------------------------------------------------------------ expressions

bool CTT::Chunk::parseFrameEventExpression(const ByteBuffer& body, FrameEventExpression& expression, string& errorMessage)
{
	expression = FrameEventExpression();
	try {
		ByteBufferReader reader(body);
		expression.cmp = reader.readI16();
		while (true) {
			INT16 objectType = reader.readI16();
			INT16 num = reader.readI16();
			if ((objectType == 0) && (num == 0)) {
				break;
			}
			INT16 size = reader.readI16();
			if (size < 6) {
				throw ByteBufferError("bad expression token size");
			}
			FrameEventExpressionToken token;
			token.objectType = objectType;
			token.num = num;
			token.body = reader.readBytes((size_t)size - 6);
			expression.tokens.push_back(token);
		}
		expression.tail = reader.readBytes(reader.remaining());
	}
	catch (const exception& e) {
		errorMessage = e.what();
		return false;
	}
	return true;
}


ByteBuffer CTT::Chunk::buildFrameEventExpression(const FrameEventExpression& expression)
{
	ByteBufferWriter writer;
	writer.writeI16(expression.cmp);
	for (size_t i = 0; i < expression.tokens.size(); i++) {
		const FrameEventExpressionToken& token = expression.tokens[i];
		if ((token.body.size() + 6) > 32767) {
			throw ByteBufferError("expression token too large");
		}
		writer.writeI16(token.objectType);
		writer.writeI16(token.num);
		writer.writeI16((INT16)(6 + token.body.size()));
		writer.writeBytes(token.body);
	}
	writer.writeI16(0);
	writer.writeI16(0);
	writer.writeBytes(expression.tail);
	return writer.getBuffer();
}


bool CTT::Chunk::getFrameEventExpressionString(const FrameEventExpressionToken& token, wstring& value)
{
	if (!token.isString()) {
		return false;
	}
	value.clear();
	const ByteBuffer& body = token.body;
	for (size_t i = 0; (i + 1) < body.size(); i += 2) {
		UINT16 c = (UINT16)(body[i] | (body[i + 1] << 8));
		if (c == 0) {
			return true;
		}
		value.push_back((wchar_t)c);
	}
	return false;	// no terminator
}


bool CTT::Chunk::setFrameEventExpressionString(FrameEventExpressionToken& token, const wstring& value)
{
	wstring oldValue;
	if (!getFrameEventExpressionString(token, oldValue)) {
		return false;
	}
	size_t tailStart = (oldValue.size() + 1) * 2;
	ByteBuffer tail(token.body.begin() + tailStart, token.body.end());
	ByteBufferWriter writer;
	writer.writeWString(value);
	writer.writeBytes(tail);
	token.body = writer.getBuffer();
	return true;
}
