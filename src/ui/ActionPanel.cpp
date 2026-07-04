#include "ui/ActionPanel.h"

#include <QLabel>
#include <QVBoxLayout>

ActionPanel::ActionPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto* hint = new QLabel(QStringLiteral("本机玩家操作\n抽地块：点击地图上闪烁地块\n交易：点击左侧玩家旁的交易按钮\n建设：直接点击地图上的可建地块"), this);
    hint->setAlignment(Qt::AlignCenter);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("font-weight: bold; color: #fce7b2;"));
    layout->addWidget(hint);

    layout->addStretch(1);
    refreshButtons();
}

void ActionPanel::setPhase(GamePhase phase)
{
    m_phase = phase;
    refreshButtons();
}

void ActionPanel::setTradeClosedForLocalPlayer(bool closed)
{
    m_tradeClosedForLocalPlayer = closed;
    refreshButtons();
}

void ActionPanel::refreshButtons()
{
    if (m_tradeButton) {
        m_tradeButton->setVisible(false);
        m_tradeButton->setEnabled(false);
    }

    if (m_buildButton) {
        m_buildButton->setVisible(false);
        m_buildButton->setEnabled(false);
    }
}

void ActionPanel::setButtonHint(const QString& buttonName, const QString& hint)
{
    if (buttonName == QStringLiteral("trade") && m_tradeButton) {
        m_tradeButton->setToolTip(hint);
    } else if (buttonName == QStringLiteral("build") && m_buildButton) {
        m_buildButton->setToolTip(hint);
    }
}
