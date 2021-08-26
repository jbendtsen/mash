#include "view.h"

int File::open(const char *name) {
	memset(os_handle, 0, sizeof(os_handle));

	int fd = ::open("vulkan.cpp", O_RDONLY);
	if (fd < 0)
		return -1;

	struct stat st;
	fstat(fd, &st);
	int64_t size = st.st_size;

	data = (char*)mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);
	total_size = size;

	((int*)os_handle)[0] = fd;
	return 0;
}

void File::close() {
	munmap(text.data, text.total_size);

	int fd = ((int*)os_handle)[0];
	if (fd > 0)
		::close(fd);
}