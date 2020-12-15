// Copyright 2020 NVIDIA Corporation
// SPDX-License-Identifier: Apache-2.0
#version 460 
#extension GL_EXT_debug_printf : require

layout(local_size_x = 16, local_size_y = 8, local_size_z = 1) in;

void main()
{
  debugPrintfEXT("Hello from invocation (%d, %d)!\n", gl_GlobalInvocationID.x, gl_GlobalInvocationID.y);
}