#include "view.h"

int File::open(const char *name) {
	memset(os_handle, 0, sizeof(os_handle));

	/*
	if (fd < 0)
		return -1;
	*/

	return 0;
}

void File::close() {
	/*
	munmap(text.data, text.total_size);

	int fd = ((int*)os_handle)[0];
	if (fd > 0)
		::close(fd);
	*/
}