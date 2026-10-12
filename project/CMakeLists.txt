# Create the executable
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "src/*.cpp"
    "src/*.h"
    "src/*.hpp"
)

add_executable(${PROJECT_NAME} ${SOURCES})

# Copy resources to output folder
set(RESOURCES_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/resources")
set(RESOURCES_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/resources/")

file(GLOB_RECURSE RESOURCE_FILES CONFIGURE_DEPENDS
    "${RESOURCES_SOURCE_DIR}"
)

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${RESOURCES_OUT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${RESOURCES_SOURCE_DIR}" "${RESOURCES_OUT_DIR}"
)

# Compile shaders
set(SHADER_COMPILE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(SHADER_COMPILE_OUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders/compiled")

file(GLOB_RECURSE SHADER_SOURCE_FILES CONFIGURE_DEPENDS
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
            -g ${SHADER}
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
set(SHADERS_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders/")

file(GLOB_RECURSE SHADER_FILES CONFIGURE_DEPENDS
    "${SHADERS_SOURCE_DIR}/*.spv"
)

add_custom_command(
    TARGET CompileShaders POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADERS_OUT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${SHADERS_SOURCE_DIR}" "${SHADERS_OUT_DIR}"
)

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
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG v3.0.0
    GIT_PROGRESS TRUE
    GIT_SHALLOW TRUE
)
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
