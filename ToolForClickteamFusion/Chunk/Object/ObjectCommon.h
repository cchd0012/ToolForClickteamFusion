#pragma once

#include <map>
#include "../../Util/ByteBufferUtil.h"

// Fusion ObjectCommon 데이터(오브젝트별 "common" 데이터: 애니메이션, 이동, 확장 데이터,
// Alterable 값/문자열 등)의 무손실 모델
//
// 이 데이터는 고정 크기 헤더와, 헤더에 i16 오프셋으로 기록된 하위 블록들로 이루어져 있다.
// 헤더 레이아웃은 세 종류가 있다 (ObjectCommonLayout 참고). 도구가 해석할 수 있는 하위 블록
// (애니메이션, Alterable 값, Alterable 문자열)은 편집 가능한 형태로 풀어서 다시 정규 형식으로 인코딩하고,
// 나머지 하위 블록(이동, 확장 데이터, 카운터, 시스템/텍스트)은 바이트 그대로 보존한다.
// 데이터를 다시 만들 때 모든 오프셋을 새로 계산하므로, 편집한 블록의 크기가 늘거나 줄어도 된다.

namespace CTT {

	namespace Chunk {

		using namespace std;
		using namespace CTT::ByteBufferUtil;

		enum class ObjectCommonLayout
		{
			TWO_FIVE_PLUS = 0,	// 2.5+ 컨테이너 (청크 0x2253 ~ 0x2257)
			BUILD_284 = 1,		// 0x2229 컨테이너, build 284 이상
			BEFORE_284 = 2		// 0x2229 컨테이너, build 284 미만
		};

		typedef struct _object_animation_direction {
			INT8 minSpeed;
			INT8 maxSpeed;
			INT16 repeat;
			INT16 backTo;
			vector<INT16> frames;	// 이미지 핸들
			_object_animation_direction() : minSpeed(0), maxSpeed(0), repeat(0), backTo(0) {}
		} ObjectAnimationDirection;

		typedef struct _object_animation {
			bool used;
			map<INT32, ObjectAnimationDirection> directions;	// key = 방향 인덱스 (0 ~ 31)
			_object_animation() : used(false) {}
		} ObjectAnimation;

		typedef struct _object_animations {
			vector<ObjectAnimation> slots;	// 원본 테이블의 슬롯 하나당 항목 하나
		} ObjectAnimations;

		class ObjectCommon
		{
		public:
			ObjectCommon();

			// 레이아웃과 build 에 따라 헤더 필드의 위치가 정해진다
			bool parse(const ByteBuffer& blob, ObjectCommonLayout commonLayout, UINT32 build, string& errorMessage);

			// 파싱한 그대로의 원본 데이터
			const ByteBuffer& getOriginal() const { return raw; }

			// 데이터를 다시 만든다: 변경하지 않은 항목은 원본 하위 블록 바이트를 그대로 쓰고, 변경한 항목만 다시 인코딩한다
			bool serialize(ByteBuffer& blob, string& errorMessage) const;
			// 파싱한 모든 항목을 다시 인코딩하여 데이터를 만든다 (인코더가 무손실인지 검증하는 용도)
			bool serializeCanonical(ByteBuffer& blob, string& errorMessage) const;

			ObjectCommonLayout getLayout() const { return layout; }
			ByteBuffer getIdentifier() const;	// 'SPRI' / 'TEXT' 등의 식별자 (구버전 레이아웃은 2바이트)

			// 애니메이션
			bool hasAnimations() const { return animationsParsed; }
			ObjectAnimations& getAnimations() { return animations; }
			void markAnimationsChanged() { animationsChanged = true; }

			// Alterable 값
			bool hasValues() const { return valuesParsed; }
			vector<INT32>& getValues() { return values; }
			ByteBuffer& getValuesTail() { return valuesTail; }	// 값 뒤에 이어지는 플래그 바이트
			void markValuesChanged() { valuesChanged = true; }

			// Alterable 문자열
			bool hasStrings() const { return stringsParsed; }
			vector<wstring>& getStrings() { return strings; }
			ByteBuffer& getStringsTail() { return stringsTail; }
			void markStringsChanged() { stringsChanged = true; }

			// 해석하지 않는 하위 블록의 원본 바이트 (없으면 빈 값)
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
				INT32 fieldPosition;	// 헤더에서 i16 오프셋 필드가 있는 바이트 위치
				INT32 offset;			// 데이터 내부 오프셋
				size_t length;			// 하위 블록의 바이트 길이
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
