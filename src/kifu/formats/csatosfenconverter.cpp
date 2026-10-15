/// @file csatosfenconverter.cpp
/// @brief CSA形式棋譜コンバータクラスの実装
///
/// 字句解析・トークン化は CsaLexer に委譲。
/// 本ファイルはファイルI/O・パーサ状態管理・オーケストレーションを担当する。

#include "csatosfenconverter.h"
#include "csalexer.h"
#include "kifdisplayitem.h"
#include "parsecommon.h"
#include "shiftjiscodec.h"
#include "shogiboard.h"

#include <QFile>
#include <QStringDecoder>

// ============================================================
// ファイル読込
// ============================================================

bool CsaToSfenConverter::readFileText(const QString& path, QString& outText, QString* warn)
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
        if (auto decoded = ShiftJisCodec::decode(raw)) {
            text = std::move(*decoded);
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
            if (auto decoded = ShiftJisCodec::decode(raw)) {
                text = std::move(*decoded);
            } else {
                QStringDecoder decSys(QStringDecoder::System);
                text = decSys(raw);
            }
        }
    }

    outText = std::move(text);
    return true;
}

QStringList CsaToSfenConverter::splitStatements(const QString& rawText, QString* warn)
{
    QString text = rawText;
    if (!text.isEmpty() && text.front() == QChar(0xFEFF)) text.remove(0, 1);
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    QStringList outLines;
    // 初期配置を含めてマルチステートメントを展開する。コメントと文字列値の
    // カンマは本文として保持し、複数対局の指し手を一つの棋譜に混ぜない。
    const QStringList physicalLines = text.split(QLatin1Char('\n'));
    for (QString line : physicalLines) {
        while (!line.isEmpty()) {
            line = line.trimmed();
            if (line == QLatin1String("/")) {
                if (warn) *warn += QStringLiteral("CSA: only the first game in the file is loaded.\n");
                return outLines;
            }
            const qsizetype comma = line.indexOf(QLatin1Char(','));
            if (comma < 0 || line.startsWith(QLatin1Char('\''))
                || line.startsWith(QLatin1Char('N')) || line.startsWith(QLatin1Char('$'))) {
                outLines.append(line);
                break;
            }
            outLines.append(line.left(comma));
            line = line.mid(comma + 1);
        }
    }
    return outLines;
}

// ============================================================
// 対局情報抽出
// ============================================================

QList<KifGameInfoItem> CsaToSfenConverter::extractGameInfo(const QString& filePath)
{
    QString text;
    QString warn;
    if (!readFileText(filePath, text, &warn)) return {};
    return extractGameInfoFromText(text);
}

