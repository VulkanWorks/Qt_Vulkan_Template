!qtConfig(vulkan): error("This application requires Qt built with Vulkan support")

QT += quick gui

CONFIG += c++20

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Refer to the documentation for the
# deprecated API to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


# Vulkan SDK
INCLUDEPATH += /home/shujaat/vulkan/1.1.126.0/x86_64/include
DEPENDPATH += /home/shujaat/vulkan/1.1.126.0/x86_64/include
LIBS+= -L/home/shujaat/vulkan/1.1.126.0/x86_64/lib -lvulkan


SOURCES += \
	src/QVulkanScene/QVkRenderer.cpp \
        src/QVulkanScene/QVulkanScene.cpp\
        src/main.cpp

HEADERS += \
	src/QVulkanScene/QVkRenderer.h \
        src/QVulkanScene/QVulkanScene.h \
        include/VkStructureHelper.h

# Compile shaders before adding them in resources.
Shaders = \
    shader.vert \
    shader.frag

$$system(mkdir $$_PRO_FILE_PWD_/shaders/compiled_shaders)

for (shader, Shaders) {
    exists($$_PRO_FILE_PWD_/shaders/source_shaders/$${shader}) {
        message(Compiling Spir-V $$_PRO_FILE_PWD_/shaders/source_shaders/$${shader})
        ERROR = $$system(/home/shujaat/vulkan/1.1.126.0/x86_64/bin/glslangValidator -V -o $$_PRO_FILE_PWD_/shaders/compiled_shaders/$${shader}.spv $$_PRO_FILE_PWD_/shaders/source_shaders/$${shader})
        message($$ERROR)
    }
}
# Shader compilation end.

RESOURCES += \
    resources.qrc

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Additional import path used to resolve QML modules just for Qt Quick Designer
QML_DESIGNER_IMPORT_PATH =

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

