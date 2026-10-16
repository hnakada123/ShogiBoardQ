/// @file appsettings.cpp
/// @brief アプリケーション全般設定の永続化実装

#include "appsettings.h"
#include "settingscommon.h"
#include "settingskeys.h"
#include "piecestylecatalog.h"
#include <QSettings>
#include <QWidget>

namespace AppSettings {

// --- UI状態 ---

QString pieceStyle()
{
    auto& settings = SettingsCommon::openSettings();
    const QString style = settings.value(SettingsKeys::kPieceStyle, PieceStyleCatalog::standardId()).toString();
    if (PieceStyleCatalog::contains(style)) return style;
    // 糸柾（wood_straight）と廃止済みの旧セットは、糸柾を採用した標準セットへ移行する。
    settings.setValue(SettingsKeys::kPieceStyle, PieceStyleCatalog::standardId());
    return PieceStyleCatalog::standardId();
}

void setPieceStyle(const QString& style)
{
    SettingsCommon::openSettings().setValue(SettingsKeys::kPieceStyle,
        PieceStyleCatalog::contains(style) ? style : PieceStyleCatalog::standardId());
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
/// 駒の絞り込みの最後の項目（項目の位置は PieceStyleCatalog::Family の値。0 は「すべて」）
constexpr int kLastPieceFilter = static_cast<int>(PieceStyleCatalog::Family::Count) - 1;
} // namespace

QString appearanceSection()
{
    const auto saved = SettingsCommon::openSettings().value(SettingsKeys::kAppearanceSection, "pieces").toString();
    // 背景タブの追加前は、タブの番号（駒・将棋盤・駒台・対局者情報・詳細）で保存していた
    bool ok = false;
    const int legacy = saved.toInt(&ok);
    if (!ok) return saved;
    static const char* const kLegacySections[] = {"pieces", "board", "stand", "information", "details"};
    return legacy >= 0 && legacy < 5 ? QString::fromLatin1(kLegacySections[legacy]) : QStringLiteral("pieces");
}

void setAppearanceSection(const QString& id)
{
    SettingsCommon::openSettings().setValue(SettingsKeys::kAppearanceSection, id);
}

int appearancePieceFilter()
{
    return qBound(0, SettingsCommon::openSettings().value(SettingsKeys::kAppearancePieceFilter, 0).toInt(),
                  kLastPieceFilter);
}

void setAppearancePieceFilter(int index)
{
    SettingsCommon::openSettings().setValue(SettingsKeys::kAppearancePieceFilter, qBound(0, index, kLastPieceFilter));
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
