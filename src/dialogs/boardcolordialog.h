#ifndef BOARDCOLORDIALOG_H
#define BOARDCOLORDIALOG_H

#include "boardcolors.h"
#include "boardcolorpresets.h"
#include "boardvisuals.h"
#include <QDialog>

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
    QList<QListWidget*> m_componentLists;
    QLabel* m_selectionSummary = nullptr;
    BoardAppearancePreview* m_preview = nullptr;
    BoardColors m_openingColors;
    BoardVisuals m_openingVisuals;
    QString m_openingStyle;
    void createWorkspace(QVBoxLayout* layout);
    void createColorPages();
    void createAppearancePage();
    void syncThemeSelection();
    void addColorField(QFormLayout* form, BoardColors::Member member,
                       const QString& label, const QString& objectName);
    void syncPresetSelection();
    QIcon presetIcon(const BoardColors& colors, const QString& style = QString(),
                     const BoardVisuals& visuals = BoardVisuals{}) const;
};

#endif // BOARDCOLORDIALOG_H
