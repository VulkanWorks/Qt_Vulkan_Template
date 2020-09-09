#include "QVkRenderer.h"

VkRenderer::~VkRenderer() {
    qDebug("start cleanup event");
    if (!m_devFuncs)
        return;

    m_devFuncs->vkDestroyPipeline(m_dev, m_pipeline, nullptr);
    m_devFuncs->vkDestroyPipelineLayout(m_dev, m_pipelineLayout, nullptr);
    
    m_devFuncs->vkDestroyDescriptorSetLayout(m_dev, m_descSetLayout, nullptr);
    m_devFuncs->vkDestroyDescriptorPool(m_dev, m_descPool, nullptr);

    m_devFuncs->vkDestroyPipelineCache(m_dev, m_pipelineCache, nullptr);

    m_devFuncs->vkDestroyBuffer(m_dev, m_vbuf, nullptr);
    m_devFuncs->vkFreeMemory(m_dev, m_vbufMem, nullptr);

    m_devFuncs->vkDestroyBuffer(m_dev, m_ubuf, nullptr);
    m_devFuncs->vkFreeMemory(m_dev, m_ubufMem, nullptr);
    
    m_devFuncs->vkDestroyCommandPool(m_dev, m_cmdPool, nullptr);
    
    qDebug("cleanup finished");
}

void VkRenderer::frameStart()
{
    QSGRendererInterface *m_rItf = m_window->rendererInterface();

    // Check if RHI api is Vulkan
    Q_ASSERT(m_rItf->graphicsApi() == QSGRendererInterface::VulkanRhi);

    if (!m_initialized)
        init(m_window->graphicsStateInfo().framesInFlight);
}

void VkRenderer::init(int framesInFlight) {
    qDebug("Start initialization");

    assert(framesInFlight <= 3);
    // Load the rendering interface used by scenegraph
    m_rItf = m_window->rendererInterface();
    
    // Deduct the used Vk instance
    {
        m_inst = reinterpret_cast<QVulkanInstance*>(
            m_rItf->getResource(
                m_window, QSGRendererInterface::VulkanInstanceResource)
        );
        assert(m_inst && m_inst->isValid());
    }
    
    // Withdraw all physical & logical devices
    {
        m_gpu = *reinterpret_cast<VkPhysicalDevice*>(
            m_rItf->getResource(
                m_window, QSGRendererInterface::PhysicalDeviceResource)
        );
        assert(m_gpu);
        m_dev = *reinterpret_cast<VkDevice*>(
            m_rItf->getResource(
                m_window, QSGRendererInterface::DeviceResource)
        );
        assert(m_dev);
    }
    
    // Load Vk family function interface
    {
        m_devFuncs = m_inst->deviceFunctions(m_dev);
        m_funcs = m_inst->functions();
        assert(m_devFuncs && m_funcs);
    }
    
    // Get queue family information
    {
        QueueFamilyIndices indices = findQueueFamilies(m_gpu);
        m_devFuncs->vkGetDeviceQueue(
            m_dev, indices.graphicsFamily, 0, &m_graphicsQueue
        );
        m_devFuncs->vkGetDeviceQueue(
            m_dev, indices.presentFamily, 0, &m_presentQueue
        );
        
    }
    
    // Get the rendering process
    {
        VkRenderPass rp = *reinterpret_cast<VkRenderPass*>(
            m_rItf->getResource(
                m_window, QSGRendererInterface::RenderPassResource)
        );
        assert(rp);
    }
    
    // Get device properties
    {
        m_funcs->vkGetPhysicalDeviceProperties(m_gpu, &m_gpuProps);
        m_funcs->vkGetPhysicalDeviceMemoryProperties(m_gpu, &m_gpuMemProps);
    }
    // Create independent command pool and command buffer
    createCommandPool();
        
    // Create vertex Buffer TODO: // indices
    createVertexBuffer(vertexData, m_vbuf, m_vbufMem);
    
    
    // Create UniformBuffer
    createUniformBuffer();
    
    // Create descriptor set and descriptor layout
    createDescriptorPoolAndSets(framesInFlight);
    
    // Create graphics rendering pipeline
    createGraphicsPipeline();
    
    // Default ubo matrix
    {
        m_proj.perspective(
            45.0f, m_viewportSize.width() / (float) m_viewportSize.height(),
            0.01f, 100.0f
        );
        m_proj.translate(0, 0, -4);
    }
    
    m_initialized = true;
    qDebug() << "init finished";
}

