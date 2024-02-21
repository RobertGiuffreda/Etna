#include "device.h"

#include "renderer/src/utilities/vkinit.h"
#include "renderer/src/renderer.h"

#include "data_structures/dynarray.h"
#include "memory/etmemory.h"
#include "core/logger.h"
#include "core/etstring.h"

// TODO: This currently picks the first device that matches requirements.
// Implement logic to further search for a better fit GPU.

typedef enum qfi_bits {
    QFI_GRAPHICS = 0x0001,
    QFI_TRANSFER = 0x0002,
    QFI_COMPUTE = 0x0004,
    QFI_PRESENTATION = 0x0008,
} qfi_bits;

typedef struct physical_device_requirements {
    u32 device_extension_count;
    const char** device_extensions;
    // Required features
    // Features
    b8 sampler_anisotropy;
    b8 int64;
    b8 int16;

    // Vulkan12Features
    b8 buffer_device_address;
    b8 descriptor_indexing;
    b8 shaderUniformBufferArrayNonUniformIndexing;
    b8 shaderSampledImageArrayNonUniformIndexing;
    b8 shaderStorageBufferArrayNonUniformIndexing;
    b8 shaderStorageImageArrayNonUniformIndexing;
    b8 descriptorBindingUniformBufferUpdateAfterBind;
    b8 descriptorBindingSampledImageUpdateAfterBind;
    b8 descriptorBindingStorageImageUpdateAfterBind;
    b8 descriptorBindingStorageBufferUpdateAfterBind;
    b8 descriptorBindingUpdateUnusedWhilePending;
    b8 descriptorBindingPartiallyBound;
    b8 descriptorBindingVariableDescriptorCount;
    b8 runtimeDescriptorArray;

    // Vulkan13Features
    b8 dynamic_rendering;
    b8 synchronization2;
    b8 maintenance4;

    b8 graphics_capable;
    b8 presentation_capable;
    b8 compute_capable;
    b8 transfer_capable;

    // To be read after device_meets_requirements function
    // Graphics|presentation|compute|transfer
    i32 g_index;
    i32 p_index;
    i32 c_index;
    i32 t_index;

    // No default surface format or present mode.
    // The spec guarantees at least one of each
} gpu_reqs;

static b8 pick_physical_device(renderer_state* state, gpu_reqs* requirements, device* out_device);

static b8 device_meets_requirements(VkPhysicalDevice device, VkSurfaceKHR surface, gpu_reqs* requirements);

static u32 hamming_weight(u32 x);

