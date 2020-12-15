// Copyright 2020 NVIDIA Corporation
// SPDX-License-Identifier: Apache-2.0
#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require
#include "../common.h"

// Binding BINDING_IMAGEDATA in set 0 is a storage image with four 32-bit floating-point channels,
// defined using a uniform image2D variable.
layout(binding = BINDING_IMAGEDATA, set = 0, rgba32f) uniform image2D storageImage;

void main()
{
  // The resolution of the image, which is the same as the launch size:
  const ivec2 resolution = imageSize(storageImage);

  // Get the coordinates of the pixel for this invocation:
  //
  // .-------.-> x
  // |       |
  // |       |
  // '-------'
  // v
  // y
  const ivec2 pixel = ivec2(gl_LaunchIDEXT.xy);

  // If the pixel is outside of the image, don't do anything:
  if((pixel.x >= resolution.x) || (pixel.y >= resolution.y))
  {
    return;
  }

  // Set the color of the pixel `pixel` in the storage image as follows:
  vec4 color = vec4(pixel.x / float(resolution.x),  //
                    pixel.y / float(resolution.y),  //
                    0.0,                            //
                    0.0);
  imageStore(storageImage, pixel, color);
}