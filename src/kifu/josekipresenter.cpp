/// @file josekipresenter.cpp
/// @brief 定跡ウィンドウのプレゼンタークラスの実装

#include "josekipresenter.h"
#include "kifmovetext.h"
#include "usimove.h"
#include "sfenutils.h"
#include "kifupresentation.h"
#include "josekirepository.h"
#include "josekitypes.h"
#include "sfenpositiontracer.h"
#include "kiftosfenconverter.h"
#include "kifdisplayitem.h"
#include "logcategories.h"
#include "notationutils.h"

#include <QStringView>
#include <QFileInfo>

JosekiPresenter::JosekiPresenter(JosekiRepository *repository, QObject *parent)
    : QObject(parent)
    , m_repository(repository)
{
}

QString JosekiPresenter::normalizeSfen(const QString &sfen)
{
    return SfenUtils::normalizeSfenKey(sfen);
}

JosekiPresenter::PositionInfo JosekiPresenter::describePosition(const QString &sfen)
{
    PositionInfo info;
    const QStringList parts = sfen.split(QLatin1Char(' '));
    if (parts.size() >= 2) info.blackToMove = parts.at(1) == QStringLiteral("b");
    if (parts.size() >= 4) {
        bool ok = false;
        info.ply = parts.at(3).toInt(&ok);
        if (!ok) info.ply = 1;
    }
    if (info.ply == 1) info.handicap = NotationUtils::handicapLabelForSfen(sfen);
    return info;
}

QString JosekiPresenter::usiMoveToJapanese(const QString &usiMove,
                                            const SfenPositionTracer &tracer)
{
    if (usiMove.isEmpty()) return QString();
    if (usiMove == QStringLiteral("none")) return tr("なし");

    if (KifuPresentation::options().notation == KifuPresentation::Notation::Western)
        return KifuPresentation::move(tracer.toSfenString(), usiMove, KifuPresentation::options());

    const Turn mover = tracer.blackToMove() ? Turn::Black : Turn::White;
    const auto move = UsiMove::parse(usiMove);
    if (!move) return (mover == Turn::Black ? QStringLiteral("▲") : QStringLiteral("△")) + usiMove;
    const Piece moving = move->isDrop() ? move->dropPieceFor(mover)
                                        : tracer.pieceAt(move->from.file, move->from.rank);
    // 定跡の表は手ごとに独立しているので「同」は使わず、棋譜欄と同じ書式で書く
    return KifMoveText::format(mover, moving, *move);
}

void JosekiPresenter::addMove(const QString &normalizedSfen, const QString &currentSfen,
                               const JosekiMove &newMove)
{
    // 手数付きSFENを登録
    const QStringList parts = currentSfen.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() >= 4) {
        m_repository->ensureSfenWithPly(normalizedSfen, currentSfen);
    } else if (parts.size() == 3) {
        m_repository->ensureSfenWithPly(normalizedSfen, currentSfen + QStringLiteral(" 1"));
    } else {
        m_repository->ensureSfenWithPly(normalizedSfen, currentSfen);
    }

    // 定跡データに追加
    m_repository->addMove(normalizedSfen, newMove);

    qCDebug(lcUi) << "Added joseki move:" << newMove.move
                   << "to position:" << normalizedSfen;

    emit dataChanged();
    emit modifiedChanged(true);
}

void JosekiPresenter::editMove(const QString &normalizedSfen, const QString &usiMove,
                                int value, int depth, int frequency, const QString &comment,
                                const QString &nextMove)
{
    m_repository->updateMove(normalizedSfen, usiMove, value, depth, frequency, comment, nextMove);

    qCDebug(lcUi) << "Updated joseki move:" << usiMove;

    emit dataChanged();
    emit modifiedChanged(true);
}

void JosekiPresenter::deleteMove(const QString &normalizedSfen, int index)
{
    m_repository->deleteMove(normalizedSfen, index);

    emit dataChanged();
    emit modifiedChanged(true);
}

bool JosekiPresenter::registerMergeMove(const QString &normalizedSfen, const QString &sfenWithPly,
                                         const QString &usiMove, const QString &currentFilePath,
                                         QString *errorMessage)
{
    return registerMergeMoves({normalizedSfen}, {sfenWithPly}, {usiMove}, currentFilePath, errorMessage);
}

