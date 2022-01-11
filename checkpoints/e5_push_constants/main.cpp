// Copyright 2020-2021 NVIDIA Corporation
// SPDX-License-Identifier: Apache-2.0
#include <array>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <nvh/fileoperations.hpp>  // For nvh::loadFile
#include <nvvk/context_vk.hpp>
#include <nvvk/descriptorsets_vk.hpp>  // For nvvk::DescriptorSetContainer
#include <nvvk/error_vk.hpp>
#include <nvvk/raytraceKHR_vk.hpp>        // For nvvk::RaytracingBuilderKHR
#include <nvvk/resourceallocator_vk.hpp>  // For NVVK memory allocators
#include <nvvk/shaders_vk.hpp>            // For nvvk::createShaderModule
#include <nvvk/structs_vk.hpp>            // For nvvk::make

#include "common.h"

PushConstants pushConstants{1024 /* render_width */, 600 /* render_height */};

VkCommandBuffer AllocateAndBeginOneTimeCommandBuffer(VkDevice device, VkCommandPool cmdPool)
{
  VkCommandBufferAllocateInfo cmdAllocInfo = nvvk::make<VkCommandBufferAllocateInfo>();
  cmdAllocInfo.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cmdAllocInfo.commandPool                 = cmdPool;
  cmdAllocInfo.commandBufferCount          = 1;
  VkCommandBuffer cmdBuffer;
  NVVK_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, &cmdBuffer));
  VkCommandBufferBeginInfo beginInfo = nvvk::make<VkCommandBufferBeginInfo>();
  beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  NVVK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo));
  return cmdBuffer;
}

void EndSubmitWaitAndFreeCommandBuffer(VkDevice device, VkQueue queue, VkCommandPool cmdPool, VkCommandBuffer& cmdBuffer)
{
  NVVK_CHECK(vkEndCommandBuffer(cmdBuffer));
  VkSubmitInfo submitInfo       = nvvk::make<VkSubmitInfo>();
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers    = &cmdBuffer;
  NVVK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
  NVVK_CHECK(vkQueueWaitIdle(queue));
  vkFreeCommandBuffers(device, cmdPool, 1, &cmdBuffer);
}

