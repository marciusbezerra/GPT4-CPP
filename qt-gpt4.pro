QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# Preferi por essas informações nos arquivos resoucres.rc e version.h:
# RC_ICONS = appicon.ico
# VERSION = 1.0.1
# DEFINES += APP_VERSION=\\\"$$VERSION\\\"

# for macos
ICON = appicon.icns

RC_FILE = resources.rc

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    mainwindow.h \
    styleditemdelegate.h \
    version.h

FORMS += \
    mainwindow.ui

TRANSLATIONS += \
    qt-gpt4_pt_BR.ts
CONFIG += lrelease
CONFIG += embed_translations

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    Resources.qrc

DISTFILES += \
    resources.rc