void VkRenderer::mainPassRecordingStart() {

    const QQuickWindow::GraphicsStateInfo &stateInfo(m_window->graphicsStateInfo());
    // Update perspective matrix
    {
        VkDeviceSize ubufOffset = stateInfo.currentFrameSlot * m_allocPerUbuf;
        // It can be understood as traversing the array according to the frame number
        quint8* data = nullptr;
        VkResult err = m_devFuncs->vkMapMemory(
            m_dev, m_ubufMem, ubufOffset,
            UNIFORM_DATA_SIZE, 0, reinterpret_cast<void **>(&data)
        );
        if (err != VK_SUCCESS)
            throw std::runtime_error("failed to map memory");
        QMatrix4x4 ubo = m_proj;
        ubo.rotate(m_rotation, 0, 1, 0);
        memcpy(data, ubo.constData(), UNIFORM_DATA_SIZE);
        m_devFuncs->vkUnmapMemory(m_dev, m_ubufMem);
    
        // Spin
        m_rotation += 1.0f;
    }
    // Start additional rendering commands
    m_window->beginExternalCommands();

    VkCommandBuffer cmdBuf = *reinterpret_cast<VkCommandBuffer *>(
        m_rItf->getResource(m_window, QSGRendererInterface::CommandListResource));
    assert(cmdBuf);
    
    m_devFuncs->vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    VkDeviceSize vbufOffset = 0;
    m_devFuncs->vkCmdBindVertexBuffers(cmdBuf, 0, 1, &m_vbuf, &vbufOffset);
    
    uint32_t dynamicOffset = m_allocPerUbuf * stateInfo.currentFrameSlot;
    m_devFuncs->vkCmdBindDescriptorSets(
        cmdBuf, 
        VK_PIPELINE_BIND_POINT_GRAPHICS, 
        m_pipelineLayout,
        0, 1,
        m_descSet,
        1,
        &dynamicOffset
    );

    VkViewport viewPort = {
        0, 0,
        float(m_viewportSize.width()),
        float(m_viewportSize.height()),
        0.0f, 1.0f          // max & min depth
    };
    m_devFuncs->vkCmdSetViewport(cmdBuf, 0, 1, &viewPort);
    
    VkRect2D scissor = {
        { 0, 0 },
        {
            uint32_t(m_viewportSize.width()),
            uint32_t(m_viewportSize.height())
        } 
    };
    m_devFuncs->vkCmdSetScissor(cmdBuf, 0, 1, &scissor);
    m_devFuncs->vkCmdDraw(cmdBuf, 3, 1, 0, 0);
    
    m_window->endExternalCommands();
    
}





uint32_t VkRenderer::findMemoryType(
	uint32_t                typeFilter,
	VkMemoryPropertyFlags   properties
) {
	for (uint32_t i = 0; i < m_gpuMemProps.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) &&
			((m_gpuMemProps.memoryTypes[i].propertyFlags & properties) == 
                properties
             )
		) {
			return i;
		}
	}
	throw std::runtime_error("failed to find suitable memory type");
}

