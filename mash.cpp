// assets.h is auto-generated by prepare-shaders.py
#include "assets.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mash.h"

//#define DEFAULT_FONT_PATH "content/RobotoMono-Regular.ttf"
#define DEFAULT_FONT_PATH "content/Monaco_Regular.ttf"

static uint32_t cursor_color = 0;

static Font_Handle font_face = nullptr;
static Font_Render font_render = {0};

static File file = {0};
static Grid grid = {0};
static Formatter formatter = {0};

static Mouse_State mouse_state = {0};

static bool needs_resubmit = true;

const char **get_required_instance_extensions(uint32_t *n_inst_exts) {
	return glfwGetRequiredInstanceExtensions(n_inst_exts);
}

VkResult create_window_surface(VkInstance& instance, void *window, VkSurfaceKHR *surface) {
	return glfwCreateWindowSurface(instance, (GLFWwindow*)window, nullptr, surface);
}

int upload_glyphsets(Vulkan& vk, Font_Handle fh, Font_Render *renders, int n_renders) {
	if (!vk.glyphset_pool.size) {
		vk.glyphset_pool = vk.allocate_gpu_memory(GLYPHSET_POOL_SIZE);
		if (!vk.glyphset_pool.size)
			return __LINE__;
	}

	renders[0].buf = vk.glyphset_pool.staging_area;
	make_font_render(fh, renders[0]);

	return vk.push_to_gpu(vk.glyphset_pool, 0, renders[0].total_size);
}

int render_and_upload_views(Vulkan& vk, View *views, int n_views, Font_Render *renders) {
	if (!vk.grids_pool.size) {
		vk.grids_pool = vk.allocate_gpu_memory(GRIDS_POOL_SIZE);
		if (!vk.grids_pool.size)
			return __LINE__;
	}

	Cell *cells = (Cell*)vk.grids_pool.staging_area;
	View& v = views[0];
	v.grid->render_into(v.file, cells, v.formatter, mouse_state);

	int res = vk.push_to_gpu(vk.grids_pool, 0, v.grid->rows * v.grid->cols * sizeof(Cell));
	if (res != 0)
		return __LINE__;

	if (!vk.view_params || n_views > vk.view_param_cap) {
		int cap = VIEW_PARAMS_INITIAL_CAP;
		while (cap < n_views)
			cap *= 2;

		auto vps = new View_Params[cap];
		if (vk.view_params)
			delete[] vk.view_params;

		vk.view_params = vps;
		vk.view_param_cap = cap;
	}
	vk.n_view_params = n_views;

	for (int i = 0; i < vk.n_view_params; i++) {
		Font_Render *r = &renders[views[i].font_render_idx];

		vk.view_params[i] = {
			.view_origin = {0, 0},
			.view_size = {(uint32_t)vk.wnd_width, (uint32_t)vk.wnd_height},
			.cell_size = {(uint32_t)r->glyph_w, (uint32_t)r->glyph_h},
			.cursor = {v.grid->rel_cursor_col, v.grid->rel_cursor_row},
			.cursor_color = cursor_color,
			.columns = (uint32_t)v.grid->cols,
			.grid_cell_offset = 0,
			.glyphset_byte_offset = 0,
			.glyph_overlap_w = (uint32_t)r->overlap_w,
			.glyph_full_w = (uint32_t)r->glyph_img_w,
		};
	}

	return 0;
}

int start_app(Vulkan& vk, GLFWwindow *window) {
	auto resize_grid = [&vk](Grid& g) {
		g.rows = (vk.wnd_height + font_render.glyph_h - 1) / font_render.glyph_h;
		g.cols = (vk.wnd_width + font_render.glyph_w - 1) / font_render.glyph_w;
	};
	resize_grid(grid);

	View view = {
		.grid = &grid,
		.file = &file,
		.formatter = &formatter,
		.font_render_idx = 0
	};

	int res = upload_glyphsets(vk, font_face, &font_render, 1);
	if (res != 0) return res;

	res = render_and_upload_views(vk, &view, 1, &font_render);
	if (res != 0) return res;

	res = vk.create_descriptor_set();
	if (res != 0) return res;

	res = vk.construct_pipeline();
	if (res != 0) return res;

	needs_resubmit = true;

	while (!glfwWindowShouldClose(window)) {
		glfwWaitEventsTimeout(0.5);

		int w, h;
		glfwGetFramebufferSize(window, &w, &h);
		if (w != vk.wnd_width || h != vk.wnd_height) {
			vk.recreate_swapchain(w, h);
			needs_resubmit = true;
		}

		if (needs_resubmit) {
			resize_grid(grid);

			res = render_and_upload_views(vk, &view, 1, &font_render);
			if (res != 0) return res;

			res = vk.update_command_buffers();
			if (res != 0) return res;

			needs_resubmit = false;

			mouse_state.advance();
		}

		res = vk.render();
	}

	return res;
}

