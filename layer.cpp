#include "vulkan/vk_layer.h"
#include "vk_layer_table.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#undef VK_LAYER_EXPORT
#define VK_LAYER_EXPORT extern "C"

static instance_table_map gInstanceDispatch;
static device_table_map  gDeviceDispatch;

static constexpr char kEnvVariable[] = "VULKAN_DEVICE_INDEX";

static VkResult ChooseDevice(VkInstance                          instance,
                             const VkuInstanceDispatchTable*     dispatch,
                             const char* const                   env,
                             VkPhysicalDevice&                   outDevice)
{
    std::vector<VkPhysicalDevice> devices;
    uint32_t count = 0;
    VkResult result = dispatch->EnumeratePhysicalDevices(instance, &count, nullptr);

    if (result != VK_SUCCESS)
    {
        return result;
    }
    else if (count == 0)
    {
        outDevice = VK_NULL_HANDLE;
        return VK_SUCCESS;
    }

    devices.resize(count);
    result = dispatch->EnumeratePhysicalDevices(instance, &count, devices.data());

    if (result != VK_SUCCESS)
    {
        return result;
    }


    if (env && !strcmp(env, "list"))
    {
        for (size_t i = 0; i < devices.size(); ++i)
        {
            VkPhysicalDeviceProperties props;
            dispatch->GetPhysicalDeviceProperties(devices[i], &props);

            VkPhysicalDeviceDriverProperties driverProps{};
            driverProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
            VkPhysicalDeviceProperties2 extendedProps{};
            extendedProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            extendedProps.pNext = &driverProps;
            dispatch->GetPhysicalDeviceProperties2(devices[i], &extendedProps);

            fprintf(stderr, "CPU%zu:\n", i);
            fprintf(stderr, "        deviceName = %s\n", props.deviceName);
            fprintf(stderr, "        driverInfo = %s\n", driverProps.driverInfo);
        }
        exit(0);
    }

    static uint8_t deviceIndex = [&]() {
        unsigned long v = env && *env ? strtoul(env, nullptr, 10) : UINT8_MAX + 1;
        return v <= UINT8_MAX ? static_cast<uint8_t>(v) : UINT8_MAX;
    }();

    // Fall back GPU Discrete -> Integrated -> Virtual -> CPU
    int8_t discrete_idx = -1;
    int8_t integrated_idx = -1;
    int8_t virtual_idx = -1;
    int8_t cpu_idx = -1;

    if (deviceIndex < 0 || deviceIndex >= static_cast<uint8_t>(count))
    {
        VkPhysicalDeviceProperties deviceProps;
        for (uint8_t i = 0; i < count; i++) {
            dispatch->GetPhysicalDeviceProperties(devices[i], &deviceProps);
            switch(deviceProps.deviceType)
            {
            case VK_PHYSICAL_DEVICE_TYPE_OTHER:
            default:
                break;
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                discrete_idx = i;
                break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                integrated_idx = i;
                break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                virtual_idx = i;
                break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU:
                deviceIndex = i;
                break;
            }
        }
    }
    else
        printf("Using Vulkan device index %d\n", deviceIndex);

    if (discrete_idx >= 0) 
    {
        deviceIndex = discrete_idx;
        fprintf(stderr, "Selected discrete GPU at index %d\n", deviceIndex);
    } 
    else if (integrated_idx >= 0) 
    {
        deviceIndex = integrated_idx;
        fprintf(stderr, "No discrete GPU found, selected integrated GPU at index %d\n", deviceIndex);
    } 
    else if (virtual_idx >= 0) 
    {
        deviceIndex = virtual_idx;
        fprintf(stderr, "No discrete/integrated GPU found, selected virtual GPU at index %d\n", deviceIndex);
    }
    else if (cpu_idx >= 0)
    {
        deviceIndex = cpu_idx;
        fprintf(stderr, "No GPU found, selected CPU at index %d\n", deviceIndex);
    }

    if (!discrete_idx && !integrated_idx && !virtual_idx && !cpu_idx)
    {
        fprintf(stderr, "Can not select any devices");
        exit(0);
    }

    outDevice = devices[deviceIndex];
    return VK_SUCCESS;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL
DeviceChooserLayer_EnumeratePhysicalDevices(VkInstance        instance,
                                            uint32_t*         pPhysicalDeviceCount,
                                            VkPhysicalDevice* pPhysicalDevices)
{
    const VkuInstanceDispatchTable* const dispatch = instance_dispatch_table(instance);

    if (!dispatch)
        return VK_ERROR_INITIALIZATION_FAILED;

    static const char* const env = getenv(kEnvVariable);

    if (!env)
    {
        return dispatch->EnumeratePhysicalDevices(instance, pPhysicalDeviceCount, pPhysicalDevices);
    }

    VkPhysicalDevice device = VK_NULL_HANDLE;
    const VkResult result = ChooseDevice(instance, dispatch, env, device);

    if (result != VK_SUCCESS)
    {
        return result;
    }
    else if (device == VK_NULL_HANDLE)
    {
        *pPhysicalDeviceCount = 0;
    }
    else if (!pPhysicalDevices)
    {
        *pPhysicalDeviceCount = 1;
    }
    else if (*pPhysicalDeviceCount > 0)
    {
        *pPhysicalDevices     = device;
        *pPhysicalDeviceCount = 1;
    }

    return VK_SUCCESS;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL
DeviceChooserLayer_EnumeratePhysicalDeviceGroupsKHR(VkInstance                          instance,
                                                    uint32_t*                           pPhysicalDeviceGroupCount,
                                                    VkPhysicalDeviceGroupPropertiesKHR* pPhysicalDeviceGroups)
{
    VkuInstanceDispatchTable* dispatch = instance_dispatch_table(instance);

    static const char* const env = getenv(kEnvVariable);
    if (!env)
    {
        return dispatch->EnumeratePhysicalDeviceGroupsKHR(instance, pPhysicalDeviceGroupCount, pPhysicalDeviceGroups);
    }

    /* Just return a single device group containing the requested device. */
    VkPhysicalDevice device;
    VkResult result = ChooseDevice(instance, dispatch, env, device);

    if (result != VK_SUCCESS)
    {
        return result;
    }
    else if (device == VK_NULL_HANDLE)
    {
        *pPhysicalDeviceGroupCount = 0;
    }
    else if (!pPhysicalDeviceGroups)
    {
        *pPhysicalDeviceGroupCount = 1;
    }
    else if (*pPhysicalDeviceGroupCount > 0)
    {
        *pPhysicalDeviceGroupCount = 1;

        pPhysicalDeviceGroups[0].physicalDeviceCount = 1;
        pPhysicalDeviceGroups[0].physicalDevices[0]  = device;
        pPhysicalDeviceGroups[0].subsetAllocation    = VK_FALSE;
    }

    return VK_SUCCESS;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL
DeviceChooserLayer_EnumeratePhysicalDeviceGroups(VkInstance                       instance,
                                                 uint32_t*                        pPhysicalDeviceGroupCount,
                                                 VkPhysicalDeviceGroupProperties* pPhysicalDeviceGroups)
{
    return DeviceChooserLayer_EnumeratePhysicalDeviceGroupsKHR(instance, pPhysicalDeviceGroupCount, pPhysicalDeviceGroups);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL
DeviceChooserLayer_CreateInstance(const VkInstanceCreateInfo*  pCreateInfo,
                                  const VkAllocationCallbacks* pAllocator,
                                  VkInstance*                  pInstance)
{
    VkLayerInstanceCreateInfo* layerCreateInfo = get_chain_info(pCreateInfo, VK_LAYER_LINK_INFO);

    PFN_vkGetInstanceProcAddr gpa = layerCreateInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    layerCreateInfo->u.pLayerInfo = layerCreateInfo->u.pLayerInfo->pNext;

    PFN_vkCreateInstance createFunc = (PFN_vkCreateInstance)gpa(VK_NULL_HANDLE, "vkCreateInstance");

    VkResult ret = createFunc(pCreateInfo, pAllocator, pInstance);
    if (ret != VK_SUCCESS)
        return ret;

    initInstanceTable(*pInstance, gpa);

    return VK_SUCCESS;
}

VK_LAYER_EXPORT void VKAPI_CALL
DeviceChooserLayer_DestroyInstance(VkInstance                   instance,
                                   const VkAllocationCallbacks* pAllocator)
{
    VkuInstanceDispatchTable* dispatch = instance_dispatch_table(instance);
    dispatch_key key = get_dispatch_key(instance);

    dispatch->DestroyInstance(instance, pAllocator);
    gInstanceDispatch.erase((void*)key);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL
DeviceChooserLayer_CreateDevice(VkPhysicalDevice             physicalDevice,
                                const VkDeviceCreateInfo*    pCreateInfo,
                                const VkAllocationCallbacks* pAllocator,
                                VkDevice*                    pDevice)
{
    VkLayerDeviceCreateInfo* layerCreateInfo = get_chain_info(pCreateInfo, VK_LAYER_LINK_INFO);

    PFN_vkGetInstanceProcAddr gipa = layerCreateInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    PFN_vkGetDeviceProcAddr gdpa   = layerCreateInfo->u.pLayerInfo->pfnNextGetDeviceProcAddr;

    layerCreateInfo->u.pLayerInfo = layerCreateInfo->u.pLayerInfo->pNext;

    PFN_vkCreateDevice createFunc = (PFN_vkCreateDevice)gipa(VK_NULL_HANDLE, "vkCreateDevice");

    VkResult ret = createFunc(physicalDevice, pCreateInfo, pAllocator, pDevice);
    if (ret != VK_SUCCESS)
    {
        return ret;
    }

    initDeviceTable(*pDevice, gdpa);

    return VK_SUCCESS;
}

VK_LAYER_EXPORT void VKAPI_CALL
DeviceChooserLayer_DestroyDevice(VkDevice                     device,
                                 const VkAllocationCallbacks* pAllocator)
{
    VkuDeviceDispatchTable* dispatch = device_dispatch_table(device);
    dispatch_key key = get_dispatch_key(device);

    dispatch->DestroyDevice(device, pAllocator);
    gDeviceDispatch.erase((void*)key);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL
DeviceChooserLayer_EnumerateInstanceLayerProperties(uint32_t*          pPropertyCount,
                                                    VkLayerProperties* pProperties)
{
    if (pPropertyCount)
        *pPropertyCount = 1;

    if (pProperties)
    {
        snprintf(pProperties->layerName,   sizeof(pProperties->layerName),   "VK_LAYER_DeviceChooserLayer");
        snprintf(pProperties->description, sizeof(pProperties->description), "Device chooser layer");

        pProperties->implementationVersion = 1;
        pProperties->specVersion           = VK_API_VERSION_1_4;
    }

    return VK_SUCCESS;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL
DeviceChooserLayer_EnumerateDeviceLayerProperties(VkPhysicalDevice   physicalDevice,
                                                  uint32_t*          pPropertyCount,
                                                  VkLayerProperties* pProperties)
{
    return DeviceChooserLayer_EnumerateInstanceLayerProperties(pPropertyCount, pProperties);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL
DeviceChooserLayer_EnumerateInstanceExtensionProperties(const char*            pLayerName,
                                                        uint32_t*              pPropertyCount,
                                                        VkExtensionProperties* pProperties)
{
    if (!pLayerName || strcmp(pLayerName, "VK_LAYER_DeviceChooserLayer"))
    {
        return VK_ERROR_LAYER_NOT_PRESENT;
    }

    if (pPropertyCount)
    {
        *pPropertyCount = 0;
    }

    return VK_SUCCESS;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL
DeviceChooserLayer_EnumerateDeviceExtensionProperties(VkPhysicalDevice       physicalDevice,
                                                      const char*            pLayerName,
                                                      uint32_t*              pPropertyCount,
                                                      VkExtensionProperties* pProperties)
{
    if (!pLayerName || strcmp(pLayerName, "VK_LAYER_DeviceChooserLayer"))
    {
        if (physicalDevice == VK_NULL_HANDLE)
            return VK_SUCCESS;

        return instance_dispatch_table(physicalDevice)->EnumerateDeviceExtensionProperties(physicalDevice,
                                                                                           pLayerName,
                                                                                           pPropertyCount,
                                                                                           pProperties);
    }

    if (pPropertyCount)
    {
        *pPropertyCount = 0;
    }

    return VK_SUCCESS;
}

#define GETPROCADDR(func) if(!strcmp(pName, "vk" #func)) return (PFN_vkVoidFunction)&DeviceChooserLayer_##func;

VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI_CALL
DeviceChooserLayer_GetDeviceProcAddr(VkDevice    device,
                                     const char* pName)
{
    GETPROCADDR(GetDeviceProcAddr);
    GETPROCADDR(EnumerateDeviceLayerProperties);
    GETPROCADDR(EnumerateDeviceExtensionProperties);
    GETPROCADDR(CreateDevice);
    GETPROCADDR(DestroyDevice);

    return device_dispatch_table(device)->GetDeviceProcAddr(device, pName);
}

VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI_CALL
DeviceChooserLayer_GetInstanceProcAddr(VkInstance  instance,
                                       const char* pName)
{
    GETPROCADDR(GetInstanceProcAddr);
    GETPROCADDR(EnumerateInstanceLayerProperties);
    GETPROCADDR(EnumerateInstanceExtensionProperties);
    GETPROCADDR(CreateInstance);
    GETPROCADDR(DestroyInstance);
    GETPROCADDR(EnumeratePhysicalDevices);
    GETPROCADDR(EnumeratePhysicalDeviceGroups);
    GETPROCADDR(EnumeratePhysicalDeviceGroupsKHR);

    GETPROCADDR(GetDeviceProcAddr);
    GETPROCADDR(EnumerateDeviceLayerProperties);
    GETPROCADDR(EnumerateDeviceExtensionProperties);
    GETPROCADDR(CreateDevice);
    GETPROCADDR(DestroyDevice);

    return instance_dispatch_table(instance)->GetInstanceProcAddr(instance, pName);
}
