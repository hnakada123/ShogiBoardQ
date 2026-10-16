#ifndef BOARDCOLORDIALOG_H
#define BOARDCOLORDIALOG_H

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
class QIcon;
class QFormLayout;
class QTabWidget;
class QCheckBox;
class QSpinBox;
class QListWidget;
class QVBoxLayout;
class BoardAppearancePreview;

/// 駒・盤・背景・駒台・対局者情報の見本と詳細設定を一つのウィンドウで選択する。
class BoardColorDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BoardColorDialog(QWidget* parent = nullptr);
    ~BoardColorDialog() override;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void chooseColor();
    void applyColor(const QColor& color);
    void refreshButtons();
    void restoreDefaults();
    void savePickerSize();
    void rebuildPresets();
    void applyPreset(int index);
    void applyTheme(int index);
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
    QComboBox* m_themeCombo = nullptr;
    QCheckBox* m_woodGrain = nullptr;
    QCheckBox* m_pieceShadow = nullptr;
    QSpinBox* m_pieceScale = nullptr;
    QCheckBox* m_standWoodGrain = nullptr;
    QList<BoardThemePreset> m_themes;
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
    void syncThemeSelection();
    void addColorField(QFormLayout* form, BoardColors::Member member,
                       const QString& label, const QString& objectName);
    void syncPresetSelection();
    QIcon presetIcon(const BoardColors& colors, const QString& style = QString(),
                     const BoardVisuals& visuals = BoardVisuals{}) const;
};

#endif // BOARDCOLORDIALOG_H
