#include "VulkanHelper.h"


#include <iostream>
#include <stdio.h>
#include <stdlib.h>

VkCommandBuffer VulkanHelper::createCommandBuffer(
    VkDevice m_device,
    VkCommandPool                         commandPool,
    VkCommandBufferLevel                  level /*= VK_COMMAND_BUFFER_LEVEL_PRIMARY*/,
    bool                                  begin,
    VkCommandBufferUsageFlags             flags /*= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT*/,
    const VkCommandBufferInheritanceInfo* pInheritanceInfo /*= nullptr*/)
{
  VkCommandBufferAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  allocInfo.level                       = level;
  allocInfo.commandPool                 = commandPool;
  allocInfo.commandBufferCount          = 1;

  VkCommandBuffer cmd;
  vkAllocateCommandBuffers(m_device, &allocInfo, &cmd);

  if(begin)
  {
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags                    = flags;
    beginInfo.pInheritanceInfo         = pInheritanceInfo;

    vkBeginCommandBuffer(cmd, &beginInfo);
  }

  return cmd;
}
void VulkanHelper::submitAndWait(VkDevice               m_device,
                                 size_t                 count,
                                 const VkCommandBuffer* cmds,
                                 VkCommandPool          commandPool,
                                 VkQueue                queue)
{
  submit(m_device,count, cmds, queue);
  VkResult result = vkQueueWaitIdle(queue);
  if(nvvk::checkResult(result, __FILE__, __LINE__))
  {
    exit(-1);
  }
  vkFreeCommandBuffers(m_device, commandPool, (uint32_t)count, cmds);
}
void VulkanHelper::submit(VkDevice               m_device,
                          size_t                 count,
                          const VkCommandBuffer* cmds,
                          VkQueue                queue,
                          VkFence                fence)
{
  for(size_t i = 0; i < count; i++)
  {
    vkEndCommandBuffer(cmds[i]);
  }

  VkSubmitInfo submit       = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submit.pCommandBuffers    = cmds;
  submit.commandBufferCount = (uint32_t)count;
  vkQueueSubmit(queue, 1, &submit, fence);
}