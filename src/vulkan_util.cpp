#include "vulkan_util.h"

#include <SDL_vulkan.h>
#include <Util/log.h>
#include <debug.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_core.h>

constexpr bool log_file_vulkanutil = true;  // disable it to disable logging

void Graphics::Vulkan::vulkanCreate(Emulator::WindowCtx* ctx) {
    Emulator::VulkanExt ext;
    vulkanGetInstanceExtensions(&ext);

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pNext = nullptr;
    app_info.pApplicationName = "shadps4";
    app_info.applicationVersion = 1;
    app_info.pEngineName = "shadps4";
    app_info.engineVersion = 1;
    app_info.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo inst_info{};
    inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    inst_info.pNext = nullptr;
    inst_info.flags = 0;
    inst_info.pApplicationInfo = &app_info;
    inst_info.enabledExtensionCount = ext.required_extensions.size();
    inst_info.ppEnabledExtensionNames = ext.required_extensions.data();
    inst_info.enabledLayerCount = 0;
    inst_info.ppEnabledLayerNames = nullptr;

    VkResult result = vkCreateInstance(&inst_info, nullptr, &ctx->m_graphic_ctx.m_instance);
    if (result == VK_ERROR_INCOMPATIBLE_DRIVER) {
        LOG_CRITICAL_IF(log_file_vulkanutil, "Can't find an compatiblie vulkan driver\n");
        std::exit(0);
    } else if (result != VK_SUCCESS) {
        LOG_CRITICAL_IF(log_file_vulkanutil, "Can't create an vulkan instance\n");
        std::exit(0);
    }

    if (SDL_Vulkan_CreateSurface(ctx->m_window, ctx->m_graphic_ctx.m_instance, &ctx->m_surface) == SDL_FALSE) {
        LOG_CRITICAL_IF(log_file_vulkanutil, "Can't create an vulkan surface\n");
        std::exit(0);
    }

    // TODO i am not sure if it's that it is neccesary or if it needs more
    std::vector<const char*> device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_EXT_DEPTH_CLIP_ENABLE_EXTENSION_NAME,
                                                  VK_EXT_COLOR_WRITE_ENABLE_EXTENSION_NAME, "VK_KHR_maintenance1"};

    ctx->m_surface_capabilities = new Emulator::VulkanSurfaceCapabilities{};
    Emulator::VulkanQueues queues;

    vulkanFindCompatiblePhysicalDevice(ctx->m_graphic_ctx.m_instance, ctx->m_surface, device_extensions, ctx->m_surface_capabilities,
                                       &ctx->m_graphic_ctx.m_physical_device, &queues);

    if (ctx->m_graphic_ctx.m_physical_device == nullptr) {
        LOG_CRITICAL_IF(log_file_vulkanutil, "Can't find compatible vulkan device\n");
        std::exit(0);
    }

    VkPhysicalDeviceProperties device_properties{};
    vkGetPhysicalDeviceProperties(ctx->m_graphic_ctx.m_physical_device, &device_properties);

   LOG_INFO_IF(log_file_vulkanutil, "GFX device to be used : {}\n", device_properties.deviceName);

   ctx->m_graphic_ctx.m_device = vulkanCreateDevice(ctx->m_graphic_ctx.m_physical_device, ctx->m_surface, &ext, queues, device_extensions);
   if (ctx->m_graphic_ctx.m_device == nullptr) {
        LOG_CRITICAL_IF(log_file_vulkanutil, "Can't create vulkan device\n");
        std::exit(0);
   }
}

