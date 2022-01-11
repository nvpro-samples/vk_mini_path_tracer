# Copyright 2020 NVIDIA Corporation
# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.5)
set(PROJNAME "vk_mini_path_tracer")
project(${PROJNAME})

#####################################################################################
_add_project_definitions(${PROJNAME})

#####################################################################################
# Source files for this project
#
file(GLOB SOURCE_FILES *.cpp *.hpp *.inl *.h *.c)

#####################################################################################
# GLSL to SPIR-V custom build
#
# more than one file can be given: _compile_GLSL("GLSL_mesh.vert;GLSL_mesh.frag" "GLSL_mesh.spv" GLSL_SOURCES)
# the SPIR-V validator is fine as long as files are for different pipeline stages (entry points still need to be main())
#_compile_GLSL(<source(s)> <target spv> <LIST where files are appended>)
SET(VULKAN_TARGET_ENV vulkan1.2)

UNSET(GLSL_SOURCES)
UNSET(SPV_OUTPUT)
file(GLOB_RECURSE GLSL_HEADER_FILES "shaders/*.h")
file(GLOB_RECURSE GLSL_SOURCE_FILES "shaders/*.glsl")
foreach(GLSL ${GLSL_SOURCE_FILES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    _compile_GLSL(${GLSL} "shaders/${FILE_NAME}.spv" GLSL_SOURCES SPV_OUTPUT)
endforeach(GLSL)

list(APPEND GLSL_SOURCES ${GLSL_HEADER_FILES})
source_group("Shader Files" FILES ${GLSL_SOURCES})

#####################################################################################
# Executable
#
add_executable(${PROJNAME} ${SOURCE_FILES} ${COMMON_SOURCE_FILES} ${GLSL_SOURCES})

#####################################################################################
# Source code group
#
source_group("Source Files" FILES ${SOURCE_FILES})

#####################################################################################
# Linkage
#
target_link_libraries(${PROJNAME} ${PLATFORM_LIBRARIES} nvpro_core)

foreach(DEBUGLIB ${LIBRARIES_DEBUG})
  target_link_libraries(${PROJNAME} debug ${DEBUGLIB})
endforeach(DEBUGLIB)

foreach(RELEASELIB ${LIBRARIES_OPTIMIZED})
  target_link_libraries(${PROJNAME} optimized ${RELEASELIB})
endforeach(RELEASELIB)

#####################################################################################
# copies binaries that need to be put next to the exe files (ZLib, etc.)
#
_finalize_target( ${PROJNAME} )

install(FILES ${SPV_OUTPUT} CONFIGURATIONS Release DESTINATION "bin_${ARCH}/${PROJNAME}/shaders")
install(FILES ${SPV_OUTPUT} CONFIGURATIONS Debug DESTINATION "bin_${ARCH}_debug/${PROJNAME}/shaders")
install(DIRECTORY "../scenes" CONFIGURATIONS Release DESTINATION "bin_${ARCH}/${PROJNAME}")
install(DIRECTORY "../scenes" CONFIGURATIONS Debug DESTINATION "bin_${ARCH}_debug/${PROJNAME}")