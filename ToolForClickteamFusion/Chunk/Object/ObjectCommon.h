#pragma once

#include <map>
#include "../../Util/ByteBufferUtil.h"

// Lossless model of a Fusion ObjectCommon blob (the per-object "common" data: animations, movements,
// extension data, alterable values / strings, ...).
//
// The blob is a fixed header followed by sub-blocks addressed by i16 offsets stored in the header.
// Three header layouts exist (see ObjectCommonLayout). Sub-blocks the tool understands (animations,
// alterable values, alterable strings) are decoded into editable views and re-encoded canonically;
// all other sub-blocks (movements, extension data, counter, system / text) are kept as raw bytes.
// Every offset is recomputed when the blob is rebuilt, so the views may grow or shrink freely.

namespace CTT {

	namespace Chunk {

		using namespace std;
		using namespace CTT::ByteBufferUtil;

		enum class ObjectCommonLayout
		{
			TWO_FIVE_PLUS = 0,	// 2.5+ container (chunks 0x2253 ~ 0x2257)
			BUILD_284 = 1,		// 0x2229 container, build >= 284
			BEFORE_284 = 2		// 0x2229 container, build < 284
		};

		typedef struct _object_animation_direction {
			INT8 minSpeed;
			INT8 maxSpeed;
			INT16 repeat;
			INT16 backTo;
			vector<INT16> frames;	// image handles
			_object_animation_direction() : minSpeed(0), maxSpeed(0), repeat(0), backTo(0) {}
		} ObjectAnimationDirection;

		typedef struct _object_animation {
			bool used;
			map<INT32, ObjectAnimationDirection> directions;	// key = direction index (0 ~ 31)
			_object_animation() : used(false) {}
		} ObjectAnimation;

		typedef struct _object_animations {
			vector<ObjectAnimation> slots;	// one entry per slot of the original table
		} ObjectAnimations;

		class ObjectCommon
		{
		public:
			ObjectCommon();

			// layout / build select the header field positions
			bool parse(const ByteBuffer& blob, ObjectCommonLayout commonLayout, UINT32 build, string& errorMessage);

			// the blob exactly as it was parsed
			const ByteBuffer& getOriginal() const { return raw; }

			// rebuild the blob: untouched views reuse the original sub-block bytes, changed views are re-encoded
			bool serialize(ByteBuffer& blob, string& errorMessage) const;
			// rebuild the blob re-encoding every parsed view (verifies that the encoders are lossless)
			bool serializeCanonical(ByteBuffer& blob, string& errorMessage) const;

			ObjectCommonLayout getLayout() const { return layout; }
			ByteBuffer getIdentifier() const;	// 'SPRI' / 'TEXT' / ... (2 bytes for the old layout)

			// animations
			bool hasAnimations() const { return animationsParsed; }
			ObjectAnimations& getAnimations() { return animations; }
			void markAnimationsChanged() { animationsChanged = true; }

			// alterable values
			bool hasValues() const { return valuesParsed; }
			vector<INT32>& getValues() { return values; }
			ByteBuffer& getValuesTail() { return valuesTail; }	// flag bytes stored after the values
			void markValuesChanged() { valuesChanged = true; }

			// alterable strings
			bool hasStrings() const { return stringsParsed; }
			vector<wstring>& getStrings() { return strings; }
			ByteBuffer& getStringsTail() { return stringsTail; }
			void markStringsChanged() { stringsChanged = true; }

			// raw access to the opaque sub-blocks (empty when absent)
			ByteBuffer getMovementBlock() const { return getSegmentBytes(SEGMENT_MOVEMENT); }
			ByteBuffer getExtensionBlock() const { return getSegmentBytes(SEGMENT_EXTENSION); }
			ByteBuffer getCounterBlock() const { return getSegmentBytes(SEGMENT_COUNTER); }
			ByteBuffer getSystemBlock() const { return getSegmentBytes(SEGMENT_SYSTEM); }

		private:
			enum Segment
			{
				SEGMENT_ANIMATION,
				SEGMENT_MOVEMENT,
				SEGMENT_EXTENSION,
				SEGMENT_COUNTER,
				SEGMENT_SYSTEM,
				SEGMENT_VALUES,
				SEGMENT_STRINGS,
				SEGMENT_COUNT
			};

			typedef struct _segment_info {
				bool present;
				INT32 fieldPosition;	// byte position of the i16 offset field in the header
				INT32 offset;			// offset inside the blob
				size_t length;			// byte length of the sub-block
				_segment_info() : present(false), fieldPosition(0), offset(0), length(0) {}
			} SegmentInfo;

			ByteBuffer getSegmentBytes(Segment segment) const;
			bool build(bool canonical, ByteBuffer& blob, string& errorMessage) const;
			static ByteBuffer encodeAnimations(const ObjectAnimations& value);
			static ByteBuffer encodeValues(const vector<INT32>& value, const ByteBuffer& tail);
			static ByteBuffer encodeStrings(const vector<wstring>& value, const ByteBuffer& tail);

			ByteBuffer raw;
			ObjectCommonLayout layout;
			size_t headerEnd;
			SegmentInfo segments[SEGMENT_COUNT];

			bool animationsParsed;
			ObjectAnimations animations;
			bool animationsChanged;

			bool valuesParsed;
			vector<INT32> values;
			ByteBuffer valuesTail;
			bool valuesChanged;

			bool stringsParsed;
			vector<wstring> strings;
			ByteBuffer stringsTail;
			bool stringsChanged;
		};

	}

}
