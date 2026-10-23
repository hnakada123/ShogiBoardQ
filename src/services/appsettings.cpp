/// @file appsettings.cpp
/// @brief アプリケーション全般設定の永続化実装

#include "appsettings.h"
#include "settingscommon.h"
#include "settingskeys.h"
#include "piecestylecatalog.h"
#include <QSettings>

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

// --- 盤面の配色 ---

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

namespace {
const StructSetting<BoardVisuals>& visualsSetting()
{
    static const StructSetting<BoardVisuals> setting{
        {SettingField<BoardVisuals>::of(SettingsKeys::kBoardWoodGrain, &BoardVisuals::woodGrain),
         SettingField<BoardVisuals>::of(SettingsKeys::kStandWoodGrain, &BoardVisuals::standWoodGrain),
         SettingField<BoardVisuals>::of(SettingsKeys::kBoardPieceShadow, &BoardVisuals::pieceShadow),
         SettingField<BoardVisuals>::of(SettingsKeys::kBoardPieceScale, &BoardVisuals::pieceScale)},
        &BoardVisuals::normalized};
    return setting;
}
} // namespace

BoardVisuals boardVisuals()
{
    BoardVisuals visuals = visualsSetting()();
    // 駒台の木目を分ける前の版は、将棋盤の木目の設定だけを保存していた
    if (!StructSetting<BoardVisuals>::isSaved(SettingsKeys::kStandWoodGrain)) visuals.standWoodGrain = visuals.woodGrain;
    return visuals;
}

void setBoardVisuals(const BoardVisuals& visuals)
{
    visualsSetting().set(visuals);
}

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

} // namespace AppSettings
