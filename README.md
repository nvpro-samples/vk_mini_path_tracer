![logo](http://nvidianews.nvidia.com/_ir/219/20157/NV_Designworks_logo_horizontal_greenblack.png)

# vk_mini_path_tracer

A relatively small, beginner-friendly path tracing tutorial.

:arrow_forward: **[Load the tutorial!](https://nvpro-samples.github.io/vk_mini_path_tracer/index.html)** :arrow_backward:

This tutorial is a beginner-friendly introduction to writing your own fast,
photorealistic path tracer in less than 300 lines of C++ code and 250 lines of
GLSL shader code using Vulkan. Here's an example of what you'll render at the
end of this tutorial!

![](docs/images/12-vk_mini_path_tracer.png)

Vulkan is a low-level API for programming GPUs – fast, highly parallel processors.
It works on a wide variety of platforms – everything from workstations, to
gaming consoles, to tablets and mobile phones, to edge devices.

Vulkan is usually known as a complex API, but I believe that when presented in
the right way, it's possible to make learning Vulkan accessible to people of all
skill levels, whether they're never programmed graphics before or whether
they're a seasoned rendering engineer. Perhaps surprisingly, one of the best
ways to introduce Vulkan may be with GPU path tracing, because the API involved
is relatively small.

We'll show how to write a small path tracer, using the NVVK helpers, included in
the nvpro-samples framework, to help with some Vulkan calls when needed.
For advanced readers, we'll also optionally talk about performance tips and some
of the implementation details inside the helpers and Vulkan itself.

The final program uses less than 300 lines of C++ and less than 250 lines of GLSL shader code, including comments. You can find it [here](https://github.com/nvpro-samples/vk_mini_path_tracer/blob/main/vk_mini_path_tracer).

Here are all the Vulkan functions, and NVVK functions and objects, that we'll use in the main tutorial:

| **Vulkan Functions**     |                          |                          |
| ------------------------ | ------------------------ | ------------------------ |
| vkAllocateCommandBuffers | vkBeginCommandBuffer     | vkCmdBindDescriptorSets  |
| vkCmdBindPipeline        | vkCmdDispatch            | vkCmdFillBuffer          |
| vkCmdPipelineBarrier     | vkCreateCommandPool      | vkCreateComputePipelines |
| vkDestroyCommandPool     | vkDestroyPipeline        | vkDestroyShaderModule    |
| vkFreeCommandBuffers     | vkGetBufferDeviceAddress | vkQueueSubmit            |
| vkQueueWaitIdle          | vkUpdateDescriptorSets   |                          |

| **NVVK Functions and Objects** |                            |                                  |
| ------------------------------ | -------------------------- | -------------------------------- |
| nvvk::Buffer                   | NVVK_CHECK                 | nvvk::Context                    |
| nvvk::ContextCreateInfo        | nvvk::createShaderModule   | nvvk::DescriptorSetContainer     |
| nvvk::make                     | nvvk::RayTracingBuilderKHR | nvvk::ResourceAllocatorDedicated |

-------

## Chapters

| **Chapter**                                                  |                                                              |                                                              |                                                              |
| ------------------------------------------------------------ | ------------------------------------------------------------ | ------------------------------------------------------------ | ------------------------------------------------------------ |
| ![small](docs/images/1-thumbnail.png)<br/>[Hello, Vulkan!](https://nvpro-samples.github.io/vk_mini_path_tracer/index.html#hello,vulkan!) | ![small](docs/images/2-thumbnail.png)<br/>[Device Extensions and Vulkan Objects](https://nvpro-samples.github.io/vk_mini_path_tracer/index.html#deviceextensionsandvulkanobjects) | ![small](docs/images/3-thumbnail.png)<br/>[Memory and Commands](https://nvpro-samples.github.io/vk_mini_path_tracer/index.html#memory) | ![small](docs/images/4-gray.png)<br/>[Writing an Image](https://nvpro-samples.github.io/vk_mini_path_tracer/index.html#writinganimage) |
| ![small](docs/images/5-thumbnail.png)<br/>[Compute Shaders](https://nvpro-samples.github.io/vk_mini_path_tracer/index.html#computeshaders) | ![small](docs/images/6-descriptors.png)<br/>[Descriptors](https://nvpro-samples.github.io/vk_mini_path_tracer/index.html#descriptors) | ![small](docs/images/7-depthMap.png)<br/>[Acceleration Structures and Ray Tracing](https://nvpro-samples.github.io/vk_mini_path_tracer/index.html#accelerationstructuresandraytracing) | ![small](docs/images/8-barycentricCoordinates.png)<br/>[Four Uses of Intersection Data](https://nvpro-samples.github.io/vk_mini_path_tracer/index.html#fourusesofintersectiondata) |
| ![small](docs/images/9-normals.png)<br/>[Accessing Mesh Data](https://nvpro-samples.github.io/vk_mini_path_tracer/index.html#accessingmeshdata) | ![small](docs/images/10-reflectionPt3.png)<br/>[Perfectly Specular Reflections](https://nvpro-samples.github.io/vk_mini_path_tracer/index.html#perfectlyspecularreflections) | ![small](docs/images/11-randomNoise.png)<br/>[Antialiasing and Pseudorandom Number Generation](https://nvpro-samples.github.io/vk_mini_path_tracer/index.html#antialiasingandpseudorandomnumbergeneration) | ![small](docs/images/12-vk_mini_path_tracer.png)<br/>[Diffuse Reflection](https://nvpro-samples.github.io/vk_mini_path_tracer/index.html#diffusereflection) |

## Extra Chapters

These are optional, extra tutorials that show how to polish and add new features to the main tutorial's path tracer. Make sure to check out the [list of further Vulkan and ray tracing resources](https://nvpro-samples.github.io/vk_mini_path_tracer/index.html#pnext:goingfurther/furtherreading) at the end of the main tutorial as well!

| **Extra Chapter**                                            |                                                              |                                                              |                                                              |
| ------------------------------------------------------------ | ------------------------------------------------------------ | ------------------------------------------------------------ | ------------------------------------------------------------ |
| ![](docs/images/e1-gaussianBlur.png)<br/> [Gaussian Filter Antialiasing](https://nvpro-samples.github.io/vk_mini_path_tracer/extras.html#gaussianfilterantialiasing) | ![](docs/images/e2-zoomRange.png)<br/>[Measuring Performance](https://nvpro-samples.github.io/vk_mini_path_tracer/extras.html#measuringperformance) | ![](docs/images/e3-thumbnail.png)<br/>[Compaction](https://nvpro-samples.github.io/vk_mini_path_tracer/extras.html#compaction) | ![](docs/images/e4-thumbnail.png)<br/>[Including Files and Matching Values Between C++ And GLSL](https://nvpro-samples.github.io/vk_mini_path_tracer/extras.html#includingfilesandmatchingvaluesbetweenc++andglsl) |
| ![](docs/images/e5-1024-600.png)<br/> [Push Constants](https://nvpro-samples.github.io/vk_mini_path_tracer/extras.html#pushconstants) | ![](docs/images/e6-output.png)<br/>[More Samples](https://nvpro-samples.github.io/vk_mini_path_tracer/extras.html#moresamples) | ![](docs/images/e7-sparse.png)<br/>[Images](https://nvpro-samples.github.io/vk_mini_path_tracer/extras.html#images) | ![](docs/images/e8-thumbnail.png)<br/>[Debug Names](https://nvpro-samples.github.io/vk_mini_path_tracer/extras.html#debugnames) |
| ![](docs/images/e9-result.png)<br/>[Instances and Transformation Matrices](https://nvpro-samples.github.io/vk_mini_path_tracer/extras.html#instancesandtransformationmatrices) | ![](docs/images/e10-closeup.png)<br/>[Multiple Materials](https://nvpro-samples.github.io/vk_mini_path_tracer/extras.html#multiplematerials) | ![](docs/images/e11-output-3.png)<br/>[Ray Tracing Pipelines](https://nvpro-samples.github.io/vk_mini_path_tracer/extras.html#raytracingpipelines) |                                                              |

## Building and Running

Please see the instructions [here](https://nvpro-samples.github.io/vk_mini_path_tracer/index.html#hello,vulkan!/settingupyourdevelopmentenvironment).

