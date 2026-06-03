#include "Application.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

#include "core/VulkanContext.h"
#include "core/SwapChain.h"
#include "renderer/GraphicsPipeline.h"
#include "renderer/Renderer.h"
#include "renderer/buffers/Mesh.h"
#include "renderer/buffers/Vertex.h"
#include "renderer/descriptors/FrameDescriptorLayout.h"
#include "renderer/descriptors/TextureDescriptorLayout.h"
#include "renderer/image_resources/Texture.h"
#include "renderer/image_resources/DepthImage.h"
#include "renderer/image_resources/MsaaColorImage.h"
#include "utils/ModelLoader.h"


Application::Application() : startTime_(std::chrono::high_resolution_clock::now())
{
    initWindow();
    initVulkan();
}


Application::~Application()
{
    glfwDestroyWindow(window_);
    glfwTerminate();
}


void Application::run()
{
    mainLoop();
}


void Application::initWindow()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window_ = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Engine", nullptr, nullptr);
    glfwSetWindowUserPointer(window_, this);

    glfwSetFramebufferSizeCallback(window_, framebufferResizeCallback);
}


void Application::initVulkan()
{
    context_ = std::make_unique<VulkanContext>(window_);
    
    swapChain_ = std::make_unique<SwapChain>(*context_, window_);  //*context_ dereferencing of context as we need it as reference.
    
    frameDescriptorLayout_ = std::make_unique<FrameDescriptorLayout>(*context_);
    
    textureDescriptorLayout_ = std::make_unique<TextureDescriptorLayout>(*context_);
    
    graphicsPipeline_ = std::make_unique<GraphicsPipeline>(
        *context_, *frameDescriptorLayout_, 
        *textureDescriptorLayout_, 
        swapChain_->getFormat(), 
        context_->findDepthFormat(),
        context_->getMsaaSamples()
    );
    

    renderer_ = std::make_unique<Renderer>(*context_, *textureDescriptorLayout_,*frameDescriptorLayout_);
    renderer_->initializePerImageResources(swapChain_->getImageCount());

    // creating multi-sample anti alias color image
    msaaColorImage_ = std::make_unique<MsaaColorImage>(
        renderer_->createMsaaColorImage(swapChain_->getExtent(),swapChain_->getFormat())
    );

    // creating depth Image buffer
    depthImage_ = std::make_unique<DepthImage>(
        renderer_->createDepthResources(swapChain_->getExtent())
    );

    // creating and binding the texture
    texture_ = std::make_unique<Texture>(renderer_->createTexture("textures/viking_room.png"));
    renderer_->bindTextureToDescriptor(*texture_);
    
    ModelData model = loadModel("models/viking_room.obj");
    std::cout << "vertex size: " << model.vertices_.size() << "\n";
    std::cout << "indices size: " << model.indices_.size() << "\n";

    mesh_ = std::make_unique<Mesh>(renderer_->createMesh(model.vertices_, model.indices_));
}


void Application::mainLoop()
{
    while (!glfwWindowShouldClose(window_))
    {
        glfwPollEvents();

        //  drawing frame
        bool was_frame_drawn = renderer_->drawFrame(
            *swapChain_, 
            *graphicsPipeline_, 
            *mesh_,
            computeUniformBufferObject(
                swapChain_->getExtent(), 
                std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - startTime_).count()
            ),
            depthImage_->getImageView(),
            msaaColorImage_->getImageView()
        );

        if (!was_frame_drawn || framebufferResized_)
        {
            onResize();
        }
        
    }

    context_->getLogicalDevice().waitIdle();
}


void Application::onResize()
{
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    while(width == 0 || height == 0)
    {
        glfwGetFramebufferSize(window_, &width, &height);
        glfwWaitEvents();
    }

    context_->getLogicalDevice().waitIdle();
    framebufferResized_ = false;
    swapChain_->recreate();
    renderer_->initializePerImageResources(swapChain_->getImageCount());
    
    msaaColorImage_ = std::make_unique<MsaaColorImage>(
        renderer_->createMsaaColorImage(swapChain_->getExtent(),swapChain_->getFormat())
    );

    depthImage_ = std::make_unique<DepthImage>(
        renderer_->createDepthResources(swapChain_->getExtent())
    );
}


/*
    this callback has been simplified, as it is using SwapChain.chooseExtent() to get the size of the frame buffer
    this is set during the SwapChain recreation.
*/
void Application::framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    app->framebufferResized_ = true;
}


UniformBufferObject Application::computeUniformBufferObject(vk::Extent2D extent, float time_seconds) const
{
    UniformBufferObject model_view_projection;
    model_view_projection.modelMatrix_ = glm::rotate(
        glm::mat4(1.0f), // transformation matrix
        time_seconds * glm::radians(90.0f), // angle in radiants
        glm::vec3(0.0f, 0.0f, 1.0f) // rotation angle
    );

    model_view_projection.viewMatrix_ = glm::lookAt(
        glm::vec3(2.0f, 2.0f, 2.0f), // look at from
        glm::vec3(0.0f, 0.0f, 0.0f), // look at to
        glm::vec3(0.0f, 0.0f, 1.0f) // up vector
    );

    model_view_projection.projectionMatrix_ = glm::perspective(
        glm::radians(45.0f), // fov
        extent.width / (float)extent.height, // aspect ratio
        0.1f, // near clipping plane
        10.0f // far clipping plane
    );

    return model_view_projection;
}