#pragma once

#include <QDialog>
#include <QStringList>

class QListWidget;

class ResourceCheckDialog : public QDialog {
    Q_OBJECT

public:
    explicit ResourceCheckDialog(QWidget* parent = nullptr);

    QStringList checkRequiredAssets() const;
    void showMissingAssets(const QStringList& missing);

private:
    QListWidget* m_list = nullptr;
};
