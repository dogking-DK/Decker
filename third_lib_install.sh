vcpkg install --recurse assimp
vcpkg install --recurse flecs 
vcpkg install --recurse fmt 
vcpkg install --recurse freetype
vcpkg install --recurse glm 
vcpkg install --recurse glslang[opt,rtti,tools]
vcpkg install --recurse imgui[freetype,sdl2-binding,vulkan-binding] 
vcpkg install --recurse implot
vcpkg install --recurse sdl2[vulkan]
vcpkg install --recurse spdlog
vcpkg install --recurse spirv-headers 
vcpkg install --recurse spirv-tools
vcpkg install --recurse stb 
vcpkg install --recurse tracy[crash-handler]
vcpkg install --recurse volk
vcpkg install --recurse vulkan
vcpkg install --recurse vulkan-headers
vcpkg install --recurse vulkan-loader 
vcpkg install --recurse vulkan-memory-allocator 
vcpkg install --recurse vk-bootstrap

echo press any button to continue...
read -n 1