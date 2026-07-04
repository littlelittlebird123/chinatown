QT += core gui widgets network multimedia

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17
TEMPLATE = app
TARGET = ChinatownGame

INCLUDEPATH += src

SOURCES += \
    main.cpp \
    src/core/GameController.cpp \
    src/core/GameState.cpp \
    src/utils/AnimationManager.cpp \
    src/utils/AssetManager.cpp \
    src/ui/ActionPanel.cpp \
    src/ui/BoardTheme.cpp \
    src/ui/BoardView.cpp \
    src/ui/BuildDialog.cpp \
    src/ui/GameLogPanel.cpp \
    src/ui/HandPanel.cpp \
    src/ui/MainWindow.cpp \
    src/ui/PlayerPanel.cpp \
    src/ui/ResourceCheckDialog.cpp \
    src/ui/TileInfoPanel.cpp \
    src/ui/TileItem.cpp \
    src/ui/ToastMessage.cpp \
    src/ui/TradeDialog.cpp \
    src/network/NetworkClient.cpp \
    src/network/NetworkMessage.cpp \
    src/network/NetworkGameBridge.cpp \
    src/server/GameServer.cpp

HEADERS += \
    src/core/GameController.h \
    src/core/GameState.h \
    src/core/GameTypes.h \
    src/utils/AnimationManager.h \
    src/utils/AssetManager.h \
    src/ui/ActionPanel.h \
    src/ui/BoardTheme.h \
    src/ui/BoardView.h \
    src/ui/BuildDialog.h \
    src/ui/GameLogPanel.h \
    src/ui/HandPanel.h \
    src/ui/MainWindow.h \
    src/ui/PlayerPanel.h \
    src/ui/ResourceCheckDialog.h \
    src/ui/TileInfoPanel.h \
    src/ui/TileItem.h \
    src/ui/ToastMessage.h \
    src/ui/TradeDialog.h \
    src/network/NetworkClient.h \
    src/network/NetworkMessage.h \
    src/network/NetworkGameBridge.h \
    src/server/GameServer.h

RESOURCES += resources.qrc
