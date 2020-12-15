// Copyright 2020 NVIDIA Corporation
// SPDX-License-Identifier: Apache-2.0

// Common file shared across C++ CPU code and GLSL GPU code.
#ifndef VK_MINI_PATH_TRACER_COMMON_H
#define VK_MINI_PATH_TRACER_COMMON_H

#define RENDER_WIDTH 800
#define RENDER_HEIGHT 600
#define WORKGROUP_WIDTH 16
#define WORKGROUP_HEIGHT 8

#define BINDING_IMAGEDATA 0
#define BINDING_TLAS 1
#define BINDING_VERTICES 2
#define BINDING_INDICES 3

#endif // #ifndef VK_MINI_PATH_TRACER_COMMON_H