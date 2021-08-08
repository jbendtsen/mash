#pragma once

#include "font.h"
#include <cstdint>

constexpr uint64_t MAX_64 = -1;

struct Highlighter {
	constexpr int N_COLORS = 32;
	uint32_t colors[N_COLORS];
};

struct Text {
	char *data;
	int64_t total_size;

	int64_t file_begin;
	int64_t lines_down;

	int64_t *newlines;
	int nl_capacity;
	int nl_size;

	void enumerate_newlines();
};

struct Cell {
	uint32_t glyph;
	uint32_t modifier;
	uint32_t foreground;
	uint32_t background;
};

struct Grid {
	int rows;
	int cols;
	int64_t row_offset;
	int64_t col_offset;

	void render_into(Text& text, Cell *cells, Highlighter& syntax);
};

struct View {
	Grid *grid;
	Text *text;
	Highlighter *highlighter;
};

struct uvec2 {
	uint32_t x, y;
};

struct View_Params {
	uvec2 view_origin;
	uvec2 view_size;
	uvec2 cell_size;
	uint32_t grid_cell_offset;
	uint32_t glyphset_byte_offset;
	uint32_t glyph_overlap_w;
};

struct Memory_Pool {
	VkDeviceMemory dst_mem;
	VkDeviceMemory src_mem;
	VkBuffer dst_buf;
	VkBuffer src_buf;
	int size;
	VkResult result;
};

enum {
	ID_GetPhysicalDeviceSurfaceSupport = 0,
	ID_GetPhysicalDeviceSurfaceCapabilities,
	ID_GetPhysicalDeviceSurfaceFormats,
	ID_CreateDebugUtilsMessenger,
	ID_DestroyDebugUtilsMessenger,
	ID_CreateSwapchain,
	ID_DestroySwapchain,
	ID_GetSwapchainImages,
	ID_AcquireNextImage,
	ID_QueuePresent,
	N_KHR_IDS
};

struct Vulkan {
	const VkImageUsageFlags img_usage =
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
		VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	void *glfw_monitor = nullptr;
	void *glfw_window = nullptr;

	int wnd_width = 0;
	int wnd_height = 0;

	VkInstance instance = {0};
	VkPhysicalDevice gpu = {0};
	VkPhysicalDeviceMemoryProperties gpu_mem = {0};
	VkQueue queue = {0};
	VkDevice device = {0};

	VkDebugUtilsMessengerEXT debug_callback = {0};

