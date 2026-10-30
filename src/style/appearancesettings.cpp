/// @file appearancesettings.cpp
/// @brief 対局画面の外観と外観ダイアログの設定の永続化実装

#include "appearancesettings.h"
#include "settingscommon.h"

#include <QSettings>

namespace AppearanceSettings {

namespace {
constexpr char kPieceStyle[] = "UI/pieceStyle";
constexpr char kBoardWoodGrain[] = "BoardVisuals/woodGrain";
constexpr char kStandWoodGrain[] = "BoardVisuals/standWoodGrain";
constexpr char kBoardPieceShadow[] = "BoardVisuals/pieceShadow";
constexpr char kBoardPieceScale[] = "BoardVisuals/pieceScale";
constexpr char kAppearanceSection[] = "UI/appearanceSection";

const StructSetting<BoardVisuals>& visualsSetting()
{
    static const StructSetting<BoardVisuals> setting{
        {SettingField<BoardVisuals>::of(kBoardWoodGrain, &BoardVisuals::woodGrain),
         SettingField<BoardVisuals>::of(kStandWoodGrain, &BoardVisuals::standWoodGrain),
         SettingField<BoardVisuals>::of(kBoardPieceShadow, &BoardVisuals::pieceShadow),
         SettingField<BoardVisuals>::of(kBoardPieceScale, &BoardVisuals::pieceScale)},
        &BoardVisuals::normalized};
    return setting;
}
} // namespace

// --- 外観 ---

QString pieceStyle()
{
    auto& settings = SettingsCommon::openSettings();
    const QString style = settings.value(kPieceStyle, PieceStyleCatalog::standardId()).toString();
    if (PieceStyleCatalog::contains(style)) return style;
    // 糸柾（wood_straight）と廃止済みの旧セットは、糸柾を採用した標準セットへ移行する。
    settings.setValue(kPieceStyle, PieceStyleCatalog::standardId());
    return PieceStyleCatalog::standardId();
}

void setPieceStyle(const QString& style)
{
    SettingsCommon::openSettings().setValue(kPieceStyle,
        PieceStyleCatalog::contains(style) ? style : PieceStyleCatalog::standardId());
}

BoardColors boardColors()
{
    QSettings& s = SettingsCommon::openSettings();
    BoardColors colors;
    for (const BoardColors::Field& field : BoardColors::fields()) {
        QColor& color = colors.*field.member;
        color = QColor(s.value(field.key, color.name(QColor::HexArgb)).toString());
    }
    return colors.normalized();
}

void setBoardColors(const BoardColors& colors)
{
    QSettings& s = SettingsCommon::openSettings();
    const BoardColors normalized = colors.normalized();
    for (const BoardColors::Field& field : BoardColors::fields()) {
        const QColor color = normalized.*field.member;
        s.setValue(field.key, color.name(color.alpha() == 255 ? QColor::HexRgb : QColor::HexArgb));
    }
}

BoardVisuals boardVisuals()
{
    BoardVisuals visuals = visualsSetting()();
    // 駒台の木目を分ける前の版は、将棋盤の木目の設定だけを保存していた
    if (!StructSetting<BoardVisuals>::isSaved(kStandWoodGrain)) visuals.standWoodGrain = visuals.woodGrain;
    return visuals;
}

void setBoardVisuals(const BoardVisuals& visuals)
{
    visualsSetting().set(visuals);
}

// --- 外観ダイアログ ---

QString appearanceSection()
{
    const auto saved = SettingsCommon::openSettings().value(kAppearanceSection, "pieces").toString();
    // 背景タブの追加前は、タブの番号（駒・将棋盤・駒台・対局者情報・詳細）で保存していた
    bool ok = false;
    const int legacy = saved.toInt(&ok);
    if (!ok) return saved;
    static const char* const kLegacySections[] = {"pieces", "board", "stand", "information", "details"};
    return legacy >= 0 && legacy < 5 ? QString::fromLatin1(kLegacySections[legacy]) : QStringLiteral("pieces");
}

void setAppearanceSection(const QString& id)
{
    SettingsCommon::openSettings().setValue(kAppearanceSection, id);
}

} // namespace AppearanceSettings
