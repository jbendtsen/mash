#include <vulkan/vulkan.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include "mash.h"

typedef unsigned int u32;

int prev_failif_line = 0;

#define FAIL_IF(cond, args...) \
prev_failif_line = __LINE__; \
if (cond) { \
	fprintf(stderr, args); \
	return __LINE__; \
}

#define DESTROY(func, handle, object, allocator) \
if (object) { \
	func(handle, object, allocator); \
	object = nullptr; \
}

VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback_func(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	fprintf(stderr, "prev_failif_line: %d\n  %s\n\n", prev_failif_line, pCallbackData->pMessage);
	return VK_FALSE;
}

void Vulkan::close() {
	vkDeviceWaitIdle(device);

	DESTROY(vkDestroySemaphore, device, sema_present, nullptr)
	DESTROY(vkDestroySemaphore, device, sema_render, nullptr)

	DESTROY(vkDestroyShaderModule, device, frag_shader, nullptr)

	DESTROY(vkDestroyFence, device, draw_fence, nullptr)
	DESTROY(vkDestroyFramebuffer, device, framebuffer, nullptr)
	DESTROY(vkDestroyImageView, device, swap_image_view, nullptr)

	DESTROY(vkDestroyCommandPool, device, cmd_pool, nullptr)

	DESTROY(vkDestroyDescriptorPool, device, dpool, nullptr)
	DESTROY(vkDestroyDescriptorSetLayout, device, ds_layout, nullptr)

	DESTROY(vkDestroyPipelineLayout, device, pl_layout, nullptr)
	DESTROY(vkDestroyPipeline, device, pipeline, nullptr)
	DESTROY(vkDestroyRenderPass, device, renderpass, nullptr)

	DESTROY(DestroySwapchainKHR, device, swapchain, nullptr)

	vkUnmapMemory(device, src_mem);

	DESTROY(vkFreeMemory, device, dst_mem, nullptr)
	DESTROY(vkFreeMemory, device, src_mem, nullptr)

	DESTROY(vkDestroyFence, device, misc_fence, nullptr)

	vkDestroyDevice(device, nullptr);

	DestroyDebugUtilsMessenger(instance, debug_callback, nullptr);

	DESTROY(vkDestroySurfaceKHR, instance, surface, nullptr)
	vkDestroyInstance(instance, nullptr);
}

VkResult Vulkan::create_instance(const char *app_name, const char *engine_name, const char **req_inst_exts, u32 n_inst_exts) {
	VkApplicationInfo app_info = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = app_name,
		.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
		.pEngineName = engine_name,
		.engineVersion = VK_MAKE_VERSION(1, 0, 0),
		.apiVersion = VK_API_VERSION_1_2
	};

	const char *layer_name = "VK_LAYER_KHRONOS_validation";

	VkInstanceCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &app_info,
		.enabledLayerCount = 1,
		.ppEnabledLayerNames = &layer_name,
		.enabledExtensionCount = n_inst_exts,
		.ppEnabledExtensionNames = req_inst_exts
	};

	return vkCreateInstance(&create_info, nullptr, &instance);
}

int find_graphics_queue(VkPhysicalDevice& gpu) {
	u32 n_queues = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(gpu, &n_queues, nullptr);
	if (n_queues == 0)
		return -1;

	auto qfp = (VkQueueFamilyProperties*)alloca(n_queues * sizeof(VkQueueFamilyProperties));
	vkGetPhysicalDeviceQueueFamilyProperties(gpu, &n_queues, qfp);

	int queue_index = -1;
	for (int i = 0; i < n_queues; i++) {
		if (qfp[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			queue_index = i;
			break;
		}
	}

	return queue_index;
}

VkResult Vulkan::create_device(const char **dev_exts, u32 n_dev_exts) {
	float priority = 0.0f;
	VkDeviceQueueCreateInfo queue_info = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = (u32)queue_index,
		.queueCount = 1,
		.pQueuePriorities = &priority
	};

	VkDeviceCreateInfo device_info = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos = &queue_info,
		.enabledExtensionCount = n_dev_exts,
		.ppEnabledExtensionNames = dev_exts
	};

	return vkCreateDevice(gpu, &device_info, nullptr, &device);
}

