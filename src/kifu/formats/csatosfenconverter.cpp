/// @file csatosfenconverter.cpp
/// @brief CSA形式棋譜コンバータクラスの実装
///
/// 字句解析・トークン化は CsaLexer に委譲。
/// 本ファイルはファイルI/O・パーサ状態管理・オーケストレーションを担当する。

#include "csatosfenconverter.h"
#include "csalexer.h"
#include "kifdisplayitem.h"
#include "parsecommon.h"

#include <QFile>
#include <QStringDecoder>

// ============================================================
// ファイル読込
// ============================================================

bool CsaToSfenConverter::readAllLinesDetectEncoding(const QString& path, QStringList& outLines, QString* warn)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (warn) *warn += QStringLiteral("Failed to open: %1\n").arg(path);
        return false;
    }

    const QByteArray raw = f.readAll();
    const QByteArray head = raw.left(128);
    const bool utf8Header  = head.contains("CSA encoding=UTF-8")
                          || head.contains("'encoding=UTF-8");
    const bool sjisHeader  = head.contains("CSA encoding=SHIFT_JIS")
                          || head.contains("CSA encoding=Shift_JIS")
                          || head.contains("'encoding=SHIFT_JIS")
                          || head.contains("'encoding=Shift_JIS");

    QString text;

    if (utf8Header) {
        QStringDecoder dec(QStringDecoder::Utf8);
        text = dec(raw);
    } else if (sjisHeader) {
        QStringDecoder decSjis("Shift-JIS");
        if (decSjis.isValid()) {
            text = decSjis(raw);
        } else {
            QStringDecoder dec(QStringDecoder::System);
            text = dec(raw);
        }
    } else {
        QStringDecoder decUtf8(QStringDecoder::Utf8);
        QString t = decUtf8(raw);
        if (!decUtf8.hasError() && !t.isEmpty()) {
            text = t;
        } else {
            QStringDecoder decSjis("Shift-JIS");
            if (decSjis.isValid()) {
                text = decSjis(raw);
            } else {
                QStringDecoder decSys(QStringDecoder::System);
                text = decSys(raw);
            }
        }
    }

    text.replace("\r\n", "\n");
    text.replace("\r", "\n");
    outLines = text.split('\n', Qt::KeepEmptyParts);
    return true;
}

// ============================================================
// 対局情報抽出
// ============================================================

QList<KifGameInfoItem> CsaToSfenConverter::extractGameInfo(const QString& filePath)
{
    QList<KifGameInfoItem> items;
    QStringList lines;
    QString warn;

    if (!readAllLinesDetectEncoding(filePath, lines, &warn)) return items;

    for (qsizetype i = 0; i < lines.size(); ++i) {
        const QString line = lines.at(i).trimmed();
        if (line.isEmpty()) continue;

        if (CsaLexer::isMoveLine(line)) break;
        if (line.startsWith(QLatin1Char('P')) || line == QLatin1String("PI")) continue;

        if (line.startsWith(QLatin1String("N+"))) {
            items.append({ QStringLiteral("先手"), line.mid(2).trimmed() });
        } else if (line.startsWith(QLatin1String("N-"))) {
            items.append({ QStringLiteral("後手"), line.mid(2).trimmed() });
        } else if (line.startsWith(QLatin1Char('$'))) {
            const qsizetype colon = line.indexOf(QLatin1Char(':'));
            if (colon > 0) {
                QString key = line.mid(1, colon - 1).trimmed();
                const QString val = line.mid(colon + 1).trimmed();

                if (key == QLatin1String("EVENT"))          key = QStringLiteral("棋戦");
                else if (key == QLatin1String("SITE"))       key = QStringLiteral("場所");
                else if (key == QLatin1String("START_TIME")) key = QStringLiteral("開始日時");
                else if (key == QLatin1String("END_TIME"))   key = QStringLiteral("終了日時");
                else if (key == QLatin1String("TIME_LIMIT")) key = QStringLiteral("持ち時間");
                else if (key == QLatin1String("TIME"))       key = QStringLiteral("持ち時間(秒/加算)");
                else if (key == QLatin1String("TIME+"))      key = QStringLiteral("先手:持ち時間(秒/加算)");
                else if (key == QLatin1String("TIME-"))      key = QStringLiteral("後手:持ち時間(秒/加算)");
                else if (key == QLatin1String("MAX_MOVES") || key == QLatin1String("SMAX_MOVES"))
                    key = QStringLiteral("最大手数");
                else if (key == QLatin1String("JISHOGI_POINTS")
                         || key == QLatin1String("JISHOGI_POINT")
                         || key == QLatin1String("JISHOGI"))
                    key = QStringLiteral("持将棋点数");
                else if (key == QLatin1String("OPENING"))    key = QStringLiteral("戦型");

                items.append({ key, val });
            }
        } else if (line.startsWith(QLatin1Char('V'))) {
            items.append({ QStringLiteral("バージョン"), line });
        }
    }
    return items;
}

