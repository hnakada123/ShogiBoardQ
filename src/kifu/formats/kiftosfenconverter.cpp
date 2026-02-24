/// @file kiftosfenconverter.cpp
/// @brief KIF形式棋譜コンバータクラスの実装

#include "kiftosfenconverter.h"
#include "kifreader.h"
#include "board/sfenpositiontracer.h"
#include "parsecommon.h"
#include "notationutils.h"

#include <QRegularExpression>
#include "logcategories.h"
#include <QMap>

// 1桁（半角/全角）→ int。QString の range-for を使わず detach 回避。
static inline int flexDigitToInt_NoDetach(QChar c)
{
    return KifuParseCommon::flexDigitToIntNoDetach(c);
}

// 文字列に含まれる（半角/全角）数字を int へ。
// QString を range-for しない（detach 回避）。
static int flexDigitsToInt_NoDetach(const QString& t)
{
    return KifuParseCommon::flexDigitsToIntNoDetach(t);
}

namespace {

// KIFコメント行: 先頭が '*'（全角 '＊' も可）
static inline bool isCommentLine(const QString& s)
{
    if (s.isEmpty()) return false;
    const QChar ch = s.front();
    return (ch == QChar(u'*') || ch == QChar(u'＊'));
}

// KIFしおり行: 先頭が '&'
static inline bool isBookmarkLine(const QString& s)
{
    return s.startsWith(QLatin1Char('&'));
}

// 時間正規表現: ( m:ss / H+:MM:SS ) の半角/全角コロン・スラッシュを許容
static const QRegularExpression& kifTimeRe()
{
    static const auto& re = *[]() {
        static const QRegularExpression r(
            QStringLiteral("\\(\\s*([0-9０-９]{1,2})[:：]([0-9０-９]{2})(?:\\s*[／/]\\s*([0-9０-９]{1,3})[:：]([0-9０-９]{2})[:：]([0-9０-９]{2}))?\\s*\\)")
            );
        return &r;
    }();
    return re;
}

// 時間の正規化 "mm:ss/HH:MM:SS"
static inline QString normalizeTimeMatch(const QRegularExpressionMatch& m)
{
    // 既存の flexDigitsToInt_NoDetach_ を想定（全角→半角も許容）
    const int mm = flexDigitsToInt_NoDetach(m.captured(1));
    const int ss = flexDigitsToInt_NoDetach(m.captured(2));
    const bool hasCum = m.lastCapturedIndex() >= 5 && m.captured(3).size();
    const int HH = hasCum ? flexDigitsToInt_NoDetach(m.captured(3)) : 0;
    const int MM = hasCum ? flexDigitsToInt_NoDetach(m.captured(4)) : 0;
    const int SS = hasCum ? flexDigitsToInt_NoDetach(m.captured(5)) : 0;
    auto z2 = [](int x){ return QStringLiteral("%1").arg(x, 2, 10, QLatin1Char('0')); };
    return z2(mm) + QLatin1Char(':') + z2(ss) + QLatin1Char('/') +
           z2(HH) + QLatin1Char(':') + z2(MM) + QLatin1Char(':') + z2(SS);
}

// --- 共通正規表現（重複排除） ---

// 変化行検出（キャプチャなし）: "変化：12手" など
static const QRegularExpression& variationHeaderRe()
{
    static const auto& re = *[]() {
        static const QRegularExpression r(QStringLiteral("^\\s*変化[:：]\\s*[0-9０-９]+\\s*手"));
        return &r;
    }();
    return re;
}

// 変化行検出（キャプチャあり）: 手数をキャプチャする
static const QRegularExpression& variationHeaderCaptureRe()
{
    static const auto& re = *[]() {
        static const QRegularExpression r(QStringLiteral("^\\s*変化[:：]\\s*([0-9０-９]+)手"));
        return &r;
    }();
    return re;
}

// 次の手番検出: 空白＋数字の開始位置を検出
static const QRegularExpression& nextMoveNumberRe()
{
    static const auto& re = *[]() {
        static const QRegularExpression r(QStringLiteral("\\s+[0-9０-９]"));
        return &r;
    }();
    return re;
}

// --- 終局語の判定（KIF仕様に準拠） ---
// 該当すれば normalized に表記をそのまま返す（例: "千日手"）
static inline bool isTerminalWord(const QString& s, QString* normalized)
{
    return KifuParseCommon::isTerminalWordExact(s, normalized);
}

// --- 持駒SFEN生成 ---
// 持駒マップからSFEN形式の持駒文字列を生成
static QString buildHandsSfen(const QMap<Piece,int>& black, const QMap<Piece,int>& white)
{
    auto emitSide = [](const QMap<Piece,int>& m, bool gote)->QString{
        const Piece order[] = {Piece::BlackRook, Piece::BlackBishop, Piece::BlackGold,
                               Piece::BlackSilver, Piece::BlackKnight, Piece::BlackLance, Piece::BlackPawn};
        QString s;
        for (Piece p : order) {
            int cnt = m.value(p, 0);
            if (cnt <= 0) continue;
            if (cnt > 1) s += QString::number(cnt);
            s += pieceToChar(gote ? toWhite(p) : p);
        }
        return s;
    };
    QString s = emitSide(black, false) + emitSide(white, true);
    if (s.isEmpty()) s = QStringLiteral("-");
    return s;
}

// 時間正規表現マッチから累計時間部分（HH:MM:SS）を抽出
static inline QString extractCumTimeFromMatch(const QRegularExpressionMatch& tm)
{
    auto z2 = [](int x){ return QStringLiteral("%1").arg(x, 2, 10, QLatin1Char('0')); };
    const bool hasCum = tm.lastCapturedIndex() >= 5 && tm.captured(3).size();
    if (!hasCum) return QStringLiteral("00:00:00");
    const int HH = KifuParseCommon::flexDigitsToIntNoDetach(tm.captured(3));
    const int MM = KifuParseCommon::flexDigitsToIntNoDetach(tm.captured(4));
    const int SS = KifuParseCommon::flexDigitsToIntNoDetach(tm.captured(5));
    return z2(HH) + QLatin1Char(':') + z2(MM) + QLatin1Char(':') + z2(SS);
}

// rest から時間抽出し、timeText と nextMoveStartIdx を設定。rest は時間/次手を除いた部分に置換される。
static void stripTimeAndNextMove(QString& rest, const QString& lineStr, int skipOffset,
                                  QRegularExpressionMatch& tm, QString& timeText, int& nextMoveStartIdx)
{
    tm = kifTimeRe().match(rest);
    if (tm.hasMatch()) {
        timeText = normalizeTimeMatch(tm);
        nextMoveStartIdx = static_cast<int>(tm.capturedEnd(0));
        rest = rest.left(tm.capturedStart(0)).trimmed();
    } else {
        QRegularExpressionMatch nextM = nextMoveNumberRe().match(rest);
        if (nextM.hasMatch()) {
            nextMoveStartIdx = static_cast<int>(nextM.capturedStart());
            rest = rest.left(nextMoveStartIdx).trimmed();
        } else {
            nextMoveStartIdx = -1;
        }
    }
    Q_UNUSED(lineStr);
    Q_UNUSED(skipOffset);
}

// rest からインラインコメント（*／＊）を除去し、commentBuf に蓄積
static void stripInlineComment(QString& rest, QString& commentBuf)
{
    int commentIdx = static_cast<int>(rest.indexOf(QChar(u'*')));
    if (commentIdx < 0) commentIdx = static_cast<int>(rest.indexOf(QChar(u'＊')));
    if (commentIdx >= 0) {
        const QString inlineC = rest.mid(commentIdx + 1).trimmed();
        if (!inlineC.isEmpty()) {
            KifuParseCommon::appendLine(commentBuf, inlineC);
        }
        rest = rest.left(commentIdx).trimmed();
    }
}

// lineStr を次の指し手位置まで進める
static void advanceToNextMove(QString& lineStr, int skipOffset, const QRegularExpressionMatch& tm,
                               int nextMoveStartIdx)
{
    if (nextMoveStartIdx != -1) {
        if (tm.hasMatch()) {
            int consumed = skipOffset + static_cast<int>(tm.capturedEnd(0));
            lineStr = lineStr.mid(consumed).trimmed();
        } else {
            QRegularExpressionMatch nm = nextMoveNumberRe().match(lineStr, skipOffset);
            if (nm.hasMatch()) {
                lineStr = lineStr.mid(nm.capturedStart()).trimmed();
            } else {
                lineStr.clear();
            }
        }
    } else {
        lineStr.clear();
    }
}

// 終局語の表示アイテムを生成
static KifDisplayItem buildTerminalItem(int moveIndex, const QString& term,
                                         const QString& timeText, const QRegularExpressionMatch& tm)
{
    const QString teban = (moveIndex % 2 != 0) ? QStringLiteral("▲") : QStringLiteral("△");
    const QString cum = tm.hasMatch() ? extractCumTimeFromMatch(tm) : QStringLiteral("00:00:00");

    KifDisplayItem item;
    item.prettyMove = teban + term;
    item.ply = moveIndex;
    if (term == QStringLiteral("千日手")) {
        item.timeText = QStringLiteral("00:00/") + cum;
    } else {
        item.timeText = timeText.isEmpty() ? (QStringLiteral("00:00/") + cum) : timeText;
    }
    item.comment = QString();
    return item;
}

} // namespace

