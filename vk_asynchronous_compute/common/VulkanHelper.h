#pragma once
#include <algorithm>
#include <platform.h>
#include "nvvk/appbase_vkpp.hpp"
#include "nvvk/debug_util_vk.hpp"
#include "nvvk/descriptorsets_vk.hpp"
#include "nvvk/error_vk.hpp"

class VulkanHelper
{

public:
 static VkCommandBuffer createCommandBuffer(
      VkDevice m_device,
      VkCommandPool                         commandPool,
      VkCommandBufferLevel                  level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      bool                                  begin = true,
      VkCommandBufferUsageFlags             flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      const VkCommandBufferInheritanceInfo* pInheritanceInfo = nullptr);
  static void submitAndWait(VkDevice               m_device,
                            size_t                 count,
                           const VkCommandBuffer* cmds,
                           VkCommandPool          commandPool,
                            VkQueue                queue);
 static void submit(VkDevice               m_device,
                    size_t                 count,
             const VkCommandBuffer* cmds,
             VkQueue                queue,
             VkFence                fence = VK_NULL_HANDLE);
};

