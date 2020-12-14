// Copyright 2020 NVIDIA Corporation
// SPDX-License-Identifier: Apache-2.0

// Common file defining all the functions used for materials.
// In the next subchapter, this file will be shared across closest-hit shaders.
#ifndef VK_MINI_PATH_TRACER_SHADER_COMMON_H
#define VK_MINI_PATH_TRACER_SHADER_COMMON_H

// This chapter doesn't use ray queries, but many readers will have this file
// from e10_materials. We'll modify this file in rt_pipeline_2.
struct HitInfo
{
  vec3 objectPosition;  // The intersection position in object-space.
  vec3 worldPosition;   // The intersection position in world-space.
  vec3 worldNormal;     // The double-sided triangle normal in world-space.
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
  result.objectPosition = v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
  // Transform from object space to world space:
  const mat4x3 objectToWorld = rayQueryGetIntersectionObjectToWorldEXT(rayQuery, true);
  result.worldPosition       = objectToWorld * vec4(result.objectPosition, 1.0f);


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
  const mat4x3 objectToWorldInverse = rayQueryGetIntersectionWorldToObjectEXT(rayQuery, true);
  result.worldNormal                = normalize((objectNormal * objectToWorldInverse).xyz);

  // Flip the normal so it points against the ray direction:
  const vec3 rayDirection = rayQueryGetWorldRayDirectionEXT(rayQuery);
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

// Steps the RNG and returns a floating-point value between 0 and 1 inclusive.
float stepAndOutputRNGFloat(inout uint rngState)
{
  // Condensed version of pcg_output_rxs_m_xs_32_32, with simple conversion to floating-point [0,1].
  rngState  = rngState * 747796405 + 1;
  uint word = ((rngState >> ((rngState >> 28) + 4)) ^ rngState) * 277803737;
  word      = (word >> 22) ^ word;
  return float(word) / 4294967295.0f;
}

const float k_pi = 3.14159265;

// The values returned by a material function to the main path tracing routine.
struct ReturnedInfo
{
  vec3 color;         // The reflectivity of the surface.
  vec3 rayOrigin;     // The new ray origin in world-space.
  vec3 rayDirection;  // The new ray direction in world-space.
};

// Returns a random diffuse (Lambertian) reflection for a surface with the
// given normal, using the given random number generator state. This is
// cosine-weighted, so directions closer to the normal are more likely to
// be chosen.
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

// Diffuse reflection off a 70% reflective surface (what we've used for most
// of this tutorial)
ReturnedInfo material0(rayQueryEXT rayQuery, inout uint rngState)
{
  HitInfo hitInfo = getObjectHitInfo(rayQuery);

  ReturnedInfo result;
  result.color        = vec3(0.7);
  result.rayOrigin    = offsetPositionAlongNormal(hitInfo.worldPosition, hitInfo.worldNormal);
  result.rayDirection = diffuseReflection(hitInfo.worldNormal, rngState);

  return result;
}

// A mirror-reflective material that absorbs 30% of incoming light.
ReturnedInfo material1(rayQueryEXT rayQuery, inout uint rngState)
{
  HitInfo hitInfo = getObjectHitInfo(rayQuery);

  ReturnedInfo result;
  result.color        = vec3(0.7);
  result.rayOrigin    = offsetPositionAlongNormal(hitInfo.worldPosition, hitInfo.worldNormal);
  result.rayDirection = reflect(rayQueryGetWorldRayDirectionEXT(rayQuery), hitInfo.worldNormal);

  return result;
}

// A diffuse surface with faces colored according to their world-space normal.
ReturnedInfo material2(rayQueryEXT rayQuery, inout uint rngState)
{
  HitInfo hitInfo = getObjectHitInfo(rayQuery);

  ReturnedInfo result;
  result.color        = vec3(0.5) + 0.5 * hitInfo.worldNormal;
  result.rayOrigin    = offsetPositionAlongNormal(hitInfo.worldPosition, hitInfo.worldNormal);
  result.rayDirection = diffuseReflection(hitInfo.worldNormal, rngState);

  return result;
}

// A linear blend of 20% of a mirror-reflective material and 80% of a perfectly
// diffuse material.
ReturnedInfo material3(rayQueryEXT rayQuery, inout uint rngState)
{
  HitInfo hitInfo = getObjectHitInfo(rayQuery);

  ReturnedInfo result;
  result.color     = vec3(0.7);
  result.rayOrigin = offsetPositionAlongNormal(hitInfo.worldPosition, hitInfo.worldNormal);
  if(stepAndOutputRNGFloat(rngState) < 0.2)
  {
    result.rayDirection = reflect(rayQueryGetWorldRayDirectionEXT(rayQuery), hitInfo.worldNormal);
  }
  else
  {
    result.rayDirection = diffuseReflection(hitInfo.worldNormal, rngState);
  }

  return result;
}

// A material where 50% of incoming rays pass through the surface (treating it
// as transparent), and the other 50% bounce off using diffuse reflection.
ReturnedInfo material4(rayQueryEXT rayQuery, inout uint rngState)
{
  HitInfo hitInfo = getObjectHitInfo(rayQuery);

  ReturnedInfo result;
  result.color = vec3(0.7);
  if(stepAndOutputRNGFloat(rngState) < 0.5)
  {
    result.rayOrigin    = offsetPositionAlongNormal(hitInfo.worldPosition, hitInfo.worldNormal);
    result.rayDirection = diffuseReflection(hitInfo.worldNormal, rngState);
  }
  else
  {
    result.rayOrigin    = offsetPositionAlongNormal(hitInfo.worldPosition, -hitInfo.worldNormal);
    result.rayDirection = rayQueryGetWorldRayDirectionEXT(rayQuery);
  }

  return result;
}

// A material with diffuse reflection that is transparent whenever
// (x + y + z) % 0.5 < 0.25 in object-space coordinates.
ReturnedInfo material5(rayQueryEXT rayQuery, inout uint rngState)
{
  HitInfo hitInfo = getObjectHitInfo(rayQuery);

  ReturnedInfo result;
  if(mod(dot(hitInfo.objectPosition, vec3(1, 1, 1)), 0.5) >= 0.25)
  {
    result.color        = vec3(0.7);
    result.rayOrigin    = offsetPositionAlongNormal(hitInfo.worldPosition, hitInfo.worldNormal);
    result.rayDirection = diffuseReflection(hitInfo.worldNormal, rngState);
  }
  else
  {
    result.color        = vec3(1.0);
    result.rayOrigin    = offsetPositionAlongNormal(hitInfo.worldPosition, -hitInfo.worldNormal);
    result.rayDirection = rayQueryGetWorldRayDirectionEXT(rayQuery);
  }

  return result;
}

// A mirror material that uses normal mapping: we perturb the geometric
// (triangle) normal to get a shading normal that varies over the surface, and
// then use the shading normal to get reflections. This is often used with a
// texture called a normal map in real-time graphics, because it can make it
// look like an object has details that aren't there in the geometry. In this
// function, we perturb the normal without textures using a mathematical
// function instead.
// There's a lot of depth (no pun intended) in normal mapping; two things to
// note in this example are:
// - It's not well-defined what happens when normal mapping produces a
// direction that goes through the surface. In this function we mirror it so
// that it doesn't go through the surface; in a different path tracer, we might
// reject this ray by setting its sample weight to 0, or do something more
// sophisticated.
// - When a BRDF (bidirectional reflectance distribution function; describes
// how much light from direction A bounces off a material in direction B) uses
// a shading normal instead of a geometric normal for shading, the BRDF has to
// be corrected in order to make the math physically correct and to avoid
// errors in bidirectional path tracers. This function ignores that (we don't
// describe BRDFs or sample weights in this tutorial!), but the authoritative
// source for how to do this is chapters 5-7 of Eric Veach's Ph.D. thesis,
// "Robust Monte Carlo Methods for Light Transport Simulation", available for
// free online.
ReturnedInfo material6(rayQueryEXT rayQuery, inout uint rngState)
{
  HitInfo hitInfo = getObjectHitInfo(rayQuery);

  ReturnedInfo result;
  result.color     = vec3(0.7);
  result.rayOrigin = offsetPositionAlongNormal(hitInfo.worldPosition, hitInfo.worldNormal);

  // Perturb the normal:
  const float scaleFactor        = 80.0;
  const vec3  perturbationAmount = 0.03
                                  * vec3(sin(scaleFactor * hitInfo.worldPosition.x),  //
                                         sin(scaleFactor * hitInfo.worldPosition.y),  //
                                         sin(scaleFactor * hitInfo.worldPosition.z));
  const vec3 shadingNormal = normalize(hitInfo.worldNormal + perturbationAmount);
  if(stepAndOutputRNGFloat(rngState) < 0.4)
  {
    result.rayDirection = reflect(rayQueryGetWorldRayDirectionEXT(rayQuery), shadingNormal);
  }
  else
  {
    result.rayDirection = diffuseReflection(shadingNormal, rngState);
  }
  // If the ray now points into the surface, reflect it across:
  if(dot(result.rayDirection, hitInfo.worldNormal) <= 0.0)
  {
    result.rayDirection = reflect(result.rayDirection, hitInfo.worldNormal);
  }

  return result;
}

// A diffuse material where the color of each triangle is determined by its
// primitive ID (the index of the triangle in the BLAS)
ReturnedInfo material7(rayQueryEXT rayQuery, inout uint rngState)
{
  HitInfo hitInfo = getObjectHitInfo(rayQuery);

  ReturnedInfo result;
  const int    primitiveID = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, true);
  result.color        = clamp(vec3(primitiveID / 36.0, primitiveID / 9.0, primitiveID / 18.0), vec3(0.0), vec3(1.0));
  result.rayOrigin    = offsetPositionAlongNormal(hitInfo.worldPosition, hitInfo.worldNormal);
  result.rayDirection = diffuseReflection(hitInfo.worldNormal, rngState);

  return result;
}

// A diffuse material with transparent cutouts arranged in slices of spheres.
ReturnedInfo material8(rayQueryEXT rayQuery, inout uint rngState)
{
  HitInfo hitInfo = getObjectHitInfo(rayQuery);

  ReturnedInfo result;
  if(mod(length(hitInfo.objectPosition), 0.2) >= 0.05)
  {
    result.color        = vec3(0.7);
    result.rayOrigin    = offsetPositionAlongNormal(hitInfo.worldPosition, hitInfo.worldNormal);
    result.rayDirection = diffuseReflection(hitInfo.worldNormal, rngState);
  }
  else
  {
    result.color        = vec3(1.0);
    result.rayOrigin    = offsetPositionAlongNormal(hitInfo.worldPosition, -hitInfo.worldNormal);
    result.rayDirection = rayQueryGetWorldRayDirectionEXT(rayQuery);
  }

  return result;
}

#endif  // #ifndef VK_MINI_PATH_TRACER_SHADER_COMMON_H