b8 device_create(renderer_state* state, device* out_device) {
    // TODO: Make configurable from outside renderer
    u32 required_extension_count = 1;
    const char* required_extensions = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    gpu_reqs requirements = {
        .device_extension_count = 1,
        .device_extensions = &required_extensions,

        .sampler_anisotropy = true,
        .int64 = true,
        .int16 = true,

        .buffer_device_address = true,
        .descriptor_indexing = true,
        .shaderUniformBufferArrayNonUniformIndexing = true,
        .shaderSampledImageArrayNonUniformIndexing = true,
        .shaderStorageBufferArrayNonUniformIndexing = true,
        .shaderStorageImageArrayNonUniformIndexing = true,
        .descriptorBindingUniformBufferUpdateAfterBind = true,
        .descriptorBindingSampledImageUpdateAfterBind = true,
        .descriptorBindingStorageImageUpdateAfterBind = true,
        .descriptorBindingStorageBufferUpdateAfterBind = true,
        .descriptorBindingUpdateUnusedWhilePending = true,
        .descriptorBindingPartiallyBound = true,
        .descriptorBindingVariableDescriptorCount = true,
        .runtimeDescriptorArray = true,

        .dynamic_rendering = true,
        .synchronization2 = true,
        .maintenance4 = true,
        
        .graphics_capable = true,
        .presentation_capable = true,
        .compute_capable = true,
        .transfer_capable = true};
    if (!pick_physical_device(state, &requirements, out_device)) {
        ETFATAL("Unable to select gpu.");
        return false;
    }

    // Get memory properties from gpu
    VkPhysicalDeviceMemoryProperties2 props = init_physical_device_memory_properties2();
    vkGetPhysicalDeviceMemoryProperties2(out_device->gpu, &props);
    out_device->gpu_memory_props = props.memoryProperties;

    u32 queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties2(out_device->gpu, &queue_family_count, 0);
    VkQueueFamilyProperties2* qf_props = 
        etallocate(sizeof(VkQueueFamilyProperties2) * queue_family_count, MEMORY_TAG_RENDERER);
    for (u32 i = 0; i < queue_family_count; ++i) {
        qf_props[i].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
        qf_props[i].pNext = 0;
    }
    vkGetPhysicalDeviceQueueFamilyProperties2(out_device->gpu, &queue_family_count, qf_props);

    // Create bitmasks for each possible queue family
    u32* qfi_flags = etallocate(sizeof(u32) * queue_family_count, MEMORY_TAG_RENDERER);
    etzero_memory(qfi_flags, sizeof(u32) * queue_family_count);
    u32 index_count = 0;
    i32* indices = etallocate(sizeof(i32) * queue_family_count, MEMORY_TAG_RENDERER);

    // TODO: Perform these OR opertations only if the capability is asked for.
    // Only really relevant when the renderer will act different based on gpu capability
    if ((qfi_flags[out_device->graphics_qfi] |= QFI_GRAPHICS) == QFI_GRAPHICS)
        indices[index_count++] = out_device->graphics_qfi;
        
    if ((qfi_flags[out_device->transfer_qfi] |= QFI_TRANSFER) == QFI_TRANSFER)
        indices[index_count++] = out_device->transfer_qfi;

    if ((qfi_flags[out_device->compute_qfi] |= QFI_COMPUTE) == QFI_COMPUTE)
        indices[index_count++] = out_device->compute_qfi;

    if ((qfi_flags[out_device->present_qfi] |= QFI_PRESENTATION) == QFI_PRESENTATION)
        indices[index_count++] = out_device->present_qfi;

    // For retrieving the amount of queue families
    u32* queue_counts = etallocate(sizeof(u32) * queue_family_count, MEMORY_TAG_RENDERER);

    VkDeviceQueueCreateInfo* queue_cinfos = 
        etallocate(sizeof(VkDeviceQueueCreateInfo) * index_count, MEMORY_TAG_RENDERER);
    f32 priorities[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    for (u32 i = 0; i < index_count; i++) {
        queue_cinfos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_cinfos[i].pNext = 0;
        queue_cinfos[i].flags = 0;

        // The number of flags set is the number of separate queues we
        // will attempt to get from the queue family. If there are not
        // enough queues available we will get the maximum.
        u32 flags_set = hamming_weight(qfi_flags[indices[i]]);
        u32 qf_queue_count = qf_props[indices[i]].queueFamilyProperties.queueCount;
        u32 queue_count = (qf_queue_count < flags_set) ? qf_queue_count : flags_set;

        // Save requested queue counts for use when fetching queues after
        queue_counts[indices[i]] = queue_count;

        // Set queue information
        queue_cinfos[i].queueCount = queue_count;
        queue_cinfos[i].queueFamilyIndex = indices[i];

        queue_cinfos[i].pQueuePriorities = priorities;
    }

    // Device features to enable
    VkPhysicalDeviceVulkan13Features enabled_features13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = 0,
        .dynamicRendering = requirements.dynamic_rendering,
        .synchronization2 = requirements.synchronization2,
        .maintenance4 = requirements.maintenance4};
    VkPhysicalDeviceVulkan12Features enabled_features12 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &enabled_features13,
        .bufferDeviceAddress = requirements.buffer_device_address,
        .descriptorIndexing = requirements.descriptor_indexing,
        .shaderUniformBufferArrayNonUniformIndexing = requirements.shaderUniformBufferArrayNonUniformIndexing,
        .shaderSampledImageArrayNonUniformIndexing = requirements.shaderSampledImageArrayNonUniformIndexing,
        .shaderStorageBufferArrayNonUniformIndexing = requirements.shaderStorageBufferArrayNonUniformIndexing,
        .shaderStorageImageArrayNonUniformIndexing = requirements.shaderStorageImageArrayNonUniformIndexing,
        .descriptorBindingUniformBufferUpdateAfterBind = requirements.descriptorBindingUniformBufferUpdateAfterBind,
        .descriptorBindingSampledImageUpdateAfterBind = requirements.descriptorBindingSampledImageUpdateAfterBind,
        .descriptorBindingStorageImageUpdateAfterBind = requirements.descriptorBindingStorageImageUpdateAfterBind,
        .descriptorBindingStorageBufferUpdateAfterBind = requirements.descriptorBindingStorageBufferUpdateAfterBind,
        .descriptorBindingUpdateUnusedWhilePending = requirements.descriptorBindingUpdateUnusedWhilePending,
        .descriptorBindingPartiallyBound = requirements.descriptorBindingPartiallyBound,
        .descriptorBindingVariableDescriptorCount = requirements.descriptorBindingVariableDescriptorCount,
        .runtimeDescriptorArray = requirements.runtimeDescriptorArray,
    };
    VkPhysicalDeviceFeatures2 enabled_features2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &enabled_features12,
        .features = {
            .samplerAnisotropy = requirements.sampler_anisotropy,
            .shaderInt16 = requirements.int16,
            .shaderInt64 = requirements.int64,
        },
    };

    VkDeviceCreateInfo device_cinfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &enabled_features2, 
        .flags = 0,
        .queueCreateInfoCount = index_count,
        .pQueueCreateInfos = queue_cinfos,
        .enabledExtensionCount = required_extension_count,
        .ppEnabledExtensionNames = &required_extensions,
        .pEnabledFeatures = 0,

        .enabledLayerCount = 0,     // Depricated
        .ppEnabledLayerNames = 0,   // Depricated
    };

    VK_CHECK(vkCreateDevice(
        out_device->gpu, 
        &device_cinfo,
        state->allocator,
        &out_device->handle));

    ETINFO("Device created.");

    // Stores the current index of the queue to be fetched for each.
    // If max has been reached the queue fetched is the zero index queue
    u32* curr_queue_indices = etallocate(sizeof(u32) * queue_family_count, MEMORY_TAG_RENDERER);
    etzero_memory(curr_queue_indices, sizeof(u32) * queue_family_count);

    // Graphics
    vkGetDeviceQueue(
        out_device->handle,
        out_device->graphics_qfi,
        curr_queue_indices[out_device->graphics_qfi]++,
        &out_device->graphics_queue);
    
    // Presentation
    b8 p_max_queues = (curr_queue_indices[out_device->present_qfi] == queue_counts[out_device->present_qfi]);
    vkGetDeviceQueue(
        out_device->handle,
        out_device->present_qfi,
        (p_max_queues) ? 0 : curr_queue_indices[out_device->present_qfi]++,
        &out_device->present_queue);

    b8 c_max_queues = (curr_queue_indices[out_device->compute_qfi] == queue_counts[out_device->compute_qfi]);
    vkGetDeviceQueue(
        out_device->handle,
        out_device->present_qfi,
        (c_max_queues) ? 0 : curr_queue_indices[out_device->compute_qfi]++,
        &out_device->compute_queue);

    b8 t_max_queues = (curr_queue_indices[out_device->transfer_qfi] == queue_counts[out_device->transfer_qfi]);
    vkGetDeviceQueue(
        out_device->handle,
        out_device->transfer_qfi,
        (t_max_queues) ? 0 : curr_queue_indices[out_device->transfer_qfi]++,
        &out_device->transfer_queue);

    ETINFO("Queues Obtained.");

    // Clean up allocated memory
    etfree(curr_queue_indices, sizeof(u32) * queue_family_count, MEMORY_TAG_RENDERER);
    etfree(queue_cinfos, sizeof(VkDeviceQueueCreateInfo) * index_count, MEMORY_TAG_RENDERER);
    etfree(queue_counts, sizeof(u32) * queue_family_count, MEMORY_TAG_RENDERER);
    etfree(indices, sizeof(i32) * queue_family_count, MEMORY_TAG_RENDERER);
    etfree(qfi_flags, sizeof(u32) * queue_family_count, MEMORY_TAG_RENDERER);
    etfree(qf_props, sizeof(VkQueueFamilyProperties2) * queue_family_count, MEMORY_TAG_RENDERER);
    return true;
}