int Vulkan::load_khr_extensions() {
	memset(khr_table, 0, N_KHR_IDS * sizeof(PFN_vkVoidFunction));

	khr_table[ID_GetPhysicalDeviceSurfaceSupport] = vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceSupportKHR");
	khr_table[ID_GetPhysicalDeviceSurfaceCapabilities] = vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
	khr_table[ID_GetPhysicalDeviceSurfaceFormats] = vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceFormatsKHR");

	khr_table[ID_CreateDebugUtilsMessenger] = vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	khr_table[ID_DestroyDebugUtilsMessenger] = vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

	khr_table[ID_CreateSwapchain] = vkGetDeviceProcAddr(device, "vkCreateSwapchainKHR");
	khr_table[ID_DestroySwapchain] = vkGetDeviceProcAddr(device, "vkDestroySwapchainKHR");
	khr_table[ID_GetSwapchainImages] = vkGetDeviceProcAddr(device, "vkGetSwapchainImagesKHR");
	khr_table[ID_AcquireNextImage] = vkGetDeviceProcAddr(device, "vkAcquireNextImageKHR");
	khr_table[ID_QueuePresent] = vkGetDeviceProcAddr(device, "vkQueuePresentKHR");

	int missing = N_KHR_IDS;
	for (int i = 0; i < N_KHR_IDS; i++) {
		if (khr_table[i] != nullptr)
			missing--;
	}

	return missing;
}

VkResult Vulkan::setup_validation() {
	VkDebugUtilsMessengerCreateInfoEXT dbg_info = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
		.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
		.pfnUserCallback = debug_callback_func,
		.pUserData = nullptr
	};

	return CreateDebugUtilsMessenger(instance, &dbg_info, nullptr, &debug_callback);
}

int Vulkan::select_bgra8_surface_format() {
	u32 n_color_formats = 0;
	VkResult res = GetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &n_color_formats, nullptr);
	if (n_color_formats == 0 || res != VK_SUCCESS)
		return -1;

	auto colors = (VkSurfaceFormatKHR*)alloca(n_color_formats * sizeof(VkSurfaceFormatKHR));
	res = GetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &n_color_formats, colors);
	if (res != VK_SUCCESS)
		return -2;

	sf_color_fmt = {VK_FORMAT_UNDEFINED};
	for (int i = 0; i < n_color_formats; i++) {
		if (colors[i].format == VK_FORMAT_B8G8R8A8_UNORM || colors[i].format == VK_FORMAT_B8G8R8A8_SRGB) {
			sf_color_fmt = colors[i];
			break;
		}
	}

	return sf_color_fmt.format == VK_FORMAT_UNDEFINED ? -3 : 0;
}

VkResult Vulkan::create_swapchain() {
	VkSwapchainCreateInfoKHR swap_info = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.pNext = nullptr,
		.flags = 0,
		.surface = surface,
		.minImageCount = 1,
		.imageFormat = sf_color_fmt.format,
		.imageColorSpace = sf_color_fmt.colorSpace,
		.imageExtent = sf_caps.currentExtent,
		.imageArrayLayers = 1,
		.imageUsage = img_usage,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
		.preTransform = (VkSurfaceTransformFlagBitsKHR)sf_caps.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR,
		.clipped = VK_TRUE,
		.oldSwapchain = nullptr
	};

	return CreateSwapchainKHR(device, &swap_info, nullptr, &swapchain);
}

VkImageViewCreateInfo make_imageview_info(VkImage *image, VkFormat format, VkImageAspectFlagBits aspect) {
	VkImage&& img = image ? *image : (VkImage){};

	return {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.image = img,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = format,
		.components = (VkComponentMapping) {
			.r = VK_COMPONENT_SWIZZLE_R,
			.g = VK_COMPONENT_SWIZZLE_G,
			.b = VK_COMPONENT_SWIZZLE_B,
			.a = VK_COMPONENT_SWIZZLE_A
		},
		.subresourceRange = (VkImageSubresourceRange) {
			.aspectMask = aspect,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		}
	};
}

int Vulkan::create_command_pool_and_draw_buffers() {
	VkCommandPoolCreateInfo cpool_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = (u32)queue_index
	};

	VkResult res = vkCreateCommandPool(device, &cpool_info, nullptr, &cmd_pool);
	if (res != VK_SUCCESS)
		return -1;

	VkCommandBufferAllocateInfo cbuf_alloc_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = cmd_pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};

	res = vkAllocateCommandBuffers(device, &cbuf_alloc_info, &draw_buffer);
	return res != VK_SUCCESS ? -2 : 0;
}

