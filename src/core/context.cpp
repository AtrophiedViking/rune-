#include "core/context.h"
#include "render/render_pass.h"
#include "core/config.h"
#include "core/window.h"
#include "core/state.h"

void instanceCreate(State* state) {
	uint32_t glfwExtensionCount;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	VkApplicationInfo appInfo{
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = state->config->windowTitle.c_str(),
		.applicationVersion = state->config->applicationVersion,
		.pEngineName = state->config->engineName.c_str(),
		.engineVersion = state->config->engineVersion,
		.apiVersion = state->config->apiVersion,
	};

	VkInstanceCreateInfo instanceCreateInfo{
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &appInfo,
		.enabledExtensionCount = glfwExtensionCount,
		.ppEnabledExtensionNames = glfwExtensions,
	};
	PANIC(vkCreateInstance(&instanceCreateInfo, nullptr, &state->context->instance), "Failed To Create Instance");
};
void instanceDestroy(State* state) {
	vkDestroyInstance(state->context->instance, nullptr);
};

bool deviceSuitabilityCheck(VkPhysicalDevice device) {
	return true;
};
void physicalDeviceSelect(State* state) {
	state->context->physicalDevice = VK_NULL_HANDLE;
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(state->context->instance, &deviceCount, nullptr);
	PANIC(deviceCount == 0, "Failed To Find Gpu!");


	VkPhysicalDevice devices[]{ {} };
	PANIC(vkEnumeratePhysicalDevices(state->context->instance, &deviceCount, devices), "Failed To Enumerate Physical Devices");
	for (const auto& device : devices) {
		if (deviceSuitabilityCheck(device)) {
			state->context->physicalDevice = device;
			state->config->msaaSamples = getMaxUsableSampleCount(state);
			break;
		};
	};
	PANIC(!state->context->physicalDevice, "Failed To Select Device");
};
void queueFamilySelect(State* state) {
	uint32_t count;
	vkGetPhysicalDeviceQueueFamilyProperties(state->context->physicalDevice, &count, NULL);
	VkQueueFamilyProperties* queueFamilies = (VkQueueFamilyProperties*)malloc(count * sizeof(VkQueueFamilyProperties));
	vkGetPhysicalDeviceQueueFamilyProperties(state->context->physicalDevice, &count, queueFamilies);
	PANIC(!queueFamilies, "Queue Families Returned Null")

		for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < count; queueFamilyIndex++) {
			VkQueueFamilyProperties properties = queueFamilies[queueFamilyIndex];
			if ((properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) && glfwGetPhysicalDevicePresentationSupport(state->context->instance, state->context->physicalDevice, queueFamilyIndex)) {
				state->context->queueFamilyIndex = queueFamilyIndex;
				break;
			};
		};
	PANIC(state->context->queueFamilyIndex == UINT32_MAX, "Failed To Find Queue Family");
	free(queueFamilies);
};

void deviceCreate(State* state) {
	physicalDeviceSelect(state);
	queueFamilySelect(state);
	float queuePriority = 1.0f;
	VkDeviceQueueCreateInfo deviceQueueInfo{
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = state->context->queueFamilyIndex,
		.queueCount = 1,
		.pQueuePriorities = &queuePriority,
	};
	VkDeviceQueueCreateInfo deviceQueueInfos[]{ {} };

	const char* deviceExtensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	VkPhysicalDeviceFeatures deviceFeatures{
		.sampleRateShading = VK_TRUE,
		.samplerAnisotropy = VK_TRUE,
	};
	VkDeviceCreateInfo deviceInfo{
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos = &deviceQueueInfo,
		.enabledExtensionCount = 1,
		.ppEnabledExtensionNames = &deviceExtensions,
		.pEnabledFeatures = &deviceFeatures,
	};
	PANIC(vkCreateDevice(state->context->physicalDevice, &deviceInfo, nullptr, &state->context->device), "Failed To Create Device");
	vkGetDeviceQueue(state->context->device, state->context->queueFamilyIndex, 0, &state->context->queue);
	vkGetDeviceQueue(state->context->device, state->context->presentFamilyIndex, 0, &state->context->presentQueue);
	printf("device created = %p\n", (void*)state->context->device);

};
void deviceDestroy(State* state) {
	vkDestroyDevice(state->context->device, nullptr);
};

