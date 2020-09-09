#ifndef VKRENDERER_H
#define VKRENDERER_H

#include <QVulkanInstance>
#include <QVulkanFunctions>
#include <QtQuick/QQuickWindow>

#include "include/VkStructureHelper.h"

#define FUNCTION
#define RESOURCE
#define Q_RESOURCE

#define TEST qDebug() << "coco";

class VkRenderer : public QObject {
    Q_OBJECT
public:
    ~VkRenderer();
    
public FUNCTION:
    void 
        setViewportSize(const QSize &size) { m_viewportSize = size; }
    void 
        setWindow(QQuickWindow *window) { m_window = window; }

public slots:
    void
        frameStart();
    void 
        mainPassRecordingStart();

private FUNCTION:
    void 
        init(int framesInFlight);
    uint32_t 
        findMemoryType (
            uint32_t                typeFilter,
            VkMemoryPropertyFlags   properties
        );
    VkDeviceSize 
        createBuffer(
            VkDeviceSize                size, 
            VkBufferUsageFlags          usage,
            VkMemoryPropertyFlags       properties,
            VkBuffer&                   buffer,
            VkDeviceMemory&             bufferMemory
        );
    static inline VkDeviceSize
        aligned(VkDeviceSize v, VkDeviceSize byteAlign) {
            return (v + byteAlign - 1) & ~(byteAlign - 1);
        }
    QueueFamilyIndices
        findQueueFamilies(VkPhysicalDevice device);
    void
        createUniformBuffer();
    void
        createGraphicsPipeline();
    void
        createCommandPool();
    void
        createDescriptorPoolAndSets(int framesInFlight);
//    void
//        createCommandBuffer();
    void 
        createVertexBuffer(
            std::vector<float>& vertexVec,
            VkBuffer&           vertBuf,
            VkDeviceMemory&     vertMem
        );
    void 
        createIndicesBuffer(
            std::vector<float>& indicesVec,
            VkBuffer&           indicesBuf,
            VkDeviceMemory&     indicesMem
        );
    void
        copyBuffer(
            VkBuffer        srcBuffer,
            VkBuffer        dstBuffer,
            VkDeviceSize    size
        );
    VkShaderModule 
        createShaderModule(
            const QByteArray& code
        );
    VkCommandBuffer
        beginSingleTimeCommand();
    void 
        endSingleTimeCommand(VkCommandBuffer);
    
private Q_RESOURCE:
    static const int
        UNIFORM_DATA_SIZE   = 16 * sizeof(float);
    std::vector<float> 
        vertexData
    {
         0.0f,   0.5f,   1.0f, 0.0f, 0.0f,
        -0.5f,  -0.5f,   0.0f, 1.0f, 0.0f,
         0.5f,  -0.5f,   0.0f, 0.0f, 1.0f
    };
    QSize 
        m_viewportSize;
    qreal
        m_value = 0;
    QQuickWindow*
        m_window;

    QByteArray
        m_vert              = {};
    QByteArray
        m_frag              = {};
    QVulkanDeviceFunctions*
        m_devFuncs          = nullptr;
    QVulkanFunctions*
        m_funcs             = nullptr;
    QVulkanInstance* 
        m_inst              = nullptr;
    QSGRendererInterface*
        m_rItf              = nullptr;
    QMatrix4x4 
        m_proj              = {};
    
private RESOURCE:
    bool 
        m_initialized       = false;
    float 
        m_rotation          = 0.0f;
    static const int
        MAX_CONCURRENT_FRAME_COUNT = 3;
    VkPhysicalDevice 
        m_gpu               = VK_NULL_HANDLE;
    VkDevice 
        m_dev               = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties
        m_gpuProps;
    VkPhysicalDeviceMemoryProperties
        m_gpuMemProps;
    VkCommandPool
        m_cmdPool           = VK_NULL_HANDLE;
    
    VkBuffer 
        m_vbuf              = VK_NULL_HANDLE;
    VkDeviceMemory
        m_vbufMem           = VK_NULL_HANDLE;
    VkBuffer
        m_ubuf              = VK_NULL_HANDLE;
    VkDeviceMemory 
        m_ubufMem           = VK_NULL_HANDLE;
    VkDeviceSize 
        m_allocPerUbuf      = 0;

    VkDescriptorBufferInfo
        m_uniformBufInfo[
            MAX_CONCURRENT_FRAME_COUNT
        ];

    VkDescriptorPool 
        m_descPool          = VK_NULL_HANDLE;
    VkDescriptorSetLayout 
        m_descSetLayout     = VK_NULL_HANDLE;
    VkDescriptorSet
        m_descSet[
            MAX_CONCURRENT_FRAME_COUNT
        ];
    
    VkPipelineCache 
        m_pipelineCache     = VK_NULL_HANDLE;

    VkPipelineLayout 
        m_pipelineLayout    = VK_NULL_HANDLE;
    VkPipeline 
        m_pipeline          = VK_NULL_HANDLE;
    
    // For the queue here, it is implicitly designated as a shared queue for drawing and writing
    VkQueue					 
        m_graphicsQueue     = {};
    VkQueue					
        m_presentQueue      = {};
};

#endif // VKRENDERER_H
