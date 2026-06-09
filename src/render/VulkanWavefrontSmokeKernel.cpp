#include "render/VulkanWavefrontSmokeKernel.h"

#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
#include <vulkan/vulkan.h>
#endif

#include <cstdint>
#include <vector>

namespace render {
  bool VulkanWavefrontSmokeKernel::deviceAvailable() const {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    VkApplicationInfo applicationInfo{};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = "raytracer Vulkan wavefront smoke probe";
    applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    applicationInfo.pEngineName = "raytracer";
    applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    applicationInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &applicationInfo;

    VkInstance instance = VK_NULL_HANDLE;
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
      return false;
    }

    struct InstanceGuard {
      ~InstanceGuard() {
        if (instance) {
          vkDestroyInstance(instance, nullptr);
        }
      }

      VkInstance instance{VK_NULL_HANDLE};
    } guard;
    guard.instance = instance;

    std::uint32_t deviceCount = 0;
    if (vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr) != VK_SUCCESS ||
        deviceCount == 0) {
      return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount, VK_NULL_HANDLE);
    if (vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()) != VK_SUCCESS) {
      return false;
    }

    for (VkPhysicalDevice device : devices) {
      std::uint32_t queueFamilyCount = 0;
      vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
      if (queueFamilyCount == 0) {
        continue;
      }

      std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
      vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
      for (const VkQueueFamilyProperties& queueFamily : queueFamilies) {
        if ((queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0 && queueFamily.queueCount > 0) {
          return true;
        }
      }
    }
    return false;
#else
    return false;
#endif
  }
}
