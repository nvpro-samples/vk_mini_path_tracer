# Copyright 2020 NVIDIA Corporation
# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 2.8)
project(vk_mini_path_tracer)

#####################################################################################
# look for shared_sources 1) as a sub-folder 2) at some other locations
# this cannot be put anywhere else since we still didn't find CMakeLists_include.txt yet
#
if(NOT BASE_DIRECTORY) # if not defined, it means this cmake file was called as the first entry point and not included
  # check if the external repository is outside or inside the project (as a sub-module)
  # testing the file CMakeLists_include.txt because when sub-modules are not cloned, the folders are still there...
  # we also assume here that if shared_sources is there, shared_external is, too...
  SET(BASE_DIRECTORY "" CACHE FILEPATH "folder containing shared_sources")
  SET(ADD_SUBDIR_BELOW 1)
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/shared_sources/CMakeLists_include.txt)
    SET(BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  elseif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../shared_sources/CMakeLists_include.txt)
    SET(BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..)
  elseif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../../shared_sources/CMakeLists_include.txt)
    SET(BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..)
  else()
  endif()
endif(NOT BASE_DIRECTORY)

if(EXISTS ${BASE_DIRECTORY}/shared_sources/CMakeLists_include.txt)
  INCLUDE(${BASE_DIRECTORY}/shared_sources/CMakeLists_include.txt)
else()
  Message(FATAL_ERROR "could not find base directory, please set BASE_DIRECTORY to folder containing shared_sources")
endif()

#####################################################################################
# Add sample packages
#
_add_package_VulkanSDK()
_add_shared_sources_lib()

#####################################################################################
# Add chapters
add_subdirectory(_edit) # Empty starting project
add_subdirectory(1_hello_vulkan)
add_subdirectory(2_extensions)
add_subdirectory(3_memory)
add_subdirectory(4_commands)
add_subdirectory(5_write_image)
add_subdirectory(6_compute_shader)
add_subdirectory(7_descriptors)
add_subdirectory(8_ray_tracing)
add_subdirectory(9_intersection)
add_subdirectory(10_mesh_data)
add_subdirectory(11_specular)
add_subdirectory(12_antialiasing)
add_subdirectory(vk_mini_path_tracer) # End of main tutorial
add_subdirectory(e1_gaussian)
# add_subdirectory(e2_performance) # This is an empty chapter
add_subdirectory(e3_compaction)
add_subdirectory(e4_include)
add_subdirectory(e5_push_constants)
add_subdirectory(e6_more_samples)
add_subdirectory(e7_image)
add_subdirectory(e8_debug_names)
add_subdirectory(e9_instances)
add_subdirectory(e10_materials)
add_subdirectory(e11_rt_pipeline_1)
add_subdirectory(e11_rt_pipeline_2)
add_subdirectory(e11_rt_pipeline_3)