void device_destroy(renderer_state* state, device* device) {
    device->graphics_queue = 0;
    device->present_queue = 0;
    device->compute_queue = 0;
    device->transfer_queue = 0;

    device->graphics_qfi = -1;
    device->present_qfi = -1;
    device->compute_qfi = -1;
    device->transfer_qfi = -1;

    vkDestroyDevice(device->handle, state->allocator);
}

static b8 pick_physical_device(renderer_state* state, gpu_reqs* requirements, device* out_device) {
    // Enumerate physical devicesp
    u32 physical_device_count = 0;
    vkEnumeratePhysicalDevices(state->instance, &physical_device_count, 0);
    VkPhysicalDevice* physical_devices = dynarray_create(physical_device_count, sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(state->instance, &physical_device_count, physical_devices);

    bool device_found = false;

    for (u32 i = 0; i < physical_device_count; ++i) {
        // Get physical device properties
        VkPhysicalDeviceProperties2 properties2 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            .pNext = 0};
        vkGetPhysicalDeviceProperties2(physical_devices[i], &properties2);
        VkPhysicalDeviceProperties* properties = &properties2.properties;

        // //TEMP: To get my laptop to not pick my integrated GPU
        // if (properties->deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        //     ETINFO("Device: %u is not a discrete GPU. Skipping", i);
        //     continue;
        // }
        // //TEMP: END

        if (!device_meets_requirements(physical_devices[i], state->surface, requirements)) {
            ETFATAL("Device Requirements not met.");
            continue;
        }

        // Requirements met so all good
        out_device->gpu = physical_devices[i];
        out_device->gpu_limits = properties->limits;
        out_device->graphics_qfi = requirements->g_index;
        out_device->compute_qfi = requirements->c_index;
        out_device->transfer_qfi = requirements->t_index;
        out_device->present_qfi = requirements->p_index;

        ETINFO("Device: %s", properties->deviceName);
        ETINFO(
            "Api Version: %d.%d.%d",
            VK_API_VERSION_MAJOR(properties->apiVersion),
            VK_API_VERSION_MINOR(properties->apiVersion),
            VK_API_VERSION_PATCH(properties->apiVersion));
        ETINFO(
            "Driver Version: %d.%d.%d",
            VK_API_VERSION_MAJOR(properties->driverVersion),
            VK_API_VERSION_MINOR(properties->driverVersion),
            VK_API_VERSION_PATCH(properties->driverVersion));

        ETINFO("Graphics index:     %d", out_device->graphics_qfi);
        ETINFO("Presentation index: %d", out_device->present_qfi);
        ETINFO("Compute index:      %d", out_device->compute_qfi);
        ETINFO("Transfer index:     %d", out_device->transfer_qfi);
        device_found = true;
    }
    
    // Clean up dynarrays
    dynarray_destroy(physical_devices);
    return device_found;
}