VkDevice Graphics::Vulkan::vulkanCreateDevice(VkPhysicalDevice physical_device, VkSurfaceKHR surface, const Emulator::VulkanExt* r,
                                              const Emulator::VulkanQueues& queues, const std::vector<const char*>& device_extensions) {

   std::vector<VkDeviceQueueCreateInfo> queue_create_info(queues.family_count);
   std::vector<std::vector<float>> queue_priority(queues.family_count);
   uint32_t queue_create_info_num = 0;

   for (uint32_t i = 0; i < queues.family_count; i++) {
        if (queues.family_used[i] != 0) {
            for (uint32_t pi = 0; pi < queues.family_used[i]; pi++) {
                queue_priority[queue_create_info_num].push_back(1.0f);
            }

            queue_create_info[queue_create_info_num].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_info[queue_create_info_num].pNext = nullptr;
            queue_create_info[queue_create_info_num].flags = 0;
            queue_create_info[queue_create_info_num].queueFamilyIndex = i;
            queue_create_info[queue_create_info_num].queueCount = queues.family_used[i];
            queue_create_info[queue_create_info_num].pQueuePriorities = queue_priority[queue_create_info_num].data();

            queue_create_info_num++;
        }
   }

   VkPhysicalDeviceFeatures device_features{};
   //TODO add neccesary device features

   VkDeviceCreateInfo create_info{};
   create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
   create_info.pNext = nullptr;
   create_info.flags = 0;
   create_info.pQueueCreateInfos = queue_create_info.data();
   create_info.queueCreateInfoCount = queue_create_info_num;
   create_info.enabledLayerCount = 0;
   create_info.ppEnabledLayerNames = nullptr;
   create_info.enabledExtensionCount = device_extensions.size();
   create_info.ppEnabledExtensionNames = device_extensions.data();
   create_info.pEnabledFeatures = &device_features;

   VkDevice device = nullptr;

   vkCreateDevice(physical_device, &create_info, nullptr, &device);

   return device;
}
void Graphics::Vulkan::vulkanGetInstanceExtensions(Emulator::VulkanExt* ext) {
    u32 required_extensions_count = 0;
    u32 available_extensions_count = 0;
    u32 available_layers_count = 0;
    auto result = SDL_Vulkan_GetInstanceExtensions(&required_extensions_count, nullptr);

    ext->required_extensions = std::vector<const char*>(required_extensions_count);

    result = SDL_Vulkan_GetInstanceExtensions(&required_extensions_count, ext->required_extensions.data());

    vkEnumerateInstanceExtensionProperties(nullptr, &available_extensions_count, nullptr);

    ext->available_extensions = std::vector<VkExtensionProperties>(available_extensions_count);

    vkEnumerateInstanceExtensionProperties(nullptr, &available_extensions_count, ext->available_extensions.data());

    vkEnumerateInstanceLayerProperties(&available_layers_count, nullptr);
    ext->available_layers = std::vector<VkLayerProperties>(available_layers_count);
    vkEnumerateInstanceLayerProperties(&available_layers_count, ext->available_layers.data());

    for (const char* ext : ext->required_extensions) {
        LOG_INFO_IF(log_file_vulkanutil, "Vulkan required extension  = {}\n", ext);
    }

    for (const auto& ext : ext->available_extensions) {
        LOG_INFO_IF(log_file_vulkanutil, "Vulkan available extension: {}, version = {}\n", ext.extensionName, ext.specVersion);
    }

    for (const auto& l : ext->available_layers) {
        LOG_INFO_IF(log_file_vulkanutil, "Vulkan available layer: {}, specVersion = {}, implVersion = {}, {}\n", l.layerName, l.specVersion,
                    l.implementationVersion, l.description);
    }
}

void Graphics::Vulkan::vulkanFindCompatiblePhysicalDevice(VkInstance instance, VkSurfaceKHR surface,
                                                          const std::vector<const char*>& device_extensions,
                                                          Emulator::VulkanSurfaceCapabilities* out_capabilities, VkPhysicalDevice* out_device,
                                                          Emulator::VulkanQueues* out_queues) {
    u32 count_devices = 0;
    vkEnumeratePhysicalDevices(instance, &count_devices, nullptr);

    std::vector<VkPhysicalDevice> devices(count_devices);
    vkEnumeratePhysicalDevices(instance, &count_devices, devices.data());

    VkPhysicalDevice found_best_device = nullptr;
    Emulator::VulkanQueues found_best_queues;

    for (const auto& device : devices) {
        VkPhysicalDeviceProperties device_properties{};
        VkPhysicalDeviceFeatures2 device_features2{};

        vkGetPhysicalDeviceProperties(device, &device_properties);
        vkGetPhysicalDeviceFeatures2(device, &device_features2);
        if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            continue;  // we don't want integrated gpu for now .Later we will check the requirements and see what we can support (TODO fix me)
        }
        LOG_INFO_IF(log_file_vulkanutil, "Vulkan device: {}\n", device_properties.deviceName);

        auto qs = vulkanFindQueues(device, surface);

        vulkanGetSurfaceCapabilities(device, surface, out_capabilities);

        found_best_device = device;
        found_best_queues = qs;
    }
    *out_device = found_best_device;
    *out_queues = found_best_queues;
}

