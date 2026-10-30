#ifndef BOARDAPPEARANCEDIALOG_H
#define BOARDAPPEARANCEDIALOG_H

/// @file boardappearancedialog.h
/// @brief 対局画面の外観ダイアログ（駒・将棋盤・背景・駒台・対局者情報の見本と詳細設定）

#include "appearancestate.h"
#include "boardappearancecatalog.h"
#include "boardcolors.h"
#include "boardcolorpresets.h"
#include "boardvisuals.h"
#include <QDialog>
#include <QStringList>
#include <array>

class QColorDialog;
class QPushButton;
class QComboBox;
class QLabel;
class QFormLayout;
class QTabWidget;
class QCheckBox;
class QSpinBox;
class QListWidget;
class QVBoxLayout;
class BoardAppearancePreview;

/**
 * @brief 駒・盤・背景・駒台・対局者情報の見本と詳細設定を一つのウィンドウで選択する
 *
 * 選んだ外観は BoardAppearance（すべての盤面が共有する正本）に書き、すぐに反映・保存される。
 * 「おすすめの組み合わせ」は BoardAppearanceCatalog::combinations()。見本の画像は BoardAppearanceSamples が描く。
 */
class BoardAppearanceDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BoardAppearanceDialog(QWidget* parent = nullptr);
    ~BoardAppearanceDialog() override;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void chooseColor();
    void applyColor(const QColor& color);
    void refreshButtons();
    void restoreDefaults();
    void rebuildPresets();
    void applyPreset(int index);
    void applyVisuals();
    void refreshVisuals();
    void selectPiece(int row);
    void selectComponent(int row);
    void filterPieces(int index);
    void syncCatalog();
    void refreshCatalogIcons();
    void applyCombination(int index);
    void restoreOpeningAppearance();

private:
    struct ColorField {
        QPushButton* button;
        QColor BoardColors::* member;
        QString label;
    };
    QList<ColorField> m_fields;
    QTabWidget* m_tabs;
    QColorDialog* m_picker;
    QColor BoardColors::* m_selectedMember = nullptr;
    QLabel* m_presetLabel;
    QComboBox* m_presetCombo;
    QList<BoardColorPreset> m_presets;
    QCheckBox* m_woodGrain = nullptr;
    QCheckBox* m_pieceShadow = nullptr;
    QSpinBox* m_pieceScale = nullptr;
    QCheckBox* m_standWoodGrain = nullptr;
    QTabWidget* m_sections = nullptr;
    QComboBox* m_pieceFilter = nullptr;
    QComboBox* m_combinations = nullptr;
    QListWidget* m_pieceList = nullptr;
    /// 部品の見本の一覧（BoardAppearanceCatalog::Component の順）
    std::array<QListWidget*, BoardAppearanceCatalog::kComponentCount> m_componentLists{};
    QStringList m_sectionIds;  ///< 外観のタブの id（タブの並び。開いていたタブの保存に使う）
    QLabel* m_selectionSummary = nullptr;
    BoardAppearancePreview* m_preview = nullptr;
    AppearanceState m_opening;  ///< 開いたときの外観（「開いたときの外観に戻す」）
    void createWorkspace(QVBoxLayout* layout);
    void createColorPages();
    void createAppearancePage();
    void addSection(const QString& id, QWidget* page, const QString& title);
    void addColorField(QFormLayout* form, const BoardColors::Field& field);
    void syncPresetSelection();
};

#endif // BOARDAPPEARANCEDIALOG_H
