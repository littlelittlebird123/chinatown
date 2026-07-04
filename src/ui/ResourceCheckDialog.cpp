#include "ui/ResourceCheckDialog.h"

#include "utils/AssetManager.h"

#include <QDialogButtonBox>
#include <QListWidget>
#include <QVBoxLayout>

ResourceCheckDialog::ResourceCheckDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("资源检查"));

    auto* layout = new QVBoxLayout(this);
    m_list = new QListWidget(this);
    layout->addWidget(m_list);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &ResourceCheckDialog::accept);
}

QStringList ResourceCheckDialog::checkRequiredAssets() const
{
    const QStringList required = {
        QStringLiteral("board/tile_empty.png"),
        QStringLiteral("board/tile_hover.png"),
        QStringLiteral("board/tile_selected.png"),
        QStringLiteral("board/tile_owned.png"),
        QStringLiteral("board/tile_built.png"),
        QStringLiteral("board/tile_buildable.png"),
        QStringLiteral("board/tile_trade_target.png"),
        QStringLiteral("shops/restaurant.png"),
        QStringLiteral("shops/laundry.png"),
        QStringLiteral("shops/jewelry.png"),
        QStringLiteral("shops/teahouse.png"),
        QStringLiteral("shops/market.png"),
        QStringLiteral("shops/pharmacy.png"),
        QStringLiteral("ui/button_normal.png"),
        QStringLiteral("ui/button_hover.png"),
        QStringLiteral("ui/button_pressed.png"),
        QStringLiteral("ui/button_disabled.png"),
        QStringLiteral("ui/panel_left.png"),
        QStringLiteral("ui/panel_bottom.png"),
        QStringLiteral("ui/trade_slot.png"),
        QStringLiteral("ui/coin.png"),
        QStringLiteral("avatars/player_1.png"),
        QStringLiteral("avatars/player_2.png"),
        QStringLiteral("avatars/player_3.png"),
        QStringLiteral("avatars/player_4.png")
    };

    QStringList missing;
    for (const QString& key : required) {
        if (!AssetManager::instance().hasPixmap(key)) {
            missing << key;
        }
    }
    return missing;
}

void ResourceCheckDialog::showMissingAssets(const QStringList& missing)
{
    m_list->clear();

    if (missing.isEmpty()) {
        m_list->addItem(QStringLiteral("所有资源均已找到"));
    } else {
        for (const QString& key : missing) {
            m_list->addItem(key);
        }
    }

    exec();
}
