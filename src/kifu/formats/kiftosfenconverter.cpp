/// @file kiftosfenconverter.cpp
/// @brief KIF形式棋譜コンバータクラスの実装

#include "kiftosfenconverter.h"
#include "kiflexer.h"
#include "kifreader.h"
#include "board/sfenpositiontracer.h"
#include "parsecommon.h"
#include "notationutils.h"

#include <QRegularExpression>
#include "logcategories.h"
#include <QMap>

// ===== public =====

QString KifToSfenConverter::mapHandicapToSfen(const QString& label) {
    return NotationUtils::mapHandicapToSfen(label);
}

QString KifToSfenConverter::detectInitialSfenFromFile(const QString& kifPath, QString* detectedLabel)
{
    QString usedEnc, warn;
    QStringList lines;
    if (!KifReader::readLinesAuto(kifPath, lines, &usedEnc, &warn)) {
        if (detectedLabel) *detectedLabel = QStringLiteral("平手(既定)");
        return NotationUtils::mapHandicapToSfen(QStringLiteral("平手"));
    }

    // 先に BOD を試す
    QString bodSfen;
    if (buildInitialSfenFromBod(lines, bodSfen, detectedLabel, &warn)) {
        qCDebug(lcKifu).noquote() << "BOD detected. sfen =" << bodSfen;
        return bodSfen;
    }

    // 従来の「手合割」ベース
    QString found;
    for (auto it = lines.cbegin(); it != lines.cend(); ++it) {
        const QString line = it->trimmed();
        if (line.contains(QStringLiteral("手合割")) || line.contains(QStringLiteral("手合"))) {
            found = line; break;
        }
    }
    if (found.isEmpty()) {
        if (detectedLabel) *detectedLabel = QStringLiteral("平手(既定)");
        return NotationUtils::mapHandicapToSfen(QStringLiteral("平手"));
    }

    static const QRegularExpression s_afterColon(QStringLiteral("^.*[:：]"));
    QString label = found;
    label.remove(s_afterColon);
    label = label.trimmed();
    if (detectedLabel) *detectedLabel = label;
    return NotationUtils::mapHandicapToSfen(label);
}

QList<KifDisplayItem> KifToSfenConverter::extractMovesWithTimes(const QString& kifPath,
                                                                QString* errorMessage)
{
    QList<KifDisplayItem> out;

    QString usedEnc;
    QStringList lines;
    if (!KifReader::readLinesAuto(kifPath, lines, &usedEnc, errorMessage)) {
        return out;
    }

    QString openingCommentBuf;
    QString openingBookmarkBuf;
    QString commentBuf;
    int moveIndex = 0;
    bool firstMoveFound = false;

    // --- ヘッダ走査（手番/手合割の確認） ---
    for (const QString& raw : std::as_const(lines)) {
        const QString line = raw.trimmed();
        if (KifLexer::startsWithMoveNumber(line)) break;
        if (line.contains(QStringLiteral("後手番"))) { moveIndex = 1; break; }
        if (line.startsWith(QStringLiteral("手合割")) || line.startsWith(QStringLiteral("手合"))) {
            if (!line.contains(QStringLiteral("平手"))) moveIndex = 1;
        }
    }

    for (const QString& raw : std::as_const(lines)) {
        QString lineStr = raw.trimmed();

        // 行頭コメント
        if (KifuParseCommon::isKifCommentLine(lineStr)) {
            const QString c = lineStr.mid(1).trimmed();
            if (!c.isEmpty()) {
                KifuParseCommon::appendLine(firstMoveFound ? commentBuf : openingCommentBuf, c);
            }
            continue;
        }

        // しおり
        if (KifuParseCommon::isBookmarkLine(lineStr)) {
            const QString name = lineStr.mid(1).trimmed();
            if (!name.isEmpty()) {
                if (firstMoveFound && out.size() > 1) {
                    KifuParseCommon::appendLine(out.last().bookmark, name);
                } else {
                    KifuParseCommon::appendLine(openingBookmarkBuf, name);
                }
            }
            continue;
        }

        if (lineStr.isEmpty() || KifuParseCommon::isKifSkippableHeaderLine(lineStr)
            || KifuParseCommon::isBoardHeaderOrFrame(lineStr)) continue;
        if (KifLexer::variationHeaderRe().match(lineStr).hasMatch()) break;

        // --- 1行に含まれる指し手をループ処理 ---
        while (!lineStr.isEmpty()) {
            int digits = 0;
            if (!KifLexer::startsWithMoveNumber(lineStr, &digits)) break;
            int i = digits;
            while (i < lineStr.size() && lineStr.at(i).isSpace()) ++i;
            if (i >= lineStr.size()) break;

            QString rest = lineStr.mid(i).trimmed();
            QRegularExpressionMatch tm;
            QString timeText;
            int nextMoveStartIdx = -1;

            KifLexer::stripTimeAndNextMove(rest, lineStr, i, tm, timeText, nextMoveStartIdx);
            KifLexer::stripInlineComment(rest, commentBuf);
            KifLexer::advanceToNextMove(lineStr, i, tm, nextMoveStartIdx);

            // 最初の指し手が見つかった時に開始局面エントリを挿入
            if (!firstMoveFound) {
                firstMoveFound = true;
                out.push_back(KifuParseCommon::createOpeningDisplayItem(openingCommentBuf, openingBookmarkBuf));
            }

            // 終局語?
            QString term;
            if (KifuParseCommon::isTerminalWordExact(rest, &term)) {
                KifuParseCommon::flushCommentToLastItem(commentBuf, out);
                ++moveIndex;
                out.push_back(KifLexer::buildTerminalItem(moveIndex, term, timeText, tm));
                lineStr.clear();
                break;
            }

            // 通常手
            if (!rest.isEmpty()) {
                KifuParseCommon::flushCommentToLastItem(commentBuf, out);
                ++moveIndex;
                const QString teban = (moveIndex % 2 != 0) ? QStringLiteral("▲") : QStringLiteral("△");
                KifDisplayItem item;
                item.prettyMove = teban + rest;
                item.timeText   = timeText.isEmpty() ? QStringLiteral("00:00/00:00:00") : timeText;
                item.ply        = moveIndex;
                out.push_back(item);
            }
        }
    }

    // 最後の指し手の後に残っているコメントを付与
    if (!commentBuf.isEmpty() && !out.isEmpty()) {
        KifuParseCommon::appendLine(out.last().comment, commentBuf);
    }

    // 指し手が一つもなかった場合でも開始局面エントリを追加
    if (out.isEmpty()) {
        out.push_back(KifuParseCommon::createOpeningDisplayItem(openingCommentBuf, openingBookmarkBuf));
    }

    return out;
}

