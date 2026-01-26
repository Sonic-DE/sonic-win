find_package(Vulkan REQUIRED)

target_sources(kwin PRIVATE
    vulkanbackend.cpp
    vulkansurfacetexture.cpp
    vulkansurfacetexture_x11.cpp
)

target_link_libraries(kwin PRIVATE Vulkan::Vulkan)
