// Copyright 2020-2024 NVIDIA Corporation
// SPDX-License-Identifier: Apache-2.0
#include <array>
#include <random>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <nvh/fileoperations.hpp>  // For nvh::loadFile
#include <nvvk/context_vk.hpp>
#include <nvvk/descriptorsets_vk.hpp>  // For nvvk::DescriptorSetContainer
#include <nvvk/error_vk.hpp>
#include <nvvk/images_vk.hpp>             // For makeImageMemoryBarrier
#include <nvvk/raytraceKHR_vk.hpp>        // For nvvk::RaytracingBuilderKHR
#include <nvvk/resourceallocator_vk.hpp>  // For NVVK memory allocators
#include <nvvk/shaders_vk.hpp>            // For nvvk::createShaderModule

#include "common.h"

PushConstants  pushConstants;
const uint32_t render_width  = 800;
const uint32_t render_height = 600;

VkCommandBuffer AllocateAndBeginOneTimeCommandBuffer(VkDevice device, VkCommandPool cmdPool)
{
  VkCommandBufferAllocateInfo cmdAllocInfo{.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                           .commandPool        = cmdPool,
                                           .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                           .commandBufferCount = 1};
  VkCommandBuffer             cmdBuffer;
  NVVK_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, &cmdBuffer));
  VkCommandBufferBeginInfo beginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                     .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
  NVVK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo));
  return cmdBuffer;
}

void EndSubmitWaitAndFreeCommandBuffer(VkDevice device, VkQueue queue, VkCommandPool cmdPool, VkCommandBuffer& cmdBuffer)
{
  NVVK_CHECK(vkEndCommandBuffer(cmdBuffer));
  VkSubmitInfo submitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cmdBuffer};
  NVVK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
  NVVK_CHECK(vkQueueWaitIdle(queue));
  vkFreeCommandBuffers(device, cmdPool, 1, &cmdBuffer);
}

