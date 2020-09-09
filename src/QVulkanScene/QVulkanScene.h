#ifndef QVULKANSCENE_H
#define QVULKANSCENE_H

#include <QQuickItem>

#define FUNCTION
#define RESOURCE

class VkRenderer;

class QVulkanScene : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(qreal value READ value WRITE setValue NOTIFY valueChanged)
    
public:
    QVulkanScene();
    
public FUNCTION:
    qreal 
        value() const { return m_value; }
    void 
        setValue(qreal value);

signals:
    void 
        valueChanged();

public slots:
    void 
        sync();
    void 
        cleanup();

private slots:
    void 
        handleWindowChanged(QQuickWindow *win);

private FUNCTION:
    void 
        releaseResources() override;
    
private RESOURCE:
    qreal 
        m_value     = 0;
    VkRenderer*
        m_renderer  = nullptr;
};

#endif  // QVULKANSCENE_H
