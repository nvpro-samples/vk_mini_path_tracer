// Copyright 2020 NVIDIA Corporation
// SPDX-License-Identifier: Apache-2.0
#version 460
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_ray_query : require

layout(local_size_x = 16, local_size_y = 8, local_size_z = 1) in;

// The scalar layout qualifier here means to align types according to the alignment
// of their scalar components, instead of e.g. padding them to std140 rules.
layout(binding = 0, set = 0, scalar) buffer storageBuffer
{
  vec3 imageData[];
};
layout(binding = 1, set = 0) uniform accelerationStructureEXT tlas;

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

  // This scene uses a right-handed coordinate system like the OBJ file format, where the
  // +x axis points right, the +y axis points up, and the -z axis points into the screen.
  // The camera is located at (-0.001, 1, 6).
  const vec3 cameraOrigin = vec3(-0.001, 1.0, 6.0);
  // Rays always originate at the camera for now. In the future, they'll
  // bounce around the scene.
  vec3 rayOrigin = cameraOrigin;
  // Compute the direction of the ray for this pixel. To do this, we first
  // transform the screen coordinates to look like this, where a is the
  // aspect ratio (width/height) of the screen:
  //           1
  //    .------+------.
  //    |      |      |
  // -a + ---- 0 ---- + a
  //    |      |      |
  //    '------+------'
  //          -1
  const vec2 screenUV = vec2((2.0 * float(pixel.x) + 1.0 - resolution.x) / resolution.y,    //
                             -(2.0 * float(pixel.y) + 1.0 - resolution.y) / resolution.y);  // Flip the y axis
  // Next, define the field of view by the vertical slope of the topmost rays,
  // and create a ray direction:
  const float fovVerticalSlope = 1.0 / 5.0;
  vec3        rayDirection     = vec3(fovVerticalSlope * screenUV.x, fovVerticalSlope * screenUV.y, -1.0);

  // Trace the ray and see if and where it intersects the scene!
  // First, initialize a ray query object:
  rayQueryEXT rayQuery;
  rayQueryInitializeEXT(rayQuery,              // Ray query
                        tlas,                  // Top-level acceleration structure
                        gl_RayFlagsOpaqueEXT,  // Ray flags, here saying "treat all geometry as opaque"
                        0xFF,                  // 8-bit instance mask, here saying "trace against all instances"
                        rayOrigin,             // Ray origin
                        0.0,                   // Minimum t-value
                        rayDirection,          // Ray direction
                        10000.0);              // Maximum t-value

  // Start traversal, and loop over all ray-scene intersections. When this finishes,
  // rayQuery stores a "committed" intersection, the closest intersection (if any).
  while(rayQueryProceedEXT(rayQuery))
  {
  }

  vec3 pixelColor;
  // Get the type of committed (true) intersection - nothing, a triangle, or
  // a generated object
  if(rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT)
  {
    // Create a vec3(0, b.x, b.y)
    pixelColor = vec3(0.0, rayQueryGetIntersectionBarycentricsEXT(rayQuery, true));
    // Set the first element to 1 - b.x - b.y, setting pixelColor to
    // (1 - b.x - b.y, b.x, b.y).
    pixelColor.x = 1 - pixelColor.y - pixelColor.z;
  }
  else
  {
    // Ray hit the sky
    pixelColor = vec3(0.0, 0.0, 0.5);
  }

  // Get the index of this invocation in the buffer:
  uint linearIndex       = resolution.x * pixel.y + pixel.x;
  imageData[linearIndex] = pixelColor;
}