// Copyright 2020 NVIDIA Corporation
// SPDX-License-Identifier: Apache-2.0
#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "shaderCommon.h"

// The payload:
layout(location = 0) rayPayloadInEXT PassableInfo pld;

void main() {
  // Returns the color of the sky in a given direction (in linear color space)
  // +y in world space is up, so:
  const float rayDirY = gl_WorldRayDirectionEXT.y;
  if(rayDirY > 0.0f)
  {
    pld.color = mix(vec3(1.0f), vec3(0.25f, 0.5f, 1.0f), rayDirY);
  }
  else
  {
    pld.color = vec3(0.03f);
  }

  pld.rayHitSky = true;
}