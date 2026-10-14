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

// --- UI状態 ---

QStringList availablePieceStyles()
{
    return {QStringLiteral("standard"),
            QStringLiteral("torafu_light"), QStringLiteral("torafu_silk"), QStringLiteral("torafu_amber"),
            QStringLiteral("torafu_red"), QStringLiteral("torafu_gold"), QStringLiteral("wood_pale"),
            QStringLiteral("wood_classic"), QStringLiteral("wood_amber"), QStringLiteral("wood_bamboo"),
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
            QStringLiteral("chess_ribbon_slate"),
            QStringLiteral("alphabet_sei_wood"),
            QStringLiteral("alphabet_sei_paper"),
            QStringLiteral("alphabet_sei_slate"),
            QStringLiteral("alphabet_rin_wood"),
            QStringLiteral("alphabet_rin_paper"),
            QStringLiteral("alphabet_rin_slate"),
            QStringLiteral("alphabet_sumi_wood"),
            QStringLiteral("alphabet_sumi_paper"),
            QStringLiteral("alphabet_sumi_slate")};
}

QString pieceStyle()
{
    auto& settings = SettingsCommon::openSettings();
    const QString style = settings.value(SettingsKeys::kPieceStyle, QStringLiteral("standard")).toString();
    if (availablePieceStyles().contains(style)) return style;
    // 糸柾（wood_straight）と廃止済みの旧セットは、糸柾を採用した標準セットへ移行する。
    settings.setValue(SettingsKeys::kPieceStyle, QStringLiteral("standard"));
    return QStringLiteral("standard");
}

void setPieceStyle(const QString& style)
{
    SettingsCommon::openSettings().setValue(SettingsKeys::kPieceStyle,
        availablePieceStyles().contains(style) ? style : QStringLiteral("standard"));
}

// --- 駒音 ---

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

namespace {
/// 外観ダイアログのタブの保存名（タブの並び順）
const QStringList& appearanceSectionNames()
{
    static const QStringList names{
        QStringLiteral("pieces"), QStringLiteral("board"), QStringLiteral("background"),
        QStringLiteral("stand"), QStringLiteral("information"), QStringLiteral("details")};
    return names;
}
}

int appearanceSection()
{
    const auto saved = SettingsCommon::openSettings().value(SettingsKeys::kAppearanceSection, "pieces").toString();
    const int index = static_cast<int>(appearanceSectionNames().indexOf(saved));
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
                                            appearanceSectionNames().at(qBound(0, index, 5)));
}

int appearancePieceFilter()
{
    return qBound(0, SettingsCommon::openSettings().value(SettingsKeys::kAppearancePieceFilter, 0).toInt(), 7);
}

void setAppearancePieceFilter(int index)
{
    SettingsCommon::openSettings().setValue(SettingsKeys::kAppearancePieceFilter, qBound(0, index, 7));
}

// --- メインウィンドウ ---

void loadWindowSize(QWidget* mainWindow)
{
    if (!mainWindow) return;
    const QSize sz = mainWindowSize();
    if (sz.isValid() && sz.width() > 100 && sz.height() > 100)
        mainWindow->resize(sz);
}

void saveWindowAndBoard(QWidget* mainWindow, int boardSquareSize)
{
    if (!mainWindow) return;
    mainWindowSize.set(mainWindow->size());
    squareSize.set(boardSquareSize);
    const QSettings& s = SettingsCommon::openSettings();
    if (s.status() != QSettings::NoError) {
        qWarning("AppSettings: Failed to save settings (status=%d)", static_cast<int>(s.status()));
    }
}

} // namespace AppSettings
