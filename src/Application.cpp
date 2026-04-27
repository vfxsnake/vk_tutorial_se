#include "Application.h"

#include "core/VulkanContext.h"
#include "core/SwapChain.h"
#include "renderer/GraphicsPipeline.h"
#include "renderer/Renderer.h"
#include "renderer/buffers/Mesh.h"
#include "renderer/buffers/Vertex.h"


Application::Application()
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
    graphicsPipeline_ = std::make_unique<GraphicsPipeline>(*context_, swapChain_->getFormat());
    renderer_ = std::make_unique<Renderer>(*context_);
    renderer_->setupPerImageResources(swapChain_->getImageCount());
 
    /*
        temporary definition of the vertex data
    */
    const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}};

    /*
        temporary definition of the index data
    */
    const std::vector<uint16_t> indices = {0, 1, 2, 2, 3, 0};

    mesh_ = std::make_unique<Mesh>(renderer_->createMesh(vertices, indices));
}


void Application::mainLoop()
{
    while (!glfwWindowShouldClose(window_))
    {
        glfwPollEvents();
        if (!(renderer_->drawFrame(*swapChain_, *graphicsPipeline_, *mesh_)) || framebufferResized_)
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
    renderer_->setupPerImageResources(swapChain_->getImageCount());
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
