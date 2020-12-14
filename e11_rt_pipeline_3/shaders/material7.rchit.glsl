// Copyright 2020 NVIDIA Corporation
// SPDX-License-Identifier: Apache-2.0
#version 460
#extension GL_GOOGLE_include_directive : require
#include "closestHitCommon.h"

void main()
{
  HitInfo hitInfo = getObjectHitInfo();

  const int primitiveID = gl_PrimitiveID;
  pld.color             = clamp(vec3(primitiveID / 36.0, primitiveID / 9.0, primitiveID / 18.0), vec3(0.0), vec3(1.0));
  pld.rayOrigin         = offsetPositionAlongNormal(hitInfo.worldPosition, hitInfo.worldNormal);
  pld.rayDirection      = diffuseReflection(hitInfo.worldNormal, pld.rngState);
  pld.rayHitSky         = false;
}