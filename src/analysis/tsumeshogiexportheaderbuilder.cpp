/// @file tsumeshogiexportheaderbuilder.cpp
/// @brief 詰将棋局面生成のファイル保存に付けるコメントヘッダの生成

#include "tsumeshogiexportheaderbuilder.h"

QStringList TsumeshogiExportHeaderBuilder::build(const QString& appVersion,
                                                 const QDateTime& generatedAt,
                                                 const TsumeshogiGenerator::Settings& settings)
{
    // 表記はダイアログの生成設定欄に合わせる
    const QString maxPositions = settings.maxPositionsToFind > 0
                                     ? QString::number(settings.maxPositionsToFind)
                                     : tr("無制限");
    const QString finalMoveCheck = settings.allowFinalMoveAlternatives
                                       ? tr("最終手の複数解を許容する")
                                       : tr("最終手の複数解を許容しない");

    const auto comment = [](const QString& text) { return QStringLiteral("# ") + text; };

    QStringList lines;
    lines << comment(tr("ShogiBoardQ %1 詰将棋局面生成").arg(appVersion))
          << comment(tr("生成日時: %1").arg(generatedAt.toString(QStringLiteral("yyyy/MM/dd HH:mm:ss"))))
          << comment(tr("エンジン: %1").arg(settings.engineName))
          << comment(tr("目標手数: %1 手詰").arg(settings.targetMoves))
          << comment(tr("攻め駒上限: %1 枚").arg(settings.posGenSettings.maxAttackPieces))
          << comment(tr("守り駒上限: %1 枚").arg(settings.posGenSettings.maxDefendPieces))
          << comment(tr("配置範囲: %1 マス（玉中心）").arg(settings.posGenSettings.attackRange))
          << comment(tr("探索時間/局面: %1 秒").arg(QString::number(settings.timeoutMs / 1000.0)))
          << comment(tr("生成上限: %1").arg(maxPositions))
          << comment(tr("余詰検査: %1").arg(finalMoveCheck));
    return lines;
}