VkResult Vulkan::create_renderpass() {
	VkAttachmentDescription attachments[] = {
		{ // Color attachment
			.flags          = 0,
			.format         = sf_color_fmt.format,
			.samples        = VK_SAMPLE_COUNT_1_BIT,
			.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
		}
	};

	VkAttachmentReference color_ref = {
		.attachment = 0,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	};

	VkSubpassDescription subpass = {
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.colorAttachmentCount = 1,
		.pColorAttachments = &color_ref
	};

	VkSubpassDependency dependencies[] = {
		{
			.srcSubpass      = VK_SUBPASS_EXTERNAL,
			.dstSubpass      = 0,
			.srcStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			.dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask   = VK_ACCESS_MEMORY_READ_BIT,
			.dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
		},
		{
			.srcSubpass      = 0,
			.dstSubpass      = VK_SUBPASS_EXTERNAL,
			.srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			.srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask   = VK_ACCESS_MEMORY_READ_BIT,
			.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
		}
	};

	VkRenderPassCreateInfo pass_info = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = attachments,
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.dependencyCount = 2,
		.pDependencies = dependencies
	};

	return vkCreateRenderPass(device, &pass_info, nullptr, &renderpass);
}

VkResult Vulkan::create_framebuffer() {
	VkFramebufferCreateInfo fb_info = {
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass = renderpass,
		.attachmentCount = 1,
		.pAttachments = &swap_image_view,
		.width = sf_caps.currentExtent.width,
		.height = sf_caps.currentExtent.height,
		.layers = 1
	};

	return vkCreateFramebuffer(device, &fb_info, nullptr, &framebuffer);
}

VkResult Vulkan::create_semaphores() {
	VkSemaphoreCreateInfo bake_sema = {};
	bake_sema.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkResult res = vkCreateSemaphore(device, &bake_sema, nullptr, &sema_present);
	if (res != VK_SUCCESS)
		return res;

	return vkCreateSemaphore(device, &bake_sema, nullptr, &sema_render);
}

void Vulkan::create_fences() {
	VkFenceCreateInfo fence_info = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT
	};

	vkCreateFence(device, &fence_info, nullptr, &misc_fence);
	vkCreateFence(device, &fence_info, nullptr, &draw_fence);
}

VkResult Vulkan::create_descriptor_pool() {
	VkDescriptorPoolSize ps_info = {
		.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1
	};

	VkDescriptorPoolCreateInfo dpool_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = 1,
		.poolSizeCount = 1,
		.pPoolSizes = &ps_info
	};

	return vkCreateDescriptorPool(device, &dpool_info, nullptr, &dpool);
}