QList<KifGameInfoItem> CsaToSfenConverter::extractGameInfoFromText(const QString& text)
{
    QList<KifGameInfoItem> items;
    QString warn;
    const QStringList lines = splitStatements(text, &warn);

    for (const QString& rawLine : std::as_const(lines)) {
        const QString line = rawLine.trimmed();
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
                QString val = line.mid(colon + 1).trimmed();

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
                else if (key == QLatin1String("NOTE")) {
                    key = QStringLiteral("備考");
                    QString decoded;
                    for (qsizetype i = 0; i < val.size(); ++i) {
                        if (val.at(i) == QLatin1Char('\\') && i + 1 < val.size()) {
                            if (val.at(i + 1) == QLatin1Char('n')) { decoded += QLatin1Char('\n'); ++i; continue; }
                            if (val.at(i + 1) == QLatin1Char('\\')) { decoded += QLatin1Char('\\'); ++i; continue; }
                        }
                        decoded += val.at(i);
                    }
                    val = decoded;
                }

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
    KifuParseCommon::KifLineDraft& line;
    ShogiBoard& board;
    Turn turn;
    QString* warn;
    BoardSquare previousTo{};   ///< 直前の手の移動先（「同」の判定に使う）
    qint64 cumMs[2] = {0, 0};
    int lastMover = -1;
    bool lastDispIsResult = false;
    int lastResultDispIndex = -1;
    int lastResultSideIdx = -1;
    int moveCount = 0;
    bool firstMoveFound = false;
    QStringList pendingComments{};
    KifDisplayItem openingItem{};
};

// 開始局面エントリをまだ追加していなければ追加し、pendingCommentsを処理する
static void ensureOpeningItemAdded(CsaParseState& st)
{
    if (st.firstMoveFound) {
        if (!st.pendingComments.isEmpty() && st.line.disp.size() > 1) {
            QString& dst = st.line.disp.last().comment;
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
    st.line.disp.append(st.openingItem);
    st.pendingComments.clear();
}

static void handleCsaComment(const QString& token, CsaParseState& st)
{
    const QString norm = CsaLexer::normalizeCsaCommentLine(token);
    if (norm.isNull()) return;

    if (st.lastDispIsResult && st.lastResultDispIndex >= 0 &&
        st.lastResultDispIndex < st.line.disp.size()) {
        QString& dst = st.line.disp[st.lastResultDispIndex].comment;
        if (!dst.isEmpty()) dst += QLatin1Char('\n');
        dst += norm;
    } else {
        st.pendingComments.append(norm);
    }
}

static void handleCsaTurnMarker(const QString& token, CsaParseState& st)
{
    st.turn = (token.at(0) == QLatin1Char('+'))
        ? Turn::Black : Turn::White;
    st.lastDispIsResult = false;
    st.lastResultDispIndex = -1;
    st.lastResultSideIdx = -1;
}

static void handleCsaResultCode(const QString& token, CsaParseState& st)
{
    ensureOpeningItemAdded(st);

    const QString sideMark = (st.turn == Turn::Black)
        ? QStringLiteral("▲") : QStringLiteral("△");
    QString label = CsaLexer::csaResultToLabel(token);
    if (label.isEmpty()) label = token;
    if ((token == QLatin1String("%+ILLEGAL_ACTION") && st.turn == Turn::White)
        || (token == QLatin1String("%-ILLEGAL_ACTION") && st.turn == Turn::Black)) {
        label = QStringLiteral("反則勝ち");
    }

    st.line.disp.append(
        KifuParseCommon::createMoveDisplayItem(st.moveCount + 1, sideMark + label));

    st.lastDispIsResult = true;
    st.lastResultDispIndex = static_cast<int>(st.line.disp.size() - 1);
    st.lastResultSideIdx = (st.turn == Turn::Black) ? 0 : 1;
    st.line.endsWithTerminal = true;
}

static void handleCsaTimeToken(const QString& token, CsaParseState& st)
{
    const auto moveMsOpt = CsaLexer::parseTimeTokenMs(token);
    if (!moveMsOpt) {
        if (st.warn) *st.warn += QStringLiteral("Failed to parse time token: %1\n").arg(token);
        return;
    }
    const qint64 moveMs = *moveMsOpt;

    if (st.lastDispIsResult && st.lastResultDispIndex >= 0 &&
        st.lastResultDispIndex < st.line.disp.size()) {
        if (st.lastResultSideIdx >= 0) {
            st.cumMs[st.lastResultSideIdx] += moveMs;
            st.line.disp[st.lastResultDispIndex].timeText =
                CsaLexer::composeTimeText(moveMs, st.cumMs[st.lastResultSideIdx]);
        } else {
            st.line.disp[st.lastResultDispIndex].timeText = CsaLexer::composeTimeText(moveMs, 0);
        }
    } else if (!st.line.disp.isEmpty() && st.lastMover >= 0) {
        st.cumMs[st.lastMover] += moveMs;
        st.line.disp.last().timeText = CsaLexer::composeTimeText(moveMs, st.cumMs[st.lastMover]);
    }
}

} // namespace

// ============================================================
// メインパーサ
// ============================================================

bool CsaToSfenConverter::parse(const QString& filePath, KifParseResult& out, QString* warn)
{
    QString text;
    if (!readFileText(filePath, text, warn)) return false;
    return parseText(text, out, warn);
}

bool CsaToSfenConverter::parseText(const QString& text, KifParseResult& out, QString* warn)
{
    const QStringList lines = splitStatements(text, warn);

    KifuParseCommon::KifLineDraft line;
    out.variations.clear();

    int idx = 0;
    ShogiBoard board;
    if (!CsaLexer::parseStartPos(lines, idx, line.baseSfen, board)) {
        if (warn) *warn += QStringLiteral("Failed to parse start position.\n");
        return false;
    }

    CsaParseState st{line, board, board.currentPlayer(), warn};
    st.openingItem = KifuParseCommon::createOpeningDisplayItem(QString(), QString());

    for (qsizetype i = idx; i < lines.size(); ++i) {
        QString s = lines.at(i);
        if (s.isEmpty()) continue;
        s = s.trimmed();
        if (s.isEmpty()) continue;

        const QStringList tokens{s}; // splitStatements で展開済み
        for (const QString& tokenRaw : std::as_const(tokens)) {
            const QString token = tokenRaw.trimmed();
            if (token.isEmpty()) continue;

            if (token.startsWith(QLatin1Char('\''))) {
                handleCsaComment(token, st);
            } else if (CsaLexer::isTurnMarker(token)) {
                handleCsaTurnMarker(token, st);
            } else if (token.startsWith(QLatin1Char('%'))) {
                handleCsaResultCode(token, st);
            } else if (token.startsWith(QLatin1Char('T'))) {
                handleCsaTimeToken(token, st);
            } else if (CsaLexer::isMoveLine(token)) {
                const Turn mover = (token.at(0) == QLatin1Char('+')) ? Turn::Black : Turn::White;

                QString usi, pretty;
                if (!CsaLexer::parseMoveLine(token, mover, st.board, st.previousTo, usi, pretty, st.warn)) {
                    if (st.warn) *st.warn += QStringLiteral("Failed to parse move: %1\n").arg(token);
                    continue;
                }

                ensureOpeningItemAdded(st);

                st.line.usiMoves.append(usi);
                ++st.moveCount;
                st.line.disp.append(
                    KifuParseCommon::createMoveDisplayItem(st.moveCount, pretty));

                st.lastMover = (mover == Turn::Black) ? 0 : 1;
                st.lastDispIsResult = false;
                st.lastResultDispIndex = -1;
                st.lastResultSideIdx = -1;
                st.turn = (mover == Turn::Black) ? Turn::White : Turn::Black;
            }
        }
    }

    if (!st.firstMoveFound) {
        st.openingItem.comment = st.pendingComments.isEmpty()
            ? QString() : st.pendingComments.join(QStringLiteral("\n"));
        st.line.disp.append(st.openingItem);
        st.pendingComments.clear();
    }

    if (!st.pendingComments.isEmpty() && !st.line.disp.isEmpty()) {
        QString& dst = st.line.disp.last().comment;
        if (!dst.isEmpty()) dst += QLatin1Char('\n');
        dst += st.pendingComments.join(QStringLiteral("\n"));
    }

    return line.resolve(out.mainline, warn);
}
