// Copyright 2020-2021 NVIDIA Corporation
// SPDX-License-Identifier: Apache-2.0
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <nvh/fileoperations.hpp>  // For nvh::loadFile
#include <nvvk/context_vk.hpp>
#include <nvvk/error_vk.hpp>              // For NVVK_CHECK
#include <nvvk/resourceallocator_vk.hpp>  // For NVVK memory allocators
#include <nvvk/shaders_vk.hpp>            // For nvvk::createShaderModule
#include <nvvk/structs_vk.hpp>            // For nvvk::make

static const uint64_t render_width     = 800;
static const uint64_t render_height    = 600;
static const uint32_t workgroup_width  = 16;
static const uint32_t workgroup_height = 8;

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

  // Add the required device extensions for Debug Printf. If this is confusing,
  // don't worry; we'll remove this in the next chapter.
  deviceInfo.addDeviceExtension(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
  VkValidationFeaturesEXT      validationInfo            = nvvk::make<VkValidationFeaturesEXT>();
  VkValidationFeatureEnableEXT validationFeatureToEnable = VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT;
  validationInfo.enabledValidationFeatureCount           = 1;
  validationInfo.pEnabledValidationFeatures              = &validationFeatureToEnable;
  deviceInfo.instanceCreateInfoExt                       = &validationInfo;
#ifdef _WIN32
  _putenv_s("DEBUG_PRINTF_TO_STDOUT", "1");
#else  // If not _WIN32
  putenv("DEBUG_PRINTF_TO_STDOUT=1");
#endif  // _WIN32

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

  const std::string        exePath(argv[0], std::string(argv[0]).find_last_of("/\\") + 1);
  std::vector<std::string> searchPaths = {exePath + PROJECT_RELDIRECTORY, exePath + PROJECT_RELDIRECTORY "..",
                                          exePath + PROJECT_RELDIRECTORY "../..", exePath + PROJECT_NAME};

  // Create the command pool
  VkCommandPoolCreateInfo cmdPoolInfo = nvvk::make<VkCommandPoolCreateInfo>();
  cmdPoolInfo.queueFamilyIndex        = context.m_queueGCT;
  VkCommandPool cmdPool;
  NVVK_CHECK(vkCreateCommandPool(context, &cmdPoolInfo, nullptr, &cmdPool));

  // Shader loading and pipeline creation
  VkShaderModule rayTraceModule =
      nvvk::createShaderModule(context, nvh::loadFile("shaders/raytrace.comp.glsl.spv", true, searchPaths));

  // Describes the entrypoint and the stage to use for this shader module in the pipeline
  VkPipelineShaderStageCreateInfo shaderStageCreateInfo = nvvk::make<VkPipelineShaderStageCreateInfo>();
  shaderStageCreateInfo.stage                           = VK_SHADER_STAGE_COMPUTE_BIT;
  shaderStageCreateInfo.module                          = rayTraceModule;
  shaderStageCreateInfo.pName                           = "main";

  // For the moment, create an empty pipeline layout. You can ignore this code
  // for now; we'll replace it in the next chapter.
  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = nvvk::make<VkPipelineLayoutCreateInfo>();
  pipelineLayoutCreateInfo.setLayoutCount             = 0;
  pipelineLayoutCreateInfo.pushConstantRangeCount     = 0;
  VkPipelineLayout pipelineLayout;
  NVVK_CHECK(vkCreatePipelineLayout(context, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

  // Create the compute pipeline
  VkComputePipelineCreateInfo pipelineCreateInfo = nvvk::make<VkComputePipelineCreateInfo>();
  pipelineCreateInfo.stage                       = shaderStageCreateInfo;
  pipelineCreateInfo.layout                      = pipelineLayout;
  // Don't modify flags, basePipelineHandle, or basePipelineIndex
  VkPipeline computePipeline;
  NVVK_CHECK(vkCreateComputePipelines(context,                 // Device
                                      VK_NULL_HANDLE,          // Pipeline cache (uses default)
                                      1, &pipelineCreateInfo,  // Compute pipeline create info
                                      nullptr,                 // Allocator (uses default)
                                      &computePipeline));      // Output

  // Allocate a command buffer
  VkCommandBufferAllocateInfo cmdAllocInfo = nvvk::make<VkCommandBufferAllocateInfo>();
  cmdAllocInfo.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cmdAllocInfo.commandPool                 = cmdPool;
  cmdAllocInfo.commandBufferCount          = 1;
  VkCommandBuffer cmdBuffer;
  NVVK_CHECK(vkAllocateCommandBuffers(context, &cmdAllocInfo, &cmdBuffer));

  // Begin recording
  VkCommandBufferBeginInfo beginInfo = nvvk::make<VkCommandBufferBeginInfo>();
  beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  NVVK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo));

  // Bind the compute shader pipeline
  vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);

  // Run the compute shader with one workgroup for now
  vkCmdDispatch(cmdBuffer, 1, 1, 1);

  // Add a command that says "Make it so that memory writes by the compute shader
  // are available to read from the CPU." (In other words, "Flush the GPU caches
  // so the CPU can read the data.") To do this, we use a memory barrier.
  // This is one of the most complex parts of Vulkan, so don't worry if this is
  // confusing! We'll talk about pipeline barriers more in the extras.
  VkMemoryBarrier memoryBarrier = nvvk::make<VkMemoryBarrier>();
  memoryBarrier.srcAccessMask   = VK_ACCESS_SHADER_WRITE_BIT;  // Make shader writes
  memoryBarrier.dstAccessMask   = VK_ACCESS_HOST_READ_BIT;     // Readable by the CPU
  vkCmdPipelineBarrier(cmdBuffer,                              // The command buffer
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,   // From the compute shader
                       VK_PIPELINE_STAGE_HOST_BIT,             // To the CPU
                       0,                                      // No special flags
                       1, &memoryBarrier,                      // An array of memory barriers
                       0, nullptr, 0, nullptr);                // No other barriers

  // End recording
  NVVK_CHECK(vkEndCommandBuffer(cmdBuffer));

  // Submit the command buffer
  VkSubmitInfo submitInfo       = nvvk::make<VkSubmitInfo>();
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers    = &cmdBuffer;
  NVVK_CHECK(vkQueueSubmit(context.m_queueGCT, 1, &submitInfo, VK_NULL_HANDLE));

  // Wait for the GPU to finish
  NVVK_CHECK(vkQueueWaitIdle(context.m_queueGCT));

  // Get the image data back from the GPU
  void* data = allocator.map(buffer);
  stbi_write_hdr("out.hdr", render_width, render_height, 3, reinterpret_cast<float*>(data));
  allocator.unmap(buffer);

  vkDestroyPipeline(context, computePipeline, nullptr);
  vkDestroyShaderModule(context, rayTraceModule, nullptr);
  vkDestroyPipelineLayout(context, pipelineLayout, nullptr);  // Will be removed in the next chapter
  vkFreeCommandBuffers(context, cmdPool, 1, &cmdBuffer);
  vkDestroyCommandPool(context, cmdPool, nullptr);
  allocator.destroy(buffer);
  allocator.deinit();
  context.deinit();  // Don't forget to clean up at the end of the program!
}
