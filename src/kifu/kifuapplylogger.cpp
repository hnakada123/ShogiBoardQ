/// @file kifuapplylogger.cpp
/// @brief 棋譜インポートのログ出力ヘルパの実装

#include "kifuapplylogger.h"
#include "kifdisplayitem.h"
#include "logcategories.h"

#include <QFileInfo>

namespace KifuApplyLogger {

void logImportSummary(const QString& filePath,
                      const QStringList& usiMoves,
                      const QList<KifDisplayItem>& disp,
                      const QString& teaiLabel,
                      const QString& warnParse,
                      const QString& warnConvert,
                      const QStringList* sfenHistory,
                      const QVector<ShogiMove>* gameMoves)
{
    if (!warnParse.isEmpty())
        qCWarning(lcKifu).noquote() << "parse warnings:\n" << warnParse.trimmed();
    if (!warnConvert.isEmpty())
        qCWarning(lcKifu).noquote() << "convert warnings:\n" << warnConvert.trimmed();

    qCDebug(lcKifu).noquote() << QStringLiteral("KIF読込: %1手（%2）")
                                     .arg(usiMoves.size())
                                     .arg(QFileInfo(filePath).fileName());
    for (qsizetype i = 0; i < qMin(qsizetype(5), usiMoves.size()); ++i) {
        qCDebug(lcKifu).noquote() << QStringLiteral("USI[%1]: %2")
        .arg(i + 1)
            .arg(usiMoves.at(i));
    }

    qCDebug(lcKifu).noquote() << QStringLiteral("手合割: %1")
                                     .arg(teaiLabel.isEmpty()
                                              ? QStringLiteral("平手(既定)")
                                              : teaiLabel);

    for (const auto& it : disp) {
        const QString time = it.timeText.isEmpty()
        ? QStringLiteral("00:00/00:00:00")
        : it.timeText;
        qCDebug(lcKifu).noquote() << QStringLiteral("「%1」「%2」").arg(it.prettyMove, time);
        if (!it.comment.trimmed().isEmpty()) {
            qCDebug(lcKifu).noquote() << QStringLiteral("  └ コメント: %1")
                                             .arg(it.comment.trimmed());
        }
    }

    if (sfenHistory) {
        for (int i = 0; i < qMin(12, sfenHistory->size()); ++i) {
            qCDebug(lcKifu).noquote() << QStringLiteral("%1) %2")
            .arg(i)
                .arg(sfenHistory->at(i));
        }
    }

    if (gameMoves) {
        qCDebug(lcKifu) << "gameMoves size:" << gameMoves->size();
        for (qsizetype i = 0; i < gameMoves->size(); ++i) {
            qCDebug(lcKifu).noquote() << QString("%1) ").arg(i + 1) << (*gameMoves)[i];
        }
    }
}

} // namespace KifuApplyLogger
