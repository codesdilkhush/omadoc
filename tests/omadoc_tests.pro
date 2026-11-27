QT += core gui quick quickcontrols2 testlib dbus
CONFIG += c++17 testcase link_pkgconfig
PKGCONFIG += poppler-cpp libarchive

TARGET = omadoc_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += \
    ../src/archiveutils.h \
    ../src/backend.h \
    ../src/documentbackend.h \
    ../src/pdfbackend.h \
    ../src/comicbackend.h \
    ../src/epubbackend.h \
    ../src/documentengine.h \
    ../src/pageimageprovider.h \
    ../src/portalfilepicker.h \
    ../src/theme.h \
    ../src/translator.h

SOURCES += \
    omadoc_tests.cpp \
    ../src/backend.cpp \
    ../src/pdfbackend.cpp \
    ../src/comicbackend.cpp \
    ../src/epubbackend.cpp \
    ../src/documentengine.cpp \
    ../src/pageimageprovider.cpp \
    ../src/portalfilepicker.cpp \
    ../src/theme.cpp \
    ../src/translator.cpp
