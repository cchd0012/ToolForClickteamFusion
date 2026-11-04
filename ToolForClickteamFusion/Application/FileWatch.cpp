#include "FileWatch.h"

using namespace CTT::App;

DirSnapshot CTT::App::takeDirSnapshot(const wstring& dir, const wstring& pattern)
{
	DirSnapshot snapshot;
	snapshot.valid = true;
	WIN32_FIND_DATAW data;
	HANDLE handle = FindFirstFileW((dir + L"\\" + pattern).c_str(), &data);
	if (handle == INVALID_HANDLE_VALUE) {
		return snapshot;
	}
	do {
		if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
			continue;
		}
		FileStamp stamp;
		stamp.time = ((ULONGLONG)data.ftLastWriteTime.dwHighDateTime << 32) | data.ftLastWriteTime.dwLowDateTime;
		stamp.size = ((ULONGLONG)data.nFileSizeHigh << 32) | data.nFileSizeLow;
		snapshot.files[data.cFileName] = stamp;
	} while (FindNextFileW(handle, &data));
	FindClose(handle);
	return snapshot;
}


WatchResult CTT::App::compareDir(const DirSnapshot& before, const wstring& dir, const wstring& pattern)
{
	WatchResult result;
	if (!before.valid) {
		return result;
	}
	result.after = takeDirSnapshot(dir, pattern);
	for (map<wstring, FileStamp>::const_iterator it = result.after.files.begin(); it != result.after.files.end(); ++it) {
		map<wstring, FileStamp>::const_iterator old = before.files.find(it->first);
		if (old == before.files.end() || !(old->second == it->second)) {
			result.changed.push_back(it->first);
		}
	}
	for (map<wstring, FileStamp>::const_iterator it = before.files.begin(); it != before.files.end(); ++it) {
		if (result.after.files.find(it->first) == result.after.files.end()) {
			result.changed.push_back(it->first);
		}
	}
	return result;
}


wstring CTT::App::fingerprintOf(const WatchResult& result)
{
	wstring text;
	for (size_t i = 0; i < result.changed.size(); i++) {
		text += result.changed[i];
		map<wstring, FileStamp>::const_iterator it = result.after.files.find(result.changed[i]);
		if (it != result.after.files.end()) {
			text += L":" + to_wstring(it->second.time) + L":" + to_wstring(it->second.size);
		}
		text += L"|";
	}
	return text;
}
