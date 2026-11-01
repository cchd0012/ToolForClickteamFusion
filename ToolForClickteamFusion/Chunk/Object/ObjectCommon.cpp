#include "ObjectCommon.h"

using namespace CTT::Chunk;


ObjectCommon::ObjectCommon()
{
	layout = ObjectCommonLayout::TWO_FIVE_PLUS;
	headerEnd = 0;
	animationsParsed = false;
	animationsChanged = false;
	valuesParsed = false;
	valuesChanged = false;
	stringsParsed = false;
	stringsChanged = false;
}


ByteBuffer ObjectCommon::getIdentifier() const
{
	size_t length = (layout == ObjectCommonLayout::BEFORE_284) ? 2 : 4;
	if (raw.size() < (0x2E + length)) {
		return ByteBuffer();
	}
	return ByteBuffer(raw.begin() + 0x2E, raw.begin() + 0x2E + length);
}


ByteBuffer ObjectCommon::getSegmentBytes(Segment segment) const
{
	const SegmentInfo& info = segments[segment];
	if (!info.present) {
		return ByteBuffer();
	}
	return ByteBuffer(raw.begin() + info.offset, raw.begin() + info.offset + info.length);
}


bool ObjectCommon::parse(const ByteBuffer& blob, ObjectCommonLayout commonLayout, UINT32 build, string& errorMessage)
{
	*this = ObjectCommon();
	raw = blob;
	layout = commonLayout;
	try {
		ByteBufferReader reader(raw);
		INT32 declaredSize = reader.readI32();
		if ((declaredSize < 0) || ((size_t)declaredSize != raw.size())) {
			throw ByteBufferError("ObjectCommon size field does not match blob length");
		}
		if (raw.size() < 0x3E) {
			throw ByteBufferError("ObjectCommon header too short");
		}

		// 하위 블록 i16 오프셋이 기록된 헤더 상의 위치
		INT32 position[SEGMENT_COUNT];
		position[SEGMENT_SYSTEM] = 0x24;
		position[SEGMENT_VALUES] = 0x26;
		position[SEGMENT_STRINGS] = 0x28;
		if (layout == ObjectCommonLayout::BEFORE_284) {
			position[SEGMENT_MOVEMENT] = 4;
			position[SEGMENT_ANIMATION] = 6;
			position[SEGMENT_COUNTER] = 10;
			position[SEGMENT_SYSTEM] = 12;
			position[SEGMENT_EXTENSION] = 36;
			position[SEGMENT_VALUES] = 38;
			position[SEGMENT_STRINGS] = 40;
		}
		else if ((layout == ObjectCommonLayout::BUILD_284) && (build == 0x11C) && (ByteBufferReader(raw, 6).readI32() == 0)) {
			// build 284 파일 중 일부는 앞쪽 필드의 순서가 다르다
			position[SEGMENT_COUNTER] = 4;
			position[SEGMENT_MOVEMENT] = 10;
			position[SEGMENT_EXTENSION] = 12;
			position[SEGMENT_ANIMATION] = 14;
		}
		else {
			position[SEGMENT_ANIMATION] = 4;
			position[SEGMENT_MOVEMENT] = 6;
			position[SEGMENT_EXTENSION] = 12;
			position[SEGMENT_COUNTER] = 14;
		}

		size_t firstOffset = raw.size();
		for (INT32 s = 0; s < SEGMENT_COUNT; s++) {
			ByteBufferReader fieldReader(raw, (size_t)position[s]);
			INT16 offset = fieldReader.readI16();
			segments[s].fieldPosition = position[s];
			if (offset > 0) {
				if ((size_t)offset >= raw.size()) {
					throw ByteBufferError("sub-block offset outside blob");
				}
				segments[s].present = true;
				segments[s].offset = offset;
				firstOffset = min(firstOffset, (size_t)offset);
			}
		}
		headerEnd = firstOffset;

		// 각 하위 블록은 다음 하위 블록이 시작하기 전까지 이어진다
		vector<pair<INT32, INT32> > order;	// (오프셋, 하위 블록 종류)
		for (INT32 s = 0; s < SEGMENT_COUNT; s++) {
			if (segments[s].present) {
				order.push_back(make_pair(segments[s].offset, s));
			}
		}
		sort(order.begin(), order.end());
		for (size_t i = 0; i < order.size(); i++) {
			size_t end = ((i + 1) < order.size()) ? (size_t)order[i + 1].first : raw.size();
			segments[order[i].second].length = end - (size_t)order[i].first;
		}

		// ---- 애니메이션 ----
		if (segments[SEGMENT_ANIMATION].present) {
			try {
				ByteBufferReader tableReader(raw, segments[SEGMENT_ANIMATION].offset);
				tableReader.readI16();	// 전체 크기
				INT16 slotCount = tableReader.readI16();
				if (slotCount < 0) {
					throw ByteBufferError("bad animation count");
				}
				vector<INT16> slotOffsets(slotCount);
				for (INT32 i = 0; i < slotCount; i++) {
					slotOffsets[i] = tableReader.readI16();
				}
				animations.slots.resize(slotCount);
				for (INT32 i = 0; i < slotCount; i++) {
					if (slotOffsets[i] == 0) {
						continue;
					}
					size_t base = (size_t)segments[SEGMENT_ANIMATION].offset + (size_t)slotOffsets[i];
					ByteBufferReader slotReader(raw, base);
					INT16 directionOffsets[32];
					for (INT32 d = 0; d < 32; d++) {
						directionOffsets[d] = slotReader.readI16();
					}
					ObjectAnimation& animation = animations.slots[i];
					animation.used = true;
					for (INT32 d = 0; d < 32; d++) {
						if (directionOffsets[d] == 0) {
							continue;
						}
						ByteBufferReader directionReader(raw, base + (size_t)directionOffsets[d]);
						ObjectAnimationDirection direction;
						direction.minSpeed = directionReader.readI8();
						direction.maxSpeed = directionReader.readI8();
						direction.repeat = directionReader.readI16();
						direction.backTo = directionReader.readI16();
						UINT16 frameCount = directionReader.readU16();
						direction.frames.resize(frameCount);
						for (UINT32 f = 0; f < frameCount; f++) {
							direction.frames[f] = directionReader.readI16();
						}
						animation.directions[d] = direction;
					}
				}
				animationsParsed = true;
			}
			catch (const exception&) {
				animationsParsed = false;	// 해석하지 못하면 원본 바이트 그대로 둔다
				animations = ObjectAnimations();
			}
		}

		// ---- Alterable 값 ----
		if (segments[SEGMENT_VALUES].present) {
			try {
				ByteBufferReader valueReader(&raw[0], segments[SEGMENT_VALUES].offset + segments[SEGMENT_VALUES].length, segments[SEGMENT_VALUES].offset);
				INT16 count = valueReader.readI16();
				if (count < 0) {
					throw ByteBufferError("bad value count");
				}
				values.resize(count);
				for (INT32 i = 0; i < count; i++) {
					values[i] = valueReader.readI32();
				}
				valuesTail = valueReader.readBytes(valueReader.remaining());
				valuesParsed = true;
			}
			catch (const exception&) {
				valuesParsed = false;
				values.clear();
			}
		}

		// ---- Alterable 문자열 ----
		if (segments[SEGMENT_STRINGS].present) {
			try {
				ByteBufferReader stringReader(&raw[0], segments[SEGMENT_STRINGS].offset + segments[SEGMENT_STRINGS].length, segments[SEGMENT_STRINGS].offset);
				INT16 count = stringReader.readI16();
				if (count < 0) {
					throw ByteBufferError("bad string count");
				}
				strings.resize(count);
				for (INT32 i = 0; i < count; i++) {
					strings[i] = stringReader.readWString();
				}
				stringsTail = stringReader.readBytes(stringReader.remaining());
				stringsParsed = true;
			}
			catch (const exception&) {
				stringsParsed = false;
				strings.clear();
			}
		}
	}
	catch (const exception& e) {
		errorMessage = e.what();
		return false;
	}
	return true;
}




