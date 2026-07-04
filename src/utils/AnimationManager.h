#pragma once

#include <QGraphicsItem>
#include <QWidget>

class AnimationManager {
public:
    static void pop(QWidget* widget);
    static void fadeIn(QWidget* widget);
    static void shake(QWidget* widget);
    static void graphicsPop(QGraphicsItem* item);

    static void cardFly(QWidget* from, QWidget* to);
    static void coinFly(QWidget* from, QWidget* to);
    static void tileFlash(QGraphicsItem* item);
    static void bannerSlideIn(QWidget* banner);
};