// ============================================================
// メインパーサ ヘルパ
// ============================================================

namespace {

// parse() のループ状態を束ねる構造体
struct CsaParseState {
    KifParseResult& out;
    CsaLexer::Board& board;
    CsaLexer::Color turn;
    QString* warn;
    int prevTx = -1;
    int prevTy = -1;
    qint64 cumMs[2] = {0, 0};
    int lastMover = -1;
    bool lastDispIsResult = false;
    int lastResultDispIndex = -1;
    int lastResultSideIdx = -1;
    int moveCount = 0;
    bool firstMoveFound = false;
    QStringList pendingComments;
    KifDisplayItem openingItem;
};

// 開始局面エントリをまだ追加していなければ追加し、pendingCommentsを処理する
static void ensureOpeningItemAdded(CsaParseState& st)
{
    if (st.firstMoveFound) {
        if (!st.pendingComments.isEmpty() && st.out.mainline.disp.size() > 1) {
            QString& dst = st.out.mainline.disp.last().comment;
            if (!dst.isEmpty()) dst += QLatin1Char('\n');
            dst += st.pendingComments.join(QStringLiteral("\n"));
            st.pendingComments.clear();
        }
        return;
    }
    st.firstMoveFound = true;
    st.openingItem.comment = st.pendingComments.isEmpty()
        ? QString()
        : st.pendingComments.join(QStringLiteral("\n"));
    st.out.mainline.disp.append(st.openingItem);
    st.pendingComments.clear();
}

static void handleCsaComment(const QString& token, CsaParseState& st, bool attachToResult)
{
    const QString norm = CsaLexer::normalizeCsaCommentLine(token);
    if (norm.isNull()) return;

    if (attachToResult && st.lastDispIsResult && st.lastResultDispIndex >= 0 &&
        st.lastResultDispIndex < st.out.mainline.disp.size()) {
        QString& dst = st.out.mainline.disp[st.lastResultDispIndex].comment;
        if (!dst.isEmpty()) dst += QLatin1Char('\n');
        dst += norm;
    } else {
        st.pendingComments.append(norm);
    }
}

static void handleCsaTurnMarker(const QString& token, CsaParseState& st)
{
    st.turn = (token.at(0) == QLatin1Char('+'))
        ? CsaLexer::Black : CsaLexer::White;
    st.lastDispIsResult = false;
    st.lastResultDispIndex = -1;
    st.lastResultSideIdx = -1;
}

static void handleCsaResultCode(const QString& token, CsaParseState& st)
{
    ensureOpeningItemAdded(st);

    const QString sideMark = (st.turn == CsaLexer::Black)
        ? QStringLiteral("▲") : QStringLiteral("△");
    QString label = CsaLexer::csaResultToLabel(token);
    if (label.isEmpty()) label = token;

    st.out.mainline.disp.append(
        KifuParseCommon::createMoveDisplayItem(st.moveCount, sideMark + label));

    st.lastDispIsResult = true;
    st.lastResultDispIndex = static_cast<int>(st.out.mainline.disp.size() - 1);
    st.lastResultSideIdx = (st.turn == CsaLexer::Black) ? 0 : 1;
}

static void handleCsaTimeToken(const QString& token, CsaParseState& st)
{
    qint64 moveMs = 0;
    if (!CsaLexer::parseTimeTokenMs(token, moveMs)) {
        if (st.warn) *st.warn += QStringLiteral("Failed to parse time token: %1\n").arg(token);
        return;
    }

    if (st.lastDispIsResult && st.lastResultDispIndex >= 0 &&
        st.lastResultDispIndex < st.out.mainline.disp.size()) {
        if (st.lastResultSideIdx >= 0) {
            st.cumMs[st.lastResultSideIdx] += moveMs;
            st.out.mainline.disp[st.lastResultDispIndex].timeText =
                CsaLexer::composeTimeText(moveMs, st.cumMs[st.lastResultSideIdx]);
        } else {
            st.out.mainline.disp[st.lastResultDispIndex].timeText = CsaLexer::composeTimeText(moveMs, 0);
        }
    } else if (!st.out.mainline.disp.isEmpty() && st.lastMover >= 0) {
        st.cumMs[st.lastMover] += moveMs;
        st.out.mainline.disp.last().timeText = CsaLexer::composeTimeText(moveMs, st.cumMs[st.lastMover]);
    }
}

} // namespace