VkDeviceAddress GetBufferDeviceAddress(VkDevice device, VkBuffer buffer)
{
  VkBufferDeviceAddressInfo addressInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = buffer};
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
  VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
  deviceInfo.addDeviceExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, false, &asFeatures);
  VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR};
  deviceInfo.addDeviceExtension(VK_KHR_RAY_QUERY_EXTENSION_NAME, false, &rayQueryFeatures);

  nvvk::Context context;     // Encapsulates device state in a single object
  context.init(deviceInfo);  // Initialize the context

  // Initialize the debug utilities:
  nvvk::DebugUtil debugUtil(context);

  // Create the allocator
  nvvk::ResourceAllocatorDedicated allocator;
  allocator.init(context, context.m_physicalDevice);

  // Create an image. Images are more complex than buffers - they can have
  // multiple dimensions, different color+depth formats, be arrays of mips,
  // have multisampling, be tiled in memory in e.g. row-linear order or in an
  // implementation-dependent way (and this layout of memory can depend on
  // what the image is being used for), and be shared across multiple queues.
  // Here's how we specify the image we'll use:
  VkImageCreateInfo imageCreateInfo =  //
      {.sType     = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
       .imageType = VK_IMAGE_TYPE_2D,
       // RGB32 images aren't usually supported, so we change this to a RGBA32 image.
       .format = VK_FORMAT_R32G32B32A32_SFLOAT,
       // Defines the size of the image:
       .extent = {render_width, render_height, 1},
       // The image is an array of length 1, and each element contains only 1 mip:
       .mipLevels   = 1,
       .arrayLayers = 1,
       // We aren't using MSAA (i.e. the image only contains 1 sample per pixel -
       // note that this isn't the same use of the word "sample" as in ray tracing):
       .samples = VK_SAMPLE_COUNT_1_BIT,
       // The driver controls the tiling of the image for performance:
       .tiling = VK_IMAGE_TILING_OPTIMAL,
       // This image is read and written on the GPU, and data can be transferred
       // from it:
       .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
       // Image is only used by one queue:
       .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
       // The image must be in either VK_IMAGE_LAYOUT_UNDEFINED or VK_IMAGE_LAYOUT_PREINITIALIZED
       // according to the specification; we'll transition the layout shortly,
       // in the same command buffer used to upload the vertex and index buffers:
       .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
  nvvk::Image image = allocator.createImage(imageCreateInfo);
  debugUtil.setObjectName(image.image, "image");

  // Create an image view for the entire image
  // When we create a descriptor for the image, we'll also need an image view
  // that the descriptor will point to. This specifies what part of the image
  // the descriptor views, and how the descriptor views it.
  VkImageViewCreateInfo imageViewCreateInfo =  //
      {.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
       .image    = image.image,
       .viewType = VK_IMAGE_VIEW_TYPE_2D,
       .format   = imageCreateInfo.format,
       // We could use imageViewCreateInfo.components to make the components of the
       // image appear to be "swizzled", but we don't want to do that. Luckily,
       // all values are set to VK_COMPONENT_SWIZZLE_IDENTITY, which means
       // "don't change anything", by zero initialization.
       // This says that the ImageView views the color part of the image (since
       // images can contain depth or stencil aspects):
       .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                            // This says that we only look at mip level and array layer 0:
                            .baseMipLevel   = 0,
                            .levelCount     = 1,
                            .baseArrayLayer = 0,
                            .layerCount     = 1}};
  VkImageView imageView;
  NVVK_CHECK(vkCreateImageView(context, &imageViewCreateInfo, nullptr, &imageView));
  debugUtil.setObjectName(imageView, "imageView");

  // Also create an image using linear tiling that can be accessed from the CPU,
  // much like how we created the buffer in the main tutorial. The first image
  // will be entirely local to the GPU for performance, while this image can
  // be mapped to CPU memory. We'll copy data from the first image to this
  // image in order to read the image data back on the CPU.
  // As before, we'll transition the image layout in the same command buffer
  // used to upload the vertex and index buffers.
  imageCreateInfo.tiling  = VK_IMAGE_TILING_LINEAR;
  imageCreateInfo.usage   = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  nvvk::Image imageLinear = allocator.createImage(imageCreateInfo,                           //
                                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT       //
                                                      | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT  //
                                                      | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
  debugUtil.setObjectName(imageLinear.image, "imageLinear");

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
  VkCommandPoolCreateInfo cmdPoolInfo{.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,  //
                                      .queueFamilyIndex = context.m_queueGCT};
  VkCommandPool           cmdPool;
  NVVK_CHECK(vkCreateCommandPool(context, &cmdPoolInfo, nullptr, &cmdPool));
  debugUtil.setObjectName(cmdPool, "cmdPool");

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

    // Also, let's transition the layout of `image` to `VK_IMAGE_LAYOUT_GENERAL`,
    // and the layout of `imageLinear` to `VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL`.
    // Although we use `imageLinear` later, we're transferring its layout as
    // early as possible. For more complex applications, tracking images and
    // operations using a graph is a good way to handle these types of images
    // automatically. However, for this tutorial, we'll show how to write
    // image transitions by hand.

    // To do this, we combine both transitions in a single pipeline barrier.
    // This pipeline barrier will say "Make it so that all writes to memory by
    const VkAccessFlags srcAccesses = 0;  // (since image and imageLinear aren't initially accessible)
    // finish and can be read correctly by
    const VkAccessFlags dstImageAccesses       = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;  // for image
    const VkAccessFlags dstImageLinearAccesses = VK_ACCESS_TRANSFER_WRITE_BIT;  // for imageLinear
    // "

    // Here's how to do that:
    const VkPipelineStageFlags srcStages = nvvk::makeAccessMaskPipelineStageFlags(srcAccesses);
    const VkPipelineStageFlags dstStages = nvvk::makeAccessMaskPipelineStageFlags(dstImageAccesses | dstImageLinearAccesses);
    VkImageMemoryBarrier imageBarriers[2];
    // Image memory barrier for `image` from UNDEFINED to GENERAL layout:
    imageBarriers[0] = nvvk::makeImageMemoryBarrier(image.image,                    // The VkImage
                                                    srcAccesses, dstImageAccesses,  // Source and destination access masks
                                                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,  // Source and destination layouts
                                                    VK_IMAGE_ASPECT_COLOR_BIT);  // Aspects of an image (color, depth, etc.)
    // Image memory barrier for `imageLinear` from UNDEFINED to TRANSFER_DST_OPTIMAL layout:
    imageBarriers[1] = nvvk::makeImageMemoryBarrier(imageLinear.image,                    // The VkImage
                                                    srcAccesses, dstImageLinearAccesses,  // Source and destination access masks
                                                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,  // Source and dst layouts
                                                    VK_IMAGE_ASPECT_COLOR_BIT);  // Aspects of an image (color, depth, etc.)
    // Include the two image barriers in the pipeline barrier:
    vkCmdPipelineBarrier(uploadCmdBuffer,       // The command buffer
                         srcStages, dstStages,  // Src and dst pipeline stages
                         0,                     // Flags for memory dependencies
                         0, nullptr,            // Global memory barrier objects
                         0, nullptr,            // Buffer memory barrier objects
                         2, imageBarriers);     // Image barrier objects

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
    VkAccelerationStructureGeometryTrianglesDataKHR triangles{
        .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .vertexFormat  = VK_FORMAT_R32G32B32_SFLOAT,
        .vertexData    = {.deviceAddress = vertexBufferAddress},
        .vertexStride  = 3 * sizeof(float),
        .maxVertex     = static_cast<uint32_t>(objVertices.size() / 3 - 1),
        .indexType     = VK_INDEX_TYPE_UINT32,
        .indexData     = {.deviceAddress = indexBufferAddress},
        .transformData = {.deviceAddress = 0}  // No transform
    };
    // Create a VkAccelerationStructureGeometryKHR object that says it handles opaque triangles and points to the above:
    VkAccelerationStructureGeometryKHR geometry{.sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
                                                .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
                                                .geometry     = {.triangles = triangles},
                                                .flags        = VK_GEOMETRY_OPAQUE_BIT_KHR};
    blas.asGeometry.push_back(geometry);
    // Create offset info that allows us to say how many triangles and vertices to read
    VkAccelerationStructureBuildRangeInfoKHR offsetInfo{
        .primitiveCount  = static_cast<uint32_t>(objIndices.size() / 3),  // Number of triangles
        .primitiveOffset = 0,                                             // Offset added when looking up triangles
        .firstVertex     = 0,  // Offset added when looking up vertices in the vertex buffer
        .transformOffset = 0   // Offset added when looking up transformation matrices, if we used them
    };
    blas.asBuildOffsetInfo.push_back(offsetInfo);
    blases.push_back(blas);
  }
  // Create the BLAS
  nvvk::RaytracingBuilderKHR raytracingBuilder;
  raytracingBuilder.setup(context, &allocator, context.m_queueGCT);
  raytracingBuilder.buildBlas(blases, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
                                          | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR);

  // Create 441 instances with random rotations pointing to BLAS 0, and build these instances into a TLAS:
  std::vector<VkAccelerationStructureInstanceKHR> instances;
  std::default_random_engine                      randomEngine;  // The random number generator
  std::uniform_real_distribution<float>           uniformDist(-0.5f, 0.5f);
  std::uniform_int_distribution<int>              uniformIntDist(0, 8);
  for(int x = -10; x <= 10; x++)
  {
    for(int y = -10; y <= 10; y++)
    {
      glm::mat4 transform = glm::translate(glm::vec3(0.0f, -1.0f, 0.0f));
      transform           = glm::rotate(uniformDist(randomEngine), glm::vec3(1.0f, 0.0f, 0.0f)) * transform;
      transform           = glm::rotate(uniformDist(randomEngine), glm::vec3(0.0f, 1.0f, 0.0f)) * transform;
      transform           = glm::scale(glm::vec3(1.0f / 2.7f)) * transform;
      transform           = glm::translate(glm::vec3(float(x), float(y), 0.0f)) * transform;

      VkAccelerationStructureInstanceKHR instance{};
      instance.transform = nvvk::toTransformMatrixKHR(transform);
      instance.instanceCustomIndex = 0;  // 24 bits accessible to ray shaders via rayQueryGetIntersectionInstanceCustomIndexEXT
      // The address of the BLAS in `blases` that this instance points to
      instance.accelerationStructureReference = raytracingBuilder.getBlasDeviceAddress(0);
      // Used for a shader offset index, accessible via rayQueryGetIntersectionInstanceShaderBindingTableRecordOffsetEXT
      instance.instanceShaderBindingTableRecordOffset = uniformIntDist(randomEngine);
      instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;  // How to trace this instance
      instance.mask  = 0xFF;
      instances.push_back(instance);
    }
  }
  raytracingBuilder.buildTlas(instances, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);

  // Here's the list of bindings for the descriptor set layout, from raytrace.comp.glsl:
  // 0 - a storage image (the image `image`)
  // 1 - an acceleration structure (the TLAS)
  // 2 - a storage buffer (the vertex buffer)
  // 3 - a storage buffer (the index buffer)
  nvvk::DescriptorSetContainer descriptorSetContainer(context);
  descriptorSetContainer.addBinding(BINDING_IMAGEDATA, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT);
  descriptorSetContainer.addBinding(BINDING_TLAS, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_COMPUTE_BIT);
  descriptorSetContainer.addBinding(BINDING_VERTICES, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
  descriptorSetContainer.addBinding(BINDING_INDICES, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
  // Create a layout from the list of bindings
  descriptorSetContainer.initLayout();
  // Create a descriptor pool from the list of bindings with space for 1 set, and allocate that set
  descriptorSetContainer.initPool(1);
  // Create a push constant range describing the amount of data for the push constants.
  static_assert(sizeof(PushConstants) % 4 == 0, "Push constant size must be a multiple of 4 per the Vulkan spec!");
  VkPushConstantRange pushConstantRange{.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,  //
                                        .offset     = 0,                            //
                                        .size       = sizeof(PushConstants)};
  // Create a pipeline layout from the descriptor set layout and push constant range:
  descriptorSetContainer.initPipeLayout(1,                    // Number of push constant ranges
                                        &pushConstantRange);  // Pointer to push constant ranges

  // Write values into the descriptor set.
  std::array<VkWriteDescriptorSet, 4> writeDescriptorSets;
  // Color image
  VkDescriptorImageInfo descriptorImageInfo{.imageView   = imageView,  // How the image should be accessed
                                            .imageLayout = VK_IMAGE_LAYOUT_GENERAL};  // The image's layout
  writeDescriptorSets[0] = descriptorSetContainer.makeWrite(0 /*set index*/, BINDING_IMAGEDATA /*binding*/, &descriptorImageInfo);
  // Top-level acceleration structure (TLAS)
  VkAccelerationStructureKHR tlasCopy = raytracingBuilder.getAccelerationStructure();  // So that we can take its address
  VkWriteDescriptorSetAccelerationStructureKHR descriptorAS{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
                                                            .accelerationStructureCount = 1,
                                                            .pAccelerationStructures    = &tlasCopy};
  writeDescriptorSets[1] = descriptorSetContainer.makeWrite(0, BINDING_TLAS, &descriptorAS);
  // Vertex buffer
  VkDescriptorBufferInfo vertexDescriptorBufferInfo{.buffer = vertexBuffer.buffer, .range = VK_WHOLE_SIZE};
  writeDescriptorSets[2] = descriptorSetContainer.makeWrite(0, BINDING_VERTICES, &vertexDescriptorBufferInfo);
  // Index buffer
  VkDescriptorBufferInfo indexDescriptorBufferInfo{.buffer = indexBuffer.buffer, .range = VK_WHOLE_SIZE};
  writeDescriptorSets[3] = descriptorSetContainer.makeWrite(0, BINDING_INDICES, &indexDescriptorBufferInfo);
  vkUpdateDescriptorSets(context,                                            // The context
                         static_cast<uint32_t>(writeDescriptorSets.size()),  // Number of VkWriteDescriptorSet objects
                         writeDescriptorSets.data(),                         // Pointer to VkWriteDescriptorSet objects
                         0, nullptr);  // An array of VkCopyDescriptorSet objects (unused)

  // Shader loading and pipeline creation
  VkShaderModule rayTraceModule =
      nvvk::createShaderModule(context, nvh::loadFile("shaders/raytrace.comp.glsl.spv", true, searchPaths));
  debugUtil.setObjectName(rayTraceModule, "rayTraceModule");

  // Describes the entrypoint and the stage to use for this shader module in the pipeline
  VkPipelineShaderStageCreateInfo shaderStageCreateInfo{.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                        .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
                                                        .module = rayTraceModule,
                                                        .pName  = "main"};

  // Create the compute pipeline
  VkComputePipelineCreateInfo pipelineCreateInfo{.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                                 .stage  = shaderStageCreateInfo,
                                                 .layout = descriptorSetContainer.getPipeLayout()};
  // Don't modify flags, basePipelineHandle, or basePipelineIndex
  VkPipeline computePipeline;
  NVVK_CHECK(vkCreateComputePipelines(context,                 // Device
                                      VK_NULL_HANDLE,          // Pipeline cache (uses default)
                                      1, &pipelineCreateInfo,  // Compute pipeline create info
                                      nullptr,                 // Allocator (uses default)
                                      &computePipeline));      // Output
  debugUtil.setObjectName(computePipeline, "computePipeline");

  const uint32_t NUM_SAMPLE_BATCHES = 32;
  for(uint32_t sampleBatch = 0; sampleBatch < NUM_SAMPLE_BATCHES; sampleBatch++)
  {
    // Create and start recording a command buffer
    VkCommandBuffer cmdBuffer = AllocateAndBeginOneTimeCommandBuffer(context, cmdPool);

    // Bind the compute shader pipeline
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
    // Bind the descriptor set
    VkDescriptorSet descriptorSet = descriptorSetContainer.getSet(0);
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, descriptorSetContainer.getPipeLayout(), 0, 1,
                            &descriptorSet, 0, nullptr);

    // Push push constants:
    pushConstants.sample_batch = sampleBatch;
    vkCmdPushConstants(cmdBuffer,                               // Command buffer
                       descriptorSetContainer.getPipeLayout(),  // Pipeline layout
                       VK_SHADER_STAGE_COMPUTE_BIT,             // Stage flags
                       0,                                       // Offset
                       sizeof(PushConstants),                   // Size in bytes
                       &pushConstants);                         // Data

    // Run the compute shader with enough workgroups to cover the entire buffer:
    vkCmdDispatch(cmdBuffer, (render_width + WORKGROUP_WIDTH - 1) / WORKGROUP_WIDTH,
                  (render_height + WORKGROUP_HEIGHT - 1) / WORKGROUP_HEIGHT, 1);

    // On the last sample batch:
    if(sampleBatch == NUM_SAMPLE_BATCHES - 1)
    {
      // Transition `image` from GENERAL to TRANSFER_SRC_OPTIMAL layout. See the
      // code for uploadCmdBuffer above to see a description of what this does:
      const VkAccessFlags        srcAccesses = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
      const VkAccessFlags        dstAccesses = VK_ACCESS_TRANSFER_READ_BIT;
      const VkPipelineStageFlags srcStages   = nvvk::makeAccessMaskPipelineStageFlags(srcAccesses);
      const VkPipelineStageFlags dstStages   = nvvk::makeAccessMaskPipelineStageFlags(dstAccesses);
      const VkImageMemoryBarrier barrier =
          nvvk::makeImageMemoryBarrier(image.image,               // The VkImage
                                       srcAccesses, dstAccesses,  // Src and dst access masks
                                       VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,  // Src and dst layouts
                                       VK_IMAGE_ASPECT_COLOR_BIT);
      vkCmdPipelineBarrier(cmdBuffer,             // Command buffer
                           srcStages, dstStages,  // Src and dst pipeline stages
                           0,                     // Dependency flags
                           0, nullptr,            // Global memory barriers
                           0, nullptr,            // Buffer memory barriers
                           1, &barrier);          // Image memory barriers

      // Now, copy the image (which has layout TRANSFER_SRC_OPTIMAL) to imageLinear
      // (which has layout TRANSFER_DST_OPTIMAL).
      {
        // We copy image color, mip 0, layer 0:
        VkImageCopy region{.srcSubresource = {.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,  //
                                              .mipLevel       = 0,                          //
                                              .baseArrayLayer = 0,                          //
                                              .layerCount     = 1},
                           // (0, 0, 0) in the first image corresponds to (0, 0, 0) in the second image:
                           .srcOffset      = {0, 0, 0},
                           .dstSubresource = region.srcSubresource,
                           .dstOffset      = {0, 0, 0},
                           // Copy the entire image:
                           .extent = {render_width, render_height, 1}};
        vkCmdCopyImage(cmdBuffer,                             // Command buffer
                       image.image,                           // Source image
                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,  // Source image layout
                       imageLinear.image,                     // Destination image
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,  // Destination image layout
                       1, &region);                           // Regions
      }

      // Add a command that says "Make it so that memory writes by transfers
      // are available to read from the CPU." (In other words, "Flush the GPU caches
      // so the CPU can read the data.") To do this, we use a memory barrier.
      VkMemoryBarrier memoryBarrier{.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                                    .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,  // Make transfer writes
                                    .dstAccessMask = VK_ACCESS_HOST_READ_BIT};      // Readable by the CPU
      vkCmdPipelineBarrier(cmdBuffer,                                               // The command buffer
                           VK_PIPELINE_STAGE_TRANSFER_BIT,                          // From transfers
                           VK_PIPELINE_STAGE_HOST_BIT,                              // To the CPU
                           0,                                                       // No special flags
                           1, &memoryBarrier,                                       // An array of memory barriers
                           0, nullptr, 0, nullptr);                                 // No other barriers
    }

    // End and submit the command buffer, then wait for it to finish:
    EndSubmitWaitAndFreeCommandBuffer(context, context.m_queueGCT, cmdPool, cmdBuffer);

    nvprintf("Rendered sample batch index %d.\n", sampleBatch);
  }

  // Get the image data back from the GPU
  void* data = allocator.map(imageLinear);
  stbi_write_hdr("out.hdr", render_width, render_height, 4, reinterpret_cast<float*>(data));
  allocator.unmap(imageLinear);

  vkDestroyPipeline(context, computePipeline, nullptr);
  vkDestroyShaderModule(context, rayTraceModule, nullptr);
  descriptorSetContainer.deinit();
  raytracingBuilder.destroy();
  allocator.destroy(vertexBuffer);
  allocator.destroy(indexBuffer);
  vkDestroyCommandPool(context, cmdPool, nullptr);
  allocator.destroy(imageLinear);
  vkDestroyImageView(context, imageView, nullptr);
  allocator.destroy(image);
  allocator.deinit();
  context.deinit();  // Don't forget to clean up at the end of the program!
}
