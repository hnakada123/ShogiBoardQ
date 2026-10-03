/// @file appsettings.cpp
/// @brief アプリケーション全般設定の永続化実装

#include "appsettings.h"
#include "settingscommon.h"
#include "settingskeys.h"
#include <QSettings>
#include <QWidget>

namespace AppSettings {

int dialogFontSize(const QString& id, int defaultSize)
{
    const QString key = QStringLiteral("DialogAppearance/%1/fontSize").arg(id);
    return qBound(8, SettingsCommon::openSettings().value(key, defaultSize).toInt(), 24);
}

void setDialogFontSize(const QString& id, int size)
{
    SettingsCommon::openSettings().setValue(QStringLiteral("DialogAppearance/%1/fontSize").arg(id), qBound(8, size, 24));
}

QSize auxiliaryDialogSize(const QString& id)
{
    const QString key = QStringLiteral("DialogAppearance/%1/size").arg(id);
    return SettingsCommon::openSettings().value(key, QSize()).toSize();
}

void setAuxiliaryDialogSize(const QString& id, const QSize& size)
{
    SettingsCommon::openSettings().setValue(QStringLiteral("DialogAppearance/%1/size").arg(id), size);
}

QSize versionDialogSize()
{
    return SettingsCommon::openSettings().value(SettingsKeys::kVersionDialogSize, QSize(680, 760)).toSize();
}

void setVersionDialogSize(const QSize& size)
{
    SettingsCommon::openSettings().setValue(SettingsKeys::kVersionDialogSize, size);
}

int versionDialogDocument()
{
    return SettingsCommon::openSettings().value(SettingsKeys::kVersionDialogDocument, 0).toInt();
}

void setVersionDialogDocument(int index)
{
    SettingsCommon::openSettings().setValue(SettingsKeys::kVersionDialogDocument, index);
}

// --- 言語 ---

QString language()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kLanguage, "system").toString();
}

void setLanguage(const QString& lang)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kLanguage, lang);
}

QString moveNotation()
{
    return SettingsCommon::openSettings().value(SettingsKeys::kMoveNotation, "auto").toString();
}
void setMoveNotation(const QString& notation)
{
    SettingsCommon::openSettings().setValue(SettingsKeys::kMoveNotation, notation);
}
bool notationOrigin()
{
    return SettingsCommon::openSettings().value(SettingsKeys::kNotationOrigin, false).toBool();
}
void setNotationOrigin(bool enabled)
{
    SettingsCommon::openSettings().setValue(SettingsKeys::kNotationOrigin, enabled);
}

// --- GUI共通フォント ---

QString uiFontFamily()
{
    return SettingsCommon::openSettings().value(SettingsKeys::kUiFontFamily, QString()).toString();
}

void setUiFontFamily(const QString& family)
{
    SettingsCommon::openSettings().setValue(SettingsKeys::kUiFontFamily, family);
}

QSize fontSettingsDialogSize()
{
    return SettingsCommon::openSettings().value(SettingsKeys::kFontSettingsDialogSize,
                                                QSize(520, 300)).toSize();
}

void setFontSettingsDialogSize(const QSize& size)
{
    SettingsCommon::openSettings().setValue(SettingsKeys::kFontSettingsDialogSize, size);
}

// --- UI状態 ---

bool legalMovesVisible()
{
    return SettingsCommon::openSettings().value(SettingsKeys::kLegalMovesVisible, true).toBool();
}

void setLegalMovesVisible(bool visible)
{
    SettingsCommon::openSettings().setValue(SettingsKeys::kLegalMovesVisible, visible);
}

int lastSelectedTabIndex()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kLastSelectedTabIndex, 0).toInt();
}

void setLastSelectedTabIndex(int index)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kLastSelectedTabIndex, index);
}

bool toolbarVisible()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kToolbarVisible, true).toBool();
}

void setToolbarVisible(bool visible)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kToolbarVisible, visible);
}

