// Copyright 2020 NVIDIA Corporation
// SPDX-License-Identifier: Apache-2.0

// Common file to make closest-hit GLSL shaders shorter to write.
// At the moment, each .glsl file can only have a single entry point, even
// though SPIR-V supports multiple entry points per module - this is why
// we have many small .rchit.glsl files.
#ifndef VK_MINI_PATH_TRACER_CLOSEST_HIT_COMMON_H
#define VK_MINI_PATH_TRACER_CLOSEST_HIT_COMMON_H

#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#include "../common.h"
#include "shaderCommon.h"

// This will store two of the barycentric coordinates of the intersection when
// closest-hit shaders are called:
hitAttributeEXT vec2 attributes;

// These shaders can access the vertex and index buffers:
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

// The payload:
layout(location = 0) rayPayloadInEXT PassableInfo pld;

struct HitInfo
{
  vec3 objectPosition;
  vec3 worldPosition;
  vec3 worldNormal;
  vec3 color;
};

// Gets hit info about the object at the intersection. This uses GLSL variables
// defined in closest hit stages instead of ray queries.
HitInfo getObjectHitInfo()
{
  HitInfo result;
  // Get the ID of the triangle
  const int primitiveID = gl_PrimitiveID;

  // Get the indices of the vertices of the triangle
  const uint i0 = indices[3 * primitiveID + 0];
  const uint i1 = indices[3 * primitiveID + 1];
  const uint i2 = indices[3 * primitiveID + 2];

  // Get the vertices of the triangle
  const vec3 v0 = vertices[i0];
  const vec3 v1 = vertices[i1];
  const vec3 v2 = vertices[i2];


  // Get the barycentric coordinates of the intersection
  vec3 barycentrics = vec3(0.0, attributes.x, attributes.y);
  barycentrics.x    = 1.0 - barycentrics.y - barycentrics.z;

  // Compute the coordinates of the intersection
  result.objectPosition = v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
  // Transform from object space to world space:
  result.worldPosition = gl_ObjectToWorldEXT * vec4(result.objectPosition, 1.0f);


  // Compute the normal of the triangle in object space, using the right-hand rule:
  //    v2      .
  //    |\      .
  //    | \     .
  //    |/ \    .
  //    /   \   .
  //   /|    \  .
  //  L v0---v1 .
  // n
  const vec3 objectNormal = cross(v1 - v0, v2 - v0);
  // Transform normals from object space to world space. These use the transpose of the inverse matrix,
  // because they're directions of normals, not positions:
  result.worldNormal = normalize((objectNormal * gl_WorldToObjectEXT).xyz);

  // Flip the normal so it points against the ray direction:
  const vec3 rayDirection = gl_WorldRayDirectionEXT;
  result.worldNormal      = faceforward(result.worldNormal, rayDirection, result.worldNormal);

  return result;
}

// offsetPositionAlongNormal shifts a point on a triangle surface so that a
// ray bouncing off the surface with tMin = 0.0 is no longer treated as
// intersecting the surface it originated from.
//
// Here's the old implementation of it we used in earlier chapters:
// vec3 offsetPositionAlongNormal(vec3 worldPosition, vec3 normal)
// {
//   return worldPosition + 0.0001 * normal;
// }
//
// However, this code uses an improved technique by Carsten Wächter and
// Nikolaus Binder from "A Fast and Robust Method for Avoiding
// Self-Intersection" from Ray Tracing Gems (version 1.7, 2020).
// The normal can be negated if one wants the ray to pass through
// the surface instead.
vec3 offsetPositionAlongNormal(vec3 worldPosition, vec3 normal)
{
  // Convert the normal to an integer offset.
  const float int_scale = 256.0f;
  const ivec3 of_i      = ivec3(int_scale * normal);

  // Offset each component of worldPosition using its binary representation.
  // Handle the sign bits correctly.
  const vec3 p_i = vec3(  //
      intBitsToFloat(floatBitsToInt(worldPosition.x) + ((worldPosition.x < 0) ? -of_i.x : of_i.x)),
      intBitsToFloat(floatBitsToInt(worldPosition.y) + ((worldPosition.y < 0) ? -of_i.y : of_i.y)),
      intBitsToFloat(floatBitsToInt(worldPosition.z) + ((worldPosition.z < 0) ? -of_i.z : of_i.z)));

  // Use a floating-point offset instead for points near (0,0,0), the origin.
  const float origin     = 1.0f / 32.0f;
  const float floatScale = 1.0f / 65536.0f;
  return vec3(  //
      abs(worldPosition.x) < origin ? worldPosition.x + floatScale * normal.x : p_i.x,
      abs(worldPosition.y) < origin ? worldPosition.y + floatScale * normal.y : p_i.y,
      abs(worldPosition.z) < origin ? worldPosition.z + floatScale * normal.z : p_i.z);
}

vec3 diffuseReflection(vec3 normal, inout uint rngState)
{
  // For a random diffuse bounce direction, we follow the approach of
  // Ray Tracing in One Weekend, and generate a random point on a sphere
  // of radius 1 centered at the normal. This uses the random_unit_vector
  // function from chapter 8.5:
  const float theta     = 2.0 * k_pi * stepAndOutputRNGFloat(rngState);  // Random in [0, 2pi]
  const float u         = 2.0 * stepAndOutputRNGFloat(rngState) - 1.0;   // Random in [-1, 1]
  const float r         = sqrt(1.0 - u * u);
  const vec3  direction = normal + vec3(r * cos(theta), r * sin(theta), u);

  // Then normalize the ray direction:
  return normalize(direction);
}

#endif  // #ifndef VK_MINI_PATH_TRACER_CLOSEST_HIT_COMMON_H