int init_vulkan(Vulkan& vk, VkShaderModuleCreateInfo& vert_shader_buf, VkShaderModuleCreateInfo& frag_shader_buf, int width, int height) {
	vk.wnd_width = width;
	vk.wnd_height = height;

	u32 n_inst_exts = 0;
	const char **req_inst_exts = get_required_instance_extensions(&n_inst_exts);
		FAIL_IF(!req_inst_exts, "Could not find any Vulkan extensions\n")

	n_inst_exts++;
	const char **inst_exts = (const char**)alloca(n_inst_exts * sizeof(void*));
	memcpy(inst_exts, req_inst_exts, (n_inst_exts-1) * sizeof(void*));
	inst_exts[n_inst_exts-1] = "VK_EXT_debug_utils";

	VkResult res = vk.create_instance("Mash", "No Engine", inst_exts, n_inst_exts);
		FAIL_IF(res != VK_SUCCESS, "vkCreateInstance() failed (%d)\n", res)

	u32 n_gpus = 1;
	res = vkEnumeratePhysicalDevices(vk.instance, &n_gpus, &vk.gpu);
		FAIL_IF(res != VK_SUCCESS, "vkEnumeratePhysicalDevices() failed (%d)\n", res)

	vkGetPhysicalDeviceMemoryProperties(vk.gpu, &vk.gpu_mem);

	u32 n_dev_exts = 0;
	res = vkEnumerateDeviceExtensionProperties(vk.gpu, nullptr, &n_dev_exts, nullptr);
		FAIL_IF(n_dev_exts <= 0 || res != VK_SUCCESS, "Could not find any Vulkan device extensions (found %d, error %d)\n", n_dev_exts, res)

	void *dev_ext_buf = alloca(n_dev_exts * (sizeof(VkExtensionProperties) + sizeof(void*)));
	auto dev_ext_props = (VkExtensionProperties*)dev_ext_buf;
	auto dev_exts = (const char**)&dev_ext_props[n_dev_exts];

	res = vkEnumerateDeviceExtensionProperties(vk.gpu, nullptr, &n_dev_exts, dev_ext_props);
		FAIL_IF(res != VK_SUCCESS, "vkEnumerateDeviceExtensionProperties() failed (%d)\n", res)

	for (int i = 0; i < n_dev_exts; i++)
		dev_exts[i] = &dev_ext_props[i].extensionName[0];

	res = vk.create_device(dev_exts, n_dev_exts);
		FAIL_IF(res != VK_SUCCESS, "vkCreateDevice() failed (%d)\n", res)

	vk.queue_index = find_graphics_queue(vk.gpu);
		FAIL_IF(vk.queue_index < 0, "Could not find a suitable graphics queue\n")

	vkGetDeviceQueue(vk.device, vk.queue_index, 0, &vk.queue);

	res = vkCreateShaderModule(vk.device, &frag_shader_buf, nullptr, &vk.frag_shader);
		FAIL_IF(res != VK_SUCCESS, "vkCreateShaderModule() failed for fragment shader (%d)\n", res)

	int missing = vk.load_khr_extensions();
		FAIL_IF(missing > 0, "Error loading KHR extension methods (missing %d)\n", missing)

	res = vk.setup_validation();
		FAIL_IF(res != VK_SUCCESS, "vkCreateDebugUtilsMessengerEXT() failed (%d)\n", res)

	res = create_window_surface(vk.instance, vk.glfw_window, &vk.surface);
		FAIL_IF(res != VK_SUCCESS, "glfwCreateWindowSurface() failed (%d)\n", res)

	int failure = vk.select_bgra8_surface_format();
		FAIL_IF(failure != 0, "Could not find a suitable surface colour format (%d)\n", failure)

	// We do this because if we don't then Vulkan gets very mad at us for some reason
	// I get the fact that there are some kinds of VkSurfaceKHRs that can't be used with a swapchain or be "presented"
	//  but I'm pretty sure that only matters if this program gets run on a literal potato
	VkBool32 supports_presentation;
	res = vk.GetPhysicalDeviceSurfaceSupportKHR(vk.gpu, vk.queue_index, vk.surface, &supports_presentation);

	res = vk.GetPhysicalDeviceSurfaceCapabilitiesKHR(vk.gpu, vk.surface, &vk.sf_caps);
	vk.sf_caps.currentExtent.width = vk.wnd_width;
	vk.sf_caps.currentExtent.height = vk.wnd_height;
		FAIL_IF(res != VK_SUCCESS, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR() failed (%d)\n", res)

	res = vk.create_swapchain();
		FAIL_IF(res != VK_SUCCESS, "vkCreateSwapchainKHR() failed (%d)\n", res)

	u32 n_images = 1;
	res = vk.GetSwapchainImagesKHR(vk.device, vk.swapchain, &n_images, &vk.swap_image);
		FAIL_IF(n_images != 1 || res != VK_SUCCESS, "Could not acquire swapchain image (n_images=%d, res=%d)\n", n_images, res)

	VkImageViewCreateInfo iv_info = make_imageview_info(&vk.swap_image, vk.sf_color_fmt.format, VK_IMAGE_ASPECT_COLOR_BIT);
	res = vkCreateImageView(vk.device, &iv_info, nullptr, &vk.swap_image_view);
		FAIL_IF(res != VK_SUCCESS, "vkCreateImageView() failed (%d)\n", res)

	failure = vk.create_command_pool_and_draw_buffers();
		FAIL_IF(failure != 0, "create_command_pool_and_buffers() failed (%d)\n", res)

	res = vk.create_renderpass();
		FAIL_IF(res != VK_SUCCESS, "vkCreateRenderPass() failed (%d)\n", res)

	res = vk.create_framebuffer();
		FAIL_IF(res != VK_SUCCESS, "vkCreateFramebuffer() failed (%d)\n", res)

	res = vk.create_semaphores();
		FAIL_IF(res != VK_SUCCESS, "Failed to create Vulkan semaphores\n")

	vk.create_fences();

	res = vk.create_descriptor_pool();
		FAIL_IF(res != VK_SUCCESS, "vkCreateDescriptorPool() failed (%d)\n", res)

	return 0;
}

int get_memory_type(VkPhysicalDeviceMemoryProperties& gpu_mem, u32 mem_req_type_bits, u32 mem_flags) {
	for (int i = 0; i < gpu_mem.memoryTypeCount; i++) {
		if ((mem_req_type_bits & (1 << i)) && (gpu_mem.memoryTypes[i].propertyFlags & mem_flags) == mem_flags)
			return i;
	}

	return -1;
}

int place_buffer(VkDevice& device, VkDeviceMemory& memory, int offset, int size, int type, const char *name, VkBuffer *buf) {
	VkBufferCreateInfo buf_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = (VkDeviceSize)size,
		.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | (VkBufferUsageFlags)type
	};

	VkResult res = vkCreateBuffer(device, &buf_info, nullptr, buf);
		FAIL_IF(res != VK_SUCCESS, "Failed to create %s buffer [vkCreateBuffer() -> %d]\n", name, res)

	res = vkBindBufferMemory(device, *buf, memory, offset);
		FAIL_IF(res != VK_SUCCESS, "Failed to bind %s buffer to device memory [vkBindBufferMemory() -> %d]\n", name, res)

	return 0;
}