static b8 device_meets_requirements(VkPhysicalDevice device, VkSurfaceKHR surface, gpu_reqs* requirements) {
    // Get physical device features
    VkPhysicalDeviceVulkan13Features features13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = 0,
    };
    VkPhysicalDeviceVulkan12Features features12 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &features13,
    };
    VkPhysicalDeviceFeatures2 features2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &features12,
    };
    vkGetPhysicalDeviceFeatures2(device, &features2);
    VkPhysicalDeviceFeatures features = features2.features;

    b8 supported = true;

    // Features
    if (requirements->sampler_anisotropy && !features.samplerAnisotropy) {
        ETFATAL("Feature Sampler anisotropy is required & not supported on this device.");
        supported = false;
    }
    if (requirements->int16 && !features.shaderInt16) {
        ETFATAL("Feature shaderInt16 is required & not supported on this device.");
        supported = false;
    }
    if (requirements->int64 && !features.shaderInt64) {
        ETFATAL("Feature shaderInt64 is required & not supported on this device.");
        supported = false;
    }

    // Features 12
    if (requirements->buffer_device_address && !features12.bufferDeviceAddress) {
        ETFATAL("Feature Buffer Device Address is required & not supported on this device.");
        supported = false;
    }
    if (requirements->descriptor_indexing && !features12.descriptorIndexing) {
        ETFATAL("Feature descriptor indexing is required & not supported on this device.");
        supported = false;
    }
    if (requirements->shaderUniformBufferArrayNonUniformIndexing && !features12.shaderUniformBufferArrayNonUniformIndexing) { 
        ETFATAL("Feature shaderUniformBufferArrayNonUniformIndexing is required and not supported on this device.");
        supported = false;
    }
    if (requirements->shaderSampledImageArrayNonUniformIndexing && !features12.shaderSampledImageArrayNonUniformIndexing) { 
        ETFATAL("Feature shaderSampledImageArrayNonUniformIndexing is required and not supported on this device.");
        supported = false;
    }
    if (requirements->shaderStorageBufferArrayNonUniformIndexing && !features12.shaderStorageBufferArrayNonUniformIndexing) { 
        ETFATAL("Feature shaderStorageBufferArrayNonUniformIndexing is required and not supported on this device.");
        supported = false;
    }
    if (requirements->shaderStorageImageArrayNonUniformIndexing && !features12.shaderStorageImageArrayNonUniformIndexing) { 
        ETFATAL("Feature shaderStorageImageArrayNonUniformIndexing is required and not supported on this device.");
        supported = false;
    }
    if (requirements->descriptorBindingUniformBufferUpdateAfterBind && !features12.descriptorBindingUniformBufferUpdateAfterBind) { 
        ETFATAL("Feature descriptorBindingUniformBufferUpdateAfterBind is required and not supported on this device.");
        supported = false;
    }
    if (requirements->descriptorBindingSampledImageUpdateAfterBind && !features12.descriptorBindingSampledImageUpdateAfterBind) { 
        ETFATAL("Feature descriptorBindingSampledImageUpdateAfterBind is required and not supported on this device.");
        supported = false;
    }
    if (requirements->descriptorBindingStorageImageUpdateAfterBind && !features12.descriptorBindingStorageImageUpdateAfterBind) { 
        ETFATAL("Feature descriptorBindingStorageImageUpdateAfterBind is required and not supported on this device.");
        supported = false;
    }
    if (requirements->descriptorBindingStorageBufferUpdateAfterBind && !features12.descriptorBindingStorageBufferUpdateAfterBind) { 
        ETFATAL("Feature descriptorBindingStorageBufferUpdateAfterBind is required and not supported on this device.");
        supported = false;
    }
    if (requirements->descriptorBindingUpdateUnusedWhilePending && !features12.descriptorBindingUpdateUnusedWhilePending) { 
        ETFATAL("Feature descriptorBindingUpdateUnusedWhilePending is required and not supported on this device.");
        supported = false;
    }
    if (requirements->descriptorBindingPartiallyBound && !features12.descriptorBindingPartiallyBound) { 
        ETFATAL("Feature descriptorBindingPartiallyBound is required and not supported on this device.");
        supported = false;
    }
    if (requirements->descriptorBindingVariableDescriptorCount && !features12.descriptorBindingVariableDescriptorCount) { 
        ETFATAL("Feature descriptorBindingVariableDescriptorCount is required and not supported on this device.");
        supported = false;
    }
    if (requirements->runtimeDescriptorArray && !features12.runtimeDescriptorArray) {
        ETFATAL("Feature runtimeDescriptorArray is required and not supported on this device.");
        supported = false;
    }

    // Features 13
    if (requirements->dynamic_rendering && !features13.dynamicRendering) {
        ETFATAL("Feature Dynamic Rendering is required & not supported on this device.");
        supported = false;
    }
    if (requirements->synchronization2 && !features13.synchronization2) {
        ETFATAL("Feature Dynamic Rendering is required & not supported on this device.");
        supported = false;
    }
    if (requirements->maintenance4 && !features13.maintenance4) {
        ETFATAL("Feature maintenance4 is required & not supported on this device.");
        supported = false;
    }

    if (!supported) return false;

    // Enumerate supported extnesions
    u32 supported_extension_count = 0;
    vkEnumerateDeviceExtensionProperties(device, 0, &supported_extension_count, 0);
    VkExtensionProperties* supported_extensions = 
        dynarray_create(supported_extension_count, sizeof(VkExtensionProperties));
    vkEnumerateDeviceExtensionProperties(device, 0, &supported_extension_count, supported_extensions);
    
    // Verify required extensions are supported
    for (u32 i = 0; i < requirements->device_extension_count; ++i) {
        b8 found = false;
        for (u32 j = 0; j < supported_extension_count; ++j) {
            if (strs_equal(
                requirements->device_extensions[i], 
                supported_extensions[j].extensionName))
            {
                found = true;
                break;
            }
        }
        if (!found) {
            ETFATAL("Device extension '%s' not found.", requirements->device_extensions[i]);
            return false;
        }
    }
    // Clean up supported extensions dynarray
    dynarray_destroy(supported_extensions);

    // Get Queue family information
    u32 queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties2(device, &queue_family_count, 0);
    VkQueueFamilyProperties2* q_family_prop2s = 
        dynarray_create(queue_family_count, sizeof(VkQueueFamilyProperties2));
    for (u32 j = 0; j < queue_family_count; ++j) {
        q_family_prop2s[j].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
        q_family_prop2s[j].pNext = 0;
    }
    vkGetPhysicalDeviceQueueFamilyProperties2(device, &queue_family_count, q_family_prop2s);

    // Graphics, compute, transfer, presentation
    i32 g_index = -1, c_index = -1, t_index = -1, p_index = -1;
    for (i32 i = 0; i < queue_family_count; ++i) {
        VkQueueFamilyProperties* q_props = &q_family_prop2s[i].queueFamilyProperties;
        if ((g_index == -1) && (q_props->queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            g_index = i;

            // TODO: Consider using compute present queue when available
            // instead of just checking the graphics queue
            VkBool32 can_present = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &can_present);
            if (can_present) {
                p_index = i;
            }
        }

        // Dedicated Compute queue?
        VkQueueFlags compute_check = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
        if ((q_props->queueFlags & compute_check) == VK_QUEUE_COMPUTE_BIT) {
            c_index = i;
        }

        // Dedicated Transfer queue?
        /* Dedicated transfer queue will not have any of the other bits in transfer check set.
        * NOTE: My graphics card has VK_QUEUE_OPTICAL_FLOW_BIT_NV & VK_QUEUE_VIDEO_DECODE_BIT_KHR
        * & VK_QUEUE_VIDEO_ENCODE_BIT_KHR in separate queue families
        * from the dedicated transfer & I am basing my reasoning on that.
        */
        VkQueueFlags transfer_check = 
            VK_QUEUE_TRANSFER_BIT |
            VK_QUEUE_COMPUTE_BIT |
            VK_QUEUE_GRAPHICS_BIT |
            VK_QUEUE_VIDEO_DECODE_BIT_KHR |
#ifdef VK_ENABLE_BETA_EXTENSIONS
            VK_QUEUE_VIDEO_ENCODE_BIT_KHR |
#endif
            VK_QUEUE_OPTICAL_FLOW_BIT_NV;
        if ((q_props->queueFlags & transfer_check) == VK_QUEUE_TRANSFER_BIT) {
            t_index = i;
        }
    }
    // If a gpu has a grpahics queue family that does not support presentation.
    // Not common at all, but the spec does not offer guarantees on presentation ability of GPU
    if (p_index == -1) {
        for (i32 i = 0; i < queue_family_count; ++i) {
            VkQueueFamilyProperties* q_props = &q_family_prop2s[i].queueFamilyProperties;
            VkBool32 can_present = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &can_present);
            if (can_present) {
                // Take first available
                p_index = i;
                break;
            }
        }
    }
    // If dedicated compute queue family absent
    if (c_index == -1) {
        for (i32 i = 0; i < queue_family_count; ++i) {
            VkQueueFamilyProperties* q_props = &q_family_prop2s[i].queueFamilyProperties;
            if (q_props->queueFlags & VK_QUEUE_COMPUTE_BIT) {
                c_index = i;
            }
        }
    }
    // If dedicated transfer queue family absent 
    if (t_index == -1) {
        // NOTE: Queue with graphics bit is guaranteed to support transfer
        t_index = g_index;
    }
    // Clean up alloated memory
    dynarray_destroy(q_family_prop2s);

    // Display queue families with information
    b8 no_graphics = requirements->graphics_capable && (g_index == -1);
    b8 no_present = requirements->presentation_capable && (p_index == -1);
    b8 no_compute = requirements->compute_capable && (c_index == -1);
    b8 no_transfer = requirements->transfer_capable && (t_index == -1);
    if (no_graphics || no_present || no_compute || no_transfer) {
        if(no_graphics) ETINFO("Graphics queue family required & not found");
        if(no_present) ETINFO("Presentation queue family required & not found");
        if(no_compute) ETINFO("Compute queue family required & not found");
        if(no_transfer) ETINFO("Transfer queue family required & not found");
        return false;
    }

    requirements->g_index = g_index;
    requirements->p_index = p_index;
    requirements->c_index = c_index;
    requirements->t_index = t_index;
    return true;
}

static u32 hamming_weight(u32 x) {
#if defined(__GNUC__) || defined(__clang__)
return __builtin_popcount(x);
#else // Windows popcount intinsic is hardware specific so we do it Brian Kernighan's way.
    u32 count = 0;
    while (x) {
        x &= (x - 1);
        count++;
    }
    return count;
#endif
}