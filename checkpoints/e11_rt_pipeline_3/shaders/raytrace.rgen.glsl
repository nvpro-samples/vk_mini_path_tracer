// Copyright 2020 NVIDIA Corporation
// SPDX-License-Identifier: Apache-2.0
#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require
#include "../common.h"
#include "shaderCommon.h"

// Binding BINDING_IMAGEDATA in set 0 is a storage image with four 32-bit floating-point channels,
// defined using a uniform image2D variable.
layout(binding = BINDING_IMAGEDATA, set = 0, rgba32f) uniform image2D storageImage;
layout(binding = BINDING_TLAS, set = 0) uniform accelerationStructureEXT tlas;

layout(push_constant) uniform PushConsts
{
  PushConstants pushConstants;
};

// Ray payloads are used to send information between shaders.
layout(location = 0) rayPayloadEXT PassableInfo pld;

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

  // State of the random number generator with an initial seed.
  pld.rngState = uint((pushConstants.sample_batch * resolution.y + pixel.y) * resolution.x + pixel.x);

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
    const vec2 randomPixelCenter = vec2(pixel) + vec2(0.5) + 0.375 * randomGaussian(pld.rngState);
    const vec2 screenUV          = vec2((2.0 * randomPixelCenter.x - resolution.x) / resolution.y,    //
                               -(2.0 * randomPixelCenter.y - resolution.y) / resolution.y);  // Flip the y axis
    // Create a ray direction:
    vec3 rayDirection = vec3(fovVerticalSlope * screenUV.x, fovVerticalSlope * screenUV.y, -1.0);
    rayDirection      = normalize(rayDirection);

    vec3 accumulatedRayColor = vec3(1.0);  // The amount of light that made it to the end of the current ray.

    // Limit the kernel to trace at most 32 segments.
    for(int tracedSegments = 0; tracedSegments < 32; tracedSegments++)
    {
      // Trace the ray into the scene and get data back!
      traceRayEXT(tlas,                  // Top-level acceleration structure
                  gl_RayFlagsOpaqueEXT,  // Ray flags, here saying "treat all geometry as opaque"
                  0xFF,                  // 8-bit instance mask, here saying "trace against all instances"
                  0,                     // SBT record offset
                  0,                     // SBT record stride for offset
                  0,                     // Miss index
                  rayOrigin,             // Ray origin
                  0.0,                   // Minimum t-value
                  rayDirection,          // Ray direction
                  10000.0,               // Maximum t-value
                  0);                    // Location of payload

      // Compute the amount of light that returns to this sample from the ray
      accumulatedRayColor *= pld.color;

      if(pld.rayHitSky)
      {
        // Done tracing this ray.
        // Sum this with the pixel's other samples.
        // (Note that we treat a ray that didn't find a light source as if it had
        // an accumulated color of (0, 0, 0)).
        summedPixelColor += accumulatedRayColor;

        break;
      }
      else
      {
        // Start a new segment
        rayOrigin    = pld.rayOrigin;
        rayDirection = pld.rayDirection;
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