QStringList KifToSfenConverter::convertFile(const QString& kifPath, QString* errorMessage)
{
    QStringList out;

    QString usedEnc;
    QStringList lines;
    if (!KifReader::readLinesAuto(kifPath, lines, &usedEnc, errorMessage)) {
        return out;
    }

    int prevToFile = 0, prevToRank = 0;

    for (const QString& raw : std::as_const(lines)) {
        QString lineStr = raw.trimmed();

        if (KifuParseCommon::isKifCommentLine(lineStr) || KifuParseCommon::isBookmarkLine(lineStr)) continue;
        if (lineStr.isEmpty() || KifuParseCommon::isKifSkippableHeaderLine(lineStr)
            || KifuParseCommon::isBoardHeaderOrFrame(lineStr)) continue;
        if (KifLexer::variationHeaderRe().match(lineStr).hasMatch()) break;
        if (KifuParseCommon::containsAnyTerminal(lineStr)) break;

        // --- 1行複数指し手ループ ---
        while (!lineStr.isEmpty()) {
            int digits = 0;
            if (!KifLexer::startsWithMoveNumber(lineStr, &digits)) break;
            int i = digits;
            while (i < lineStr.size() && lineStr.at(i).isSpace()) ++i;
            if (i >= lineStr.size()) break;

            QString rest = lineStr.mid(i).trimmed();
            QRegularExpressionMatch tm;
            QString timeText;
            int nextMoveStartIdx = -1;

            KifLexer::stripTimeAndNextMove(rest, lineStr, i, tm, timeText, nextMoveStartIdx);

            // インラインコメント除去
            int cIdx = static_cast<int>(rest.indexOf(QChar(u'*')));
            if (cIdx < 0) cIdx = static_cast<int>(rest.indexOf(QChar(u'＊')));
            if (cIdx >= 0) rest = rest.left(cIdx).trimmed();

            // USI変換
            QString usi;
            if (KifLexer::convertMoveLine(rest, usi, prevToFile, prevToRank)) {
                out << usi;
            } else {
                if (errorMessage) *errorMessage += QStringLiteral("[skip ?] %1\n").arg(raw);
            }

            KifLexer::advanceToNextMove(lineStr, i, tm, nextMoveStartIdx);
        }
    }

    qCDebug(lcKifu).noquote() << QStringLiteral("moves = %1").arg(out.size());
    return out;
}

