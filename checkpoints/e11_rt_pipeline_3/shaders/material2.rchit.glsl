// Copyright 2020 NVIDIA Corporation
// SPDX-License-Identifier: Apache-2.0
#version 460
#extension GL_GOOGLE_include_directive : require
#include "closestHitCommon.h"

void main()
{
  HitInfo hitInfo = getObjectHitInfo();

  pld.color        = vec3(0.5) + 0.5 * hitInfo.worldNormal;
  pld.rayOrigin    = offsetPositionAlongNormal(hitInfo.worldPosition, hitInfo.worldNormal);
  pld.rayDirection = diffuseReflection(hitInfo.worldNormal, pld.rngState);
  pld.rayHitSky    = false;
}