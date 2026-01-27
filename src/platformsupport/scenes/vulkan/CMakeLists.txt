find_package(Vulkan REQUIRED)

# Check for Vulkan shader compiler
find_program(GLSLC_EXECUTABLE glslc HINTS $ENV{VULKAN_SDK}/bin)
if(NOT GLSLC_EXECUTABLE)
    message(WARNING "glslc not found - Vulkan shaders will need to be pre-compiled")
endif()

# Compile shaders if glslc is available
if(GLSLC_EXECUTABLE)
    set(SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
    set(SHADER_BINARY_DIR "${CMAKE_BINARY_DIR}/shaders/vulkan")
    file(MAKE_DIRECTORY ${SHADER_BINARY_DIR})

    set(SHADER_SOURCES
        ${SHADER_SOURCE_DIR}/basic.vert
        ${SHADER_SOURCE_DIR}/main.frag
    )

    foreach(SHADER ${SHADER_SOURCES})
        get_filename_component(SHADER_NAME ${SHADER} NAME)
        set(SPIRV "${SHADER_BINARY_DIR}/${SHADER_NAME}.spv")

        add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${GLSLC_EXECUTABLE} --target-env=vulkan1.2 ${SHADER} -o ${SPIRV}
            DEPENDS ${SHADER}
            COMMENT "Compiling ${SHADER_NAME} to SPIR-V"
        )

        list(APPEND SPIRV_BINARY_FILES ${SPIRV})
    endforeach()

    add_custom_target(vulkan_shaders DEPENDS ${SPIRV_BINARY_FILES})
    add_dependencies(kwin vulkan_shaders)

    # Install compiled shaders
    install(FILES ${SPIRV_BINARY_FILES} DESTINATION ${KDE_INSTALL_DATADIR}/kwin/shaders/vulkan)
endif()

# VMA (Vulkan Memory Allocator) - header-only library
# Check if system has VMA installed, otherwise use bundled version
find_path(VMA_INCLUDE_DIR vk_mem_alloc.h
    HINTS
        /usr/include
        /usr/local/include
        $ENV{VULKAN_SDK}/include
)

if(VMA_INCLUDE_DIR)
    message(STATUS "Found VMA at ${VMA_INCLUDE_DIR}")
else()
    message(STATUS "VMA not found in system, using bundled version")
    # Will need to download/bundle VMA header
    # For now, assume it will be provided or downloaded
endif()

target_sources(kwin PRIVATE
    # Core Vulkan backend
    vulkanbackend.cpp

    # Context and resource management
    vulkancontext.cpp
    vulkanbuffer.cpp
    vulkantexture.cpp

    # Render infrastructure
    vulkanrenderpass.cpp
    vulkanframebuffer.cpp
    vulkanswapchain.cpp
    vulkanrendertarget.cpp

    # Pipeline management
    vulkanpipeline.cpp
    vulkanpipelinemanager.cpp

    # Surface textures
    vulkansurfacetexture.cpp
    vulkansurfacetexture_x11.cpp
)

target_link_libraries(kwin PRIVATE
    Vulkan::Vulkan
    XCB::DRI3
    Libdrm::Libdrm
)

# Add VMA include directory
target_include_directories(kwin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty)