QueueFamilyIndices VkRenderer::findQueueFamilies(VkPhysicalDevice device) {
	QueueFamilyIndices indices;

	uint32_t queueFamilyCount = 0;
	m_funcs->vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	m_funcs->vkGetPhysicalDeviceQueueFamilyProperties(
        device, &queueFamilyCount, queueFamilies.data()
	);

	int index = 0;
	for (const auto& queueFamily : queueFamilies) {
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphicsFamily = index;
		}

		VkBool32 presentSupport = false;
        auto vkGetPhysicalDeviceSurfaceSupportKHR =
            (PFN_vkGetPhysicalDeviceSurfaceSupportKHR)
                m_inst->getInstanceProcAddr("vkGetPhysicalDeviceSurfaceSupportKHR");
        
		vkGetPhysicalDeviceSurfaceSupportKHR(
            device, index, m_inst->surfaceForWindow(m_window), &presentSupport);
		if (queueFamily.queueCount > 0 && presentSupport) {
			indices.presentFamily = index;
		}

		if (indices.isComplete()) {
			break;
		}

		index++;
	}
	return indices;
}

void VkRenderer::createCommandPool() {
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(
        m_gpu
    );
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType =
    VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;
    poolInfo.flags = 0;
    
    if (m_devFuncs->vkCreateCommandPool(
        m_dev, &poolInfo, nullptr, &m_cmdPool
    ) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool");
    }
}

VkDeviceSize VkRenderer::createBuffer(
	VkDeviceSize                size, 
	VkBufferUsageFlags          usage,
	VkMemoryPropertyFlags       properties,
	VkBuffer&                   buffer,
	VkDeviceMemory&             bufferMemory
) {
	VkBufferCreateInfo bufferInfo {};
	bufferInfo.sType =
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	
	if (m_devFuncs->vkCreateBuffer(m_dev, &bufferInfo, nullptr, &buffer)
		!= VK_SUCCESS
	) {
		throw std::runtime_error("failed to create buffer");
	}

	VkMemoryRequirements memRequirements;
	m_devFuncs->vkGetBufferMemoryRequirements(
		m_dev, buffer, &memRequirements
	);
	VkMemoryAllocateInfo allocInfo {};
	allocInfo.sType =
		VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(
		memRequirements.memoryTypeBits, properties
	);

	if (m_devFuncs->vkAllocateMemory(
		m_dev, &allocInfo, nullptr, &bufferMemory
	) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate buffer memory");
	}

	m_devFuncs->vkBindBufferMemory(m_dev, buffer, bufferMemory, 0);
    
    return allocInfo.allocationSize;
}