void copy_buffer_simple(VkCommandBuffer copy_cmd, VkBuffer src_buf, int src_offset, VkBuffer dst_buf, int dst_offset, int size) {
	VkBufferCopy copy_info = {
		.srcOffset = (VkDeviceSize)src_offset,
		.dstOffset = (VkDeviceSize)dst_offset,
		.size = (VkDeviceSize)size
	};
	vkCmdCopyBuffer(copy_cmd, src_buf, dst_buf, 1, &copy_info);
}

// TODO: Custom allocator to use with both glyphsets and grids

Memory_Pool allocate_gpu_memory(int size) {
	Memory_Pool pool = {0};

	VkBufferCreateInfo src_buf_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = (VkDeviceSize)size,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
	};

	res = vkCreateBuffer(device, &src_buf_info, nullptr, &src_buf);
		FAIL_IF(res != VK_SUCCESS, "Failed to create host buffer [vkCreateBuffer() -> %d]\n", res)

	VkMemoryRequirements src_mem_reqs;
	vkGetBufferMemoryRequirements(device, src_buf, &src_mem_reqs);

	int src_mem_type = get_memory_type(gpu_mem, src_mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		FAIL_IF(src_mem_type < 0, "Could not find a suitable memory type for the host buffer\n")

	src_allocd = src_mem_reqs.size;

	VkMemoryAllocateInfo src_alloc_info = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = src_mem_reqs.size,
		.memoryTypeIndex = (u32)src_mem_type
	};

	res = vkAllocateMemory(device, &src_alloc_info, nullptr, &src_mem);
		FAIL_IF(res != VK_SUCCESS, "Could not allocate host memory [vkAllocateMemory() -> %d]\n", res)

	res = vkMapMemory(device, src_mem, 0, src_alloc_info.allocationSize, 0, &staging_area);
		FAIL_IF(res != VK_SUCCESS, "Could not map host memory [vkMapMemory() -> %d]\n", res)

	res = vkBindBufferMemory(device, src_buf, src_mem, 0);
		FAIL_IF(res != VK_SUCCESS, "Could not bind host memory [vkBindBufferMemory() -> %d]\n", res)

	VkBufferCreateInfo dst_buf_info = src_buf_info;
	dst_buf_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	VkBuffer dst_buf;
	res = vkCreateBuffer(device, &dst_buf_info, nullptr, &dst_buf);
		FAIL_IF(res != VK_SUCCESS, "Failed to create device buffer [vkCreateBuffer() -> %d]\n", res)

	VkMemoryRequirements dst_mem_reqs;
	vkGetBufferMemoryRequirements(device, dst_buf, &dst_mem_reqs);

	int dst_mem_type = get_memory_type(gpu_mem, dst_mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		FAIL_IF(dst_mem_type < 0, "Could not find a suitable memory type for the device buffer\n")

	VkMemoryAllocateInfo dst_alloc_info = src_alloc_info;
	dst_alloc_info.allocationSize = dst_mem_reqs.size;
	dst_alloc_info.memoryTypeIndex = (u32)dst_mem_type;

	res = vkAllocateMemory(device, &dst_alloc_info, nullptr, &dst_mem);
		FAIL_IF(res != VK_SUCCESS, "Could not allocate device memory [vkAllocateMemory() -> %d]\n", res)
}

