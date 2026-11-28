/// @file tsumeretrogradecheckpoint.cpp
/// @brief 逆算生成の途中保存（チェックポイント）ファイルの形式と読み書きの実装

#include "tsumeretrogradecheckpoint.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

namespace {
constexpr int kCheckpointVersion = 1;
} // namespace

QJsonObject TsumeRetrogradeCheckpoint::begin(const TsumeRetrogradeGenerator::Settings& settings)
{
    QJsonObject root;
    root[QStringLiteral("checkpoint_version")] = kCheckpointVersion;

    QJsonObject cfg;
    cfg[QStringLiteral("target_moves")] = settings.targetMoves;
    cfg[QStringLiteral("max_positions")] = settings.maxPositionsToFind;
    cfg[QStringLiteral("beam_width")] = settings.beamWidth;
    cfg[QStringLiteral("branch_factor")] = settings.branchFactor;
    cfg[QStringLiteral("max_attack_pieces")] = settings.maxAttackPieces;
    cfg[QStringLiteral("max_defend_pieces")] = settings.maxDefendPieces;
    cfg[QStringLiteral("max_total_board_pieces")] = settings.maxTotalBoardPieces;
    cfg[QStringLiteral("add_remaining_to_defender_hand")] = settings.addRemainingToDefenderHand;
    cfg[QStringLiteral("allow_final_move_alternatives")] = settings.allowFinalMoveAlternatives;
    cfg[QStringLiteral("require_no_surplus")] = settings.requireNoSurplus;
    cfg[QStringLiteral("strict_min_length")] = settings.strictMinLength;
    root[QStringLiteral("settings")] = cfg;
    return root;
}

bool TsumeRetrogradeCheckpoint::save(const QString& path, const QJsonObject& root, QString* errorMessage)
{
    const QFileInfo info(path);
    if (!info.dir().exists() && !QDir().mkpath(info.dir().absolutePath())) {
        if (errorMessage) *errorMessage = tr("チェックポイント保存先ディレクトリを作成できませんでした。");
        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage) *errorMessage = file.errorString();
        return false;
    }
    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Compact);
    if (file.write(payload) != payload.size() || !file.commit()) {
        if (errorMessage) *errorMessage = file.errorString();
        return false;
    }
    return true;
}

std::optional<QJsonObject> TsumeRetrogradeCheckpoint::load(const QString& path,
                                                           const TsumeRetrogradeGenerator::Settings& settings,
                                                           QString* errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) *errorMessage = tr("チェックポイントファイルを開けません: %1").arg(file.errorString());
        return std::nullopt;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorMessage) *errorMessage = tr("チェックポイントファイルの JSON が不正です: %1").arg(parseError.errorString());
        return std::nullopt;
    }

    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("checkpoint_version")).toInt() != kCheckpointVersion) {
        if (errorMessage) *errorMessage = tr("未対応のチェックポイントバージョンです。");
        return std::nullopt;
    }

    const QJsonObject cfg = root.value(QStringLiteral("settings")).toObject();
    if (cfg.value(QStringLiteral("target_moves")).toInt() != settings.targetMoves) {
        if (errorMessage) {
            *errorMessage = tr("チェックポイントの目標手数 (%1) が指定 (%2) と一致しません。")
                                .arg(cfg.value(QStringLiteral("target_moves")).toInt())
                                .arg(settings.targetMoves);
        }
        return std::nullopt;
    }
    if (cfg.value(QStringLiteral("allow_final_move_alternatives")).toBool() != settings.allowFinalMoveAlternatives
        || cfg.value(QStringLiteral("require_no_surplus")).toBool() != settings.requireNoSurplus) {
        if (errorMessage) {
            *errorMessage = tr("チェックポイントの厳密検証条件（最終手許容・駒余り禁止）が指定と一致しません。");
        }
        return std::nullopt;
    }
    return root;
}