// TODO: Get font_render from font_renders[get_current_view()->font_render_idx] or something
//  also Get grid from get_current_view()->grid or something

// This function **doesn't** get called from a different thread, so we can let it access globals
static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
	int64_t move_down = 0;
	int64_t move_right = 0;

	if (action == GLFW_PRESS || action == GLFW_REPEAT) {
		if (key == GLFW_KEY_UP)
			move_down--;
		else if (key == GLFW_KEY_DOWN)
			move_down++;
		else if (key == GLFW_KEY_LEFT)
			move_right--;
		else if (key == GLFW_KEY_RIGHT)
			move_right++;
	}

	if (move_down != 0 || move_right != 0)
		grid.adjust_offsets(&file, move_down, move_right);

	needs_resubmit = true;
}

static void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
	if (xoffset == 0.0 && yoffset == 0.0)
		return;

	int64_t move_down = 0;
	int64_t move_right = 0;

	double dx, dy;
	if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
		dx = yoffset;
		dy = xoffset;
	}
	else {
		dx = xoffset;
		dy = yoffset;
	}

	if (dy > 0.0)
		move_down = -1;
	else if (dy < 0.0)
		move_down = 1;

	if (dx > 0.0)
		move_right = -1;
	else if (dx < 0.0)
		move_right = 1;

	if (move_down != 0 || move_right != 0)
		grid.adjust_offsets(&file, move_down, move_right);

	needs_resubmit = true;
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
	bool left_pressed  = action == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_LEFT;
	bool right_pressed = action == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_RIGHT;

	mouse_state.left_flags  = (mouse_state.left_flags & ~1) | (left_pressed & 1);
	mouse_state.right_flags = (mouse_state.right_flags & ~1) | (right_pressed & 1);

	needs_resubmit = true;
}

static void cursor_callback(GLFWwindow *window, double xpos, double ypos) {
	mouse_state.x = (int)xpos / font_render.glyph_w;
	mouse_state.y = (int)ypos / font_render.glyph_h;

	if (mouse_state.left_flags & 3)
		needs_resubmit = true;
}

int main(int argc, char **argv) {
	const char *file_name = "vulkan.cpp";
	if (argc > 1)
		file_name = argv[1];

	atexit([](){ft_quit();});

	font_face = load_font_face(DEFAULT_FONT_PATH);
	if (!font_face)
		return 1;

	// TODO: Use system DPI
	font_render = size_up_font_render(font_face, 10, 96, 96);

	formatter.spaces_per_tab = 4;

	formatter.modes[0].fore_color_idx = 1;
	formatter.modes[0].glyphset = 2; // italic

	formatter.colors[0] = 0x080808ff;
	formatter.colors[1] = 0xf0f0f0ff;

	cursor_color = 0xf0f0f0ff;

	VkShaderModuleCreateInfo vertex_buf = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = sizeof(vertex_spv_data),
		.pCode = (uint32_t*)vertex_spv_data
	};
	VkShaderModuleCreateInfo fragment_buf = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = sizeof(fragment_spv_data),
		.pCode = (uint32_t*)fragment_spv_data
	};

	if (file.open(file_name) < 0)
		return 2;

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

	glfwSetKeyCallback(window, key_callback);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetCursorPosCallback(window, cursor_callback);

	Vulkan vk;
	vk.glfw_monitor = (void*)monitor;
	vk.glfw_window = (void*)window;

	int res = init_vulkan(vk, vertex_buf, fragment_buf, width, height);
	if (res == 0)
		res = start_app(vk, window);

	file.close();

	vk.close();
	glfwDestroyWindow(window);
	glfwTerminate();
	return res;
}
