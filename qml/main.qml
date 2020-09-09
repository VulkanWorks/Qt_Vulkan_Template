import QtQuick 2.14
import QtQuick.Controls 2.14

import VulkanBackend 1.14

Item {
    id:         root
    width:      450
    height:     800

    VulkanScene {
        id: vkScene
    }
    
    Slider {
        anchors {
            horizontalCenter: parent.horizontalCenter
            bottom: parent.bottom
            bottomMargin: 20
        }
        width: parent.width / 2
        height: 20
        onValueChanged: {
            vkScene.value = value
        }
    }
}
