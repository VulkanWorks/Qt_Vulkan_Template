#ifndef VKSTRUCTUREHELPER_H
#define VKSTRUCTUREHELPER_H

#include <QMatrix4x4>
#include <vulkan/vulkan.h>

struct QueueFamilyIndices {
	int graphicsFamily = -1;
	int presentFamily = -1;

	bool isComplete() {
		return graphicsFamily >= 0 &&
			presentFamily >= 0;
	}
};

struct UniformBufferObject {
	QMatrix4x4		model;
	QMatrix4x4		view;
	QMatrix4x4		proj;
};

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;

	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR>	presentModes;
};

#endif // VKSTRUCTUREHELPER_H
