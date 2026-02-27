/// @file josekipresenter.cpp
/// @brief 定跡ウィンドウのプレゼンタークラスの実装

#include "josekipresenter.h"
#include "josekirepository.h"
#include "josekiwindow.h"  // JosekiMove 構造体
#include "josekimergedialog.h"  // KifuMergeEntry 構造体
#include "sfenpositiontracer.h"
#include "kiftosfenconverter.h"
#include "kifdisplayitem.h"
#include "logcategories.h"

#include <QStringView>
#include <QFileInfo>

JosekiPresenter::JosekiPresenter(JosekiRepository *repository, QObject *parent)
    : QObject(parent)
    , m_repository(repository)
{
}

// 全角数字と漢数字のテーブル
static constexpr QStringView kZenkakuDigits = u" １２３４５６７８９";  // インデックス1-9
static constexpr QStringView kKanjiRanks = u" 一二三四五六七八九";    // インデックス1-9

QString JosekiPresenter::normalizeSfen(const QString &sfen)
{
    const QStringList parts = sfen.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() >= 3) {
        return parts[0] + QLatin1Char(' ') + parts[1] + QLatin1Char(' ') + parts[2];
    }
    return sfen;
}

QString JosekiPresenter::pieceToKanji(QChar pieceChar, bool promoted)
{
    switch (pieceChar.toUpper().toLatin1()) {
    case 'P': return promoted ? QStringLiteral("と") : QStringLiteral("歩");
    case 'L': return promoted ? QStringLiteral("杏") : QStringLiteral("香");
    case 'N': return promoted ? QStringLiteral("圭") : QStringLiteral("桂");
    case 'S': return promoted ? QStringLiteral("全") : QStringLiteral("銀");
    case 'G': return QStringLiteral("金");
    case 'B': return promoted ? QStringLiteral("馬") : QStringLiteral("角");
    case 'R': return promoted ? QStringLiteral("龍") : QStringLiteral("飛");
    case 'K': return QStringLiteral("玉");
    default: return QStringLiteral("?");
    }
}

QString JosekiPresenter::usiMoveToJapanese(const QString &usiMove, int plyNumber,
                                            SfenPositionTracer &tracer)
{
    if (usiMove.isEmpty()) return QString();

    // 手番記号
    QString teban = (plyNumber % 2 != 0) ? QStringLiteral("▲") : QStringLiteral("△");

    // 駒打ちのパターン: P*7f
    if (usiMove.size() >= 4 && usiMove.at(1) == QChar('*')) {
        QChar pieceChar = usiMove.at(0);
        int toFile = usiMove.at(2).toLatin1() - '0';
        int toRank = usiMove.at(3).toLatin1() - 'a' + 1;

        QString kanji = pieceToKanji(pieceChar);

        QString result = teban;
        if (toFile >= 1 && toFile <= 9) result += kZenkakuDigits.at(toFile);
        if (toRank >= 1 && toRank <= 9) result += kKanjiRanks.at(toRank);
        result += kanji + QStringLiteral("打");

        return result;
    }

    // 通常移動のパターン: 7g7f, 7g7f+
    if (usiMove.size() >= 4) {
        int fromFile = usiMove.at(0).toLatin1() - '0';
        QChar fromRankLetter = usiMove.at(1);
        int fromRank = fromRankLetter.toLatin1() - 'a' + 1;
        int toFile = usiMove.at(2).toLatin1() - '0';
        int toRank = usiMove.at(3).toLatin1() - 'a' + 1;
        bool promotes = (usiMove.size() >= 5 && usiMove.at(4) == QChar('+'));

        // 範囲チェック
        if (fromFile < 1 || fromFile > 9 || fromRank < 1 || fromRank > 9 ||
            toFile < 1 || toFile > 9 || toRank < 1 || toRank > 9) {
            return teban + usiMove;
        }

        // 移動元の駒を取得
        QString pieceToken = tracer.tokenAtFileRank(fromFile, fromRankLetter);
        bool isPromoted = pieceToken.startsWith(QChar('+'));
        QChar pieceChar = isPromoted ? pieceToken.at(1) : (pieceToken.isEmpty() ? QChar('?') : pieceToken.at(0));

        QString result = teban;

        // 移動先
        if (toFile >= 1 && toFile <= 9) result += kZenkakuDigits.at(toFile);
        if (toRank >= 1 && toRank <= 9) result += kKanjiRanks.at(toRank);

        // 駒種
        QString kanji = pieceToKanji(pieceChar, isPromoted);
        result += kanji;

        // 成り
        if (promotes) {
            result += QStringLiteral("成");
        }

        // 移動元
        result += QStringLiteral("(") + QString::number(fromFile) + QString::number(fromRank) + QStringLiteral(")");

        return result;
    }

    return teban + usiMove;
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
                                int value, int depth, int frequency, const QString &comment)
{
    m_repository->updateMove(normalizedSfen, usiMove, value, depth, frequency, comment);

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
                                         const QString &usiMove, const QString &currentFilePath)
{
    m_repository->registerMergeMove(normalizedSfen, sfenWithPly, usiMove);

    emit dataChanged();

    // 自動保存
    if (!currentFilePath.isEmpty()) {
        if (m_repository->saveToFile(currentFilePath)) {
            emit modifiedChanged(false);
            return true;
        }
    }

    return false;
}