bool KifToSfenConverter::parseWithVariations(const QString& kifPath,
                                             KifParseResult& out,
                                             QString* errorMessage)
{
    out = KifParseResult{};

    // フェーズ1: 本譜抽出
    extractMainLine(kifPath, out, errorMessage);

    // フェーズ2: 変化抽出用にファイル読み込み
    QString usedEnc;
    QStringList lines;
    if (!KifReader::readLinesAuto(kifPath, lines, &usedEnc, errorMessage)) {
        return false;
    }

    // フェーズ3: 変化ブロックの収集と解析
    QVector<KifVariation> vars;
    int i = 0;

    while (i < lines.size()) {
        const QString l = lines.at(i).trimmed();
        if (l.isEmpty() || KifuParseCommon::isKifSkippableHeaderLine(l)
            || KifuParseCommon::isBoardHeaderOrFrame(l)) { ++i; continue; }

        const QRegularExpressionMatch m = KifLexer::variationHeaderCaptureRe().match(l);
        if (!m.hasMatch()) { ++i; continue; }

        const int startPly = KifuParseCommon::flexDigitsToIntNoDetach(m.captured(1));

        // ブロック行の収集（次の変化ヘッダまたは空行まで）
        ++i;
        QStringList block;
        for (; i < lines.size(); ++i) {
            const QString s = lines.at(i).trimmed();
            if (s.isEmpty()) break;
            if (s.startsWith(QStringLiteral("変化"))) break;
            block << s;
        }

        // 分岐元の局面SFENを特定
        const QString baseSfen = findBranchBaseSfen(vars, out.mainline, startPly - 1);

        // 変化ブロック解析
        vars.push_back(parseVariationBlock(block, startPly, baseSfen));
    }

    out.variations = vars;
    return true;
}

// ---------- parseWithVariations ヘルパ ----------

void KifToSfenConverter::extractMainLine(const QString& kifPath,
                                          KifParseResult& out,
                                          QString* errorMessage)
{
    QString teai;
    out.mainline.baseSfen = detectInitialSfenFromFile(kifPath, &teai);
    out.mainline.disp     = extractMovesWithTimes(kifPath, errorMessage);
    out.mainline.usiMoves = convertFile(kifPath, errorMessage);

    out.mainline.sfenList = SfenPositionTracer::buildSfenRecord(
        out.mainline.baseSfen, out.mainline.usiMoves, false);
    qCDebug(lcKifu).noquote() << "mainline sfenList built: size=" << out.mainline.sfenList.size();
}

QString KifToSfenConverter::findBranchBaseSfen(const QVector<KifVariation>& vars,
                                                const KifLine& mainLine,
                                                int branchPointPly)
{
    // まず既存の分岐から探す
    for (qsizetype vi = vars.size() - 1; vi >= 0; --vi) {
        const KifVariation& prevVar = vars.at(vi);
        if (prevVar.startPly <= branchPointPly && !prevVar.line.sfenList.isEmpty()) {
            const qsizetype idx = branchPointPly - (prevVar.startPly - 1);
            if (idx >= 0 && idx < prevVar.line.sfenList.size()) {
                qCDebug(lcKifu).noquote()
                    << "variation baseSfen from variation" << vi
                    << "(startPly=" << prevVar.startPly << ")";
                return prevVar.line.sfenList.at(idx);
            }
        }
    }

    // 分岐から見つからなければ本譜から探す
    if (branchPointPly >= 0 && branchPointPly < mainLine.sfenList.size()) {
        qCDebug(lcKifu).noquote() << "variation baseSfen from mainline";
        return mainLine.sfenList.at(branchPointPly);
    }

    qCDebug(lcKifu).noquote() << "variation baseSfen: NOT FOUND";
    return {};
}

KifVariation KifToSfenConverter::parseVariationBlock(const QStringList& blockLines,
                                                      int startPly,
                                                      const QString& baseSfen)
{
    KifVariation var;
    var.startPly = startPly;
    var.line.baseSfen = baseSfen;

    extractMovesFromBlock(blockLines, startPly, var.line);

    if (!var.line.baseSfen.isEmpty() && !var.line.usiMoves.isEmpty()) {
        var.line.sfenList = SfenPositionTracer::buildSfenRecord(
            var.line.baseSfen, var.line.usiMoves, false);
        var.line.gameMoves = SfenPositionTracer::buildGameMoves(
            var.line.baseSfen, var.line.usiMoves);
        qCDebug(lcKifu).noquote() << "variation startPly=" << var.startPly
                           << "sfenList built: size=" << var.line.sfenList.size()
                           << "gameMoves built: size=" << var.line.gameMoves.size();
    }

    return var;
}