namespace {

// KIF/BOD 共通：駒の漢字/略字 → (Piece基底, 成りフラグ)
// 例: 「歩/と」「香/杏」「桂/圭」「銀/全」「角/馬」「飛/龍/竜」「玉/王」
static inline bool mapKanjiPiece(const QString& s, Piece& base, bool& promoted)
{
    return KifuParseCommon::mapKanjiPiece(s, base, promoted);
}

// BOD盤面解析用: 空白判定（全角スペースを含む）
static inline bool isSpaceLike(QChar ch) {
    return ch.isSpace() || ch == QChar(0x3000);
}

// BOD盤面解析用: 漢数字→int（〇/零→0対応、不明→-1）
// 注意: KifToSfenConverter::kanjiDigitToInt() とは別仕様
static inline int bodKanjiDigit(QChar c) {
    switch (c.unicode()) {
    case u'〇': case u'零': return 0;
    case u'一': return 1; case u'二': return 2; case u'三': return 3;
    case u'四': return 4; case u'五': return 5; case u'六': return 6;
    case u'七': return 7; case u'八': return 8; case u'九': return 9;
    default: return -1;
    }
}

// 漢数字文字列→int（「十六」→16 など）
static int parseKanjiNumberString(const QString& s) {
    if (s.isEmpty()) return -1;
    qsizetype idx = s.indexOf(QChar(u'十'));
    if (idx >= 0) {
        int tens = 1;
        if (idx > 0) {
            int d = bodKanjiDigit(s.at(0));
            if (d <= 0) return -1;
            tens = d;
        }
        int ones = 0;
        if (idx + 1 < s.size()) {
            int d = bodKanjiDigit(s.at(idx + 1));
            if (d < 0) return -1;
            ones = d;
        }
        return tens * 10 + ones;
    }
    if (s.size() == 1) {
        int d = bodKanjiDigit(s.at(0));
        return (d >= 0) ? d : -1;
    }
    return -1;
}

// 持駒トークンから個数を解析（「歩十八」→18, 「金」→1）
static int parseCountSuffixFlexible(const QString& token) {
    if (token.size() <= 1) return 1;
    QString rest = token.mid(1).trimmed();
    if (rest.isEmpty()) return 1;
    static const QRegularExpression reAscii(QStringLiteral("^(?:[×x*]?)(\\d+)$"));
    QRegularExpressionMatch m = reAscii.match(rest);
    if (m.hasMatch()) return m.captured(1).toInt();
    int n = parseKanjiNumberString(rest);
    return (n > 0) ? n : 1;
}

// 全角数字→半角ASCII変換
static QString zenk2ascii(QString s) {
    static const QString z = QStringLiteral("０１２３４５６７８９");
    for (qsizetype i = 0; i < s.size(); ++i) {
        qsizetype idx = z.indexOf(s.at(i));
        if (idx >= 0) s[i] = QChar('0' + static_cast<int>(idx));
    }
    return s;
}

// BOD枠付き行から内容とランク文字を抽出
// "| ... |一" → inner="...", rankKanji='一'
static bool extractFramedContent(const QString& line, QString& inner, QChar& rankKanji) {
    static const QRegularExpression rowRe(QStringLiteral("^\\s*\\|(.+)\\|\\s*([一二三四五六七八九])\\s*$"));
    QRegularExpressionMatch m = rowRe.match(line);
    if (!m.hasMatch()) return false;
    inner = m.captured(1);
    rankKanji = m.captured(2).at(0);
    return true;
}

// 盤面行の内容文字列から9マスの駒トークンを抽出
static QStringList tokenizeBodRowContent(const QString& inner) {
    static const QString promotedSingles = QStringLiteral("と杏圭全馬龍竜");
    static const QString baseSingles = QStringLiteral("歩香桂銀金角飛玉王") + promotedSingles;

    QStringList tokens;
    qsizetype i = 0, n = inner.size();
    while (i < n && tokens.size() < 9) {
        while (i < n && isSpaceLike(inner.at(i))) ++i;
        if (i >= n) break;

        QChar ch = inner.at(i);

        // 特殊空白・プレフィックス処理
        if (ch == QChar(0xFF65)) { tokens << QStringLiteral("・"); ++i; continue; } // ･
        if (ch == QChar(0xFF56)) { ch = QLatin1Char('v'); } // ｖ

        if (ch == QChar(0x30FB)) { // '・'
            tokens << QStringLiteral("・");
            ++i;
            continue;
        }

        // 後手駒 (v + 駒文字)
        if (ch == QLatin1Char('v')) {
            ++i;
            while (i < n && isSpaceLike(inner.at(i))) ++i;
            if (i < n && baseSingles.contains(inner.at(i))) {
                tokens << (QStringLiteral("v") + inner.at(i));
                ++i;
            } else {
                tokens << QStringLiteral("・");
            }
            continue;
        }

        // 先手駒 (単独文字)
        if (baseSingles.contains(ch)) {
            tokens << QString(ch);
            ++i;
            continue;
        }

        // どれにも当てはまらない文字は読み飛ばす
        ++i;
    }

    // 9マスに満たない場合は空白で埋める
    while (tokens.size() < 9) tokens << QStringLiteral("・");
    return tokens;
}

// 盤面行を解析して9マスの駒トークンとランク文字を返す
static bool parseBodRow(const QString& line, QStringList& outTokens, QChar& outRankKanji, bool hasFrame) {
    QString inner;
    if (hasFrame) {
        if (!extractFramedContent(line, inner, outRankKanji)) return false;
    } else {
        inner = line.trimmed();
        outRankKanji = QChar();
    }
    outTokens = tokenizeBodRowContent(inner);
    return true;
}

// 駒トークン列からSFEN行文字列を生成（空点を数字に圧縮）
static QString rowTokensToSfen(const QStringList& tokens) {
    QString row;
    int empty = 0;
    auto flushEmpty = [&](){
        if (empty > 0) { row += QString::number(empty); empty = 0; }
    };

    int used = 0;
    for (const QString& tok : std::as_const(tokens)) {
        if (used >= 9) break;

        if (tok == QStringLiteral("・")) {
            ++empty; ++used;
            continue;
        }

        const bool gote = tok.startsWith(QLatin1Char('v'));
        const QString body = gote ? tok.mid(1) : tok;

        Piece base; bool promoted = false;
        if (!mapKanjiPiece(body, base, promoted)) {
            ++empty; ++used;
            continue;
        }

        flushEmpty();
        if (promoted) row += QLatin1Char('+');
        row += pieceToChar(gote ? toWhite(base) : base);
        ++used;
    }
    while (used < 9) { ++empty; ++used; }
    flushEmpty();
    return row;
}

// BOD持駒行の解析
static void parseBodHandsLine(const QString& line, QMap<Piece,int>& outCounts, bool isBlack) {
    static const QString prefixB = QStringLiteral("先手の持駒");
    static const QString prefixW = QStringLiteral("後手の持駒");
    QString t = line.trimmed();
    if (!t.startsWith(prefixB) && !t.startsWith(prefixW)) return;

    const bool sideBlack = t.startsWith(prefixB);
    if (sideBlack != isBlack) return;

    qsizetype idx = t.indexOf(QChar(u'：')); if (idx < 0) idx = t.indexOf(QLatin1Char(':'));
    if (idx < 0) return;

    QString rhs = t.mid(idx+1).trimmed();
    if (rhs == QStringLiteral("なし")) return;

    rhs.replace(QChar(u'、'), QLatin1Char(' '));
    rhs.replace(QChar(0x3000), QLatin1Char(' '));
    static const QRegularExpression kWhitespaceRe(QStringLiteral("\\s+"));
    const QStringList toks = rhs.split(kWhitespaceRe, Qt::SkipEmptyParts);

    for (QString tok : toks) {
        tok = tok.trimmed();
        if (tok.isEmpty()) continue;

        QChar head = tok.at(0);
        Piece base; bool promoted = false;
        if (!mapKanjiPiece(QString(head), base, promoted)) continue;
        if (base == Piece::BlackKing) continue;

        int cnt = parseCountSuffixFlexible(tok);
        outCounts[base] += cnt;
    }
}

} // namespace

