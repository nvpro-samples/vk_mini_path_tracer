// Copyright 2020 NVIDIA Corporation
// SPDX-License-Identifier: Apache-2.0
#version 460 
#extension GL_EXT_scalar_block_layout : require

layout(local_size_x = 16, local_size_y = 8, local_size_z = 1) in;

// The scalar layout qualifier here means to align types according to the alignment
// of their scalar components, instead of e.g. padding them to std140 rules.
layout(binding = 0, set = 0, scalar) buffer storageBuffer
{
  vec3 imageData[];
};

void main()
{
  // The resolution of the buffer, which in this case is a hardcoded vector
  // of 2 unsigned integers:
  const uvec2 resolution = uvec2(800, 600);

  // Get the coordinates of the pixel for this invocation:
  //
  // .-------.-> x
  // |       |
  // |       |
  // '-------'
  // v
  // y
  const uvec2 pixel = gl_GlobalInvocationID.xy;

  // If the pixel is outside of the image, don't do anything:
  if((pixel.x >= resolution.x) || (pixel.y >= resolution.y))
  {
    return;
  }

  // Create a vector of 3 floats with a different color per pixel.
  const vec3 pixelColor = vec3(float(pixel.x) / resolution.x,  // Red
                               float(pixel.y) / resolution.y,  // Green
                               0.0);                           // Blue
  // Get the index of this invocation in the buffer:
  uint linearIndex = resolution.x * pixel.y + pixel.x;
  // Write the color to the buffer.
  imageData[linearIndex] = pixelColor;
}