VkDeviceAddress GetBufferDeviceAddress(VkDevice device, VkBuffer buffer)
{
  VkBufferDeviceAddressInfo addressInfo = nvvk::make<VkBufferDeviceAddressInfo>();
  addressInfo.buffer                    = buffer;
  return vkGetBufferDeviceAddress(device, &addressInfo);
}

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
  VkDeviceSize bufferSizeBytes = uint64_t(pushConstants.render_width) * uint64_t(pushConstants.render_height) * 3 * sizeof(float);
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

  // Load the mesh of the first shape from an OBJ file
  const std::string        exePath(argv[0], std::string(argv[0]).find_last_of("/\\") + 1);
  std::vector<std::string> searchPaths = {exePath + PROJECT_RELDIRECTORY, exePath + PROJECT_RELDIRECTORY "..",
                                          exePath + PROJECT_RELDIRECTORY "../..", exePath + PROJECT_NAME};
  tinyobj::ObjReader       reader;  // Used to read an OBJ file
  reader.ParseFromFile(nvh::findFile("scenes/CornellBox-Original-Merged.obj", searchPaths));
  assert(reader.Valid());  // Make sure tinyobj was able to parse this file
  const std::vector<tinyobj::real_t>   objVertices = reader.GetAttrib().GetVertices();
  const std::vector<tinyobj::shape_t>& objShapes   = reader.GetShapes();  // All shapes in the file
  assert(objShapes.size() == 1);                                          // Check that this file has only one shape
  const tinyobj::shape_t& objShape = objShapes[0];                        // Get the first shape
  // Get the indices of the vertices of the first mesh of `objShape` in `attrib.vertices`:
  std::vector<uint32_t> objIndices;
  objIndices.reserve(objShape.mesh.indices.size());
  for(const tinyobj::index_t& index : objShape.mesh.indices)
  {
    objIndices.push_back(index.vertex_index);
  }

  // Create the command pool
  VkCommandPoolCreateInfo cmdPoolInfo = nvvk::make<VkCommandPoolCreateInfo>();
  cmdPoolInfo.queueFamilyIndex        = context.m_queueGCT;
  VkCommandPool cmdPool;
  NVVK_CHECK(vkCreateCommandPool(context, &cmdPoolInfo, nullptr, &cmdPool));

  // Upload the vertex and index buffers to the GPU.
  nvvk::Buffer vertexBuffer, indexBuffer;
  {
    // Start a command buffer for uploading the buffers
    VkCommandBuffer uploadCmdBuffer = AllocateAndBeginOneTimeCommandBuffer(context, cmdPool);
    // We get these buffers' device addresses, and use them as storage buffers and build inputs.
    const VkBufferUsageFlags usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                                     | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    vertexBuffer = allocator.createBuffer(uploadCmdBuffer, objVertices, usage);
    indexBuffer  = allocator.createBuffer(uploadCmdBuffer, objIndices, usage);

    EndSubmitWaitAndFreeCommandBuffer(context, context.m_queueGCT, cmdPool, uploadCmdBuffer);
    allocator.finalizeAndReleaseStaging();
  }

  // Describe the bottom-level acceleration structure (BLAS)
  std::vector<nvvk::RaytracingBuilderKHR::BlasInput> blases;
  {
    nvvk::RaytracingBuilderKHR::BlasInput blas;
    // Get the device addresses of the vertex and index buffers
    VkDeviceAddress vertexBufferAddress = GetBufferDeviceAddress(context, vertexBuffer.buffer);
    VkDeviceAddress indexBufferAddress  = GetBufferDeviceAddress(context, indexBuffer.buffer);
    // Specify where the builder can find the vertices and indices for triangles, and their formats:
    VkAccelerationStructureGeometryTrianglesDataKHR triangles = nvvk::make<VkAccelerationStructureGeometryTrianglesDataKHR>();
    triangles.vertexFormat                                    = VK_FORMAT_R32G32B32_SFLOAT;
    triangles.vertexData.deviceAddress                        = vertexBufferAddress;
    triangles.vertexStride                                    = 3 * sizeof(float);
    triangles.maxVertex                                       = static_cast<uint32_t>(objVertices.size() / 3 - 1);
    triangles.indexType                                       = VK_INDEX_TYPE_UINT32;
    triangles.indexData.deviceAddress                         = indexBufferAddress;
    triangles.transformData.deviceAddress                     = 0;  // No transform
    // Create a VkAccelerationStructureGeometryKHR object that says it handles opaque triangles and points to the above:
    VkAccelerationStructureGeometryKHR geometry = nvvk::make<VkAccelerationStructureGeometryKHR>();
    geometry.geometry.triangles                 = triangles;
    geometry.geometryType                       = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.flags                              = VK_GEOMETRY_OPAQUE_BIT_KHR;
    blas.asGeometry.push_back(geometry);
    // Create offset info that allows us to say how many triangles and vertices to read
    VkAccelerationStructureBuildRangeInfoKHR offsetInfo;
    offsetInfo.firstVertex     = 0;
    offsetInfo.primitiveCount  = static_cast<uint32_t>(objIndices.size() / 3);  // Number of triangles
    offsetInfo.primitiveOffset = 0;
    offsetInfo.transformOffset = 0;
    blas.asBuildOffsetInfo.push_back(offsetInfo);
    blases.push_back(blas);
  }
  // Create the BLAS
  nvvk::RaytracingBuilderKHR raytracingBuilder;
  raytracingBuilder.setup(context, &allocator, context.m_queueGCT);
  raytracingBuilder.buildBlas(blases, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
                                          | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR);

  // Create an instance pointing to this BLAS, and build it into a TLAS:
  std::vector<VkAccelerationStructureInstanceKHR> instances;
  {
    VkAccelerationStructureInstanceKHR instance;
    instance.accelerationStructureReference = raytracingBuilder.getBlasDeviceAddress(0);  // The address of the BLAS in `blases` that this instance points to
    // Set the instance transform to the identity matrix:
    instance.transform.matrix[0][0] = instance.transform.matrix[1][1] = instance.transform.matrix[2][2] = 1.0f;
    instance.instanceCustomIndex = 0;  // 24 bits accessible to ray shaders via rayQueryGetIntersectionInstanceCustomIndexEXT
    // The address of the BLAS in `blases` that this instance points to
    // Used for a shader offset index, accessible via rayQueryGetIntersectionInstanceShaderBindingTableRecordOffsetEXT
    instance.instanceShaderBindingTableRecordOffset = 0;
    instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;  // How to trace this instance
    instance.mask  = 0xFF;
    instances.push_back(instance);
  }
  raytracingBuilder.buildTlas(instances, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);

  // Here's the list of bindings for the descriptor set layout, from raytrace.comp.glsl:
  // 0 - a storage buffer (the buffer `buffer`)
  // 1 - an acceleration structure (the TLAS)
  // 2 - a storage buffer (the vertex buffer)
  // 3 - a storage buffer (the index buffer)
  nvvk::DescriptorSetContainer descriptorSetContainer(context);
  descriptorSetContainer.addBinding(BINDING_IMAGEDATA, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
  descriptorSetContainer.addBinding(BINDING_TLAS, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_COMPUTE_BIT);
  descriptorSetContainer.addBinding(BINDING_VERTICES, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
  descriptorSetContainer.addBinding(BINDING_INDICES, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
  // Create a layout from the list of bindings
  descriptorSetContainer.initLayout();
  // Create a descriptor pool from the list of bindings with space for 1 set, and allocate that set
  descriptorSetContainer.initPool(1);
  // Create a push constant range describing the amount of data for the push constants.
  static_assert(sizeof(PushConstants) % 4 == 0, "Push constant size must be a multiple of 4 per the Vulkan spec!");
  VkPushConstantRange pushConstantRange;
  pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
  pushConstantRange.offset     = 0;
  pushConstantRange.size       = sizeof(PushConstants);
  // Create a pipeline layout from the descriptor set layout and push constant range:
  descriptorSetContainer.initPipeLayout(1,                    // Number of push constant ranges
                                        &pushConstantRange);  // Pointer to push constant ranges

  // Write values into the descriptor set.
  std::array<VkWriteDescriptorSet, 4> writeDescriptorSets;
  // Color buffer
  VkDescriptorBufferInfo descriptorBufferInfo{};
  descriptorBufferInfo.buffer = buffer.buffer;    // The VkBuffer object
  descriptorBufferInfo.range  = bufferSizeBytes;  // The length of memory to bind; offset is 0.
  writeDescriptorSets[0] = descriptorSetContainer.makeWrite(0 /*set index*/, BINDING_IMAGEDATA /*binding*/, &descriptorBufferInfo);
  // Top-level acceleration structure (TLAS)
  VkWriteDescriptorSetAccelerationStructureKHR descriptorAS = nvvk::make<VkWriteDescriptorSetAccelerationStructureKHR>();
  VkAccelerationStructureKHR tlasCopy = raytracingBuilder.getAccelerationStructure();  // So that we can take its address
  descriptorAS.accelerationStructureCount = 1;
  descriptorAS.pAccelerationStructures    = &tlasCopy;
  writeDescriptorSets[1]                  = descriptorSetContainer.makeWrite(0, BINDING_TLAS, &descriptorAS);
  // Vertex buffer
  VkDescriptorBufferInfo vertexDescriptorBufferInfo{};
  vertexDescriptorBufferInfo.buffer = vertexBuffer.buffer;
  vertexDescriptorBufferInfo.range  = VK_WHOLE_SIZE;
  writeDescriptorSets[2] = descriptorSetContainer.makeWrite(0, BINDING_VERTICES, &vertexDescriptorBufferInfo);
  // Index buffer
  VkDescriptorBufferInfo indexDescriptorBufferInfo{};
  indexDescriptorBufferInfo.buffer = indexBuffer.buffer;
  indexDescriptorBufferInfo.range  = VK_WHOLE_SIZE;
  writeDescriptorSets[3]           = descriptorSetContainer.makeWrite(0, BINDING_INDICES, &indexDescriptorBufferInfo);
  vkUpdateDescriptorSets(context,                                            // The context
                         static_cast<uint32_t>(writeDescriptorSets.size()),  // Number of VkWriteDescriptorSet objects
                         writeDescriptorSets.data(),                         // Pointer to VkWriteDescriptorSet objects
                         0, nullptr);  // An array of VkCopyDescriptorSet objects (unused)

  // Shader loading and pipeline creation
  VkShaderModule rayTraceModule =
      nvvk::createShaderModule(context, nvh::loadFile("shaders/raytrace.comp.glsl.spv", true, searchPaths));

  // Describes the entrypoint and the stage to use for this shader module in the pipeline
  VkPipelineShaderStageCreateInfo shaderStageCreateInfo = nvvk::make<VkPipelineShaderStageCreateInfo>();
  shaderStageCreateInfo.stage                           = VK_SHADER_STAGE_COMPUTE_BIT;
  shaderStageCreateInfo.module                          = rayTraceModule;
  shaderStageCreateInfo.pName                           = "main";

  // Create the compute pipeline
  VkComputePipelineCreateInfo pipelineCreateInfo = nvvk::make<VkComputePipelineCreateInfo>();
  pipelineCreateInfo.stage                       = shaderStageCreateInfo;
  pipelineCreateInfo.layout                      = descriptorSetContainer.getPipeLayout();
  // Don't modify flags, basePipelineHandle, or basePipelineIndex
  VkPipeline computePipeline;
  NVVK_CHECK(vkCreateComputePipelines(context,                 // Device
                                      VK_NULL_HANDLE,          // Pipeline cache (uses default)
                                      1, &pipelineCreateInfo,  // Compute pipeline create info
                                      nullptr,                 // Allocator (uses default)
                                      &computePipeline));      // Output

  // Create and start recording a command buffer
  VkCommandBuffer cmdBuffer = AllocateAndBeginOneTimeCommandBuffer(context, cmdPool);

  // Bind the compute shader pipeline
  vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
  // Bind the descriptor set
  VkDescriptorSet descriptorSet = descriptorSetContainer.getSet(0);
  vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, descriptorSetContainer.getPipeLayout(), 0, 1,
                          &descriptorSet, 0, nullptr);

  // Push push constants:
  vkCmdPushConstants(cmdBuffer,                               // Command buffer
                     descriptorSetContainer.getPipeLayout(),  // Pipeline layout
                     VK_SHADER_STAGE_COMPUTE_BIT,             // Stage flags
                     0,                                       // Offset
                     sizeof(PushConstants),                   // Size in bytes
                     &pushConstants);                         // Data

  // Run the compute shader with enough workgroups to cover the entire buffer:
  vkCmdDispatch(cmdBuffer, (pushConstants.render_width + WORKGROUP_WIDTH - 1) / WORKGROUP_WIDTH,
                (pushConstants.render_height + WORKGROUP_HEIGHT - 1) / WORKGROUP_HEIGHT, 1);

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

  // End and submit the command buffer, then wait for it to finish:
  EndSubmitWaitAndFreeCommandBuffer(context, context.m_queueGCT, cmdPool, cmdBuffer);

  // Get the image data back from the GPU
  void* data = allocator.map(buffer);
  stbi_write_hdr("out.hdr", pushConstants.render_width, pushConstants.render_height, 3, reinterpret_cast<float*>(data));
  allocator.unmap(buffer);

  vkDestroyPipeline(context, computePipeline, nullptr);
  vkDestroyShaderModule(context, rayTraceModule, nullptr);
  descriptorSetContainer.deinit();
  raytracingBuilder.destroy();
  allocator.destroy(vertexBuffer);
  allocator.destroy(indexBuffer);
  vkDestroyCommandPool(context, cmdPool, nullptr);
  allocator.destroy(buffer);
  allocator.deinit();
  context.deinit();  // Don't forget to clean up at the end of the program!
}
