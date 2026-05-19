# Create the executable
add_executable(${PROJECT_NAME} 
    "src/libs/vk_mem_alloc.h"
    "src/libs/VmaUsage.cpp"
    
    "src/main.cpp"
    "src/VulkanContext.h"
    "src/BufferHelpers.h"
    "src/ImageHelpers.h"
    "src/Texture.cpp"
    "src/VulkanContext.cpp"
    "src/VulkanSetup.h"
    "src/VulkanSetup.cpp"
    "src/structs.h"
    "src/BaseApplication.h" 
    "src/Scene.h" 
    "src/Scene.cpp"
    "src/Model.h"
    "src/Model.cpp"
    "src/DescriptorSetBuilder.h" 
    "src/Camera.h"
    "src/Application.h" 
    "src/Application.cpp"
    "src/VmaUsage.h"
    "src/DeletionQueue.h"
    "src/PipelineBuilder.h"
    "src/PipelineBuilder.cpp")

# Copy resources to output folder (preserve folder structure)
set(RESOURCES_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/resources")
file(GLOB_RECURSE RESOURCE_FILES
    "${RESOURCES_SOURCE_DIR}/*.jpg"
    "${RESOURCES_SOURCE_DIR}/*.png"
    "${RESOURCES_SOURCE_DIR}/*.obj"
    "${RESOURCES_SOURCE_DIR}/*.fx"
    "${RESOURCES_SOURCE_DIR}/*.gltf"
    "${RESOURCES_SOURCE_DIR}/*.bin"
)
set(RESOURCES_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/resources/")
file(MAKE_DIRECTORY ${RESOURCES_OUT_DIR})
foreach(RESOURCE ${RESOURCE_FILES})
    # Compute path relative to the resources source dir
    file(RELATIVE_PATH REL_PATH "${RESOURCES_SOURCE_DIR}" "${RESOURCE}")
    # Get directory component of the relative path
    get_filename_component(REL_DIR "${REL_PATH}" DIRECTORY)
    if(REL_DIR STREQUAL ".")
        set(DEST_DIR "${RESOURCES_OUT_DIR}")
    else()
        set(DEST_DIR "${RESOURCES_OUT_DIR}/${REL_DIR}")
    endif()
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DEST_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy "${RESOURCE}" "${DEST_DIR}"
    )
endforeach()

# Compile shaders
set(SHADER_COMPILE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(SHADER_COMPILE_OUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders/compiled")

file(GLOB_RECURSE SHADER_SOURCE_FILES
    "${SHADER_COMPILE_DIR}/*.vert"
    "${SHADER_COMPILE_DIR}/*.frag"
    "${SHADER_COMPILE_DIR}/*.comp"
)

file(MAKE_DIRECTORY ${SHADER_COMPILE_OUT_DIR})

set(SPIRV_BINARY_FILES)

foreach(SHADER ${SHADER_SOURCE_FILES})

    get_filename_component(SHADER_NAME ${SHADER} NAME_WE)
    get_filename_component(SHADER_EXT ${SHADER} EXT)

    if(SHADER_EXT STREQUAL ".vert")
        set(SHADER_STAGE "vert")
    elseif(SHADER_EXT STREQUAL ".frag")
        set(SHADER_STAGE "frag")
    elseif(SHADER_EXT STREQUAL ".comp")
        set(SHADER_STAGE "comp")
    endif()

    set(OUTPUT_FILE
        "${SHADER_COMPILE_OUT_DIR}/${SHADER_NAME}_${SHADER_STAGE}.spv"
    )

    add_custom_command(
        OUTPUT ${OUTPUT_FILE}

        COMMAND glslangValidator
            -V
            -g
            ${SHADER}
            -o ${OUTPUT_FILE}

        DEPENDS ${SHADER}

        COMMENT "Compiling shader ${SHADER}"
        VERBATIM
    )

    list(APPEND SPIRV_BINARY_FILES ${OUTPUT_FILE})

endforeach()

# Create shader compilation target
add_custom_target(
    CompileShaders ALL
    DEPENDS ${SPIRV_BINARY_FILES}
)

# Make your executable depend on shaders
add_dependencies(${PROJECT_NAME} CompileShaders)

# Copy shaders to output folder
set(SHADERS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders/compiled")
file(GLOB_RECURSE SHADER_FILES
    "${SHADERS_SOURCE_DIR}/*.spv"
)
set(SHADERS_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders/")
file(MAKE_DIRECTORY ${SHADERS_OUT_DIR})
foreach(SHADER ${SHADER_FILES})
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${SHADER} ${SHADERS_OUT_DIR}
        DEPENDS ${SHADER}
    )
endforeach(SHADER)

# Vulkan setup
find_package(Vulkan REQUIRED)
if (!Vulkan_FOUND)
    message(FATAL_ERROR "Vulkan not found")
endif()

# Fetch used libraries
include(FetchContent)
Set(FETCHCONTENT_QUIET FALSE)

# Fetch glm
FetchContent_Declare(
	glm
	GIT_REPOSITORY	https://github.com/g-truc/glm.git
	GIT_TAG 	0af55ccecd98d4e5a8d1fad7de25ba429d60e863 #refs/tags/1.0.1
)

FetchContent_MakeAvailable(glm)

# Fetch glfw
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_PROGRESS TRUE
    GIT_SHALLOW TRUE
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)       # Disable GLFW documentation
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)   # Disable GLFW examples
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)      # Disable GLFW tests
FetchContent_MakeAvailable(glfw)

# Fetch stb_image
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
    GIT_PROGRESS TRUE
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(stb)

# Fetch tinygltf
FetchContent_Declare(
    tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf
    GIT_TAG release
    GIT_PROGRESS TRUE
    GIT_SHALLOW TRUE
)   #GIT_TAG v2.9.5
FetchContent_MakeAvailable(tinygltf)

# Fetch tiny_obj_loader
FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG release
    GIT_PROGRESS TRUE
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL TRUE
)
FetchContent_MakeAvailable(tinyobjloader)

# Add the stb_image and tiny_object_loader directory to your include paths
target_include_directories(${PROJECT_NAME} PRIVATE ${stb_SOURCE_DIR})

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE glfw Vulkan::Vulkan glm::glm tinyobjloader tinygltf)
target_include_directories(${PROJECT_NAME} PRIVATE glfw glm::glm)