QStringList availablePieceStyles()
{
    return {QStringLiteral("standard"),
            QStringLiteral("torafu_light"), QStringLiteral("torafu_silk"), QStringLiteral("torafu_amber"),
            QStringLiteral("torafu_red"), QStringLiteral("torafu_gold"), QStringLiteral("wood_pale"),
            QStringLiteral("wood_straight"), QStringLiteral("wood_amber"), QStringLiteral("wood_bamboo"),
            QStringLiteral("wood_walnut"), QStringLiteral("tint_linen"), QStringLiteral("tint_sakura"),
            QStringLiteral("tint_celadon"), QStringLiteral("tint_moon"), QStringLiteral("tint_wisteria"),
            QStringLiteral("deep_ebony"), QStringLiteral("deep_navy"), QStringLiteral("deep_green"),
            QStringLiteral("deep_grape"), QStringLiteral("deep_gold"), QStringLiteral("sengoku"),
            QStringLiteral("chess_facet_wood"),
            QStringLiteral("chess_facet_paper"),
            QStringLiteral("chess_facet_slate"),
            QStringLiteral("chess_atelier_wood"),
            QStringLiteral("chess_atelier_paper"),
            QStringLiteral("chess_atelier_slate"),
            QStringLiteral("chess_ribbon_wood"),
            QStringLiteral("chess_ribbon_paper"),
            QStringLiteral("chess_ribbon_slate")};
}

QString pieceStyle()
{
    auto& settings = SettingsCommon::openSettings();
    const QString style = settings.value(SettingsKeys::kPieceStyle, QStringLiteral("standard")).toString();
    if (availablePieceStyles().contains(style)) return style;
    // 旧セットの選択も、現在の木目の駒を採用した標準セットへ移行する。
    settings.setValue(SettingsKeys::kPieceStyle, QStringLiteral("standard"));
    return QStringLiteral("standard");
}

void setPieceStyle(const QString& style)
{
    SettingsCommon::openSettings().setValue(SettingsKeys::kPieceStyle,
        availablePieceStyles().contains(style) ? style : QStringLiteral("standard"));
}

// --- 駒音 ---

bool pieceSoundEnabled()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kPieceSoundEnabled, true).toBool();
}

void setPieceSoundEnabled(bool enabled)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kPieceSoundEnabled, enabled);
}

int pieceSoundVolume()
{
    QSettings& s = SettingsCommon::openSettings();
    return qBound(0, s.value(SettingsKeys::kPieceSoundVolume, 30).toInt(), 100);
}

void setPieceSoundVolume(int percent)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kPieceSoundVolume, qBound(0, percent, 100));
}

PieceSoundTone pieceSoundTone()
{
    QSettings& s = SettingsCommon::openSettings();
    PieceSoundTone tone;
    tone.pitchSemitones = s.value(SettingsKeys::kPieceSoundPitch, 0).toInt();
    tone.lowDb = s.value(SettingsKeys::kPieceSoundEqLow, 0).toInt();
    tone.midDb = s.value(SettingsKeys::kPieceSoundEqMid, 0).toInt();
    tone.highDb = s.value(SettingsKeys::kPieceSoundEqHigh, 0).toInt();
    return tone.clamped();
}

void setPieceSoundTone(const PieceSoundTone& tone)
{
    const PieceSoundTone t = tone.clamped();
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kPieceSoundPitch, t.pitchSemitones);
    s.setValue(SettingsKeys::kPieceSoundEqLow, t.lowDb);
    s.setValue(SettingsKeys::kPieceSoundEqMid, t.midDb);
    s.setValue(SettingsKeys::kPieceSoundEqHigh, t.highDb);
}

// --- 盤面の配色 ---

