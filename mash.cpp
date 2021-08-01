#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <memory>

#include "mash.h"

typedef unsigned char u8;
typedef unsigned int u32;

const char **get_required_instance_extensions(u32 *n_inst_exts) {
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
	shader.pCode = (u32*)buf;
	shader.codeSize = sz;

	return uptr;
}

int start_app(Vulkan& vk, GLFWwindow *window) {
	res = vk.create_descriptor_set();
	if (res != 0) return res;

	res = vk.construct_pipeline();
	if (res != 0) return res;

	while (!glfwWindowShouldClose(window)) {
		glfwWaitEventsTimeout(0.5);

		int w, h;
		glfwGetFramebufferSize(window, &w, &h);
		if (w != vk.wnd_width || h != vk.wnd_height)
			vk.recreate_swapchain(w, h);

		res = vk.update_command_buffers();
		if (res != 0) return res;

		res = vk.render();
	}

	return res;
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {

}

int main(int argc, char **argv) {
	VkShaderModuleCreateInfo vertex_buf, fragment_buf;

	std::unique_ptr<char[]> vertex_uptr = load_shader_spv(vertex_buf, "vertex.spv");
	if (!vertex_uptr)
		return 1;

	std::unique_ptr<char[]> fragment_uptr = load_shader_spv(fragment_buf, "fragment.spv");
	if (!fragment_uptr)
		return 1;

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
		return -1;
	}

	Vulkan vk;
	vk.glfw_monitor = (void*)monitor;
	vk.glfw_window = (void*)window;

	int res = init_vulkan(vk, compute_buf, width, height);
	if (res == 0)
		res = start_app(vk, window);

	vk.close();
	glfwDestroyWindow(window);
	glfwTerminate();
	return res;
}
