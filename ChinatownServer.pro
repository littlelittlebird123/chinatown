QT += core network

CONFIG += c++17 console
CONFIG -= app_bundle
TEMPLATE = app
TARGET = ChinatownServer

INCLUDEPATH += src

SOURCES += \
    src/server/server_main.cpp \
    src/server/GameServer.cpp \
    src/network/NetworkMessage.cpp

HEADERS += \
    src/server/GameServer.h \
    src/network/NetworkMessage.h
