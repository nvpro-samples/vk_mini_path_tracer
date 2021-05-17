// Copyright 2020-2021 NVIDIA Corporation
// SPDX-License-Identifier: Apache-2.0

#include <nvvk/context_vk.hpp>
#include <nvvk/resourceallocator_vk.hpp>  // For NVVK memory allocators
#include <nvvk/structs_vk.hpp>            // For nvvk::make

static const uint64_t render_width  = 800;
static const uint64_t render_height = 600;

int main(int argc, const char** argv)
{
  // Create the Vulkan context, consisting of an instance, device, physical device, and queues.
  nvvk::ContextCreateInfo deviceInfo;  // One can modify this to load different extensions or pick the Vulkan core version
  deviceInfo.apiMajor = 1;             // Specify the version of Vulkan we'll use
  deviceInfo.apiMinor = 2;
  // Required by KHR_acceleration_structure; allows work to be offloaded onto background threads and parallelized
  deviceInfo.addDeviceExtension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
  VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures = nvvk::make<VkPhysicalDeviceAccelerationStructureFeaturesKHR>();
  deviceInfo.addDeviceExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, false, &asFeatures);
  VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures = nvvk::make<VkPhysicalDeviceRayQueryFeaturesKHR>();
  deviceInfo.addDeviceExtension(VK_KHR_RAY_QUERY_EXTENSION_NAME, false, &rayQueryFeatures);

  nvvk::Context context;     // Encapsulates device state in a single object
  context.init(deviceInfo);  // Initialize the context
  // Device must support acceleration structures and ray queries:
  assert(asFeatures.accelerationStructure == VK_TRUE && rayQueryFeatures.rayQuery == VK_TRUE);

  // Create the allocator
  nvvk::ResourceAllocatorDedicated allocator;
  allocator.init(context, context.m_physicalDevice);

  // Create a buffer
  VkDeviceSize       bufferSizeBytes  = render_width * render_height * 3 * sizeof(float);
  VkBufferCreateInfo bufferCreateInfo = nvvk::make<VkBufferCreateInfo>();
  bufferCreateInfo.size               = bufferSizeBytes;
  bufferCreateInfo.usage              = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  // VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT means that the CPU can read this buffer's memory.
  // VK_MEMORY_PROPERTY_HOST_CACHED_BIT means that the CPU caches this memory.
  // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT means that the CPU side of cache management
  // is handled automatically, with potentially slower reads/writes.
  nvvk::Buffer buffer = allocator.createBuffer(bufferCreateInfo,                         //
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT       //
                                                   | VK_MEMORY_PROPERTY_HOST_CACHED_BIT  //
                                                   | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  // In future chapters, we'll program the GPU to write to the buffer.
  // For now, let's read back undefined memory.

  // Get the image data back from the GPU
  void*  data    = allocator.map(buffer);
  float* fltData = reinterpret_cast<float*>(data);
  printf("First three elements: %f, %f, %f\n", fltData[0], fltData[1], fltData[2]);
  allocator.unmap(buffer);

  allocator.destroy(buffer);
  allocator.deinit();
  context.deinit();  // Don't forget to clean up at the end of the program!
}