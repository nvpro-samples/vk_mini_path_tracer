// Copyright 2020 NVIDIA Corporation
// SPDX-License-Identifier: Apache-2.0
#version 460
#extension GL_GOOGLE_include_directive : require
#include "closestHitCommon.h"

void main()
{
  HitInfo hitInfo = getObjectHitInfo();

  pld.color     = vec3(0.7);
  pld.rayOrigin = offsetPositionAlongNormal(hitInfo.worldPosition, hitInfo.worldNormal);

  // Perturb the normal:
  const float scaleFactor        = 80.0;
  const vec3  perturbationAmount = 0.03
                                  * vec3(sin(scaleFactor * hitInfo.worldPosition.x),  //
                                         sin(scaleFactor * hitInfo.worldPosition.y),  //
                                         sin(scaleFactor * hitInfo.worldPosition.z));
  const vec3 shadingNormal = normalize(hitInfo.worldNormal + perturbationAmount);
  if(stepAndOutputRNGFloat(pld.rngState) < 0.4)
  {
    pld.rayDirection = reflect(gl_WorldRayDirectionEXT, shadingNormal);
  }
  else
  {
    pld.rayDirection = diffuseReflection(shadingNormal, pld.rngState);
  }
  // If the ray now points into the surface, reflect it across:
  if(dot(pld.rayDirection, hitInfo.worldNormal) <= 0.0)
  {
    pld.rayDirection = reflect(pld.rayDirection, hitInfo.worldNormal);
  }
  pld.rayHitSky = false;
}