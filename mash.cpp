#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

#include "mash.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

#define DEFAULT_FONT_PATH "content/Monaco_Regular.ttf"

static Font_Handle font_face = nullptr;
static Font_Render font_render = {0};

static Text text = {0};
static Highlighter syntax = {0};

const char **get_required_instance_extensions(uint32_t *n_inst_exts) {
	return glfwGetRequiredInstanceExtensions(n_inst_exts);
}

VkResult create_window_surface(VkInstance& instance, void *window, VkSurfaceKHR *surface) {
	return glfwCreateWindowSurface(instance, (GLFWwindow*)window, nullptr, surface);
}

std::unique_ptr<char[]> load_shader_spv(VkShaderModuleCreateInfo& shader, const char *name) {
	std::unique_ptr<char[]> uptr;

	FILE *f = fopen(name, "rb");
	if (!f) {
		fprintf(stderr, "Could not find shader \"%s\"\n", name);
		return uptr;
	}

	fseek(f, 0, SEEK_END);
	int sz = ftell(f);
	rewind(f);

	if (sz < 1) {
		fprintf(stderr, "Could not read from \"%s\"\n", name);
		return uptr;
	}

	uptr = std::make_unique<char[]>(sz);
	char *buf = uptr.get();
	fread(buf, 1, sz, f);
	fclose(f);

	memset(&shader, 0, sizeof(VkShaderModuleCreateInfo));
	shader.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shader.pCode = (uint32_t*)buf;
	shader.codeSize = sz;

	return uptr;
}

int start_app(Vulkan& vk, GLFWwindow *window) {
	Grid grid = {
		.rows = (vk.wnd_height + font_render.glyph_h - 1) / font_render.glyph_h,
		.cols = (vk.wnd_width + font_render.glyph_w - 1) / font_render.glyph_w,
	};

	View view = {
		.grid = &grid,
		.text = &text,
		.highlighter = &syntax,
		.font_render_idx = 0
	};

	int res = vk.upload_glyphsets(font_face, &font_render, 1);
	if (res != 0) return res;

	res = vk.render_and_upload_views(&view, 1, &font_render);
	if (res != 0) return res;

	res = vk.create_descriptor_set();
	if (res != 0) return res;

	res = vk.construct_pipeline();
	if (res != 0) return res;

	bool needs_resubmit = true;

	while (!glfwWindowShouldClose(window)) {
		glfwWaitEventsTimeout(0.5);

		int w, h;
		glfwGetFramebufferSize(window, &w, &h);
		if (w != vk.wnd_width || h != vk.wnd_height) {
			vk.recreate_swapchain(w, h);
			needs_resubmit = true;
		}

		if (needs_resubmit) {
			res = vk.update_command_buffers();
			if (res != 0) return res;

			needs_resubmit = false;
		}

		res = vk.render();
	}

	return res;
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {

}

int main(int argc, char **argv) {
	atexit([](){ft_quit();});

	font_face = load_font_face(DEFAULT_FONT_PATH);
	if (!font_face)
		return 1;

	// TODO: Use system DPI
	font_render = size_up_font_render(font_face, 10, 96, 96);

	syntax.colors[0] = 0x080808ff;
	syntax.colors[1] = 0xf0f0f0ff;

	VkShaderModuleCreateInfo vertex_buf, fragment_buf;

	std::unique_ptr<char[]> vertex_uptr = load_shader_spv(vertex_buf, "vertex.spv");
	if (!vertex_uptr)
		return 2;

	std::unique_ptr<char[]> fragment_uptr = load_shader_spv(fragment_buf, "fragment.spv");
	if (!fragment_uptr)
		return 2;

	int fd = open("vulkan.cpp", O_RDONLY);
	struct stat st;
	fstat(fd, &st);
	int64_t size = st.st_size;

	text.data = (char*)mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);
	text.total_size = size;

	text.enumerate_newlines();

	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	GLFWmonitor *monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode *vid_mode = glfwGetVideoMode(monitor);
	//int width = vid_mode->width, height = vid_mode->height;
	int width = 800, height = 600;

	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	GLFWwindow *window = glfwCreateWindow(width, height, "Mash", nullptr, nullptr);
	if (!window) {
		fprintf(stderr, "Failed to create GLFW window\n");
		glfwTerminate();
		return 3;
	}

	Vulkan vk;
	vk.glfw_monitor = (void*)monitor;
	vk.glfw_window = (void*)window;

	int res = init_vulkan(vk, vertex_buf, fragment_buf, width, height);
	if (res == 0)
		res = start_app(vk, window);

	munmap(text.data, text.total_size);
	close(fd);

	vk.close();
	glfwDestroyWindow(window);
	glfwTerminate();
	return res;
}
