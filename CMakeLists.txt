cmake_minimum_required(VERSION 3.20)
project(ClinicalWalkthrough)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

add_executable(ClinicalWalkthrough
    src/main.cpp
    src/mesh.cpp
    src/scene.cpp
    src/steps.cpp
    src/canvas.cpp
    src/font.cpp
    src/ui.cpp
)

target_link_libraries(ClinicalWalkthrough PRIVATE Vulkan::Vulkan glfw)
target_include_directories(ClinicalWalkthrough PRIVATE ${Vulkan_INCLUDE_DIRS} src)

# --- Compile shaders to SPIR-V as part of the build ---
find_program(GLSLC glslc HINTS $ENV{VULKAN_SDK}/bin /usr/local/opt/shaderc/bin /opt/homebrew/opt/shaderc/bin)
if(NOT GLSLC)
    message(FATAL_ERROR "glslc not found. Install the Vulkan SDK or `brew install shaderc`.")
endif()

file(GLOB SHADER_SOURCES ${CMAKE_SOURCE_DIR}/shaders/*.vert ${CMAKE_SOURCE_DIR}/shaders/*.frag)

foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SPIRV ${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}.spv)
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders
        COMMAND ${GLSLC} ${SHADER} -o ${SPIRV}
        DEPENDS ${SHADER}
        COMMENT "Compiling shader ${SHADER_NAME}"
    )
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach()

add_custom_target(Shaders DEPENDS ${SPIRV_BINARY_FILES})
add_dependencies(ClinicalWalkthrough Shaders)

# So the app can find shaders/*.spv relative to the binary at runtime.
add_custom_command(TARGET ClinicalWalkthrough POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_BINARY_DIR}/shaders
        $<TARGET_FILE_DIR:ClinicalWalkthrough>/shaders
)