int Vulkan::upload_glyphsets(Font_Handle fh, Font_Render *renders, int n_renders) {
	if (!glyphset_buffer)
		
	return 0;
}

int Vulkan::render_and_upload_views(View *views, int n_views) {
	return 0;
}

int Vulkan::create_descriptor_set() {
	VkDescriptorSetLayoutBinding ds_bindings[] = {
		{
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
		},
		{
			.binding = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
		}
	};

	VkDescriptorSetLayoutCreateInfo ds_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = 2,
		.pBindings = ds_bindings
	};

	VkResult res = vkCreateDescriptorSetLayout(device, &ds_info, nullptr, &ds_layout);
		FAIL_IF(res != VK_SUCCESS, "vkCreateDescriptorSetLayout() failed (%d)\n", res)

	VkDescriptorSetAllocateInfo ds_alloc_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = dpool,
		.descriptorSetCount = 1,
		.pSetLayouts = &ds_layout
	};

	res = vkAllocateDescriptorSets(device, &ds_alloc_info, &desc_set);
		FAIL_IF(res != VK_SUCCESS, "vkAllocateDescriptorSets() failed (%d)\n", res)

	return 0;
}

int Vulkan::construct_pipeline() {
	VkPipelineLayoutCreateInfo pl_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &ds_layout
	};

	VkResult res = vkCreatePipelineLayout(device, &pl_info, nullptr, &pl_layout);
		FAIL_IF(res != VK_SUCCESS, "vkCreatePipelineLayout() failed (%d)\n", res)

	VkPipelineInputAssemblyStateCreateInfo asm_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP
	};

	VkPipelineRasterizationStateCreateInfo raster_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.lineWidth = 1.0f
	};

	VkPipelineColorBlendAttachmentState cblend_att = {
		.colorWriteMask = 0xf
	};

	VkPipelineColorBlendStateCreateInfo cblend_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &cblend_att
	};

	VkPipelineViewportStateCreateInfo vp_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1
	};

	VkDynamicState dyn_vars[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dyn_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = 2,
		.pDynamicStates = dyn_vars
	};

	VkPipelineMultisampleStateCreateInfo ms_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
	};

	VkPipelineShaderStageCreateInfo shader_stages[] = {
		{
			.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage  = VK_SHADER_STAGE_VERTEX_BIT,
			.module = vert_shader,
			.pName  = "main"
		},
		{
			.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = frag_shader,
			.pName  = "main"
		}
	};

	VkGraphicsPipelineCreateInfo pipe_info = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = 2,
		.pStages = shader_stages,
		.pInputAssemblyState = &asm_info,
		.pViewportState = &vp_info,
		.pRasterizationState = &raster_info,
		.pMultisampleState = &ms_info,
		.pColorBlendState = &cblend_info,
		.pDynamicState = &dyn_info,
		.layout = pl_layout,
		.renderPass = renderpass
	};

	res = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipe_info, nullptr, &pipeline);
		FAIL_IF(res != VK_SUCCESS, "vkCreateGraphicsPipelines() failed (%d)\n", res)

	return 0;
}