bool JosekiPresenter::registerMergeMoves(const QStringList &normalizedSfens, const QStringList &sfensWithPly,
                                          const QStringList &usiMoves, const QString &currentFilePath,
                                          QString *errorMessage)
{
    // 保存が成功するまで編集中のデータと登録済み状態を変更しない。
    JosekiRepository updated = *m_repository;
    for (qsizetype i = 0; i < usiMoves.size(); ++i) {
        const QString key = normalizedSfens.value(i) + QLatin1Char(':') + usiMoves.at(i);
        if (!updated.mergeRegisteredMoves().contains(key)) {
            updated.registerMergeMove(normalizedSfens.value(i), sfensWithPly.value(i), usiMoves.at(i));
        }
    }
    if (!updated.saveToFile(currentFilePath, errorMessage)) return false;

    *m_repository = std::move(updated);
    emit dataChanged();
    emit modifiedChanged(false);
    return true;
}

bool JosekiPresenter::hasDuplicateMove(const QString &normalizedSfen, const QString &usiMove) const
{
    if (!m_repository->containsPosition(normalizedSfen)) {
        return false;
    }

    const QList<JosekiMove> &moves = m_repository->movesForPosition(normalizedSfen);
    for (const JosekiMove &move : std::as_const(moves)) {
        if (move.move == usiMove) {
            return true;
        }
    }
    return false;
}

QList<KifuMergeEntry> JosekiPresenter::buildMergeEntries(const QStringList &sfenList,
                                                            const QStringList &moveList,
                                                            const QStringList &japaneseMoveList,
                                                            int currentPly) const
{
    QList<KifuMergeEntry> entries;

    for (int i = 0; i < moveList.size(); ++i) {
        if (i >= sfenList.size()) break;

        if (moveList[i].isEmpty()) continue;

        KifuMergeEntry entry;
        entry.ply = i + 1;
        entry.sfen = sfenList[i];
        entry.usiMove = moveList[i];

        // 日本語表記（japaneseMoveList[i] が i+1 手目）
        if (i < japaneseMoveList.size()) {
            entry.japaneseMove = KifuPresentation::label(japaneseMoveList[i], sfenList.value(i), moveList.at(i));
        } else {
            entry.japaneseMove = moveList[i];
        }

        entry.isCurrentMove = (i + 1 == currentPly);

        entries.append(entry);
    }

    return entries;
}

bool JosekiPresenter::buildMergeEntriesFromKifFile(const QString &kifFilePath,
                                                    QList<KifuMergeEntry> &entries,
                                                    QString *errorMessage) const
{
    // KIFファイルを解析
    KifParseResult parseResult;
    QString parseError;

    if (!KifToSfenConverter::parseWithVariations(kifFilePath, parseResult, &parseError)) {
        qCWarning(lcUi) << "buildMergeEntriesFromKifFile: parse failed:" << parseError;
        if (errorMessage) {
            *errorMessage = parseError;
        }
        return false;
    }

    const KifLine &mainline = parseResult.mainline;

    qCDebug(lcUi) << "buildMergeEntriesFromKifFile: parse succeeded";
    qCDebug(lcUi) << "  mainline.baseSfen:" << mainline.baseSfen;
    qCDebug(lcUi) << "  mainline.usiMoves().size():" << mainline.usiMoves().size();
    qCDebug(lcUi) << "  mainline.sfenList().size():" << mainline.sfenList().size();

    if (mainline.usiMoves().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QString();  // 空 = 指し手がない
        }
        return false;
    }

    QString before = mainline.baseSfen;
    for (const auto& imported : mainline.entries) {
        const auto& record = imported.record;
        if (record.isTerminal()) break;
        KifuMergeEntry entry;
        entry.ply = static_cast<int>(entries.size()) + 1;
        entry.sfen = before;
        entry.usiMove = record.move.toUsiMove().toString();
        entry.japaneseMove = KifuPresentation::label(record.displayText, before, entry.usiMove);
        entry.isCurrentMove = false;
        entries.append(entry);
        before = record.sfen;
    }

    return !entries.isEmpty();
}