void KifToSfenConverter::extractMovesFromBlock(const QStringList& blockLines,
                                                int startPly,
                                                KifLine& line)
{
    int prevToFile = 0, prevToRank = 0;
    int moveIndex = startPly - 1;
    QString commentBuf;
    bool firstMoveFound = false;

    for (const QString& raw : std::as_const(blockLines)) {
        QString lineStr = raw;

        if (KifuParseCommon::isKifCommentLine(lineStr)) {
            const QString c = lineStr.mid(1).trimmed();
            if (!c.isEmpty()) KifuParseCommon::appendLine(commentBuf, c);
            continue;
        }

        if (KifuParseCommon::isBookmarkLine(lineStr)) {
            const QString name = lineStr.mid(1).trimmed();
            if (!name.isEmpty() && !line.disp.isEmpty()) {
                KifuParseCommon::appendLine(line.disp.last().bookmark, name);
            }
            continue;
        }

        if (lineStr.isEmpty() || KifuParseCommon::isKifSkippableHeaderLine(lineStr)
            || KifuParseCommon::isBoardHeaderOrFrame(lineStr)) continue;

        // 1行複数手ループ
        while (!lineStr.isEmpty()) {
            int digits = 0;
            if (!KifLexer::startsWithMoveNumber(lineStr, &digits)) break;
            int j = digits;
            while (j < lineStr.size() && lineStr.at(j).isSpace()) ++j;
            if (j >= lineStr.size()) break;

            QString rest = lineStr.mid(j).trimmed();
            QRegularExpressionMatch tm;
            QString timeText;
            int nextMoveStartIdx = -1;

            KifLexer::stripTimeAndNextMove(rest, lineStr, j, tm, timeText, nextMoveStartIdx);
            KifLexer::stripInlineComment(rest, commentBuf);

            // 終局語
            QString term;
            if (KifuParseCommon::isTerminalWordExact(rest, &term)) {
                KifuParseCommon::flushCommentToLastItem(commentBuf, line.disp, 0);
                ++moveIndex;
                line.disp.push_back(KifLexer::buildTerminalItem(moveIndex, term, timeText, tm));
                lineStr.clear();
                break;
            }

            // 通常手
            if (!commentBuf.isEmpty() && firstMoveFound && !line.disp.isEmpty()) {
                KifuParseCommon::appendLine(line.disp.last().comment, commentBuf);
                commentBuf.clear();
            } else if (firstMoveFound) {
                commentBuf.clear();
            }

            firstMoveFound = true;

            ++moveIndex;
            const QString teban = (moveIndex % 2 != 0) ? QStringLiteral("▲") : QStringLiteral("△");
            KifDisplayItem item;
            item.prettyMove = teban + rest;
            item.timeText   = timeText.isEmpty() ? QStringLiteral("00:00/00:00:00") : timeText;
            item.ply        = moveIndex;
            line.disp.push_back(item);

            // USI変換
            QString usi;
            if (KifLexer::convertMoveLine(rest, usi, prevToFile, prevToRank)) {
                line.usiMoves << usi;
            }

            KifLexer::advanceToNextMove(lineStr, j, tm, nextMoveStartIdx);
        }
    }

    // 最後の指し手の後に残っているコメントを付与
    if (!commentBuf.isEmpty() && !line.disp.isEmpty()) {
        KifuParseCommon::appendLine(line.disp.last().comment, commentBuf);
    }
}

// ===== extractGameInfo =====

