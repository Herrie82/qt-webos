# -------------------------------------------------
# Project created by QtCreator 2009-12-17T16:35:12
# -------------------------------------------------
QT += core \
    gui
TARGET = webos
TEMPLATE = lib
CONFIG += plugin
LIBS += -lSDL
DESTDIR = $$[QT_INSTALL_PLUGINS]/gfxdrivers
SOURCES += screenplugin.cpp \
    sdlscreen.cpp \
    sdlscreen_private.cpp \
    sdlscreenthread.cpp \
    palmprekeyboard.cpp \
    dlgsymbols.cpp \
    palmpremouse.cpp \
    symbolbutton.cpp
HEADERS += screenplugin.h \
    sdlscreen.h \
    sdlscreen_private.h \
    sdlscreenthread.h \
    palmprekeyboard.h \
    dlgsymbols.h \
    palmpremouse_private.h \
    palmpremouse.h \
    palmprekeyboard_private.h \
    symbolbutton.h

INCLUDEPATH += /opt/PalmPDK.palm/include/SDL
#NCLUDEPATH += SDL

#LIBS += -lz -Wl,-rpath,.

