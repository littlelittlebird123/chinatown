#include "utils/AnimationManager.h"

#include <QGraphicsObject>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>
#include <QTimer>

void AnimationManager::pop(QWidget* widget)
{
    if (!widget) {
        return;
    }

    QRect base = widget->geometry();
    QRect enlarged = base.adjusted(-4, -4, 4, 4);

    auto* group = new QSequentialAnimationGroup(widget);

    auto* grow = new QPropertyAnimation(widget, "geometry", group);
    grow->setDuration(80);
    grow->setStartValue(base);
    grow->setEndValue(enlarged);

    auto* shrink = new QPropertyAnimation(widget, "geometry", group);
    shrink->setDuration(100);
    shrink->setStartValue(enlarged);
    shrink->setEndValue(base);

    group->addAnimation(grow);
    group->addAnimation(shrink);
    group->start(QAbstractAnimation::DeleteWhenStopped);
}

void AnimationManager::fadeIn(QWidget* widget)
{
    if (!widget) {
        return;
    }

    widget->setWindowOpacity(0.0);
    widget->show();

    auto* animation = new QPropertyAnimation(widget, "windowOpacity", widget);
    animation->setDuration(180);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void AnimationManager::shake(QWidget* widget)
{
    if (!widget) {
        return;
    }

    const QRect base = widget->geometry();
    auto* group = new QSequentialAnimationGroup(widget);

    for (int i = 0; i < 4; ++i) {
        auto* move = new QPropertyAnimation(widget, "geometry", group);
        move->setDuration(35);
        const int offset = (i % 2 == 0) ? 8 : -8;
        move->setStartValue(base.translated(offset, 0));
        move->setEndValue(base.translated(-offset, 0));
        group->addAnimation(move);
    }

    auto* reset = new QPropertyAnimation(widget, "geometry", group);
    reset->setDuration(35);
    reset->setEndValue(base);
    group->addAnimation(reset);

    group->start(QAbstractAnimation::DeleteWhenStopped);
}

void AnimationManager::graphicsPop(QGraphicsItem* item)
{
    auto* object = dynamic_cast<QGraphicsObject*>(item);
    if (!object) {
        return;
    }

    auto* group = new QSequentialAnimationGroup(object);

    auto* up = new QPropertyAnimation(object, "scale", group);
    up->setDuration(80);
    up->setStartValue(1.0);
    up->setEndValue(1.12);

    auto* down = new QPropertyAnimation(object, "scale", group);
    down->setDuration(120);
    down->setStartValue(1.12);
    down->setEndValue(1.0);

    group->addAnimation(up);
    group->addAnimation(down);
    group->start(QAbstractAnimation::DeleteWhenStopped);
}

void AnimationManager::cardFly(QWidget* from, QWidget* to)
{
    Q_UNUSED(from);
    if (to) {
        pop(to);
    }
}

void AnimationManager::coinFly(QWidget* from, QWidget* to)
{
    Q_UNUSED(from);
    if (to) {
        pop(to);
    }
}

void AnimationManager::tileFlash(QGraphicsItem* item)
{
    auto* object = dynamic_cast<QGraphicsObject*>(item);
    if (!object) {
        return;
    }

    auto* animation = new QPropertyAnimation(object, "opacity", object);
    animation->setDuration(240);
    animation->setStartValue(0.35);
    animation->setEndValue(1.0);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void AnimationManager::bannerSlideIn(QWidget* banner)
{
    if (!banner) {
        return;
    }

    QRect end = banner->geometry();
    QRect start = end.translated(0, -end.height() - 16);

    auto* animation = new QPropertyAnimation(banner, "geometry", banner);
    animation->setDuration(220);
    animation->setStartValue(start);
    animation->setEndValue(end);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}
