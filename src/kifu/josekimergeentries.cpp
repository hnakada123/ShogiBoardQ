/// @file josekimergeentries.cpp
/// @brief 棋譜から定跡にマージする指し手の一覧の実装

#include "josekimergeentries.h"

#include "kifparsetypes.h"
#include "kifuloadparser.h"
#include "kifupresentation.h"
#include "logcategories.h"

namespace JosekiMergeEntries {

QList<KifuMergeEntry> fromLine(const QStringList &sfens, const QStringList &usiMoves,
                               const QStringList &moveTexts, int currentPly)
{
    QList<KifuMergeEntry> entries;
    for (qsizetype i = 0; i < usiMoves.size() && i < sfens.size(); ++i) {
        if (usiMoves.at(i).isEmpty()) continue;
        KifuMergeEntry entry;
        entry.ply = static_cast<int>(i) + 1;
        entry.sfen = sfens.at(i);
        entry.usiMove = usiMoves.at(i);
        entry.japaneseMove = i < moveTexts.size()
            ? KifuPresentation::label(moveTexts.at(i), sfens.at(i), usiMoves.at(i)) : usiMoves.at(i);
        entry.isCurrentMove = (entry.ply == currentPly);
        entries.append(entry);
    }
    return entries;
}

bool fromKifFile(const QString &kifFilePath, QList<KifuMergeEntry> &entries, QString *errorMessage)
{
    const KifuLoadResult loaded = KifuLoadParser::parseFile(kifFilePath, KifuFormat::Kif);
    if (!loaded.success) {
        qCWarning(lcKifu) << "JosekiMergeEntries::fromKifFile: parse failed:" << loaded.error;
        if (errorMessage) *errorMessage = loaded.error;
        return false;
    }
    if (errorMessage) errorMessage->clear();

    const RecordLine &mainline = loaded.record.mainline;
    QString before = mainline.startSfen;
    for (const RecordEntry &record : mainline.entries) {
        if (record.isTerminal()) break;
        KifuMergeEntry entry;
        entry.ply = static_cast<int>(entries.size()) + 1;
        entry.sfen = before;
        entry.usiMove = record.move.move.toString();
        entry.japaneseMove = KifuPresentation::label(record.displayText, before, entry.usiMove);
        entry.isCurrentMove = false;
        entries.append(entry);
        before = record.sfen;
    }
    return !entries.isEmpty();
}

} // namespace JosekiMergeEntries
