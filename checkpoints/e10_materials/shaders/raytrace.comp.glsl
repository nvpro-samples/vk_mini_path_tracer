// Copyright 2020 NVIDIA Corporation
// SPDX-License-Identifier: Apache-2.0
#version 460
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_ray_query : require
#extension GL_GOOGLE_include_directive : require
#include "../common.h"

layout(local_size_x = WORKGROUP_WIDTH, local_size_y = WORKGROUP_HEIGHT, local_size_z = 1) in;

// Binding BINDING_IMAGEDATA in set 0 is a storage image with four 32-bit floating-point channels,
// defined using a uniform image2D variable.
layout(binding = BINDING_IMAGEDATA, set = 0, rgba32f) uniform image2D storageImage;
layout(binding = BINDING_TLAS, set = 0) uniform accelerationStructureEXT tlas;
// The scalar layout qualifier here means to align types according to the alignment
// of their scalar components, instead of e.g. padding them to std140 rules.
layout(binding = BINDING_VERTICES, set = 0, scalar) buffer Vertices
{
  vec3 vertices[];
};
layout(binding = BINDING_INDICES, set = 0, scalar) buffer Indices
{
  uint indices[];
};

layout(push_constant) uniform PushConsts
{
  PushConstants pushConstants;
};

#include "shaderCommon.h"

// Uses the Box-Muller transform to return a normally distributed (centered
// at 0, standard deviation 1) 2D point.
vec2 randomGaussian(inout uint rngState)
{
  // Almost uniform in (0, 1] - make sure the value is never 0:
  const float u1    = max(1e-38, stepAndOutputRNGFloat(rngState));
  const float u2    = stepAndOutputRNGFloat(rngState);  // In [0, 1]
  const float r     = sqrt(-2.0 * log(u1));
  const float theta = 2 * k_pi * u2;  // Random in [0, 2pi]
  return r * vec2(cos(theta), sin(theta));
}

// Returns the color of the sky in a given direction (in linear color space)
vec3 skyColor(vec3 direction)
{
  // +y in world space is up, so:
  if(direction.y > 0.0f)
  {
    return mix(vec3(1.0f), vec3(0.25f, 0.5f, 1.0f), direction.y);
  }
  else
  {
    return vec3(0.03f);
  }
}

void main()
{
  // The resolution of the image:
  const ivec2 resolution = imageSize(storageImage);

  // Get the coordinates of the pixel for this invocation:
  //
  // .-------.-> x
  // |       |
  // |       |
  // '-------'
  // v
  // y
  const ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);

  // If the pixel is outside of the image, don't do anything:
  if((pixel.x >= resolution.x) || (pixel.y >= resolution.y))
  {
    return;
  }

  // State of the random number generator with an initial seed.
  uint rngState = uint((pushConstants.sample_batch * resolution.y + pixel.y) * resolution.x + pixel.x);

  // This scene uses a right-handed coordinate system like the OBJ file format, where the
  // +x axis points right, the +y axis points up, and the -z axis points into the screen.
  // The camera is located at (-0.001, 0, 53).
  const vec3 cameraOrigin = vec3(-0.001, 0.0, 53.0);
  // Define the field of view by the vertical slope of the topmost rays:
  const float fovVerticalSlope = 1.0 / 5.0;

  // The sum of the colors of all of the samples.
  vec3 summedPixelColor = vec3(0.0);

  // Limit the kernel to trace at most 64 samples.
  const int NUM_SAMPLES = 64;
  for(int sampleIdx = 0; sampleIdx < NUM_SAMPLES; sampleIdx++)
  {
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
    // Use a Gaussian with standard deviation 0.375 centered at the center of
    // the pixel:
    const vec2 randomPixelCenter = vec2(pixel) + vec2(0.5) + 0.375 * randomGaussian(rngState);
    const vec2 screenUV          = vec2((2.0 * randomPixelCenter.x - resolution.x) / resolution.y,    //
                               -(2.0 * randomPixelCenter.y - resolution.y) / resolution.y);  // Flip the y axis
    // Create a ray direction:
    vec3 rayDirection = vec3(fovVerticalSlope * screenUV.x, fovVerticalSlope * screenUV.y, -1.0);
    rayDirection      = normalize(rayDirection);

    vec3 accumulatedRayColor = vec3(1.0);  // The amount of light that made it to the end of the current ray.

    // Limit the kernel to trace at most 32 segments.
    for(int tracedSegments = 0; tracedSegments < 32; tracedSegments++)
    {
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

      // Get the type of committed (true) intersection - nothing, a triangle, or
      // a generated object
      if(rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT)
      {
        // Get the ID of the shader:
        const int sbtOffset = int(rayQueryGetIntersectionInstanceShaderBindingTableRecordOffsetEXT(rayQuery, true));

        // Get information about the absorption, new ray origin, and new ray color:
        ReturnedInfo returnedInfo;
        switch(sbtOffset)
        {
          case 0:
            returnedInfo = material0(rayQuery, rngState);
            break;
          case 1:
            returnedInfo = material1(rayQuery, rngState);
            break;
          case 2:
            returnedInfo = material2(rayQuery, rngState);
            break;
          case 3:
            returnedInfo = material3(rayQuery, rngState);
            break;
          case 4:
            returnedInfo = material4(rayQuery, rngState);
            break;
          case 5:
            returnedInfo = material5(rayQuery, rngState);
            break;
          case 6:
            returnedInfo = material6(rayQuery, rngState);
            break;
          case 7:
            returnedInfo = material7(rayQuery, rngState);
            break;
          default:
            returnedInfo = material8(rayQuery, rngState);
            break;
        }

        // Apply color absorption
        accumulatedRayColor *= returnedInfo.color;

        // Start a new segment
        rayOrigin    = returnedInfo.rayOrigin;
        rayDirection = returnedInfo.rayDirection;
      }
      else
      {
        // Ray hit the sky
        accumulatedRayColor *= skyColor(rayDirection);

        // Sum this with the pixel's other samples.
        // (Note that we treat a ray that didn't find a light source as if it had
        // an accumulated color of (0, 0, 0)).
        summedPixelColor += accumulatedRayColor;

        break;
      }
    }
  }

  // Blend with the averaged image in the buffer:
  vec3 averagePixelColor = summedPixelColor / float(NUM_SAMPLES);
  if(pushConstants.sample_batch != 0)
  {
    // Read the storage image:
    const vec3 previousAverageColor = imageLoad(storageImage, pixel).rgb;
    // Compute the new average:
    averagePixelColor =
        (pushConstants.sample_batch * previousAverageColor + averagePixelColor) / (pushConstants.sample_batch + 1);
  }
  // Set the color of the pixel `pixel` in the storage image to `averagePixelColor`:
  imageStore(storageImage, pixel, vec4(averagePixelColor, 0.0));
}