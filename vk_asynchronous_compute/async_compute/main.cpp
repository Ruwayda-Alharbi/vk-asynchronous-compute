/* Copyright (c) 2014-2018, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// ImGui - standalone example application for Glfw + Vulkan, using programmable
// pipeline If you are new to ImGui, see examples/README.txt and documentation
// at the top of imgui.cpp.

#include <array>
#include <random>
#include <vulkan/vulkan.hpp>
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

#include "imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"

#include "hello_vulkan.h"
#include "imgui/extras/imgui_camera_widget.h"
#include "nvh/cameramanipulator.hpp"
#include "nvh/fileoperations.hpp"
#include "nvpsystem.hpp"
#include "nvvk/appbase_vkpp.hpp"
#include "nvvk/commands_vk.hpp"
#include "nvvk/context_vk.hpp"


//////////////////////////////////////////////////////////////////////////
#define UNUSED(x) (void)(x)
//////////////////////////////////////////////////////////////////////////

// Default search path for shaders
std::vector<std::string> defaultSearchPaths;

// GLFW Callback functions
static void onErrorCallback(int error, const char* description)
{
  fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// Extra UI
void renderUI(HelloVulkan& helloVk)
{
  ImGuiH::CameraWidget();
  if(ImGui::CollapsingHeader("Light"))
  {
    ImGui::RadioButton("Point", &helloVk.m_pushConstant.lightType, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Infinite", &helloVk.m_pushConstant.lightType, 1);

    ImGui::SliderFloat3("Position", &helloVk.m_pushConstant.lightPosition.x, -50.f, 50.f);
    ImGui::SliderFloat("Intensity", &helloVk.m_pushConstant.lightIntensity, 0.f, 150.f);
  }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
static int const SAMPLE_WIDTH  = 1280;
static int const SAMPLE_HEIGHT = 720;

//--------------------------------------------------------------------------------------------------
// Application Entry
//
int main(int argc, char** argv)
{
  UNUSED(argc);

  // Setup GLFW window
  glfwSetErrorCallback(onErrorCallback);
  if(!glfwInit())
  {
    return 1;
  }
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow* window =
      glfwCreateWindow(SAMPLE_WIDTH, SAMPLE_HEIGHT, PROJECT_NAME, nullptr, nullptr);

  // Setup camera
  CameraManip.setWindowSize(SAMPLE_WIDTH, SAMPLE_HEIGHT);
  CameraManip.setLookat(nvmath::vec3f(5, 4, -4), nvmath::vec3f(0, 1, 0), nvmath::vec3f(0, 1, 0));

  // Setup Vulkan
  if(!glfwVulkanSupported())
  {
    printf("GLFW: Vulkan Not Supported\n");
    return 1;
  }

  // setup some basic things for the sample, logging file for example
  NVPSystem system(PROJECT_NAME);

  // Search path for shaders and other media
  defaultSearchPaths = {
      NVPSystem::exePath() + PROJECT_RELDIRECTORY,
      NVPSystem::exePath() + PROJECT_RELDIRECTORY "..",
      std::string(PROJECT_NAME),
  };

  nvvk::ContextCreateInfo contextInfo(true);
  contextInfo.setVersion(1, 2);
 // =============  Requesting Vulkan extensions and layers  ================================
  contextInfo.addInstanceLayer("VK_LAYER_LUNARG_monitor", true);
  contextInfo.addInstanceExtension(VK_KHR_SURFACE_EXTENSION_NAME);
  contextInfo.addInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, true);
#ifdef WIN32
  contextInfo.addInstanceExtension(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#else
  contextInfo.addInstanceExtension(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
  contextInfo.addInstanceExtension(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#endif
  // Display
  contextInfo.addDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

  // Ray tracing
  contextInfo.addInstanceExtension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
  //contextInfo.addDeviceExtension(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);
  contextInfo.addDeviceExtension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
  contextInfo.addDeviceExtension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
  contextInfo.addDeviceExtension(VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME);
  // #VKRay: Activate the ray tracing extension
  contextInfo.addDeviceExtension(VK_KHR_MAINTENANCE3_EXTENSION_NAME);
  contextInfo.addDeviceExtension(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
  contextInfo.addDeviceExtension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
  contextInfo.addDeviceExtension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
  vk::PhysicalDeviceAccelerationStructureFeaturesKHR accelFeature;
  contextInfo.addDeviceExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, false,
                                 &accelFeature);
  vk::PhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeature;
  contextInfo.addDeviceExtension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, false,
                                 &rtPipelineFeature);
  vk::PhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures =
      VkPhysicalDeviceRayQueryFeaturesKHR{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR};
  contextInfo.addDeviceExtension(VK_KHR_RAY_QUERY_EXTENSION_NAME, false, &rayQueryFeatures);
  // Semaphores - interop Vulkan/Cuda
  /* contextInfo.addDeviceExtension(VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME);
   // contextInfo.addDeviceExtension(VK_KHR_EXTERNAL_FENCE_EXTENSION_NAME);
#ifdef WIN32
  //  contextInfo.addDeviceExtension(VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME);
   // contextInfo.addDeviceExtension(VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME);
   // contextInfo.addDeviceExtension(VK_KHR_EXTERNAL_FENCE_WIN32_EXTENSION_NAME);
#else
   // contextInfo.addDeviceExtension(VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME);
  //  contextInfo.addDeviceExtension(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
   // contextInfo.addDeviceExtension(VK_KHR_EXTERNAL_FENCE_FD_EXTENSION_NAME);
#endif
    // Synchronization (mix of timeline and binary semaphores)
   // contextInfo.addDeviceExtension(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME, false);
   // contextInfo.addDeviceExtension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME, false);

   // contextInfo.addDeviceExtension(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);*/

  //Atomic operation
  contextInfo.addDeviceExtension(VK_KHR_SHADER_ATOMIC_INT64_EXTENSION_NAME);

  // Buffer - interop
  // contextInfo.addDeviceExtension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
  contextInfo.addDeviceExtension(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);
  //Shader - Debug Printf.
  contextInfo.addDeviceExtension(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
  // Shader - random number
  vk::PhysicalDeviceShaderClockFeaturesKHR clockFeatures;
  clockFeatures.shaderSubgroupClock = true;
  contextInfo.addDeviceExtension(VK_KHR_SHADER_CLOCK_EXTENSION_NAME, false, &clockFeatures);
  //================== END Vulkan extensions   =========================
  VkValidationFeaturesEXT validationInfo =
      VkValidationFeaturesEXT{VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};
  VkValidationFeatureEnableEXT validationFeatureToEnable =
      VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT;
  validationInfo.enabledValidationFeatureCount = 1;
  validationInfo.pEnabledValidationFeatures    = &validationFeatureToEnable;
  contextInfo.instanceCreateInfoExt            = &validationInfo;

  contextInfo.addInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  // Creating Vulkan base application
  nvvk::Context vkctx{};
  vkctx.initInstance(contextInfo);
  // Find all compatible devices
  auto compatibleDevices = vkctx.getCompatibleDevices(contextInfo);
  assert(!compatibleDevices.empty());
  // Use a compatible device
  vkctx.initDevice(compatibleDevices[0], contextInfo);

  // Create example
  HelloVulkan helloVk;

  // Window need to be opened to get the surface on which to draw
  const vk::SurfaceKHR surface = helloVk.getVkSurface(vkctx.m_instance, window);
  vkctx.setGCTQueueWithPresent(surface);


  helloVk.setup(vkctx);
  helloVk.createSwapchain(surface, SAMPLE_WIDTH, SAMPLE_HEIGHT);
  helloVk.createDepthBuffer();
  helloVk.createRenderPass();
  helloVk.createFrameBuffers();

  // Setup Imgui
  helloVk.initGUI(0);  // Using sub-pass 0

 /* // Creation of the example
  std::random_device              rd;  //Will be used to obtain a seed for the random number engine
  std::mt19937                    gen(rd());  //Standard mersenne_twister_engine seeded with rd()
  std::normal_distribution<float> dis(1.0f, 1.0f);
  std::normal_distribution<float> disn(0.05f, 0.05f);
  for(int n = 0; n < 100; ++n)
  {
    helloVk.loadModel(nvh::findFile("media/scenes/cube_multi.obj", defaultSearchPaths, true));
    HelloVulkan::ObjInstance& inst = helloVk.m_objInstance.back();

    float         scale = fabsf(disn(gen));
    nvmath::mat4f mat =
        nvmath::translation_mat4(nvmath::vec3f{dis(gen), 2.0f + dis(gen), dis(gen)});
    mat              = mat * nvmath::rotation_mat4_x(dis(gen));
    mat              = mat * nvmath::scale_mat4(nvmath::vec3f(scale));
    inst.transform   = mat;
    inst.transformIT = nvmath::transpose(nvmath::invert((inst.transform)));
  }

  helloVk.loadModel(nvh::findFile("media/scenes/plane.obj", defaultSearchPaths, true));
  */
   // Creation of the example
  //helloVk.loadModel(nvh::findFile("media/scenes/cube.obj", defaultSearchPaths, true));

  helloVk.loadModel(nvh::findFile("media/scenes/plane.obj", defaultSearchPaths, true));
  
  helloVk.loadModel(nvh::findFile("media/scenes/cube_multi.obj", defaultSearchPaths, true));
  helloVk.createComputeShaderPipline();

  std::random_device              rd;  // Will be used to obtain a seed for the random number engine
  std::mt19937                    gen(rd());  // Standard mersenne_twister_engine seeded with rd()
  std::normal_distribution<float> dis(5.0f, 5.0f);
  std::normal_distribution<float> disn(0.5f, 0.5f);
  int                             nbCubes = 1000;
  for(int n = 0; n < nbCubes; ++n)
  {
    HelloVulkan::ObjInstance& inst = helloVk.m_objInstance.back();
    inst.txtOffset      = 0;
    float         scale = fabsf(disn(gen));// 0.5;
    nvmath::mat4f mat =
        nvmath::translation_mat4(nvmath::vec3f{dis(gen), 2.0f + dis(gen), dis(gen)});
    mat              = mat * nvmath::rotation_mat4_x(dis(gen));
    mat              = mat * nvmath::scale_mat4(nvmath::vec3f(scale));
    inst.transform   = mat;
    inst.transformIT = nvmath::transpose(nvmath::invert((inst.transform)));
    helloVk.m_objInstance.push_back(inst);
  }

  helloVk.createOffscreenRender();
  helloVk.createDescriptorSetLayout();
  helloVk.createGraphicsPipeline();
  helloVk.createUniformBuffer();
  helloVk.createSceneDescriptionBuffer();
  helloVk.updateDescriptorSet();

  // #VKRay
  helloVk.initRayTracing();
  helloVk.createBottomLevelAS();
  helloVk.createTopLevelAS();
  helloVk.createRtDescriptorSet();
  helloVk.createRtPipeline();
  helloVk.createRtShaderBindingTable();

  helloVk.createPostDescriptor();
  helloVk.createPostPipeline();
  helloVk.updatePostDescriptorSet();


  nvmath::vec4f clearColor   = nvmath::vec4f(1, 1, 1, 1.00f);
  bool          useRaytracer = true;

  
  bool m_runTestComputeShader = false;
  int m_numberOfUsedQueues = 2;
  helloVk.setupGlfwCallbacks(window);
  ImGui_ImplGlfw_InitForVulkan(window, true);

  // Main loop
  while(!glfwWindowShouldClose(window))
  {
    glfwPollEvents();
    if(helloVk.isMinimized())
      continue;

    // Start the Dear ImGui frame
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Show UI window.
    if(helloVk.showGui())
    {
      ImGuiH::Panel::Begin();
      ImGui::ColorEdit3("Clear color", reinterpret_cast<float*>(&clearColor));
      ImGui::Checkbox("Ray Tracer mode", &useRaytracer);  // Switch between raster and ray tracing
      
      renderUI(helloVk);
      if(ImGui::CollapsingHeader("Test Async Compute", ImGuiTreeNodeFlags_DefaultOpen))
      {
        static int x = 2;
        static int y = 0;
        static int z = 0;
        ImGui::SliderInt("t1", &x, 0, 100);
        ImGui::SliderInt("t2", &y, 0, 1000);
        ImGui::SliderInt("t3", &z, 0, 1000000000);
        helloVk.m_PushConstant.m_threads = x + y + z;
        ImGui::Text("#Threads = %d", helloVk.m_PushConstant.m_threads);
        ImGui::Separator();
        ImGui::Text("Use for running Compute/Graphics cammand");
        ImGui::RadioButton("One Queue", &m_numberOfUsedQueues, 1);
        ImGui::SameLine();
        ImGui::RadioButton("Two Queues", &m_numberOfUsedQueues, 2);
        if(ImGui::Button("Run Compute Shader"))
        {
           m_runTestComputeShader = true;
           clearColor             = nvmath::vec4f(1, 0, 0, 1.00f);
        }
        
      }
      ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                  1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
      ImGuiH::Control::Info("", "", "(F10) Toggle Pane", ImGuiH::Control::Flags::Disabled);
      ImGuiH::Panel::End();
    }
    // Start rendering the scene
    helloVk.prepareFrame();
    // Start command buffer of this frame
    auto                     curFrame = helloVk.getCurFrame();
    const vk::CommandBuffer& cmdBuf   = helloVk.getCommandBuffers()[curFrame];
    
    cmdBuf.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    // Updating camera buffer
    helloVk.updateUniformBuffer(cmdBuf);


    // Clearing screen
    vk::ClearValue clearValues[2];
    clearValues[0].setColor(
        std::array<float, 4>({clearColor[0], clearColor[1], clearColor[2], clearColor[3]}));
    clearValues[1].setDepthStencil({1.0f, 0});


    // Offscreen render pass
    {
      vk::RenderPassBeginInfo offscreenRenderPassBeginInfo;
      offscreenRenderPassBeginInfo.setClearValueCount(2);
      offscreenRenderPassBeginInfo.setPClearValues(clearValues);
      offscreenRenderPassBeginInfo.setRenderPass(helloVk.m_offscreenRenderPass);
      offscreenRenderPassBeginInfo.setFramebuffer(helloVk.m_offscreenFramebuffer);
      offscreenRenderPassBeginInfo.setRenderArea({{}, helloVk.getSize()});

      // Rendering Scene
      if(useRaytracer)
      {
        helloVk.raytrace(cmdBuf, clearColor);
      }
      else
      {
        cmdBuf.beginRenderPass(offscreenRenderPassBeginInfo, vk::SubpassContents::eInline);
        helloVk.rasterize(cmdBuf);
        cmdBuf.endRenderPass();
      }
    }

    // 2nd rendering pass: tone mapper, UI
    {
      vk::RenderPassBeginInfo postRenderPassBeginInfo;
      postRenderPassBeginInfo.setClearValueCount(2);
      postRenderPassBeginInfo.setPClearValues(clearValues);
      postRenderPassBeginInfo.setRenderPass(helloVk.getRenderPass());
      postRenderPassBeginInfo.setFramebuffer(helloVk.getFramebuffers()[curFrame]);
      postRenderPassBeginInfo.setRenderArea({{}, helloVk.getSize()});

      cmdBuf.beginRenderPass(postRenderPassBeginInfo, vk::SubpassContents::eInline);
      // Rendering tonemapper
      helloVk.drawPost(cmdBuf);
      // Rendering UI
      ImGui::Render();
      ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuf);
      cmdBuf.endRenderPass();
    }

    // Submit for display
    cmdBuf.end();
    helloVk.submitFrame();
    //==============================================
    if(m_runTestComputeShader)
    {

      helloVk.m_isTestComputeShaderRunning = true;
      m_runTestComputeShader               = false;
    }
    else if(helloVk.m_isTestComputeShaderRunning)
    {
      try
      {

        helloVk.m_isTestComputeShaderRunning = false;
        helloVk.m_waitingComputeShaderFence  = false;
        helloVk.prepareComputeShader();
        if(m_numberOfUsedQueues == 2)
        {

          helloVk.executeComputeShaderPipline(helloVk.getCompCommandBuffer());
          helloVk.m_waitingComputeShaderFence = true;
        }
        else
        {
          helloVk.executeComputeShaderPipline_graphicsQueue();
          clearColor = nvmath::vec4f(1, 1, 1, 1.00f);
          helloVk.printCounter();
        }
      }
      catch(std::exception& e)
      {
        const char* what = e.what();
        LOGE("There was an error: %s \n", what);
        exit(1);
      }
    }
    //=====================================
    if(helloVk.m_waitingComputeShaderFence)
    {
      if(helloVk.isComputeShaderExecutionDone() == true)
      {
        clearColor                           = nvmath::vec4f(1, 1, 1, 1.00f);
        helloVk.printCounter();
        helloVk.m_waitingComputeShaderFence  = false;
      }
    }
  }

  // Cleanup
  helloVk.getDevice().waitIdle();
  helloVk.destroyResources();
  helloVk.destroy();

  vkctx.deinit();

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