Emulator::VulkanQueues Graphics::Vulkan::vulkanFindQueues(VkPhysicalDevice device, VkSurfaceKHR surface) {
    Emulator::VulkanQueues qs;

    u32 queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

    qs.family_count = queue_family_count;

    u32 family = 0;
    for (auto& f : queue_families) {
        VkBool32 presentation_supported = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, family, surface, &presentation_supported);

        LOG_INFO_IF(log_file_vulkanutil, "queue family: {}, count = {}, present = {}\n", string_VkQueueFlags(f.queueFlags).c_str(), f.queueCount,
                    (presentation_supported == VK_TRUE ? "true" : "false"));
        for (uint32_t i = 0; i < f.queueCount; i++) {
            Emulator::VulkanQueueInfo info;
            info.family = family;
            info.index = i;
            info.is_graphics = (f.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
            info.is_compute = (f.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0;
            info.is_transfer = (f.queueFlags & VK_QUEUE_TRANSFER_BIT) != 0;
            info.is_present = (presentation_supported == VK_TRUE);

            qs.available.push_back(info);
        }

        qs.family_used.push_back(0);

        family++;
    }
    u32 index = 0;
    for (u32 i = 0; i < VULKAN_QUEUE_GRAPHICS_NUM; i++) {
        for (const auto& idx : qs.available) {
            if (idx.is_graphics) {
                qs.family_used[qs.available.at(index).family]++;
                qs.graphics.push_back(qs.available.at(index));
                qs.available.erase(qs.available.begin() + index);
                break;
            }
            index++;
        }
    }
    index = 0;
    for (u32 i = 0; i < VULKAN_QUEUE_COMPUTE_NUM; i++) {
        for (const auto& idx : qs.available) {
            if (idx.is_graphics) {
                qs.family_used[qs.available.at(index).family]++;
                qs.compute.push_back(qs.available.at(index));
                qs.available.erase(qs.available.begin() + index);
                break;
            }
            index++;
        }
    }
    index = 0;
    for (uint32_t i = 0; i < VULKAN_QUEUE_TRANSFER_NUM; i++) {
        for (const auto& idx : qs.available) {
            if (idx.is_graphics) {
                qs.family_used[qs.available.at(index).family]++;
                qs.transfer.push_back(qs.available.at(index));
                qs.available.erase(qs.available.begin() + index);
                break;
            }
            index++;
        }
    }
    index = 0;
    for (uint32_t i = 0; i < VULKAN_QUEUE_PRESENT_NUM; i++) {
        for (const auto& idx : qs.available) {
            if (idx.is_graphics) {
                qs.family_used[qs.available.at(index).family]++;
                qs.present.push_back(qs.available.at(index));
                qs.available.erase(qs.available.begin() + index);
                break;
            }
            index++;
        }
    }
    return qs;
}

void Graphics::Vulkan::vulkanGetSurfaceCapabilities(VkPhysicalDevice physical_device, VkSurfaceKHR surface,
                                                    Emulator::VulkanSurfaceCapabilities* surfaceCap) {
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surfaceCap->capabilities);

    uint32_t formats_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &formats_count, nullptr);

    surfaceCap->formats = std::vector<VkSurfaceFormatKHR>(formats_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &formats_count, surfaceCap->formats.data());

    uint32_t present_modes_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_modes_count, nullptr);

    surfaceCap->present_modes = std::vector<VkPresentModeKHR>(present_modes_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_modes_count, surfaceCap->present_modes.data());

    for (const auto& f : surfaceCap->formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceCap->is_format_srgb_bgra32 = true;
            break;
        }
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceCap->is_format_unorm_bgra32 = true;
            break;
        }
    }
}