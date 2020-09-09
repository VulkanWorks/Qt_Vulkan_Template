#include <QGuiApplication>
#include <QtQuick/QQuickView>
#include "QVulkanScene/QVulkanScene.h"

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    qmlRegisterType<QVulkanScene>("VulkanBackend", 1, 14, "VulkanScene");

    // This example needs Vulkan. It will not run otherwise.
    QQuickWindow::setSceneGraphBackend(QSGRendererInterface::VulkanRhi);

    QQuickView view;
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setSource(QUrl("qrc:/qml/main.qml"));
    view.show();

    return app.exec();
}
