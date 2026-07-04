#pragma once

#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTableWidget>

#include "core/GameTypes.h"

class GameController;

class BuildDialog : public QDialog {
    Q_OBJECT

public:
    explicit BuildDialog(QWidget* parent = nullptr);

    void setController(GameController* controller);
    void setPlayerId(int playerId);
    void setCandidateTiles(const QList<int>& tileIds);

signals:
    void buildConfirmed(ShopType type, QList<int> tileIds);

private slots:
    void refreshPreview();
    void confirmBuild();
    void confirmBatchBuild();

private:
    void refreshShopChoices();
    void refreshTileChoices();
    void refreshBatchChoices();

    QList<int> selectedTileIds() const;
    ShopType selectedShopType() const;
    QList<QPair<ShopType, int>> selectedBatchPlans(QString* errorMessage = nullptr) const;

    GameController* m_controller = nullptr;
    int m_playerId = -1;
    QList<int> m_candidateTiles;

    QComboBox* m_shopCombo = nullptr;
    QListWidget* m_tileList = nullptr;
    QLabel* m_previewLabel = nullptr;
    QPushButton* m_confirmButton = nullptr;

    QTableWidget* m_batchTable = nullptr;
    QLabel* m_batchPreviewLabel = nullptr;
    QPushButton* m_batchConfirmButton = nullptr;
};