ByteBuffer ObjectCommon::encodeAnimations(const ObjectAnimations& value)
{
	const size_t slotCount = value.slots.size();
	const size_t headLength = 4 + 2 * slotCount;
	ByteBufferWriter body;
	vector<INT32> slotOffsets(slotCount, 0);
	size_t current = headLength;
	for (size_t i = 0; i < slotCount; i++) {
		const ObjectAnimation& animation = value.slots[i];
		if (!animation.used) {
			continue;
		}
		slotOffsets[i] = (INT32)current;
		ByteBufferWriter directionBlocks;
		INT32 directionOffsets[32];
		size_t directionCurrent = 64;
		for (INT32 d = 0; d < 32; d++) {
			map<INT32, ObjectAnimationDirection>::const_iterator it = animation.directions.find(d);
			if (it == animation.directions.end()) {
				directionOffsets[d] = 0;
				continue;
			}
			const ObjectAnimationDirection& direction = it->second;
			directionOffsets[d] = (INT32)directionCurrent;
			directionBlocks.writeI8(direction.minSpeed);
			directionBlocks.writeI8(direction.maxSpeed);
			directionBlocks.writeI16(direction.repeat);
			directionBlocks.writeI16(direction.backTo);
			directionBlocks.writeU16((UINT16)direction.frames.size());
			for (size_t f = 0; f < direction.frames.size(); f++) {
				directionBlocks.writeI16(direction.frames[f]);
			}
			directionCurrent = 64 + directionBlocks.size();
		}
		ByteBufferWriter block;
		for (INT32 d = 0; d < 32; d++) {
			if (directionOffsets[d] > 32767) {
				throw ByteBufferError("animation direction offset overflow");
			}
			block.writeI16((INT16)directionOffsets[d]);
		}
		block.writeBytes(directionBlocks.getBuffer());
		body.writeBytes(block.getBuffer());
		current += block.size();
	}
	size_t total = headLength + body.size();
	if ((total > 32767) || (current > 32767)) {
		throw ByteBufferError("animation block too large for i16 offsets");
	}
	for (size_t i = 0; i < slotCount; i++) {
		if (slotOffsets[i] > 32767) {
			throw ByteBufferError("animation offset overflow");
		}
	}
	ByteBufferWriter writer;
	writer.writeI16((INT16)total);
	writer.writeI16((INT16)slotCount);
	for (size_t i = 0; i < slotCount; i++) {
		writer.writeI16((INT16)slotOffsets[i]);
	}
	writer.writeBytes(body.getBuffer());
	return writer.getBuffer();
}


