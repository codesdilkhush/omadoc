QT += core gui qml quick quickcontrols2 dbus
CONFIG += c++17 release link_pkgconfig
PKGCONFIG += poppler-cpp

TARGET = omadoc
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/documentengine.h \
    src/pageimageprovider.h \
    src/portalfilepicker.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/documentengine.cpp \
    src/pageimageprovider.cpp \
    src/portalfilepicker.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
