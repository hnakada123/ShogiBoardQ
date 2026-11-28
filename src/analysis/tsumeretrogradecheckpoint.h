#ifndef TSUMERETROGRADECHECKPOINT_H
#define TSUMERETROGRADECHECKPOINT_H

/// @file tsumeretrogradecheckpoint.h
/// @brief 逆算生成の途中保存（チェックポイント）ファイルの形式と読み書き

#include "tsumeretrogradegenerator.h"

#include <QCoreApplication>
#include <QJsonObject>
#include <QString>

#include <optional>

/**
 * @brief 逆算生成の途中保存ファイル
 *
 * ファイルの版・生成の条件の記録と、再開のときの照合（条件の違うチェックポイントからは再開しない）、
 * 原子的な保存を受け持つ。探索の状態そのものは TsumeRetrogradeBeam、進行中の厳密証明は
 * TsumeStrictCertifier が自分の分を書く。
 */
class TsumeRetrogradeCheckpoint
{
    Q_DECLARE_TR_FUNCTIONS(TsumeRetrogradeCheckpoint)

public:
    /// 版と生成の条件を書いた、チェックポイントの根（ここに探索の状態を足していく）
    static QJsonObject begin(const TsumeRetrogradeGenerator::Settings& settings);
    /// path へ原子的に保存する（フォルダが無ければ作る）
    static bool save(const QString& path, const QJsonObject& root, QString* errorMessage = nullptr);
    /// path を読み、版と条件が settings に合えば根を返す。合わなければ nullopt（理由は errorMessage）
    static std::optional<QJsonObject> load(const QString& path, const TsumeRetrogradeGenerator::Settings& settings,
                                           QString* errorMessage = nullptr);
};

#endif // TSUMERETROGRADECHECKPOINT_H