	VkSurfaceKHR surface = {0};
	VkSurfaceFormatKHR sf_color_fmt = {VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
	VkSurfaceCapabilitiesKHR sf_caps = {0};

	VkSwapchainKHR swapchain = {0};
	VkRenderPass renderpass = {0};

	VkImage swap_image;
	VkImageView swap_image_view;
	VkFramebuffer framebuffer;
	VkCommandBuffer draw_buffer;
	VkFence draw_fence;

	VkCommandPool cmd_pool = {0};
	VkFence misc_fence = {0};

	VkSemaphore sema_present = {0};
	VkSemaphore sema_render = {0};

	int queue_index = 0;

	VkDescriptorPool dpool = {0};
	VkDescriptorSetLayout ds_layout = {0};
	VkDescriptorSet desc_set = {0};

	VkDeviceMemory src_mem = {0};
	VkBuffer src_buf = {0};
	void *staging_area = nullptr;
	int stage_total_size = 0;
	int stage_vertex_offset = 0;
	int stage_vertex_size = 0;
	int stage_index_offset = 0;
	int stage_index_size = 0;

	VkDeviceMemory dst_mem = {0};
	VkBuffer mvp_buf = {0};
	VkBuffer vert_buf = {0};
	VkBuffer index_buf = {0};

	VkShaderModule vert_shader = {0};
	VkShaderModule frag_shader = {0};

	VkPipelineStageFlags wait_stage_mask = 0;
	VkPipelineLayout pl_layout = {0};
	VkPipeline pipeline = {0};

	VkSubmitInfo submit_info = {};
	VkPresentInfoKHR present_info = {};

	PFN_vkVoidFunction khr_table[N_KHR_IDS] = {0};

	VkResult GetPhysicalDeviceSurfaceSupportKHR(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, VkSurfaceKHR surface, VkBool32* pSupported) {
		return ((PFN_vkGetPhysicalDeviceSurfaceSupportKHR)khr_table[ID_GetPhysicalDeviceSurfaceSupport])(physicalDevice, queueFamilyIndex, surface, pSupported);
	}
	VkResult GetPhysicalDeviceSurfaceCapabilitiesKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR *pSurfaceCapabilities) {
		return ((PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)khr_table[ID_GetPhysicalDeviceSurfaceCapabilities])(physicalDevice, surface, pSurfaceCapabilities);
	}
	VkResult GetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* pSurfaceFormatCount, VkSurfaceFormatKHR* pSurfaceFormats) {
		return ((PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)khr_table[ID_GetPhysicalDeviceSurfaceFormats])(physicalDevice, surface, pSurfaceFormatCount, pSurfaceFormats);
	}

	VkResult CreateDebugUtilsMessenger(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pCallback) {
		return ((PFN_vkCreateDebugUtilsMessengerEXT)khr_table[ID_CreateDebugUtilsMessenger])(instance, pCreateInfo, pAllocator, pCallback);
	}
	void DestroyDebugUtilsMessenger(VkInstance instance, VkDebugUtilsMessengerEXT callback, const VkAllocationCallbacks* pAllocator) {
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)khr_table[ID_DestroyDebugUtilsMessenger];
		if (func) func(instance, callback, pAllocator);
	}

	VkResult CreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain) {
		return ((PFN_vkCreateSwapchainKHR)khr_table[ID_CreateSwapchain])(device, pCreateInfo, pAllocator, pSwapchain);
	}
	void DestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks* pAllocator) {
		auto func = (PFN_vkDestroySwapchainKHR)khr_table[ID_DestroySwapchain];
		if (func) func(device, swapchain, pAllocator);
	}
	VkResult GetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain, uint32_t* pSwapchainImageCount, VkImage* pSwapchainImages) {
		return ((PFN_vkGetSwapchainImagesKHR)khr_table[ID_GetSwapchainImages])(device, swapchain, pSwapchainImageCount, pSwapchainImages);
	}
	VkResult AcquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore, VkFence fence, uint32_t* pImageIndex) {
		return ((PFN_vkAcquireNextImageKHR)khr_table[ID_AcquireNextImage])(device, swapchain, timeout, semaphore, fence, pImageIndex);
	}
	VkResult QueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) {
		return ((PFN_vkQueuePresentKHR)khr_table[ID_QueuePresent])(queue, pPresentInfo);
	}

	void close();
	VkResult create_instance(const char *app_name, const char *engine_name, const char **req_inst_exts, uint32_t n_inst_exts);
	VkResult create_device(const char **dev_exts, uint32_t n_inst_exts);
	int load_khr_extensions();
	VkResult setup_validation();
	int select_bgra8_surface_format();
	VkResult create_swapchain();
	VkResult create_swapchain_imageview();
	int create_command_pool_and_draw_buffers();
	VkResult create_renderpass();
	VkResult create_framebuffer();
	VkResult create_semaphores();
	void create_fences();
	VkResult create_descriptor_pool();

	int upload_glyphsets(Font_Handle fh, Font_Render *renders, int n_renders);
	int render_and_upload_views(View *views, int n_views);

	int create_descriptor_set();
	int construct_pipeline();
	int update_command_buffers();

	int recreate_swapchain(int width, int height);

	int render();
};

const char **get_required_instance_extensions(uint32_t *n_inst_exts);
VkResult create_window_surface(VkInstance& instance, void *window, VkSurfaceKHR *surface);

int init_vulkan(Vulkan& vk, VkShaderModuleCreateInfo& vert_shader_buf, VkShaderModuleCreateInfo& frag_shader_buf, int width, int height);