bool JosekiPresenter::hasDuplicateMove(const QString &normalizedSfen, const QString &usiMove) const
{
    if (!m_repository->containsPosition(normalizedSfen)) {
        return false;
    }

    const QVector<JosekiMove> &moves = m_repository->movesForPosition(normalizedSfen);
    for (const JosekiMove &move : moves) {
        if (move.move == usiMove) {
            return true;
        }
    }
    return false;
}

QVector<KifuMergeEntry> JosekiPresenter::buildMergeEntries(const QStringList &sfenList,
                                                            const QStringList &moveList,
                                                            const QStringList &japaneseMoveList,
                                                            int currentPly) const
{
    QVector<KifuMergeEntry> entries;

    for (int i = 0; i < moveList.size(); ++i) {
        if (i >= sfenList.size()) break;

        if (moveList[i].isEmpty()) continue;

        KifuMergeEntry entry;
        entry.ply = i + 1;
        entry.sfen = sfenList[i];
        entry.usiMove = moveList[i];

        // 日本語表記を取得（japaneseMoveListの先頭は「=== 開始局面 ===」なので+1）
        if (i + 1 < japaneseMoveList.size()) {
            entry.japaneseMove = japaneseMoveList[i + 1];
        } else {
            entry.japaneseMove = moveList[i];
        }

        entry.isCurrentMove = (i + 1 == currentPly);

        entries.append(entry);
    }

    return entries;
}

bool JosekiPresenter::buildMergeEntriesFromKifFile(const QString &kifFilePath,
                                                    QVector<KifuMergeEntry> &entries,
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
    qCDebug(lcUi) << "  mainline.usiMoves.size():" << mainline.usiMoves.size();
    qCDebug(lcUi) << "  mainline.sfenList.size():" << mainline.sfenList.size();

    if (mainline.usiMoves.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QString();  // 空 = 指し手がない
        }
        return false;
    }

    // sfenListが空の場合はbaseSfenとusiMovesから生成
    QStringList sfenList = mainline.sfenList;
    if (sfenList.isEmpty() && !mainline.baseSfen.isEmpty()) {
        sfenList = SfenPositionTracer::buildSfenRecord(mainline.baseSfen, mainline.usiMoves, false);
    }

    for (int i = 0; i < mainline.usiMoves.size(); ++i) {
        if (i >= sfenList.size()) break;
        if (mainline.usiMoves[i].isEmpty()) continue;

        KifuMergeEntry entry;
        entry.ply = i + 1;
        entry.sfen = sfenList[i];
        entry.usiMove = mainline.usiMoves[i];

        // 日本語表記を取得
        int dispIndex = i;
        if (mainline.disp.size() > mainline.usiMoves.size()) {
            dispIndex = i + 1;
        }

        if (dispIndex < mainline.disp.size() && !mainline.disp[dispIndex].prettyMove.isEmpty()) {
            entry.japaneseMove = mainline.disp[dispIndex].prettyMove;
        } else {
            entry.japaneseMove = mainline.usiMoves[i];
        }

        entry.isCurrentMove = false;

        entries.append(entry);
    }

    return !entries.isEmpty();
}
