#include "VulkanShaderModule.hpp"

#include "VulkanResult.hpp"

#include "AREngine/Core/Assert.hpp"

#include <cstdint>
#include <fstream>
#include <vector>

namespace AREngine::Rendering::Vulkan
{
    namespace
    {
        std::vector<char> ReadFileBinary(const std::string& path)
        {
            std::ifstream file(path, std::ios::ate | std::ios::binary);
            AR_ASSERT_MSG(file.is_open(), "Failed to open SPIR-V file - was it compiled? (build-time artifact, not user input)");

            const std::streamsize size = file.tellg();
            std::vector<char> buffer(static_cast<std::size_t>(size));
            file.seekg(0);
            file.read(buffer.data(), size);
            return buffer;
        }
    }

    VulkanShaderModule::VulkanShaderModule(VkDevice device, const std::string& spirvPath)
        : m_device(device)
    {
        const std::vector<char> code = ReadFileBinary(spirvPath);
        AR_ASSERT_MSG(code.size() % 4 == 0, "SPIR-V bytecode size must be a multiple of 4 bytes");

        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const std::uint32_t*>(code.data());

        const VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, &m_module);
        CheckVkResult(result, "vkCreateShaderModule");
    }

    VulkanShaderModule::~VulkanShaderModule()
    {
        if (m_module != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(m_device, m_module, nullptr);
            m_module = VK_NULL_HANDLE;
        }
    }
}
