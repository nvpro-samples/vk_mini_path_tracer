// Copyright 2020-2024 NVIDIA Corporation
// SPDX-License-Identifier: Apache-2.0

#include <nvvk/context_vk.hpp>
#include <nvvk/error_vk.hpp>              // For NVVK_CHECK
#include <nvvk/resourceallocator_vk.hpp>  // For NVVK memory allocators

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
  VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
  deviceInfo.addDeviceExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, false, &asFeatures);
  VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR};
  deviceInfo.addDeviceExtension(VK_KHR_RAY_QUERY_EXTENSION_NAME, false, &rayQueryFeatures);

  nvvk::Context context;     // Encapsulates device state in a single object
  context.init(deviceInfo);  // Initialize the context

  // Create the allocator
  nvvk::ResourceAllocatorDedicated allocator;
  allocator.init(context, context.m_physicalDevice);

  // Create a buffer
  VkDeviceSize       bufferSizeBytes = render_width * render_height * 3 * sizeof(float);
  VkBufferCreateInfo bufferCreateInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                      .size  = bufferSizeBytes,
                                      .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT};
  // VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT means that the CPU can read this buffer's memory.
  // VK_MEMORY_PROPERTY_HOST_CACHED_BIT means that the CPU caches this memory.
  // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT means that the CPU side of cache management
  // is handled automatically, with potentially slower reads/writes.
  nvvk::Buffer buffer = allocator.createBuffer(bufferCreateInfo,                         //
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT       //
                                                   | VK_MEMORY_PROPERTY_HOST_CACHED_BIT  //
                                                   | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  // Create the command pool
  VkCommandPoolCreateInfo cmdPoolInfo{.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,  //
                                      .queueFamilyIndex = context.m_queueGCT};
  VkCommandPool           cmdPool;
  NVVK_CHECK(vkCreateCommandPool(context, &cmdPoolInfo, nullptr, &cmdPool));

  // Allocate a command buffer
  VkCommandBufferAllocateInfo cmdAllocInfo{.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                           .commandPool        = cmdPool,
                                           .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                           .commandBufferCount = 1};
  VkCommandBuffer             cmdBuffer;
  NVVK_CHECK(vkAllocateCommandBuffers(context, &cmdAllocInfo, &cmdBuffer));

  // Begin recording
  VkCommandBufferBeginInfo beginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                     .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
  NVVK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo));

  // Fill the buffer
  const float     fillValue    = 0.5f;
  const uint32_t& fillValueU32 = reinterpret_cast<const uint32_t&>(fillValue);
  vkCmdFillBuffer(cmdBuffer, buffer.buffer, 0, bufferSizeBytes, fillValueU32);

  // Add a command that says "Make it so that memory writes by the vkCmdFillBuffer call
  // are available to read from the CPU." (In other words, "Flush the GPU caches
  // so the CPU can read the data.") To do this, we use a memory barrier.
  // This is one of the most complex parts of Vulkan, so don't worry if this is
  // confusing! We'll talk about pipeline barriers more in the extras.
  VkMemoryBarrier memoryBarrier{.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                                .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,  // Make transfer writes
                                .dstAccessMask = VK_ACCESS_HOST_READ_BIT};      // Readable by the CPU
  vkCmdPipelineBarrier(cmdBuffer,                                               // The command buffer
                       VK_PIPELINE_STAGE_TRANSFER_BIT,                          // From the transfer stage
                       VK_PIPELINE_STAGE_HOST_BIT,                              // To the CPU
                       0,                                                       // No special flags
                       1, &memoryBarrier,                                       // An array of memory barriers
                       0, nullptr, 0, nullptr);                                 // No other barriers

  // End recording
  NVVK_CHECK(vkEndCommandBuffer(cmdBuffer));

  // Submit the command buffer
  VkSubmitInfo submitInfo{.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,  //
                          .commandBufferCount = 1,                              //
                          .pCommandBuffers    = &cmdBuffer};
  NVVK_CHECK(vkQueueSubmit(context.m_queueGCT, 1, &submitInfo, VK_NULL_HANDLE));

  // Wait for the GPU to finish
  NVVK_CHECK(vkQueueWaitIdle(context.m_queueGCT));

  // Get the image data back from the GPU
  void*  data    = allocator.map(buffer);
  float* fltData = reinterpret_cast<float*>(data);
  printf("First three elements: %f, %f, %f\n", fltData[0], fltData[1], fltData[2]);
  allocator.unmap(buffer);

  vkFreeCommandBuffers(context, cmdPool, 1, &cmdBuffer);
  vkDestroyCommandPool(context, cmdPool, nullptr);
  allocator.destroy(buffer);
  allocator.deinit();
  context.deinit();  // Don't forget to clean up at the end of the program!
}