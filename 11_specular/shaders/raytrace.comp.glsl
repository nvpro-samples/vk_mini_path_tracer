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
layout(binding = 2, set = 0, scalar) buffer Vertices
{
  vec3 vertices[];
};
layout(binding = 3, set = 0, scalar) buffer Indices
{
  uint indices[];
};

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

struct HitInfo
{
  vec3 color;
  vec3 worldPosition;
  vec3 worldNormal;
};

HitInfo getObjectHitInfo(rayQueryEXT rayQuery)
{
  HitInfo result;
  // Get the ID of the triangle
  const int primitiveID = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, true);

  // Get the indices of the vertices of the triangle
  const uint i0 = indices[3 * primitiveID + 0];
  const uint i1 = indices[3 * primitiveID + 1];
  const uint i2 = indices[3 * primitiveID + 2];

  // Get the vertices of the triangle
  const vec3 v0 = vertices[i0];
  const vec3 v1 = vertices[i1];
  const vec3 v2 = vertices[i2];

  // Get the barycentric coordinates of the intersection
  vec3 barycentrics = vec3(0.0, rayQueryGetIntersectionBarycentricsEXT(rayQuery, true));
  barycentrics.x    = 1.0 - barycentrics.y - barycentrics.z;

  // Compute the coordinates of the intersection
  const vec3 objectPos = v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
  // For the main tutorial, object space is the same as world space:
  result.worldPosition = objectPos;

  // Compute the normal of the triangle in object space, using the right-hand rule:
  //    v2      .
  //    |\      .
  //    | \     .
  //    |/ \    .
  //    /   \   .
  //   /|    \  .
  //  L v0---v1 .
  // n
  const vec3 objectNormal = normalize(cross(v1 - v0, v2 - v0));
  // For the main tutorial, object space is the same as world space:
  result.worldNormal = objectNormal;

  result.color = vec3(0.7f);

  return result;
}

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
  rayDirection                 = normalize(rayDirection);

  vec3 accumulatedRayColor = vec3(1.0);  // The amount of light that made it to the end of the current ray.
  vec3 pixelColor          = vec3(0.0);

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
      // Ray hit a triangle
      HitInfo hitInfo = getObjectHitInfo(rayQuery);

      // Apply color absorption
      accumulatedRayColor *= hitInfo.color;

      // Flip the normal so it points against the ray direction:
      hitInfo.worldNormal = faceforward(hitInfo.worldNormal, rayDirection, hitInfo.worldNormal);

      // Start a new ray at the hit position, but offset it slightly along the normal:
      rayOrigin = hitInfo.worldPosition + 0.0001 * hitInfo.worldNormal;

      // Reflect the direction of the ray using the triangle normal:
      rayDirection = reflect(rayDirection, hitInfo.worldNormal);
    }
    else
    {
      // Ray hit the sky
      pixelColor = accumulatedRayColor * skyColor(rayDirection);
      break;
    }
  }

  // Get the index of this invocation in the buffer:
  uint linearIndex       = resolution.x * pixel.y + pixel.x;
  imageData[linearIndex] = pixelColor;
}