// ============================================================
// メインパーサ
// ============================================================

bool CsaToSfenConverter::parse(const QString& filePath, KifParseResult& out, QString* warn)
{
    QStringList lines;
    if (!readAllLinesDetectEncoding(filePath, lines, warn)) return false;

    out.mainline.baseSfen.clear();
    out.mainline.usiMoves.clear();
    out.mainline.disp.clear();
    out.variations.clear();

    int   idx  = 0;
    CsaLexer::Color stm  = CsaLexer::Black;
    CsaLexer::Board board;
    CsaLexer::setHirate(board);
    if (!CsaLexer::parseStartPos(lines, idx, out.mainline.baseSfen, stm, board)) {
        if (warn) *warn += QStringLiteral("Failed to parse start position.\n");
        return false;
    }

    CsaParseState st{out, board, stm, warn,
                      -1, -1, {0, 0}, -1,
                      false, -1, -1, 0, false,
                      {}, {}};
    st.openingItem = KifuParseCommon::createOpeningDisplayItem(QString(), QString());

    for (qsizetype i = idx; i < lines.size(); ++i) {
        QString s = lines.at(i);
        if (s.isEmpty()) continue;
        s = s.trimmed();
        if (s.isEmpty()) continue;

        const bool isCommaLine = s.contains(QLatin1Char(','));
        const QStringList tokens = s.split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (const QString& tokenRaw : std::as_const(tokens)) {
            const QString token = tokenRaw.trimmed();
            if (token.isEmpty()) continue;

            if (token.startsWith(QLatin1Char('\''))) {
                handleCsaComment(token, st, isCommaLine);
            } else if (CsaLexer::isTurnMarker(token)) {
                handleCsaTurnMarker(token, st);
            } else if (token.startsWith(QLatin1Char('%'))) {
                handleCsaResultCode(token, st);
            } else if (isCommaLine && token.startsWith(QLatin1Char('T'))) {
                handleCsaTimeToken(token, st);
            } else if (CsaLexer::isMoveLine(token)) {
                const QChar head  = token.at(0);
                const CsaLexer::Color mover = (head == QLatin1Char('+')) ? CsaLexer::Black : CsaLexer::White;

                QString usi, pretty;
                if (!CsaLexer::parseMoveLine(token, mover, st.board, st.prevTx, st.prevTy,
                                    usi, pretty, st.warn)) {
                    if (st.warn) *st.warn += QStringLiteral("Failed to parse move: %1\n").arg(token);
                    continue;
                }

                ensureOpeningItemAdded(st);

                st.out.mainline.usiMoves.append(usi);
                ++st.moveCount;
                st.out.mainline.disp.append(
                    KifuParseCommon::createMoveDisplayItem(st.moveCount, pretty));

                st.lastMover = (mover == CsaLexer::Black) ? 0 : 1;
                st.lastDispIsResult = false;
                st.lastResultDispIndex = -1;
                st.lastResultSideIdx = -1;
                st.turn = (mover == CsaLexer::Black) ? CsaLexer::White : CsaLexer::Black;
            }
        }
    }

    if (!st.firstMoveFound) {
        st.openingItem.comment = st.pendingComments.isEmpty()
            ? QString() : st.pendingComments.join(QStringLiteral("\n"));
        st.out.mainline.disp.append(st.openingItem);
        st.pendingComments.clear();
    }

    if (!st.pendingComments.isEmpty() && !st.out.mainline.disp.isEmpty()) {
        QString& dst = st.out.mainline.disp.last().comment;
        if (!dst.isEmpty()) dst += QLatin1Char('\n');
        dst += st.pendingComments.join(QStringLiteral("\n"));
    }

    return true;
}