int Vulkan::update_command_buffers() {
	VkDescriptorBufferInfo grid_buf_info = {
		.buffer = grids_buf,
		.offset = 0,
		.range = (VkDeviceSize)grids_size
	};
	VkDescriptorBufferInfo glyphset_buf_info = {
		.buffer = glyphsets_buf,
		.offset = 0,
		.range = (VkDeviceSize)glyphsets_size
	};

	VkWriteDescriptorSet write_info[] = {
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = desc_set,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = &grid_buf_info
		},
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = desc_set,
			.dstBinding = 1,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = &glyphset_buf_info
		}
	};

	vkUpdateDescriptorSets(device, 2, write_info, 0, nullptr);

	VkCommandBufferBeginInfo cbuf_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
	};

	VkClearValue clear_value = {
		.color = { { 0.0f, 0.0f, 0.2f, 1.0f } }
	};

	VkRenderPassBeginInfo rp_info = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = renderpass,
		.framebuffer = framebuffer,
		.renderArea = {
			.offset = { .x = 0, .y = 0 },
			.extent = sf_caps.currentExtent
		},
		.clearValueCount = 1,
		.pClearValues = &clear_value
	};

	vkBeginCommandBuffer(draw_buffer, &cbuf_info);
	vkCmdBeginRenderPass(draw_buffer, &rp_info, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindDescriptorSets(draw_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pl_layout, 0, 1, &desc_set, 0, nullptr);
	vkCmdBindPipeline(draw_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	{
		VkRect2D *scissor = &rp_info.renderArea;

		VkViewport viewport = {
			.width = (float)sf_caps.currentExtent.width,
			.height = (float)sf_caps.currentExtent.height,
			.minDepth = 0.0f,
			.maxDepth = 1.0f
		};

		vkCmdSetViewport(draw_buffer, 0, 1, &viewport);
		vkCmdSetScissor(draw_buffer, 0, 1, scissor);

		View_Params view_params = views[i].params;
		vkCmdPushConstants(draw_buffer, pl_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(View_Params), &view_params);

		vkCmdDraw(draw_buffer, 4, 1, 0, 1);
	}

	vkCmdEndRenderPass(draw_buffer);
	vkEndCommandBuffer(draw_buffer);

	wait_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	submit_info = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &sema_present,
		.pWaitDstStageMask = &wait_stage_mask,
		.commandBufferCount = 1,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &sema_render
	};

	present_info = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &sema_render,
		.swapchainCount = 1,
		.pSwapchains = &swapchain
	};

	return 0;
}

int Vulkan::recreate_swapchain(int width, int height) {
	vkDeviceWaitIdle(device);

	wnd_width = width;
	wnd_height = height;
	sf_caps.currentExtent.width = width;
	sf_caps.currentExtent.height = height;

	vkDestroyFramebuffer(device, framebuffer, nullptr);
	vkDestroyImageView(device, swap_image_view, nullptr);
	vkResetCommandBuffer(draw_buffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

	vkDestroySwapchainKHR(device, swapchain, nullptr);

	VkResult res = create_swapchain();
		FAIL_IF(res != VK_SUCCESS, "vkCreateSwapchainKHR() failed (%d)\n", res)

	u32 n_images = 1;
	res = GetSwapchainImagesKHR(device, swapchain, &n_images, &swap_image);
		FAIL_IF(res != VK_SUCCESS, "Could not acquire swapchain images (%d)\n", res)

	res = create_swapchain_imageview();
		FAIL_IF(res != VK_SUCCESS, "vkCreateImageView() failed (%d)\n", res)

	res = create_framebuffer();
		FAIL_IF(res != VK_SUCCESS, "vkCreateFramebuffer() failed (%d)\n", res)

	int failure = update_command_buffers();
	if (failure != 0)
		return failure;

	return 0;
}

int Vulkan::render() {
	int idx;
	VkResult res = AcquireNextImageKHR(device, swapchain, -1, sema_present, NULL, (u32*)&idx);
		FAIL_IF(res != VK_SUCCESS, "vkAcquireNextImageKHR() failed (%d)\n", res)

	res = vkWaitForFences(device, 1, &draw_fence, VK_TRUE, -1);
		FAIL_IF(res != VK_SUCCESS, "vkWaitForFences() failed (%d)\n", res)

	res = vkResetFences(device, 1, &draw_fence);
		FAIL_IF(res != VK_SUCCESS, "vkResetFences() failed (%d)\n", res)

	submit_info.pCommandBuffers = &draw_buffer;
	res = vkQueueSubmit(queue, 1, &submit_info, draw_fence);
		FAIL_IF(res != VK_SUCCESS, "vkQueueSubmit() failed (%d)\n", res)

	present_info.pImageIndices = (u32*)&idx;
	res = QueuePresentKHR(queue, &present_info);
		FAIL_IF(res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR, "vkQueuePresentKHR() failed (%d)\n", res)

	return 0;
}