VkCommandBuffer VkRenderer::beginSingleTimeCommand() {
	VkCommandBufferAllocateInfo allocInfo {};
	allocInfo.sType =
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level =
		VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = m_cmdPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	m_devFuncs->vkAllocateCommandBuffers(
		m_dev, &allocInfo, &commandBuffer
	);

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType =
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags =
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	m_devFuncs->vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

void VkRenderer::endSingleTimeCommand(VkCommandBuffer commandBuffer) {
	m_devFuncs->vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType =
		VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	m_devFuncs->vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	m_devFuncs->vkQueueWaitIdle(m_graphicsQueue);

	m_devFuncs->vkFreeCommandBuffers(m_dev, m_cmdPool, 1, &commandBuffer);
}

void VkRenderer::copyBuffer(
	VkBuffer        srcBuffer,
	VkBuffer        dstBuffer,
	VkDeviceSize    size
) {
    VkCommandBuffer commandBuffer =
		beginSingleTimeCommand();

	VkBufferCopy copyRegion = {};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = size;
	m_devFuncs->vkCmdCopyBuffer(
        commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion
	);

	endSingleTimeCommand(commandBuffer);
}

void VkRenderer::createVertexBuffer(
    std::vector<float>& vertexVec,
    VkBuffer&           vertBuf,
    VkDeviceMemory&     vertMem
) {
    const VkDeviceSize bufSize = 
        sizeof(vertexVec[0]) * vertexVec.size();
    
    VkBuffer stgBuffer;
    VkDeviceMemory stgBufferMem;
    
    VkDeviceSize allocationSize = createBuffer(
        bufSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        stgBuffer,
        stgBufferMem
    );
    
    void* data = nullptr;
    m_devFuncs->vkMapMemory(
        m_dev, stgBufferMem, 0, allocationSize, 0, &data
    );
    memcpy(data, vertexVec.data(), bufSize);
    m_devFuncs->vkUnmapMemory(
        m_dev, stgBufferMem
    );
    
    VkDeviceSize vertAllocationSize = createBuffer(
        bufSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        vertBuf,
        vertMem
    );
    
    copyBuffer(stgBuffer, vertBuf, vertAllocationSize);
    
    m_devFuncs->vkDestroyBuffer(m_dev, stgBuffer, nullptr);
    m_devFuncs->vkFreeMemory(m_dev, stgBufferMem, nullptr);
    
}

void VkRenderer::createIndicesBuffer(
        std::vector<float>& indicesVec,
        VkBuffer&           indicesBuf,
        VkDeviceMemory&     indicesMem
) {
    VkDeviceSize bufSize = sizeof(indicesVec[0]) * indicesVec.size();
    
    VkBuffer stgBuffer;
    VkDeviceMemory stgBufferMem;
    
    createBuffer(
        bufSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        stgBuffer,
        stgBufferMem
    );
    
    void* data = nullptr;
    m_devFuncs->vkMapMemory(
        m_dev, stgBufferMem, 0, bufSize, 0, &data
    );
    memcpy(data, indicesVec.data(), size_t(bufSize));
    m_devFuncs->vkUnmapMemory(
        m_dev, stgBufferMem
    );
    
    createBuffer(
        bufSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        indicesBuf,
        indicesMem
    );
    
    copyBuffer(stgBuffer, indicesBuf, bufSize);
    
    m_devFuncs->vkDestroyBuffer(m_dev, stgBuffer, nullptr);
    m_devFuncs->vkFreeMemory(m_dev, stgBufferMem, nullptr);    
}

void VkRenderer::createUniformBuffer() {
    m_allocPerUbuf =
        aligned(
            UNIFORM_DATA_SIZE, 
            m_gpuProps.limits.minUniformBufferOffsetAlignment
        );
    createBuffer(
        m_window->graphicsStateInfo().framesInFlight * m_allocPerUbuf,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        m_ubuf,
        m_ubufMem
    );

    VkResult err = m_devFuncs->vkBindBufferMemory(m_dev, m_ubuf, m_ubufMem, 0);
    if (err != VK_SUCCESS)
        throw std::runtime_error("failed to bind uniform buffer memory");

    VkMemoryRequirements memRequirements;
	m_devFuncs->vkGetBufferMemoryRequirements(
		m_dev, m_ubuf, &memRequirements
	);
    
    uint8_t *data;
    err = m_devFuncs->vkMapMemory(
        m_dev, m_ubufMem, 0, memRequirements.size, 0, reinterpret_cast<void **>(&data)
    );
    if (err != VK_SUCCESS)
        throw std::runtime_error("failed to map uniform buffer memory");
    
    QMatrix4x4 ident;
    memset(m_uniformBufInfo, 0, sizeof(m_uniformBufInfo));
    for (int i = 0; i < m_window->graphicsStateInfo().framesInFlight; i++) {
        const VkDeviceSize offset = i * m_allocPerUbuf;
        memcpy(data + offset, ident.constData(), 16 * sizeof(float)); //size of 4x4 martix
        m_uniformBufInfo[i].buffer = m_ubuf;
        m_uniformBufInfo[i].offset = offset;
        m_uniformBufInfo[i].range = m_allocPerUbuf;
    }
    m_devFuncs->vkUnmapMemory(m_dev, m_ubufMem);
}

VkShaderModule VkRenderer::createShaderModule(
	const QByteArray& code
) {

	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType =
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(
		code.data());

	VkShaderModule shaderModule;
	if (m_devFuncs->vkCreateShaderModule(
		m_dev, &createInfo, nullptr, &shaderModule
	) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shader module");
	}
	return shaderModule;
}

void VkRenderer::createDescriptorPoolAndSets(int framesInFlight) {
    VkDescriptorPoolSize descPoolSizes = { 
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uint32_t(framesInFlight) 
    };
    VkDescriptorPoolCreateInfo descPoolInfo {};
    descPoolInfo.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descPoolInfo.maxSets =
        framesInFlight;
    descPoolInfo.poolSizeCount =
        1;
    descPoolInfo.pPoolSizes =
        &descPoolSizes;
    VkResult err = m_devFuncs->vkCreateDescriptorPool(
        m_dev, &descPoolInfo, nullptr, &m_descPool
    );
    if (err != VK_SUCCESS)
        throw std::runtime_error("failed to create descriptor pool");

    VkDescriptorSetLayoutBinding layoutBinding = {
        0, // binding
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        1,
        VK_SHADER_STAGE_VERTEX_BIT,
        nullptr
    };
    VkDescriptorSetLayoutCreateInfo descLayoutInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        nullptr,
        0,
        1,
        &layoutBinding
    };
    err = m_devFuncs->vkCreateDescriptorSetLayout(
        m_dev, &descLayoutInfo, nullptr, &m_descSetLayout
    );
    if (err != VK_SUCCESS)
        throw std::runtime_error("failed to create descriptor set layout");
    
    for (int i = 0; i < framesInFlight; ++i) {
        VkDescriptorSetAllocateInfo descSetAllocInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            nullptr,
            m_descPool,
            1,
            &m_descSetLayout
        };
        err = m_devFuncs->vkAllocateDescriptorSets(m_dev, &descSetAllocInfo, &m_descSet[i]);
        if (err != VK_SUCCESS)
            throw std::runtime_error("failed to allocate descriptor set: %d");
        
        VkWriteDescriptorSet descWrite {};
        descWrite.sType =
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descWrite.dstSet =
            m_descSet[i];
        descWrite.descriptorCount =
            1;
        descWrite.descriptorType =
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descWrite.pBufferInfo =
            &m_uniformBufInfo[i];
        m_devFuncs->vkUpdateDescriptorSets(m_dev, 1, &descWrite, 0, nullptr);
    }
}