namespace {
struct ColorSetting {
    const char* key;
    BoardColors::Member member;
};
constexpr ColorSetting kColorSettings[] = {
    {SettingsKeys::kBoardBackgroundColor, &BoardColors::background},
    {SettingsKeys::kBoardSurfaceColor, &BoardColors::board},
    {SettingsKeys::kBoardStandColor, &BoardColors::stand},
    {SettingsKeys::kBoardGridColor, &BoardColors::grid},
    {SettingsKeys::kBoardCardBackgroundColor, &BoardColors::cardBackground},
    {SettingsKeys::kBoardCardBorderColor, &BoardColors::cardBorder},
    {SettingsKeys::kBoardActiveCardBorderColor, &BoardColors::activeCardBorder},
    {SettingsKeys::kBoardTurnBackgroundColor, &BoardColors::turnBackground},
    {SettingsKeys::kBoardTurnBorderColor, &BoardColors::turnBorder},
    {SettingsKeys::kBoardTurnTextColor, &BoardColors::turnText},
    {SettingsKeys::kBoardNameBackgroundColor, &BoardColors::nameBackground},
    {SettingsKeys::kBoardNameBorderColor, &BoardColors::nameBorder},
    {SettingsKeys::kBoardNameTextColor, &BoardColors::nameText},
    {SettingsKeys::kBoardClockBackgroundColor, &BoardColors::clockBackground},
    {SettingsKeys::kBoardClockBorderColor, &BoardColors::clockBorder},
    {SettingsKeys::kBoardClockTextColor, &BoardColors::clockText},
    {SettingsKeys::kBoardClockWarningTextColor, &BoardColors::clockWarningText},
    {SettingsKeys::kBoardClockCriticalTextColor, &BoardColors::clockCriticalText},
};
}

BoardColors boardColors()
{
    QSettings& s = SettingsCommon::openSettings();
    BoardColors colors;
    for (const auto& entry : kColorSettings) {
        QColor& color = colors.*entry.member;
        color = QColor(s.value(entry.key, color.name(QColor::HexArgb)).toString());
    }
    return colors.normalized();
}

void setBoardColors(const BoardColors& colors)
{
    QSettings& s = SettingsCommon::openSettings();
    const BoardColors normalized = colors.normalized();
    for (const auto& entry : kColorSettings) {
        const QColor color = normalized.*entry.member;
        s.setValue(entry.key, color.name(color.alpha() == 255 ? QColor::HexRgb : QColor::HexArgb));
    }
}

BoardVisuals boardVisuals()
{
    auto& s = SettingsCommon::openSettings();
    const BoardVisuals defaults;
    return BoardVisuals{s.value(SettingsKeys::kBoardWoodGrain, defaults.woodGrain).toBool(),
                        s.value(SettingsKeys::kBoardPieceShadow, defaults.pieceShadow).toBool(),
                        s.value(SettingsKeys::kBoardPieceScale, defaults.pieceScale).toInt(),
                        s.value(SettingsKeys::kStandWoodGrain,
                                s.value(SettingsKeys::kBoardWoodGrain, defaults.woodGrain)).toBool()}.normalized();
}

void setBoardVisuals(const BoardVisuals& visuals)
{
    auto& s = SettingsCommon::openSettings();
    const auto normalized = visuals.normalized();
    s.setValue(SettingsKeys::kBoardWoodGrain, normalized.woodGrain);
    s.setValue(SettingsKeys::kStandWoodGrain, normalized.standWoodGrain);
    s.setValue(SettingsKeys::kBoardPieceShadow, normalized.pieceShadow);
    s.setValue(SettingsKeys::kBoardPieceScale, normalized.pieceScale);
}

int boardColorDialogTab()
{
    return SettingsCommon::openSettings().value(SettingsKeys::kBoardColorDialogTab, 5).toInt();
}

namespace {
const QStringList kAppearanceSections{
    QStringLiteral("pieces"), QStringLiteral("board"), QStringLiteral("background"),
    QStringLiteral("stand"), QStringLiteral("information"), QStringLiteral("details")};
}

int appearanceSection()
{
    const auto saved = SettingsCommon::openSettings().value(SettingsKeys::kAppearanceSection, "pieces").toString();
    const int index = static_cast<int>(kAppearanceSections.indexOf(saved));
    if (index >= 0) return index;
    // 背景タブの追加前に保存した番号も、同じ項目を開くよう引き継ぐ。
    bool ok = false;
    const int legacy = saved.toInt(&ok);
    if (!ok || legacy < 0 || legacy > 4) return 0;
    return legacy < 2 ? legacy : legacy + 1;
}

