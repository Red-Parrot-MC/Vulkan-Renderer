# Source files
set(SOURCES 
	"src/main.cpp"
)

# Create the executable
add_executable(${PROJECT_NAME} ${SOURCES} "src/Application.h")

# Copy resources to output folder
set(RESOURCES_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/resources")
file(GLOB_RECURSE RESOURCE_FILES
    "${RESOURCES_SOURCE_DIR}/*.jpg"
    "${RESOURCES_SOURCE_DIR}/*.png"
    "${RESOURCES_SOURCE_DIR}/*.obj"
    "${RESOURCES_SOURCE_DIR}/*.fx"
)
set(RESOURCES_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/resources/")
file(MAKE_DIRECTORY ${RESOURCES_OUT_DIR})
foreach(RESOURCE ${RESOURCE_FILES})
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${RESOURCE}
    ${RESOURCES_OUT_DIR})
endforeach(RESOURCE)

# Compile shaders
set(SHADER_COMPILE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
file(GLOB_RECURSE SHADER_SOURCE_FILES
    "${SHADER_COMPILE_DIR}/*.vert"
    "${SHADER_COMPILE_DIR}/*.frag"
)
set(SHADER_COMPILE_OUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders/compiled")
file(MAKE_DIRECTORY ${SHADER_COMPILE_OUT_DIR})
foreach(SHADER ${SHADER_SOURCE_FILES})
    get_filename_component(SHADER_NAME ${SHADER} NAME_WE)
    get_filename_component(SHADER_EXT ${SHADER} EXT)
    if(SHADER_EXT STREQUAL ".vert")
        set(SHADER_STAGE "vert")
    elseif(SHADER_EXT STREQUAL ".frag")
        set(SHADER_STAGE "frag")
    endif()

    message(STATUS "Compiling shader: ${SHADER} (stage: ${SHADER_STAGE})")

    add_custom_command(
        TARGET ${PROJECT_NAME} PRE_BUILD
        COMMAND C:/VulkanSDK/1.4.341.1/Bin/glslc.exe ${SHADER} -o "${SHADER_COMPILE_OUT_DIR}/${SHADER_NAME}_${SHADER_STAGE}.spv"
    )
endforeach(SHADER)

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