void VkRenderer::createGraphicsPipeline() {
    
    VkResult err;
    
    //-------------load shader binary code--------------//
    auto vertShaderPath =
            QLatin1String(":/shaders/compiled_shaders/shader.vert.spv");
    auto fragShaderPath =
            QLatin1String(":/shaders/compiled_shaders/shader.frag.spv");
    VkShaderModule vertShaderModule = {};
    VkShaderModule fragShaderModule = {};
    {
        QFile vertShader(vertShaderPath);
        if(!vertShader.open(QIODevice::ReadOnly))
            throw std::runtime_error("failed to read vert shader");
        const QByteArray vertShaderCode = 
                vertShader.readAll();
        assert(!vertShaderCode.isEmpty());
        
        vertShaderModule = createShaderModule(vertShaderCode);
    }
    {
        QFile fragShader(fragShaderPath);
        if(!fragShader.open(QIODevice::ReadOnly))
            throw std::runtime_error("failed to read frag shader");
        const QByteArray fragShaderCode = 
                fragShader.readAll();
        assert(!fragShaderCode.isEmpty());
        
        fragShaderModule = createShaderModule(fragShaderCode);
    }
    
    VkVertexInputBindingDescription vertexBindingDesc = {
        0, // binding
        5 * sizeof(float),
        VK_VERTEX_INPUT_RATE_VERTEX
    };
    VkVertexInputAttributeDescription vertexAttrDesc[] = {
        { // position
            0, // location
            0, // binding
            VK_FORMAT_R32G32_SFLOAT,
            0
        },
        { // color
            1,
            0,
            VK_FORMAT_R32G32B32_SFLOAT,
            2 * sizeof(float)
        }
    };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo {};
    vertexInputInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.pNext = 
        nullptr;
    vertexInputInfo.flags =
        0;
    vertexInputInfo.vertexBindingDescriptionCount =
        1;
    vertexInputInfo.pVertexBindingDescriptions =
        &vertexBindingDesc;
    vertexInputInfo.vertexAttributeDescriptionCount =
        2;
    vertexInputInfo.pVertexAttributeDescriptions =
        vertexAttrDesc;

    // Pipeline cache
    VkPipelineCacheCreateInfo pipelineCacheInfo {};
    pipelineCacheInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    err = m_devFuncs->vkCreatePipelineCache(
        m_dev, &pipelineCacheInfo, nullptr, &m_pipelineCache);
    if (err != VK_SUCCESS)
        throw std::runtime_error("failed to create pipeline cache");

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
    pipelineLayoutInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount =
        1;
    pipelineLayoutInfo.pSetLayouts =
        &m_descSetLayout;
    err = m_devFuncs->vkCreatePipelineLayout(
        m_dev, &pipelineLayoutInfo, nullptr, &m_pipelineLayout
    );
    if (err != VK_SUCCESS)
        throw std::runtime_error("failed to create pipeline layout");
    
    // Graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo {};
    pipelineInfo.sType =
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

    VkPipelineShaderStageCreateInfo shaderStages[2] = {
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr,
            0,
            VK_SHADER_STAGE_VERTEX_BIT,
            vertShaderModule,
            "main",
            nullptr
        },
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr,
            0,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            fragShaderModule,
            "main",
            nullptr
        }
    };
    pipelineInfo.stageCount =
        2;
    pipelineInfo.pStages =
        shaderStages;

    pipelineInfo.pVertexInputState =
        &vertexInputInfo;

    VkPipelineInputAssemblyStateCreateInfo ia {};
    ia.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology =
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pipelineInfo.pInputAssemblyState =
        &ia;

    // Viewpoint and cropping will be dynamically set in vkCmdSetViewport/Scissor
    VkPipelineViewportStateCreateInfo vp {};
    vp.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount =
        1;
    vp.scissorCount =
        1;
    pipelineInfo.pViewportState =
        &vp;

    VkPipelineRasterizationStateCreateInfo rs {};
    rs.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode =
        VK_POLYGON_MODE_FILL;
    rs.cullMode =
        VK_CULL_MODE_NONE; // Setting needs back
    rs.frontFace =
        VK_FRONT_FACE_CLOCKWISE;    //Draw clockwise
    rs.lineWidth =
        1.0f;
    pipelineInfo.pRasterizationState =
        &rs;

    VkPipelineMultisampleStateCreateInfo ms {};
    ms.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    // Apply multiple sampling
    ms.rasterizationSamples =
        VK_SAMPLE_COUNT_1_BIT;
    pipelineInfo.pMultisampleState =
        &ms;

    VkPipelineDepthStencilStateCreateInfo ds {};
    ds.sType =
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable =
        VK_TRUE;
    ds.depthWriteEnable =
        VK_TRUE;
    ds.depthCompareOp =
        VK_COMPARE_OP_LESS_OR_EQUAL;
    pipelineInfo.pDepthStencilState =
        &ds;

    VkPipelineColorBlendStateCreateInfo cb {};
    cb.sType =
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    // Do not use mixing, directly output rgba
    VkPipelineColorBlendAttachmentState att {};
    att.colorWriteMask =
        0xF;
    cb.attachmentCount =
        1;
    cb.pAttachments =
        &att;
    pipelineInfo.pColorBlendState =
        &cb;

    VkDynamicState dynEnable[] = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR 
    };
    VkPipelineDynamicStateCreateInfo dyn {};
    dyn.sType =
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount =
        sizeof(dynEnable) / sizeof(VkDynamicState);
    dyn.pDynamicStates =
        dynEnable;
    pipelineInfo.pDynamicState =
        &dyn;

    pipelineInfo.layout =
        m_pipelineLayout;
    pipelineInfo.renderPass =
        *reinterpret_cast<VkRenderPass *>(
            m_rItf->getResource(m_window, QSGRendererInterface::RenderPassResource)
        );
    
    err = m_devFuncs->vkCreateGraphicsPipelines(
        m_dev, m_pipelineCache, 1, &pipelineInfo, nullptr, &m_pipeline
    );
    if (err != VK_SUCCESS)
        throw std::runtime_error("failed to create graphics pipeline");
    
}