QList<KifGameInfoItem> KifToSfenConverter::extractGameInfo(const QString& filePath)
{
    QList<KifGameInfoItem> ordered;
    if (filePath.isEmpty()) return ordered;

    QString usedEnc, warn;
    QStringList lines;
    if (!KifReader::readLinesAuto(filePath, lines, &usedEnc, &warn)) {
        qCWarning(lcKifu).noquote() << "read failed:" << filePath << "warn:" << warn;
        return ordered;
    }
    qCDebug(lcKifu).noquote()
        << QStringLiteral("encoding = %1 , lines = %2").arg(usedEnc).arg(lines.size());

    static const QRegularExpression kHeaderLine(
        QStringLiteral("^\\s*([^：:]+?)\\s*[：:]\\s*(.*?)\\s*$")
        );
    static const QRegularExpression kLineIsComment(
        QStringLiteral("^\\s*[#＃\\*\\＊]")
        );
    static const QRegularExpression kMovesHeader(
        QStringLiteral("^\\s*手数[-－ー]+指手[-－ー]+消費時間")
        );
    static const QRegularExpression kLineLooksLikeMoveNo(
        QStringLiteral("^\\s*[0-9０-９]+\\s")
        );
    static const QRegularExpression kVariationHead(
        QStringLiteral("^\\s*変化\\s*[：:]\\s*[0-9０-９]+\\s*手")
        );

    auto isBodHeld = [](const QString& t) {
        return t.startsWith(QStringLiteral("先手の持駒")) ||
               t.startsWith(QStringLiteral("後手の持駒")) ||
               t.startsWith(QStringLiteral("先手の持ち駒")) ||
               t.startsWith(QStringLiteral("後手の持ち駒"));
    };

    for (const QString& rawLine : std::as_const(lines)) {
        const QString line = rawLine;
        const QString t = line.trimmed();

        if (t.isEmpty()) continue;
        if (kLineIsComment.match(t).hasMatch()) continue;
        if (kMovesHeader.match(t).hasMatch()) continue;
        if (kLineLooksLikeMoveNo.match(t).hasMatch()) break;
        if (kVariationHead.match(t).hasMatch()) continue;
        if (isBodHeld(t)) continue;

        QRegularExpressionMatch m = kHeaderLine.match(line);
        if (!m.hasMatch()) continue;

        QString key = m.captured(1).trimmed();
        if (key.endsWith(u'：') || key.endsWith(u':')) key.chop(1);
        key = key.trimmed();

        QString val = m.captured(2).trimmed();
        val.replace(QStringLiteral("\\n"), QStringLiteral("\n"));

        ordered.push_back({ key, val });
    }

    return ordered;
}

QMap<QString, QString> KifToSfenConverter::extractGameInfoMap(const QString& filePath)
{
    return KifuParseCommon::toGameInfoMap(extractGameInfo(filePath));
}

// ===== BOD =====

bool KifToSfenConverter::buildInitialSfenFromBod(const QStringList& lines,
                                                 QString& outSfen,
                                                 QString* detectedLabel,
                                                 QString* /*warn*/)
{
    static const QChar ranks_1to9[9] = {u'一',u'二',u'三',u'四',u'五',u'六',u'七',u'八',u'九'};

    // 1) 盤面情報の収集
    QMap<QChar, QString> rowByRank;
    if (!KifLexer::collectBodRows(lines, rowByRank, detectedLabel)) return false;

    // 2) 持駒
    QMap<Piece,int> handB, handW;
    for (const QString& line : std::as_const(lines)) {
        KifLexer::parseBodHandsLine(line, handB, true);
        KifLexer::parseBodHandsLine(line, handW, false);
    }

    // 3) 手番・手数
    QChar turn;
    int moveNumber;
    KifLexer::parseBodTurnAndMoveNumber(lines, turn, moveNumber);

    // 4) SFEN組み立て
    QStringList boardRows;
    for (QChar rk : ranks_1to9) {
        boardRows << rowByRank.value(rk);
    }

    const QString board = boardRows.join(QLatin1Char('/'));
    const QString hands = KifLexer::buildHandsSfen(handB, handW);
    outSfen = QStringLiteral("%1 %2 %3 %4")
                  .arg(board, QString(turn), hands, QString::number(moveNumber));

    return true;
}

QString KifToSfenConverter::extractOpeningComment(const QString& filePath)
{
    QString usedEnc, warn;
    QStringList lines;
    if (!KifReader::readLinesAuto(filePath, lines, &usedEnc, &warn)) {
        qCWarning(lcKifu).noquote() << "read failed for opening comment:" << filePath << "warn:" << warn;
        return QString();
    }

    QString buf;

    for (const QString& raw : std::as_const(lines)) {
        const QString t = raw.trimmed();
        if (t.isEmpty()) continue;
        if (KifuParseCommon::isKifSkippableHeaderLine(t)
            || KifuParseCommon::isBoardHeaderOrFrame(t)) continue;

        if (KifLexer::startsWithMoveNumber(t)) break;
        if (KifLexer::variationHeaderRe().match(t).hasMatch()) break;

        if (t.startsWith(QChar(u'*')) || t.startsWith(QChar(u'＊'))) {
            const QString c = t.mid(1).trimmed();
            if (!c.isEmpty()) {
                if (!buf.isEmpty()) buf += QLatin1Char('\n');
                buf += c;
            }
            continue;
        }

        if (t.startsWith(QLatin1Char('&'))) {
            continue;
        }
    }
    return buf;
}
