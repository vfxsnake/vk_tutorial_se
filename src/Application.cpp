#include "Application.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <random>
#include <thread>

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
#include "renderer/compute/ParticleDescriptorLayout.h"
#include "renderer/compute/ComputePipeline.h"
#include "renderer/compute/ComputeThreadPool.h"
#include "renderer/compute/ParticleGraphicsPipeline.h"
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
    
    particleDescriptorLayout_ = std::make_unique<ParticleDescriptorLayout>(*context_);
    
    computePipeline_ = std::make_unique<ComputePipeline>(*context_, *particleDescriptorLayout_);

    uint32_t thread_count = std::max(1u, std::thread::hardware_concurrency() -1);
    computeThreadPool_ = std::make_unique<ComputeThreadPool>(*context_, *computePipeline_, *particleDescriptorLayout_, thread_count);

    particleGraphicsPipeline_ = std::make_unique<ParticleGraphicsPipeline>(
        *context_,
        *particleDescriptorLayout_,
        swapChain_->getFormat(),
        context_->findDepthFormat(),
        context_->getMsaaSamples()
    );


    renderer_ = std::make_unique<Renderer>(
        *context_, 
        *textureDescriptorLayout_,
        *frameDescriptorLayout_,
        *computePipeline_,
        *particleGraphicsPipeline_,
        *particleDescriptorLayout_,
        *computeThreadPool_
    );

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

    uint32_t mesh_index = renderer_->addMesh(renderer_->createMesh(model.vertices_, model.indices_));
    
    // instancing 3 times the viking room model 
    scene::Object instance_1;
    instance_1.mesh_index = mesh_index;
    instance_1.transform.position = {0.5, 0.5, 0.5};
    instance_1.transform.scale = {0.5, 0.5, 0.5};
    instance_1.transform.rotation.x = glm::radians(-30.0f);
    
    scene::Object instance_2; 
    instance_2.mesh_index = mesh_index;
    instance_2.transform.rotation.x = glm::radians(30.0f);
    
    scene::Object instance_3;
    instance_3.mesh_index = mesh_index;
    instance_3.transform.position = {-0.5, -0.5, -0.5};
    instance_3.transform.rotation.x = glm::radians(90.0f);
    instance_3.transform.scale = {.25, .25, .25};

    objects_.push_back(instance_1);
    objects_.push_back(instance_2);
    objects_.push_back(instance_3);

    // creating objects render data 
    for (const auto& obj : objects_)
    {
        renderer_->createObjectRenderData(obj.mesh_index);
    }

    renderer_->createParticleSystem(generateParticles());
}


void Application::mainLoop()
{
    // initializing lastFrameTime as this will be the starting point of the delta time.
    lastFrameTime_ = std::chrono::high_resolution_clock::now();

    while (!glfwWindowShouldClose(window_))
    {
        glfwPollEvents();
        
        auto current_time = std::chrono::high_resolution_clock::now();
        float delta_time = std::chrono::duration<float>(current_time - lastFrameTime_).count(); 
        lastFrameTime_ = current_time;
        float elapsed_time = std::chrono::duration<float>(current_time - startTime_).count(); 

        std::vector<UniformBufferObject> uniform_buffer_objects;
        uniform_buffer_objects.reserve(objects_.size());

        for (const auto& obj : objects_)
        {
            uniform_buffer_objects.push_back(
                computeUniformBufferObject(swapChain_->getExtent(),elapsed_time, obj)
            );
        }

        //  drawing frame
        bool was_frame_drawn = renderer_->drawFrame(
            *swapChain_, 
            *graphicsPipeline_,
            uniform_buffer_objects,
            depthImage_->getImageView(),
            msaaColorImage_->getImageView(),
            delta_time
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


UniformBufferObject Application::computeUniformBufferObject(
    vk::Extent2D extent, 
    float time_seconds,
    const scene::Object& scene_object
) const
{
    UniformBufferObject model_view_projection;
    model_view_projection.modelMatrix_ = scene_object.transform.getModelMatrix();
    
    model_view_projection.modelMatrix_ = glm::rotate(
        model_view_projection.modelMatrix_, // transformation matrix
        time_seconds * glm::radians(90.0f), // angle in radiants
        glm::vec3(0.0f, 0.0f, 1.0f) // rotation vector axis
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


std::vector<Particle> Application::generateParticles() const
{
    // Initialize particles
    std::default_random_engine rndEngine(static_cast<unsigned>(time(nullptr)));
    std::uniform_real_distribution rndDist(0.0f, 1.0f);

    // Initial particle positions on a circle
    std::vector<Particle> particles(PARTICLE_COUNT);
    for (auto &particle : particles)
    {
        float r = 0.25f * sqrtf(rndDist(rndEngine));
        float theta = rndDist(rndEngine) * 2.0f * glm::pi<float>();
        float x = r * cosf(theta) * (static_cast<float>(HEIGHT) / static_cast<float>(WIDTH));
        float y = r * sinf(theta);
        particle.position_ = glm::vec2(x, y);
        particle.velocity_ = normalize(glm::vec2(x, y)) * 0.25f;
        particle.color_ = glm::vec4(rndDist(rndEngine), rndDist(rndEngine), rndDist(rndEngine), 1.0f);
    }

    return particles;
}