ByteBuffer ObjectCommon::encodeValues(const vector<INT32>& value, const ByteBuffer& tail)
{
	if (value.size() > 32767) {
		throw ByteBufferError("too many alterable values");
	}
	ByteBufferWriter writer;
	writer.writeI16((INT16)value.size());
	for (size_t i = 0; i < value.size(); i++) {
		writer.writeI32(value[i]);
	}
	writer.writeBytes(tail);
	return writer.getBuffer();
}


ByteBuffer ObjectCommon::encodeStrings(const vector<wstring>& value, const ByteBuffer& tail)
{
	if (value.size() > 32767) {
		throw ByteBufferError("too many alterable strings");
	}
	ByteBufferWriter writer;
	writer.writeI16((INT16)value.size());
	for (size_t i = 0; i < value.size(); i++) {
		writer.writeWString(value[i]);
	}
	writer.writeBytes(tail);
	return writer.getBuffer();
}




bool ObjectCommon::build(bool canonical, ByteBuffer& blob, string& errorMessage) const
{
	try {
		// 하위 블록을 원래 순서대로 모은다
		vector<pair<INT32, INT32> > order;
		for (INT32 s = 0; s < SEGMENT_COUNT; s++) {
			if (segments[s].present) {
				order.push_back(make_pair(segments[s].offset, s));
			}
		}
		sort(order.begin(), order.end());

		ByteBufferWriter writer;
		writer.writeBytes(&raw[0], headerEnd);
		vector<pair<INT32, size_t> > newOffsets;	// (하위 블록 종류, 새 오프셋)
		for (size_t i = 0; i < order.size(); i++) {
			INT32 segment = order[i].second;
			ByteBuffer data;
			if ((segment == SEGMENT_ANIMATION) && animationsParsed && (canonical || animationsChanged)) {
				data = encodeAnimations(animations);
			}
			else if ((segment == SEGMENT_VALUES) && valuesParsed && (canonical || valuesChanged)) {
				data = encodeValues(values, valuesTail);
			}
			else if ((segment == SEGMENT_STRINGS) && stringsParsed && (canonical || stringsChanged)) {
				data = encodeStrings(strings, stringsTail);
			}
			else {
				data = getSegmentBytes((Segment)segment);
			}
			newOffsets.push_back(make_pair(segment, writer.size()));
			writer.writeBytes(data);
		}
		if (writer.size() > 0x7FFFFFFF) {
			throw ByteBufferError("ObjectCommon too large");
		}
		for (size_t i = 0; i < newOffsets.size(); i++) {
			if (newOffsets[i].second > 32767) {
				throw ByteBufferError("ObjectCommon sub-block offset exceeds 32767");
			}
			writer.patchI16((size_t)segments[newOffsets[i].first].fieldPosition, (INT16)newOffsets[i].second);
		}
		writer.patchI32(0, (INT32)writer.size());
		blob = writer.getBuffer();
	}
	catch (const exception& e) {
		errorMessage = e.what();
		return false;
	}
	return true;
}


bool ObjectCommon::serialize(ByteBuffer& blob, string& errorMessage) const
{
	return build(false, blob, errorMessage);
}


bool ObjectCommon::serializeCanonical(ByteBuffer& blob, string& errorMessage) const
{
	return build(true, blob, errorMessage);
}