void setAppearanceSection(int index)
{
    SettingsCommon::openSettings().setValue(SettingsKeys::kAppearanceSection,
                                            kAppearanceSections.at(qBound(0, index, 5)));
}

int appearancePieceFilter()
{
    return qBound(0, SettingsCommon::openSettings().value(SettingsKeys::kAppearancePieceFilter, 0).toInt(), 4);
}

void setAppearancePieceFilter(int index)
{
    SettingsCommon::openSettings().setValue(SettingsKeys::kAppearancePieceFilter, qBound(0, index, 4));
}

void setBoardColorDialogTab(int index)
{
    SettingsCommon::openSettings().setValue(SettingsKeys::kBoardColorDialogTab, index);
}

QSize boardColorDialogSize()
{
    return SettingsCommon::openSettings().value(SettingsKeys::kBoardColorDialogSize, QSize(1180, 800)).toSize();
}

void setBoardColorDialogSize(const QSize& size)
{
    SettingsCommon::openSettings().setValue(SettingsKeys::kBoardColorDialogSize, size);
}

QSize boardColorPickerSize()
{
    return SettingsCommon::openSettings().value(SettingsKeys::kBoardColorPickerSize, QSize()).toSize();
}

void setBoardColorPickerSize(const QSize& size)
{
    SettingsCommon::openSettings().setValue(SettingsKeys::kBoardColorPickerSize, size);
}

// --- メニューウィンドウ ---

QStringList menuWindowFavorites()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kMenuWindowFavorites, QStringList()).toStringList();
}

void setMenuWindowFavorites(const QStringList& favorites)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kMenuWindowFavorites, favorites);
}

QSize menuWindowSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kMenuWindowSize, QSize(500, 400)).toSize();
}

void setMenuWindowSize(const QSize& size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kMenuWindowSize, size);
}

int menuWindowButtonSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kMenuWindowButtonSize, 104).toInt();
}

void setMenuWindowButtonSize(int size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kMenuWindowButtonSize, size);
}

int menuWindowFontSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kMenuWindowFontSize, 12).toInt();
}

void setMenuWindowFontSize(int size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kMenuWindowFontSize, size);
}

int menuWindowCurrentTab()
{
    const auto& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kMenuWindowCurrentTab, 0).toInt();
}

void setMenuWindowCurrentTab(int index)
{
    auto& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kMenuWindowCurrentTab, index);
}

// --- メインウィンドウ ---

void loadWindowSize(QWidget* mainWindow)
{
    if (!mainWindow) return;
    QSettings& s = SettingsCommon::openSettings();
    const QSize sz = s.value(SettingsKeys::kMainWindowSize, QSize(1100, 720)).toSize();
    if (sz.isValid() && sz.width() > 100 && sz.height() > 100)
        mainWindow->resize(sz);
}

void saveWindowAndBoard(QWidget* mainWindow, int squareSize)
{
    if (!mainWindow) return;
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kMainWindowSize, mainWindow->size());
    s.setValue(SettingsKeys::kSquareSize,     squareSize);
    if (s.status() != QSettings::NoError) {
        qWarning("AppSettings: Failed to save settings (status=%d)", static_cast<int>(s.status()));
    }
}

// --- マイグレーション ---

void migrateSettingsIfNeeded()
{
    QSettings& s = SettingsCommon::openSettings();
    const int version = s.value(SettingsKeys::kSettingsVersion, 0).toInt();
    if (version >= SettingsKeys::kCurrentSettingsVersion) {
        return;
    }

    // 将来のマイグレーション処理をここに追加:
    // if (version < 2) { ... }
    // if (version < 3) { ... }

    s.setValue(SettingsKeys::kSettingsVersion, SettingsKeys::kCurrentSettingsVersion);
}

} // namespace AppSettings