namespace {

// 行頭が手数（半/全角）か？（「手数＝」などのBODは除外）
bool startsWithMoveNumber(const QString& line, int* outDigits=nullptr) {
    int i = 0, digits = 0;
    while (i < line.size()) {
        const QChar ch = line.at(i);
        const ushort u = ch.unicode();
        const bool ascii = (u >= '0' && u <= '9');
        const bool zenk  = QStringLiteral("０１２３４５６７８９").contains(ch);
        if (ascii || zenk) { ++i; ++digits; }
        else break;
    }
    if (digits == 0) return false;
    // 直後が '手' '＝' なら BOD の「手数＝」類とみなす
    if (i < line.size()) {
        QChar next = line.at(i);
        if (next == QChar(u'手') || next == QChar(u'＝')) return false;
    }
    if (outDigits) *outDigits = digits;
    return true;
}
} // namespace

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
        if (startsWithMoveNumber(line)) break;
        if (line.contains(QStringLiteral("後手番"))) { moveIndex = 1; break; }
        if (line.startsWith(QStringLiteral("手合割")) || line.startsWith(QStringLiteral("手合"))) {
            if (!line.contains(QStringLiteral("平手"))) moveIndex = 1;
        }
    }

    for (const QString& raw : std::as_const(lines)) {
        QString lineStr = raw.trimmed();

        // 行頭コメント
        if (isCommentLine(lineStr)) {
            const QString c = lineStr.mid(1).trimmed();
            if (!c.isEmpty()) {
                KifuParseCommon::appendLine(firstMoveFound ? commentBuf : openingCommentBuf, c);
            }
            continue;
        }

        // しおり
        if (isBookmarkLine(lineStr)) {
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

        if (lineStr.isEmpty() || isSkippableLine(lineStr) || isBoardHeaderOrFrame(lineStr)) continue;
        if (variationHeaderRe().match(lineStr).hasMatch()) break;

        // --- 1行に含まれる指し手をループ処理 ---
        while (!lineStr.isEmpty()) {
            int digits = 0;
            if (!startsWithMoveNumber(lineStr, &digits)) break;
            int i = digits;
            while (i < lineStr.size() && lineStr.at(i).isSpace()) ++i;
            if (i >= lineStr.size()) break;

            QString rest = lineStr.mid(i).trimmed();
            QRegularExpressionMatch tm;
            QString timeText;
            int nextMoveStartIdx = -1;

            stripTimeAndNextMove(rest, lineStr, i, tm, timeText, nextMoveStartIdx);
            stripInlineComment(rest, commentBuf);
            advanceToNextMove(lineStr, i, tm, nextMoveStartIdx);

            // 最初の指し手が見つかった時に開始局面エントリを挿入
            if (!firstMoveFound) {
                firstMoveFound = true;
                out.push_back(KifuParseCommon::createOpeningDisplayItem(openingCommentBuf, openingBookmarkBuf));
            }

            // 終局語?
            QString term;
            if (isTerminalWord(rest, &term)) {
                KifuParseCommon::flushCommentToLastItem(commentBuf, out);
                ++moveIndex;
                out.push_back(buildTerminalItem(moveIndex, term, timeText, tm));
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

        if (isCommentLine(lineStr) || isBookmarkLine(lineStr)) continue;
        if (lineStr.isEmpty() || isSkippableLine(lineStr) || isBoardHeaderOrFrame(lineStr)) continue;
        if (variationHeaderRe().match(lineStr).hasMatch()) break;
        if (containsAnyTerminal(lineStr)) break;

        // --- 1行複数指し手ループ ---
        while (!lineStr.isEmpty()) {
            int digits = 0;
            if (!startsWithMoveNumber(lineStr, &digits)) break;
            int i = digits;
            while (i < lineStr.size() && lineStr.at(i).isSpace()) ++i;
            if (i >= lineStr.size()) break;

            QString rest = lineStr.mid(i).trimmed();
            QRegularExpressionMatch tm;
            QString timeText;
            int nextMoveStartIdx = -1;

            stripTimeAndNextMove(rest, lineStr, i, tm, timeText, nextMoveStartIdx);

            // インラインコメント除去（commentBuf不要なので簡略化）
            int cIdx = static_cast<int>(rest.indexOf(QChar(u'*')));
            if (cIdx < 0) cIdx = static_cast<int>(rest.indexOf(QChar(u'＊')));
            if (cIdx >= 0) rest = rest.left(cIdx).trimmed();

            // USI変換
            QString usi;
            if (convertMoveLine(rest, usi, prevToFile, prevToRank)) {
                out << usi;
            } else {
                if (errorMessage) *errorMessage += QStringLiteral("[skip ?] %1\n").arg(raw);
            }

            advanceToNextMove(lineStr, i, tm, nextMoveStartIdx);
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
        if (l.isEmpty() || isSkippableLine(l) || isBoardHeaderOrFrame(l)) { ++i; continue; }

        const QRegularExpressionMatch m = variationHeaderCaptureRe().match(l);
        if (!m.hasMatch()) { ++i; continue; }

        const int startPly = flexDigitsToInt_NoDetach(m.captured(1));

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
    // まず既存の分岐から探す（後方の分岐が前方の分岐から派生する可能性があるため）
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

        if (isCommentLine(lineStr)) {
            const QString c = lineStr.mid(1).trimmed();
            if (!c.isEmpty()) KifuParseCommon::appendLine(commentBuf, c);
            continue;
        }

        if (isBookmarkLine(lineStr)) {
            const QString name = lineStr.mid(1).trimmed();
            if (!name.isEmpty() && !line.disp.isEmpty()) {
                KifuParseCommon::appendLine(line.disp.last().bookmark, name);
            }
            continue;
        }

        if (lineStr.isEmpty() || isSkippableLine(lineStr) || isBoardHeaderOrFrame(lineStr)) continue;

        // 1行複数手ループ
        while (!lineStr.isEmpty()) {
            int digits = 0;
            if (!startsWithMoveNumber(lineStr, &digits)) break;
            int j = digits;
            while (j < lineStr.size() && lineStr.at(j).isSpace()) ++j;
            if (j >= lineStr.size()) break;

            QString rest = lineStr.mid(j).trimmed();
            QRegularExpressionMatch tm;
            QString timeText;
            int nextMoveStartIdx = -1;

            stripTimeAndNextMove(rest, lineStr, j, tm, timeText, nextMoveStartIdx);
            stripInlineComment(rest, commentBuf);

            // 終局語
            QString term;
            if (isTerminalWord(rest, &term)) {
                KifuParseCommon::flushCommentToLastItem(commentBuf, line.disp, 0);
                ++moveIndex;
                line.disp.push_back(buildTerminalItem(moveIndex, term, timeText, tm));
                lineStr.clear();
                break;
            }

            // 通常手: 直前の指し手にコメントを付与（変化の最初の指し手の前のコメントは無視）
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
            if (convertMoveLine(rest, usi, prevToFile, prevToRank)) {
                line.usiMoves << usi;
            }

            advanceToNextMove(lineStr, j, tm, nextMoveStartIdx);
        }
    }

    // 最後の指し手の後に残っているコメントを付与
    if (!commentBuf.isEmpty() && !line.disp.isEmpty()) {
        KifuParseCommon::appendLine(line.disp.last().comment, commentBuf);
    }
}

// ===== private helpers =====
bool KifToSfenConverter::isSkippableLine(const QString& line)
{
    return KifuParseCommon::isKifSkippableHeaderLine(line);
}

bool KifToSfenConverter::isBoardHeaderOrFrame(const QString& line)
{
    return KifuParseCommon::isBoardHeaderOrFrame(line);
}

bool KifToSfenConverter::containsAnyTerminal(const QString& s, QString* matched)
{
    return KifuParseCommon::containsAnyTerminal(s, matched);
}

bool KifToSfenConverter::findDestination(const QString& line, int& toFile, int& toRank, bool& isSameAsPrev)
{
    isSameAsPrev = false;

    if (line.contains(QStringLiteral("同"))) {
        toFile = toRank = 0;
        isSameAsPrev = true;
        return true;
    }

    static const auto& s_paren = *[]() {
        static const QRegularExpression r(QStringLiteral("[\\(（]"));
        return &r;
    }();
    const int paren = static_cast<int>(line.indexOf(s_paren));
    const QString head = (paren >= 0) ? line.left(paren) : line;

    static const auto& s_digitKanji = *[]() {
        static const QRegularExpression r(QStringLiteral("([1-9１-９])([一二三四五六七八九])"));
        return &r;
    }();
    static const auto& s_digitDigit = *[]() {
        static const QRegularExpression r(QStringLiteral("([1-9１-９])([1-9１-９])"));
        return &r;
    }();

    QRegularExpressionMatch m = s_digitKanji.match(head);
    if (!m.hasMatch()) m = s_digitDigit.match(head);
    if (!m.hasMatch()) return false;

    const QChar fch = m.capturedView(1).at(0);
    const QChar rch = m.capturedView(2).at(0);

    toFile = flexDigitToInt_NoDetach(fch);
    int r  = NotationUtils::kanjiDigitToInt(rch); // 漢数字を優先
    if (r == 0) r = flexDigitToInt_NoDetach(rch);     // 半/全角数字ならこちら
    toRank = r;

    return (toFile >= 1 && toFile <= 9 && toRank >= 1 && toRank <= 9);
}

QChar KifToSfenConverter::pieceKanjiToUsiUpper(const QString& s)
{
    Piece base = Piece::None; bool promoted = false;
    if (!mapKanjiPiece(s, base, promoted)) return QChar();
    // ドロップでは成り情報は不要。USIの基底文字（先手=大文字）を返す
    return pieceToChar(base);
}

bool KifToSfenConverter::isPromotionMoveText(const QString& line)
{
    // KIFの「成」は2パターン:
    //  (A) ○○成 … この指し手で成る（USI末尾に'+'が必要）
    //  (B) 成銀/成桂/成香/成歩/馬/龍/竜 … 既に成っている駒の種類名（'+'は不要）
    // ここでは (A) のみ true にする。
    if (line.contains(QStringLiteral("不成"))) return false;

    // 判定対象は「座標や(77)、打の手前」までのヘッドを取り出す
    QString head = line;
    static const auto& kParenRe = *[]() {
        static const QRegularExpression r(QStringLiteral("[\\(（]"));
        return &r;
    }();
    int p = static_cast<int>(head.indexOf(kParenRe));
    if (p >= 0) head = head.left(p);
    int u = static_cast<int>(head.indexOf(QChar(u'打')));
    if (u >= 0) head = head.left(u);

    // 装飾語を除去（右/左/上/引/寄/直/行）
    static const auto& kModsRe = *[]() {
        static const QRegularExpression r(QStringLiteral("[右左上下引寄直行]+"));
        return &r;
    }();
    head.remove(kModsRe);
    head.remove(QChar(u'▲'));
    head.remove(QChar(u'△'));
    head = head.trimmed();

    // 「歩/香/桂/銀/角/飛 + 成」で終わっていればプロモーション
    static const auto& kPromoteSuffix = *[]() {
        static const QRegularExpression r(QStringLiteral("(歩|香|桂|銀|角|飛)成$"));
        return &r;
    }();
    return kPromoteSuffix.match(head).hasMatch();
}

bool KifToSfenConverter::convertMoveLine(const QString& moveText,
                                         QString& usi,
                                         int& prevToFile, int& prevToRank)
{
    // パス対応
    if (moveText == QStringLiteral("パス")) {
        usi = QStringLiteral("pass");
        return true;
    }

    // 装飾語を除去
    static const auto& kMods = *[]() {
        static const QRegularExpression r(QStringLiteral("[右左上下引寄直行]+"));
        return &r;
    }();
    QString line = moveText;
    line.remove(kMods);
    line = line.trimmed();

    // 目的地
    int toF = 0, toR = 0; bool same = false;
    if (!findDestination(line, toF, toR, same)) return false;
    if (same) { toF = prevToFile; toR = prevToRank; }
    if (!(toF >= 1 && toF <= 9 && toR >= 1 && toR <= 9)) return false;

    // 打ち？
    const bool isDrop = line.contains(QStringLiteral("打"));

    // from は "(xy)"（半角/全角）から読む（なければ 0）
    int fromF = 0, fromR = 0;
    {
        static const auto& kParenAscii = *[]() {
            static const QRegularExpression r(QStringLiteral("\\(([0-9０-９])([0-9０-９])\\)"));
            return &r;
        }();
        static const auto& kParenZenkaku = *[]() {
            static const QRegularExpression r(QStringLiteral("（([0-9０-９])([0-9０-９])）"));
            return &r;
        }();
        QRegularExpressionMatch m = kParenAscii.match(line);
        if (!m.hasMatch()) m = kParenZenkaku.match(line);
        if (m.hasMatch() && m.lastCapturedIndex() >= 2) {
            const QChar a = m.capturedView(1).at(0);
            const QChar b = m.capturedView(2).at(0);

            fromF = flexDigitToInt_NoDetach(a);
            fromR = flexDigitToInt_NoDetach(b);
        }
    }

    if (isDrop) {
        // 駒種を拾う
        const QChar usiPiece = pieceKanjiToUsiUpper(line);
        if (usiPiece.isNull()) return false;
        usi = NotationUtils::formatSfenDrop(usiPiece, toF, toR);
    } else {
        if (!(fromF >= 1 && fromF <= 9 && fromR >= 1 && fromR <= 9)) return false;
        usi = NotationUtils::formatSfenMove(fromF, fromR, toF, toR, isPromotionMoveText(line));
    }

    // 「同」のために保存
    prevToFile = toF;
    prevToRank = toR;
    return true;
}

QList<KifGameInfoItem> KifToSfenConverter::extractGameInfo(const QString& filePath)
{
    QList<KifGameInfoItem> ordered;
    if (filePath.isEmpty()) return ordered;

    // 文字コード自動判別
    QString usedEnc, warn;
    QStringList lines;
    if (!KifReader::readLinesAuto(filePath, lines, &usedEnc, &warn)) {
        qCWarning(lcKifu).noquote() << "read failed:" << filePath << "warn:" << warn;
        return ordered;
    }
    qCDebug(lcKifu).noquote()
        << QStringLiteral("encoding = %1 , lines = %2").arg(usedEnc).arg(lines.size());

    // 行種別の判定用パターン
    static const QRegularExpression kHeaderLine(
        // 行頭の「key：value」または「key:value」：全角/半角コロン両対応
        QStringLiteral("^\\s*([^：:]+?)\\s*[：:]\\s*(.*?)\\s*$")
        );
    static const QRegularExpression kLineIsComment(
        // コメント行（#／＃／*／＊）
        QStringLiteral("^\\s*[#＃\\*\\＊]")
        );
    static const QRegularExpression kMovesHeader(
        // 「手数----指手---------消費時間--」の見出し
        QStringLiteral("^\\s*手数[-－ー]+指手[-－ー]+消費時間")
        );
    static const QRegularExpression kLineLooksLikeMoveNo(
        // 手数で始まる指し手行（半角/全角数字対応）
        QStringLiteral("^\\s*[0-9０-９]+\\s")
        );
    static const QRegularExpression kVariationHead(
        // 「変化：◯手」
        QStringLiteral("^\\s*変化\\s*[：:]\\s*[0-9０-９]+\\s*手")
        );

    // 盤面図(BOD)の持駒行は対局情報から除外
    auto isBodHeld = [](const QString& t) {
        return t.startsWith(QStringLiteral("先手の持駒")) ||
               t.startsWith(QStringLiteral("後手の持駒")) ||
               t.startsWith(QStringLiteral("先手の持ち駒")) ||
               t.startsWith(QStringLiteral("後手の持ち駒"));
    };

    for (const QString& rawLine : std::as_const(lines)) {
        const QString line = rawLine;            // detach 回避（参照）
        const QString t = line.trimmed();

        // 1) 明らかに対象外の行をスキップ
        if (t.isEmpty()) continue;
        if (kLineIsComment.match(t).hasMatch()) continue;
        if (kMovesHeader.match(t).hasMatch()) continue;
        if (kLineLooksLikeMoveNo.match(t).hasMatch()) break;   // 以降は指し手パート
        if (kVariationHead.match(t).hasMatch()) continue;
        if (isBodHeld(t)) continue;

        // 2) 見出しの key/value 抽出（全角/半角コロン両対応）
        QRegularExpressionMatch m = kHeaderLine.match(line);
        if (!m.hasMatch()) continue;

        // --- 正規化（関数内にヘルパ定義を置かない） ---
        QString key = m.captured(1).trimmed();
        // 末尾に「：」または「:」が残っていれば落とす（保険）
        if (key.endsWith(u'：') || key.endsWith(u':')) key.chop(1);
        key = key.trimmed();

        QString val = m.captured(2).trimmed();
        // 「\n」リテラルを実際の改行へ
        val.replace(QStringLiteral("\\n"), QStringLiteral("\n"));

        // 3) 出現順のまま格納（重複キーも別行として保持）
        ordered.push_back({ key, val });
    }

    return ordered;
}

QMap<QString, QString> KifToSfenConverter::extractGameInfoMap(const QString& filePath)
{
    // 注意: QMap はキー重複を保持しません（後勝ち）。
    // 複数値を保持したい場合は、シグネチャを QMultiMap<QString, QString> に変更してください。
    QMap<QString, QString> m;
    const auto items = extractGameInfo(filePath);
    for (const auto& it : items) m.insert(it.key, it.value); // 後勝ち
    return m;
}

// BOD行を収集して rowByRank マップに格納。配置：形式と通常BOD枠の両方に対応
static bool collectBodRows(const QStringList& lines, QMap<QChar, QString>& rowByRank, QString* detectedLabel)
{
    static const QChar ranks_1to9[9] = {u'一',u'二',u'三',u'四',u'五',u'六',u'七',u'八',u'九'};

    // モード判別：「配置：」ヘッダがあるか
    int placementStart = -1;
    for (qsizetype i = 0; i < lines.size(); ++i) {
        if (lines.at(i).contains(QStringLiteral("配置：")) || lines.at(i).contains(QStringLiteral("配置:"))) {
            placementStart = static_cast<int>(i);
            break;
        }
    }

    if (placementStart >= 0) {
        int currentRankIdx = 0;
        for (qsizetype i = placementStart + 1; i < lines.size() && currentRankIdx < 9; ++i) {
            const QString& raw = lines.at(i);
            if (raw.trimmed().isEmpty()) continue;
            QStringList toks; QChar dummyRank;
            if (parseBodRow(raw, toks, dummyRank, false)) {
                rowByRank[ranks_1to9[currentRankIdx]] = rowTokensToSfen(toks);
                currentRankIdx++;
            }
        }
        if (detectedLabel) *detectedLabel = QStringLiteral("配置");
    } else {
        for (const QString& raw : std::as_const(lines)) {
            QStringList toks; QChar rank;
            if (parseBodRow(raw, toks, rank, true)) {
                rowByRank[rank] = rowTokensToSfen(toks);
            }
        }
        if (detectedLabel && !rowByRank.isEmpty()) *detectedLabel = QStringLiteral("BOD");
    }

    return (rowByRank.size() == 9);
}

// BODから手番と手数を解析
static void parseBodTurnAndMoveNumber(const QStringList& lines, QChar& turn, int& moveNumber)
{
    turn = QLatin1Char('b');
    for (const QString& l : std::as_const(lines)) {
        const QString t = l.trimmed();
        if (t.contains(QStringLiteral("先手番"))) { turn = QLatin1Char('b'); break; }
        if (t.contains(QStringLiteral("後手番"))) { turn = QLatin1Char('w'); break; }
    }

    moveNumber = 1;
    static const QRegularExpression reTeSu(QStringLiteral("手数\\s*[=＝]\\s*([0-9０-９]+)"));
    for (const QString& l : std::as_const(lines)) {
        auto mt = reTeSu.match(l);
        if (mt.hasMatch()) {
            moveNumber = zenk2ascii(mt.captured(1)).toInt() + 1;
            break;
        }
    }
}

bool KifToSfenConverter::buildInitialSfenFromBod(const QStringList& lines,
                                                 QString& outSfen,
                                                 QString* detectedLabel,
                                                 QString* /*warn*/)
{
    static const QChar ranks_1to9[9] = {u'一',u'二',u'三',u'四',u'五',u'六',u'七',u'八',u'九'};

    // 1) 盤面情報の収集
    QMap<QChar, QString> rowByRank;
    if (!collectBodRows(lines, rowByRank, detectedLabel)) return false;

    // 2) 持駒
    QMap<Piece,int> handB, handW;
    for (const QString& line : std::as_const(lines)) {
        parseBodHandsLine(line, handB, true);
        parseBodHandsLine(line, handW, false);
    }

    // 3) 手番・手数
    QChar turn;
    int moveNumber;
    parseBodTurnAndMoveNumber(lines, turn, moveNumber);

    // 4) SFEN組み立て
    QStringList boardRows;
    for (QChar rk : ranks_1to9) {
        boardRows << rowByRank.value(rk);
    }

    const QString board = boardRows.join(QLatin1Char('/'));
    const QString hands = buildHandsSfen(handB, handW);
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

    // 「最初の指し手」までを走査。BOD・ヘッダ類は既存ヘルパでスキップ。
    for (const QString& raw : std::as_const(lines)) {
        const QString t = raw.trimmed();
        if (t.isEmpty()) continue;
        if (isSkippableLine(t) || isBoardHeaderOrFrame(t)) continue;

        // 指し手の開始なら終了
        if (startsWithMoveNumber(t)) break;

        // 変化の開始もここでは境界とみなす
        if (variationHeaderRe().match(t).hasMatch()) break;

        // コメント行（開始局面コメント）
        if (t.startsWith(QChar(u'*')) || t.startsWith(QChar(u'＊'))) {
            const QString c = t.mid(1).trimmed();
            if (!c.isEmpty()) {
                if (!buf.isEmpty()) buf += QLatin1Char('\n');
                buf += c;
            }
            continue;
        }

        // しおり（開始局面に対するしおり）- コメントには含めずスキップ
        if (t.startsWith(QLatin1Char('&'))) {
            continue;
        }
    }
    return buf;
}
