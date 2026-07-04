#pragma once

#include <QWidget>

class ToastMessage : public QWidget {
    Q_OBJECT

public:
    static void showMessage(QWidget* parent,
                            const QString& text,
                            int duration = 2000);
};
