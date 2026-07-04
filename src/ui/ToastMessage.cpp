#include "ui/ToastMessage.h"

#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QPropertyAnimation>
#include <QTimer>
#include <QVBoxLayout>

void ToastMessage::showMessage(QWidget* parent,
                               const QString& text,
                               int duration)
{
    if (!parent) {
        return;
    }

    auto* toast = new QWidget(parent);
    toast->setAttribute(Qt::WA_DeleteOnClose);
    toast->setWindowFlags(Qt::FramelessWindowHint);
    toast->setStyleSheet(QStringLiteral(
        "QWidget { background: rgba(255, 214, 84, 242); color: #2f261f; border: 2px solid #7a3f00; border-radius: 10px; }"
        "QLabel { color: #2f261f; font-weight: 700; font-size: 15px; padding: 10px 16px; }"));

    auto* layout = new QVBoxLayout(toast);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* label = new QLabel(text, toast);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);

    toast->adjustSize();
    const int x = parent->width() - toast->width() - 18;
    const int y = 18;
    toast->move(x, y);

    auto* effect = new QGraphicsOpacityEffect(toast);
    toast->setGraphicsEffect(effect);
    effect->setOpacity(0.0);

    toast->show();

    auto* fadeIn = new QPropertyAnimation(effect, "opacity", toast);
    fadeIn->setDuration(120);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);
    fadeIn->start(QAbstractAnimation::DeleteWhenStopped);

    QTimer::singleShot(duration, toast, [toast, effect] {
        auto* fadeOut = new QPropertyAnimation(effect, "opacity", toast);
        fadeOut->setDuration(260);
        fadeOut->setStartValue(1.0);
        fadeOut->setEndValue(0.0);
        QObject::connect(fadeOut, &QPropertyAnimation::finished, toast, &QWidget::close);
        fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
    });
}
