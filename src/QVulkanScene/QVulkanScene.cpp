#include "QVkRenderer.h"
#include "QVulkanScene.h"

#include <QtCore/QRunnable>

QVulkanScene::QVulkanScene()
{
    connect(this, &QQuickItem::windowChanged, this, &QVulkanScene::handleWindowChanged);
}

void QVulkanScene::setValue(qreal value)
{
    if (value == m_value)
        return;
    m_value = value;
    emit valueChanged();
    if (window())
        window()->update();
}

void QVulkanScene::handleWindowChanged(QQuickWindow *win)
{
    if (win) {
        connect(win, &QQuickWindow::beforeSynchronizing, this, &QVulkanScene::sync, Qt::DirectConnection);
        connect(win, &QQuickWindow::sceneGraphInvalidated, this, &QVulkanScene::cleanup, Qt::DirectConnection);

        // Ensure we start with cleared to black. The squircle's blend mode relies on this.
        win->setColor(Qt::black);
    }
}

// The safe way to release custom graphics resources is to both connect to
// sceneGraphInvalidated() and implement releaseResources(). To support
// threaded render loops the latter performs the VkRenderer destruction
// via scheduleRenderJob(). Note that the QVulkanScene may be gone by the time
// the QRunnable is invoked.

void QVulkanScene::cleanup()
{
    delete m_renderer;
    m_renderer = nullptr;
}

class CleanupJob : public QRunnable
{
public:
    CleanupJob(VkRenderer *renderer) : m_renderer(renderer) { }
    void run() override { delete m_renderer; }
private:
    VkRenderer *m_renderer;
};

void QVulkanScene::releaseResources()
{
    window()->scheduleRenderJob(new CleanupJob(m_renderer), QQuickWindow::BeforeSynchronizingStage);
    m_renderer = nullptr;
}

void QVulkanScene::sync()
{
    if (!m_renderer) {
        m_renderer = new VkRenderer;
        // Initializing resources is done before starting to record the
        // renderpass, regardless of wanting an underlay or overlay.
        connect(window(), &QQuickWindow::beforeRendering, m_renderer, &VkRenderer::frameStart, Qt::DirectConnection);
        // Here we want an underlay and therefore connect to
        // beforeRenderPassRecording. Changing to afterRenderPassRecording
        // would render the squircle on top (overlay).
        connect(window(), &QQuickWindow::beforeRenderPassRecording, m_renderer, &VkRenderer::mainPassRecordingStart, Qt::DirectConnection);
    }
    m_renderer->setViewportSize(window()->size() * window()->devicePixelRatio());
    m_renderer->setWindow(window());
}
