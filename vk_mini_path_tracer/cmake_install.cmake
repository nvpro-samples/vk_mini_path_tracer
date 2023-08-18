# Install script for directory: C:/Users/ankit/OneDrive/Desktop/My VK_Mini_Path_Tracer/vk_mini_path_tracer

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Users/ankit/OneDrive/Desktop/My VK_Mini_Path_Tracer/_install")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("C:/Users/ankit/OneDrive/Desktop/My VK_Mini_Path_Tracer/vk_mini_path_tracer/vk_mini_path_tracer/nvpro_core/cmake_install.cmake")
  include("C:/Users/ankit/OneDrive/Desktop/My VK_Mini_Path_Tracer/vk_mini_path_tracer/vk_mini_path_tracer/_edit/cmake_install.cmake")
  include("C:/Users/ankit/OneDrive/Desktop/My VK_Mini_Path_Tracer/vk_mini_path_tracer/vk_mini_path_tracer/checkpoints/1_hello_vulkan/cmake_install.cmake")
  include("C:/Users/ankit/OneDrive/Desktop/My VK_Mini_Path_Tracer/vk_mini_path_tracer/vk_mini_path_tracer/checkpoints/2_extensions/cmake_install.cmake")
  include("C:/Users/ankit/OneDrive/Desktop/My VK_Mini_Path_Tracer/vk_mini_path_tracer/vk_mini_path_tracer/checkpoints/3_memory/cmake_install.cmake")
  include("C:/Users/ankit/OneDrive/Desktop/My VK_Mini_Path_Tracer/vk_mini_path_tracer/vk_mini_path_tracer/checkpoints/4_commands/cmake_install.cmake")
  include("C:/Users/ankit/OneDrive/Desktop/My VK_Mini_Path_Tracer/vk_mini_path_tracer/vk_mini_path_tracer/checkpoints/5_write_image/cmake_install.cmake")
  include("C:/Users/ankit/OneDrive/Desktop/My VK_Mini_Path_Tracer/vk_mini_path_tracer/vk_mini_path_tracer/checkpoints/6_compute_shader/cmake_install.cmake")
  include("C:/Users/ankit/OneDrive/Desktop/My VK_Mini_Path_Tracer/vk_mini_path_tracer/vk_mini_path_tracer/checkpoints/7_descriptors/cmake_install.cmake")
  include("C:/Users/ankit/OneDrive/Desktop/My VK_Mini_Path_Tracer/vk_mini_path_tracer/vk_mini_path_tracer/checkpoints/8_ray_tracing/cmake_install.cmake")
  include("C:/Users/ankit/OneDrive/Desktop/My VK_Mini_Path_Tracer/vk_mini_path_tracer/vk_mini_path_tracer/checkpoints/9_intersection/cmake_install.cmake")
  include("C:/Users/ankit/OneDrive/Desktop/My VK_Mini_Path_Tracer/vk_mini_path_tracer/vk_mini_path_tracer/checkpoints/10_mesh_data/cmake_install.cmake")
  include("C:/Users/ankit/OneDrive/Desktop/My VK_Mini_Path_Tracer/vk_mini_path_tracer/vk_mini_path_tracer/checkpoints/11_specular/cmake_install.cmake")
  include("C:/Users/ankit/OneDrive/Desktop/My VK_Mini_Path_Tracer/vk_mini_path_tracer/vk_mini_path_tracer/checkpoints/12_antialiasing/cmake_install.cmake")
  include("C:/Users/ankit/OneDrive/Desktop/My VK_Mini_Path_Tracer/vk_mini_path_tracer/vk_mini_path_tracer/vk_mini_path_tracer/cmake_install.cmake")
  include("C:/Users/ankit/OneDrive/Desktop/My VK_Mini_Path_Tracer/vk_mini_path_tracer/vk_mini_path_tracer/checkpoints/e1_gaussian/cmake_install.cmake")
  include("C:/Users/ankit/OneDrive/Desktop/My VK_Mini_Path_Tracer/vk_mini_path_tracer/vk_mini_path_tracer/checkpoints/e3_compaction/cmake_install.cmake")
  include("C:/Users/ankit/OneDrive/Desktop/My VK_Mini_Path_Tracer/vk_mini_path_tracer/vk_mini_path_tracer/checkpoints/e4_include/cmake_install.cmake")
  include("C:/Users/ankit/OneDrive/Desktop/My VK_Mini_Path_Tracer/vk_mini_path_tracer/vk_mini_path_tracer/checkpoints/e5_push_constants/cmake_install.cmake")
  include("C:/Users/ankit/OneDrive/Desktop/My VK_Mini_Path_Tracer/vk_mini_path_tracer/vk_mini_path_tracer/checkpoints/e6_more_samples/cmake_install.cmake")
  include("C:/Users/ankit/OneDrive/Desktop/My VK_Mini_Path_Tracer/vk_mini_path_tracer/vk_mini_path_tracer/checkpoints/e7_image/cmake_install.cmake")
  include("C:/Users/ankit/OneDrive/Desktop/My VK_Mini_Path_Tracer/vk_mini_path_tracer/vk_mini_path_tracer/checkpoints/e8_debug_names/cmake_install.cmake")
  include("C:/Users/ankit/OneDrive/Desktop/My VK_Mini_Path_Tracer/vk_mini_path_tracer/vk_mini_path_tracer/checkpoints/e9_instances/cmake_install.cmake")
  include("C:/Users/ankit/OneDrive/Desktop/My VK_Mini_Path_Tracer/vk_mini_path_tracer/vk_mini_path_tracer/checkpoints/e10_materials/cmake_install.cmake")
  include("C:/Users/ankit/OneDrive/Desktop/My VK_Mini_Path_Tracer/vk_mini_path_tracer/vk_mini_path_tracer/checkpoints/e11_rt_pipeline_1/cmake_install.cmake")
  include("C:/Users/ankit/OneDrive/Desktop/My VK_Mini_Path_Tracer/vk_mini_path_tracer/vk_mini_path_tracer/checkpoints/e11_rt_pipeline_2/cmake_install.cmake")
  include("C:/Users/ankit/OneDrive/Desktop/My VK_Mini_Path_Tracer/vk_mini_path_tracer/vk_mini_path_tracer/checkpoints/e11_rt_pipeline_3/cmake_install.cmake")

endif()

if(CMAKE_INSTALL_COMPONENT)
  set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
file(WRITE "C:/Users/ankit/OneDrive/Desktop/My VK_Mini_Path_Tracer/vk_mini_path_tracer/vk_mini_path_tracer/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
