#pragma once

#include "CommonUtilConstant.h"
#include "../Common.h"

namespace CTT {

	namespace CommonUtil {

		using namespace std;

#define SEGMENT_SIZE		0x800000

		wstring getExtentionRemovedPath(wstring &path);
		wstring getExtention(wstring& path);
		wstring getOnlyName(wstring &path);
		wstring getOnlyDirPath(wstring &path);
		wstring getUniStr(string str, CodePage cp);
		string getAnsiStr(wstring wstr, CodePage cp);

		wstring byteToHexStr(BYTE b);

		bool contains(const vector<wstring>& valueList, const wstring value);
		bool contains(const vector<wstring>& valueList, const wstring value, INT32& index);

		bool contains(const vector<UINT32>& valueList, const UINT32 value);
		bool contains(const vector<UINT32>& valueList, const UINT32 value, INT32& index);

		wstring trim(const wstring& str);
		vector<wstring> split(const wstring& str);
		vector<wstring> split(const wstring& str, const wstring& delimiters);
		wstring join(vector<wstring> const& strings, wstring delimiter);

		void splitTwice(const wstring& str, wstring& first, wstring& second, wstring& third);

		wstring replaceAll(wstring& str, const wstring& from, const wstring& to);

		FSOType isFileOrDir(wstring& path);

		bool createDirectoryRecursively(const wstring& path);

		bool segmentedReadFile(HANDLE hRead, BYTE* buffer, UINT32 size);
		void segmentedWriteFile(HANDLE hWrite, BYTE* buffer, UINT32 size);
		bool segmentedTransferData(HANDLE hSrc, HANDLE hDest, UINT32 size);

		UINT32 getChecksum(BYTE* buffer, UINT32 size);

	}

}
