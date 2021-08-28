#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include "view.h"

int File::open(const char *name) {
	HANDLE *handles = (HANDLE*)&os_handle[0];
	handles[0] = handles[1] = nullptr;

	handles[0] = CreateFileA(
		name,
		GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		nullptr
	);
	if (!handles[0])
		return -1;

	BY_HANDLE_FILE_INFORMATION info = {0};
	GetFileInformationByHandle(handles[0], &info);
	total_size = (int64_t)info.nFileSizeHigh << 32LL | (int64_t)info.nFileSizeLow;

	handles[1] = CreateFileMapping(handles[0], NULL, PAGE_READONLY, 0, 0, NULL);
	if (!handles[1])
		return -2;

	data = (char*)MapViewOfFile(handles[1], FILE_MAP_READ, 0, 0, 0);
	if (!data)
		return -3;

	return 0;
}

void File::close() {
	HANDLE *handles = (HANDLE*)&os_handle[0];

	if (data)
		UnmapViewOfFile(data);
	if (handles[0])
		CloseHandle(handles[0]);
	if (handles[1])
		CloseHandle(handles[1]);

	data = nullptr;
	handles[0] = handles[1] = nullptr;
}