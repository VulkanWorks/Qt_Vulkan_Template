cmake_minimum_required(VERSION 3.5)

project(Qt_Vulkan_Template LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories("/home/shujaat/vulkan/1.1.126.0/x86_64/include")
link_directories("/home/shujaat/vulkan/1.1.126.0/x86_64/lib")

# Shaders compilation start.
file(GLOB shaders "${CMAKE_CURRENT_SOURCE_DIR}/shaders/source_shaders/*.frag"
                  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/source_shaders/*.vert")

file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/shaders/compiled_shaders)

foreach (file ${shaders})
    get_filename_component(CURRENT_SHADER ${file} NAME)
    set(SHADER_SPIRV_PATH ${CMAKE_CURRENT_SOURCE_DIR}/shaders/compiled_shaders/${CURRENT_SHADER}.spv)
    execute_process(COMMAND glslc -I ${CMAKE_CURRENT_SOURCE_DIR}/shaders/source_shaders ${file} -o ${SHADER_SPIRV_PATH})
endforeach()
# Shaders compilation end.

find_package(Qt5 COMPONENTS Core Quick REQUIRED)

add_executable(Qt_Vulkan_Template
    src/main.cpp
    src/QVulkanScene/QVulkanScene.h
    src/QVulkanScene/QVulkanScene.cpp
    src/QVulkanScene/QVkRenderer.h
    src/QVulkanScene/QVkRenderer.cpp
    include/VkStructureHelper.h
    resources.qrc
)

target_compile_definitions(Qt_Vulkan_Template
  PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)
target_link_libraries(Qt_Vulkan_Template
  PRIVATE Qt5::Core Qt5::Quick vulkan)
