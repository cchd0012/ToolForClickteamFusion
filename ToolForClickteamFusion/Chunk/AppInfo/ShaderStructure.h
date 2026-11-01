#pragma once

#include "../Chunk.h"
#include "../../Util/CommonStructure.h"

namespace CTT {

	namespace Chunk {

		using namespace CTT::CommonUtil;

		typedef struct _shader_header
		{
			UINT32 nameOffset;
			UINT32 compiledDataOffset;
			UINT32 parameterOffset;
			UINT32 backgroundTexture;	// 이게 맞는건지는 잘 모르겠음
			UINT32 entrypointOffset;	// 정확하진 않긴 한데 아마 맞을 것으로 보임
			UINT32 rev;					// 전부 0인걸 보면 아마 그럴듯?
		} ShaderHeader;

		// int (4바이트) / float (4바이트) / float4_rgba (4바이트?) / image (이미지 핸들, 4바이트) / 이외에는 모름
		enum ShaderParameterType : BYTE
		{
			INT = 0x0,
			FLOAT = 0x1,
			FLOAT4_RGBA = 0x2,	// 자료형은 4바이트지만, 각 rgba 4바이트는 연산할때 float처럼 쓰임
			IMAGE_HANDLE = 0x3,
		};

		typedef struct _shader_parameter
		{
			wstring name;
			ShaderParameterType type;
		} ShaderParameter;

		typedef struct _shader_argument
		{
			ShaderParameter info;	// 여기서의 패러미터는 ShaderParameter 를 가져와 쓰는게 맞다
			UINT32 intValue;		// info의 ShaderParameterType::INT에 대응
			float floatValue;		// info의 ShaderParameterType::FLOAT에 대응
			Color colorValue;		// info의 ShaderParameterType::FLOAT4_RGBA에 대응
			UINT32 imageHandle;		// info의 ShaderParameterType::IMAGE_HANDLE에 대응
		} ShaderArgument;

	}

}