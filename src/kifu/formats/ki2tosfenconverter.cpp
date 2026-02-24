/// @file ki2tosfenconverter.cpp
/// @brief KI2形式棋譜コンバータクラスの実装

#include "ki2tosfenconverter.h"
#include "kiftosfenconverter.h"
#include "kifreader.h"
#include "parsecommon.h"
#include "notationutils.h"

#include <QRegularExpression>
#include "logcategories.h"
#include <QMap>

// ============================================================================
// 静的ヘルパ関数（無名名前空間）
// ============================================================================

namespace {

// 1桁（半角/全角）→ int
static inline int flexDigitToInt_NoDetach(QChar c)
{
    return KifuParseCommon::flexDigitToIntNoDetach(c);
}

// 文字列に含まれる（半角/全角）数字を int へ
static int flexDigitsToInt_NoDetach(const QString& t)
{
    return KifuParseCommon::flexDigitsToIntNoDetach(t);
}

// --- 終局語の統一リスト（KI2仕様に準拠 + 表記ゆらぎ吸収） ---
// isTerminalWord と containsAnyTerminal で共通使用
static const auto& kTerminalWords() {
    return KifuParseCommon::terminalWords();
}

// --- 終局語の判定 ---
static inline bool isTerminalWord(const QString& s, QString* normalized)
{
    return KifuParseCommon::isTerminalWordContains(s, normalized);
}

// --- 共通正規表現（重複排除） ---

// 結果行検出（キャプチャあり）: "まで○手で..." の手数をキャプチャ
static const QRegularExpression& resultPatternCaptureRe()
{
    static const auto& re = *[]() {
        static const QRegularExpression r(QStringLiteral("^\\s*まで([0-9０-９]+)手"));
        return &r;
    }();
    return re;
}

// 結果行検出（キャプチャなし）: "まで○手で..." の判定のみ
static const QRegularExpression& resultPatternRe()
{
    static const auto& re = *[]() {
        static const QRegularExpression r(QStringLiteral("^\\s*まで[0-9０-９]+手"));
        return &r;
    }();
    return re;
}

// コロン以降を削除: "棋戦：タイトル" → "タイトル"
static const QRegularExpression& afterColonRe()
{
    static const auto& re = *[]() {
        static const QRegularExpression r(QStringLiteral("^.*[:：]"));
        return &r;
    }();
    return re;
}

// --- 「まで○手で...」行を解析して終局語と手番を取得 ---
// 例: "まで2手で後手の勝ち" → terminalWord="投了", moveCount=2, blackWon=false
// 例: "まで96手で千日手" → terminalWord="千日手", moveCount=96
// 例: "まで2手で中断" → terminalWord="中断", moveCount=2
// 例: "まで2手で詰" → terminalWord="詰み", moveCount=2
// blackToMove: 終局語の手番を決める（奇数手目なら先手番）
static bool parseResultLine(const QString& line, QString& terminalWord, int& moveCount)
{
    const QRegularExpressionMatch m = resultPatternCaptureRe().match(line);
    if (!m.hasMatch()) return false;
    
    // 手数を抽出
    moveCount = flexDigitsToInt_NoDetach(m.captured(1));
    
    // 終局語を判定
    // 「○手で後手の勝ち」→ 投了
    // 「○手で先手の勝ち」→ 投了
    // 「○手で千日手」→ 千日手
    // 「○手で中断」→ 中断
    // 「○手で詰」または「○手で詰み」→ 詰み
    // 「○手で持将棋」→ 持将棋
    
    if (line.contains(QStringLiteral("千日手"))) {
        terminalWord = QStringLiteral("千日手");
        return true;
    }
    if (line.contains(QStringLiteral("持将棋"))) {
        terminalWord = QStringLiteral("持将棋");
        return true;
    }
    if (line.contains(QStringLiteral("中断"))) {
        terminalWord = QStringLiteral("中断");
        return true;
    }
    if (line.contains(QStringLiteral("切れ負け"))) {
        terminalWord = QStringLiteral("切れ負け");
        return true;
    }
    if (line.contains(QStringLiteral("反則勝ち"))) {
        terminalWord = QStringLiteral("反則勝ち");
        return true;
    }
    if (line.contains(QStringLiteral("反則負け"))) {
        terminalWord = QStringLiteral("反則負け");
        return true;
    }
    if (line.contains(QStringLiteral("入玉勝ち"))) {
        terminalWord = QStringLiteral("入玉勝ち");
        return true;
    }
    if (line.contains(QStringLiteral("不戦勝"))) {
        terminalWord = QStringLiteral("不戦勝");
        return true;
    }
    if (line.contains(QStringLiteral("不戦敗"))) {
        terminalWord = QStringLiteral("不戦敗");
        return true;
    }
    if (line.contains(QStringLiteral("詰み")) || line.contains(QStringLiteral("詰"))) {
        terminalWord = QStringLiteral("詰み");
        return true;
    }
    if (line.contains(QStringLiteral("不詰"))) {
        terminalWord = QStringLiteral("不詰");
        return true;
    }
    // 「○手で後手の勝ち」「○手で先手の勝ち」→ 投了
    if (line.contains(QStringLiteral("勝ち")) || line.contains(QStringLiteral("勝"))) {
        terminalWord = QStringLiteral("投了");
        return true;
    }
    
    return false;
}

// --- 終局行かどうか判定 ---
static inline bool isResultLine(const QString& line)
{
    return resultPatternRe().match(line).hasMatch();
}

// 駒の漢字 → (USI基底駒, 成りフラグ)
static inline bool mapKanjiPiece(const QString& s, Piece& base, bool& promoted)
{
    return KifuParseCommon::mapKanjiPiece(s, base, promoted);
}

} // anonymous namespace

// ============================================================================
// Ki2ToSfenConverter 静的メンバ関数の実装
// ============================================================================

// ----------------------------------------------------------------------------
// isCommentLine / isBookmarkLine
// ----------------------------------------------------------------------------

bool Ki2ToSfenConverter::isCommentLine(const QString& s)
{
    if (s.isEmpty()) return false;
    const QChar ch = s.front();
    return (ch == QChar(u'*') || ch == QChar(u'＊'));
}

bool Ki2ToSfenConverter::isBookmarkLine(const QString& s)
{
    return s.startsWith(QLatin1Char('&'));
}

// ----------------------------------------------------------------------------
// isKi2MoveLine - KI2の指し手行判定
// ----------------------------------------------------------------------------

bool Ki2ToSfenConverter::isKi2MoveLine(const QString& line)
{
    const QString t = line.trimmed();
    if (t.isEmpty()) return false;
    const QChar first = t.at(0);
    return (first == QChar(u'▲') || first == QChar(u'△') || first == QChar(u'▽') ||
            first == QChar(u'☗') || first == QChar(u'☖'));
}

// ----------------------------------------------------------------------------
// mapHandicapToSfen
// ----------------------------------------------------------------------------

QString Ki2ToSfenConverter::mapHandicapToSfen(const QString& label)
{
    return NotationUtils::mapHandicapToSfen(label);
}

// ----------------------------------------------------------------------------
// isSkippableLine
// ----------------------------------------------------------------------------

bool Ki2ToSfenConverter::isSkippableLine(const QString& line)
{
    if (line.isEmpty()) return true;
    if (line.startsWith(QLatin1Char('#'))) return true;

    static const QStringList keys = {
        QStringLiteral("開始日時"), QStringLiteral("終了日時"), QStringLiteral("対局日"),
        QStringLiteral("棋戦"), QStringLiteral("戦型"), QStringLiteral("持ち時間"),
        QStringLiteral("秒読み"), QStringLiteral("消費時間"), QStringLiteral("場所"),
        QStringLiteral("表題"),   QStringLiteral("掲載"),   QStringLiteral("記録係"),
        QStringLiteral("備考"),   QStringLiteral("図"),     QStringLiteral("振り駒"),
        QStringLiteral("先手省略名"), QStringLiteral("後手省略名"),
        QStringLiteral("先手："), QStringLiteral("後手："),
        QStringLiteral("先手番"), QStringLiteral("後手番"),
        QStringLiteral("手合割"), QStringLiteral("手合"),
        QStringLiteral("手数＝")
    };
    for (const auto& k : keys) {
        if (line.contains(k)) return true;
    }

    if (line.startsWith(QLatin1Char('&'))) return true;

    // 「まで○手で...」は終局行として処理するのでスキップしない
    // （この判定を削除）

    return false;
}

// ----------------------------------------------------------------------------
// isBoardHeaderOrFrame
// ----------------------------------------------------------------------------

bool Ki2ToSfenConverter::isBoardHeaderOrFrame(const QString& line)
{
    if (line.trimmed().isEmpty()) return false;

    static const QString kDigitsZ   = QStringLiteral("１２３４５６７８９");
    static const QString kKanjiRow  = QStringLiteral("一二三四五六七八九");
    // 公式BOD形式はASCII罫線(+, -, |)を使用するが、
    // ウェブ等からコピーされた盤面がUnicode罫線を含む場合にも対応
    static const QString kBoxChars  = QStringLiteral("┌┬┐┏┳┓└┴┘┗┻┛│┃─━┼");

    // 先頭「９ ８ ７ … １」
    {
        int digitCount = 0;
        bool onlyDigitsAndSpace = true;
        for (qsizetype i = 0; i < line.size(); ++i) {
            const QChar ch = line.at(i);
            if (ch.isSpace()) continue;
            const ushort u = ch.unicode();
            const bool ascii19 = (u >= QChar(u'1').unicode() && u <= QChar(u'9').unicode());
            const bool zenk19  = kDigitsZ.contains(ch);
            if (ascii19 || zenk19) { ++digitCount; continue; }
            onlyDigitsAndSpace = false; break;
        }
        if (onlyDigitsAndSpace && digitCount >= 5) return true;
    }

    // 罫線
    {
        const QString s = line.trimmed();
        if ((s.startsWith(QLatin1Char('+')) && s.endsWith(QLatin1Char('+'))) ||
            (s.startsWith(QChar(u'|')) && s.endsWith(QChar(u'|')))) {
            return true;
        }
        int boxCount = 0;
        for (qsizetype i = 0; i < s.size(); ++i) if (kBoxChars.contains(s.at(i))) ++boxCount;
        if (boxCount >= qMax(3, static_cast<int>(s.size()) / 2)) return true;
    }

    // 持駒見出し
    if (line.contains(QStringLiteral("先手の持駒")) ||
        line.contains(QStringLiteral("後手の持駒")))
        return true;

    // 下部見出し（"一二三…九"）
    {
        const QString t = line.trimmed();
        if (!t.isEmpty()) {
            bool ok = true; int kanjiCount = 0;
            for (qsizetype i = 0; i < t.size(); ++i) {
                const QChar ch = t.at(i);
                if (kKanjiRow.contains(ch)) { ++kanjiCount; continue; }
                if (ch.isSpace()) continue;
                ok = false; break;
            }
            if (ok && kanjiCount >= 5) return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------------------
// containsAnyTerminal
// ----------------------------------------------------------------------------

bool Ki2ToSfenConverter::containsAnyTerminal(const QString& s, QString* matched)
{
    // kTerminalWords() を使用（終局語リストの統一）
    for (const QString& w : kTerminalWords()) {
        if (s.contains(w)) { if (matched) *matched = w; return true; }
    }
    return false;
}

// ----------------------------------------------------------------------------
// findDestination - 移動先座標を取得
// ----------------------------------------------------------------------------

bool Ki2ToSfenConverter::findDestination(const QString& moveText, int& toFile, int& toRank, bool& isSameAsPrev)
{
    isSameAsPrev = false;

    if (moveText.contains(QStringLiteral("同"))) {
        toFile = toRank = 0;
        isSameAsPrev = true;
        return true;
    }

    // ▲/△を除去
    QString line = moveText;
    line.remove(QChar(u'▲'));
    line.remove(QChar(u'△'));
    line.remove(QChar(u'▽'));
    line.remove(QChar(u'☗'));
    line.remove(QChar(u'☖'));

    static const QRegularExpression s_digitKanji(QStringLiteral("([1-9１-９])([一二三四五六七八九])"));
    static const QRegularExpression s_digitDigit(QStringLiteral("([1-9１-９])([1-9１-９])"));

    QRegularExpressionMatch m = s_digitKanji.match(line);
    if (!m.hasMatch()) m = s_digitDigit.match(line);
    if (!m.hasMatch()) return false;

    const QChar fch = m.capturedView(1).at(0);
    const QChar rch = m.capturedView(2).at(0);

    toFile = flexDigitToInt_NoDetach(fch);
    int r  = NotationUtils::kanjiDigitToInt(rch);
    if (r == 0) r = flexDigitToInt_NoDetach(rch);
    toRank = r;

    return (toFile >= 1 && toFile <= 9 && toRank >= 1 && toRank <= 9);
}

// ----------------------------------------------------------------------------
// pieceKanjiToUsiUpper - 駒種（漢字）→ USI基底文字
// ----------------------------------------------------------------------------

Piece Ki2ToSfenConverter::pieceKanjiToUsiUpper(const QString& s)
{
    Piece base = Piece::None; bool promoted = false;
    if (!mapKanjiPiece(s, base, promoted)) return Piece::None;
    return base;
}

// 駒種と成りフラグを取得するヘルパ
static bool getPieceTypeAndPromoted(const QString& moveText, Piece& pieceUpper, bool& isPromoted)
{
    isPromoted = false;
    pieceUpper = Piece::None;

    Piece base = Piece::None;
    if (!mapKanjiPiece(moveText, base, isPromoted)) return false;
    pieceUpper = base;
    return true;
}

// ----------------------------------------------------------------------------
// isPromotionMoveText - 成り判定
// ----------------------------------------------------------------------------

bool Ki2ToSfenConverter::isPromotionMoveText(const QString& moveText)
{
    if (moveText.contains(QStringLiteral("不成"))) return false;

    QString head = moveText;
    qsizetype u = head.indexOf(QChar(u'打'));
    if (u >= 0) head = head.left(u);

    static const QRegularExpression kDirectionRe(QStringLiteral("[右左上下引寄直行]+"));
    head.remove(kDirectionRe);
    head.remove(QChar(u'▲'));
    head.remove(QChar(u'△'));
    head.remove(QChar(u'▽'));
    head.remove(QChar(u'☗'));
    head.remove(QChar(u'☖'));
    head = head.trimmed();

    static const QRegularExpression kPromoteSuffix(QStringLiteral("(歩|香|桂|銀|角|飛)成$"));
    return kPromoteSuffix.match(head).hasMatch();
}

// ----------------------------------------------------------------------------
// extractMoveModifier - 移動修飾語を抽出
// ----------------------------------------------------------------------------

QString Ki2ToSfenConverter::extractMoveModifier(const QString& moveText)
{
    QString result;
    static const QStringList modifiers = {
        QStringLiteral("右"),
        QStringLiteral("左"),
        QStringLiteral("上"),
        QStringLiteral("引"),
        QStringLiteral("寄"),
        QStringLiteral("直"),
        QStringLiteral("行")
    };
    for (const QString& mod : modifiers) {
        if (moveText.contains(mod)) {
            result += mod;
        }
    }
    return result;
}

// ----------------------------------------------------------------------------
// extractMovesFromLine - KI2の1行から指し手を抽出
// ----------------------------------------------------------------------------

QStringList Ki2ToSfenConverter::extractMovesFromLine(const QString& line)
{
    QStringList moves;
    const QString t = line.trimmed();
    
    // ▲または△で区切って指し手を抽出
    static const QRegularExpression moveRe(QStringLiteral("([▲△▽☗☖][^▲△▽☗☖]+)"));
    QRegularExpressionMatchIterator it = moveRe.globalMatch(t);
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        QString move = m.captured(1).trimmed();
        // コメント部分を除去
        qsizetype commentIdx = move.indexOf(QChar(u'*'));
        if (commentIdx < 0) commentIdx = move.indexOf(QChar(u'＊'));
        if (commentIdx >= 0) {
            move = move.left(commentIdx).trimmed();
        }
        if (!move.isEmpty()) {
            moves << move;
        }
    }
    return moves;
}

// ----------------------------------------------------------------------------
// parseToken - 盤面トークンを解析
// ----------------------------------------------------------------------------

bool Ki2ToSfenConverter::parseToken(const QString& token, Piece& pieceUpper, bool& isPromoted, bool& isBlack)
{
    if (token.isEmpty()) return false;

    isPromoted = (token.startsWith(QLatin1Char('+')));
    const QChar ch = isPromoted ? token.at(1) : token.at(0);
    isBlack = ch.isUpper();
    pieceUpper = toBlack(charToPiece(ch));
    return true;
}

// ----------------------------------------------------------------------------
// initBoardFromSfen - SFENから盤面を初期化
// ----------------------------------------------------------------------------

void Ki2ToSfenConverter::initBoardFromSfen(const QString& sfen,
                                            QString boardState[9][9],
                                            QMap<Piece, int>& blackHands,
                                            QMap<Piece, int>& whiteHands)
{
    // 盤面クリア
    for (int r = 0; r < 9; ++r) {
        for (int f = 0; f < 9; ++f) {
            boardState[r][f].clear();
        }
    }
    blackHands.clear();
    whiteHands.clear();

    const QStringList parts = sfen.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.isEmpty()) return;

    // 盤面部分（第1フィールド）
    const QString board = parts[0];
    const QStringList ranks = board.split(QLatin1Char('/'));
    
    for (qsizetype r = 0; r < qMin(qsizetype(9), ranks.size()); ++r) {
        const QString& row = ranks[r];
        int f = 0; // file index (0 = 9筋, 8 = 1筋)
        for (qsizetype i = 0; i < row.size() && f < 9; ++i) {
            const QChar ch = row.at(i);
            if (ch.isDigit()) {
                int n = ch.digitValue();
                while (n-- > 0 && f < 9) { boardState[r][f++].clear(); }
            } else if (ch == QLatin1Char('+')) {
                if (i + 1 < row.size()) {
                    const QChar p = row.at(++i);
                    boardState[r][f++] = QString(QLatin1Char('+')) + p;
                }
            } else {
                boardState[r][f++] = QString(ch);
            }
        }
    }

    // 持ち駒（第3フィールド）
    if (parts.size() >= 3) {
        const QString hands = parts[2];
        if (hands != QStringLiteral("-")) {
            int num = 0;
            for (qsizetype i = 0; i < hands.size(); ++i) {
                const QChar ch = hands.at(i);
                if (ch.isDigit()) {
                    num = num * 10 + ch.digitValue();
                } else {
                    const bool black = ch.isUpper();
                    const int n = (num > 0) ? num : 1;
                    num = 0;
                    const Piece basePiece = toBlack(charToPiece(ch));
                    if (black) {
                        blackHands[basePiece] += n;
                    } else {
                        whiteHands[basePiece] += n;
                    }
                }
            }
        }
    }
}

// ----------------------------------------------------------------------------
// applyMoveToBoard - 盤面に指し手を適用
// ----------------------------------------------------------------------------

void Ki2ToSfenConverter::applyMoveToBoard(const QString& usi,
                                           QString boardState[9][9],
                                           QMap<Piece, int>& blackHands,
                                           QMap<Piece, int>& whiteHands,
                                           bool blackToMove)
{
    if (usi.isEmpty()) return;

    // 駒打ち: "P*5e"
    if (usi.contains(QLatin1Char('*'))) {
        const qsizetype star = usi.indexOf('*');
        if (star != 1 || usi.size() < 4) return;

        const Piece up = toBlack(charToPiece(usi.at(0)));
        const int file = usi.at(2).toLatin1() - '0'; // 1-9
        const int rankIdx = usi.at(3).toLatin1() - 'a'; // 0-8

        if (file < 1 || file > 9 || rankIdx < 0 || rankIdx > 8) return;

        const int colIdx = 9 - file; // 9筋が col 0

        // 持ち駒から減らす
        QMap<Piece, int>& hands = blackToMove ? blackHands : whiteHands;
        if (hands.value(up, 0) > 0) {
            hands[up]--;
        }

        // 盤に置く
        const QChar pieceChar = pieceToChar(blackToMove ? up : toWhite(up));
        boardState[rankIdx][colIdx] = QString(pieceChar);
        return;
    }

    // 通常手: "7g7f" or "2b3c+"
    if (usi.size() < 4) return;

    const int fileFrom = usi.at(0).toLatin1() - '0';
    const int rankFromIdx = usi.at(1).toLatin1() - 'a';
    const int fileTo = usi.at(2).toLatin1() - '0';
    const int rankToIdx = usi.at(3).toLatin1() - 'a';
    const bool promote = (usi.size() >= 5 && usi.at(4) == QLatin1Char('+'));

    if (fileFrom < 1 || fileFrom > 9 || rankFromIdx < 0 || rankFromIdx > 8) return;
    if (fileTo < 1 || fileTo > 9 || rankToIdx < 0 || rankToIdx > 8) return;

    const int colFrom = 9 - fileFrom;
    const int colTo = 9 - fileTo;

    // 移動元の駒を取得
    QString fromToken = boardState[rankFromIdx][colFrom];
    if (fromToken.isEmpty()) return;

    // 移動先に駒があれば持ち駒に
    const QString toToken = boardState[rankToIdx][colTo];
    if (!toToken.isEmpty()) {
        Piece capPiece = Piece::None;
        bool capPromoted, capBlack;
        if (parseToken(toToken, capPiece, capPromoted, capBlack)) {
            // 成り駒は元の駒に戻す
            QMap<Piece, int>& hands = blackToMove ? blackHands : whiteHands;
            hands[capPiece]++;
        }
    }

    // 駒を移動
    Piece movingPiece = Piece::None;
    bool wasPromoted, isBlack;
    if (parseToken(fromToken, movingPiece, wasPromoted, isBlack)) {
        const bool nowPromoted = promote || wasPromoted;
        const QChar outChar = pieceToChar(blackToMove ? movingPiece : toWhite(movingPiece));
        if (nowPromoted) {
            boardState[rankToIdx][colTo] = QString(QLatin1Char('+')) + outChar;
        } else {
            boardState[rankToIdx][colTo] = QString(outChar);
        }
    }

    boardState[rankFromIdx][colFrom].clear();
}

// ----------------------------------------------------------------------------
// canPieceMoveTo - 駒が移動可能かチェック
// ----------------------------------------------------------------------------

bool Ki2ToSfenConverter::canPieceMoveTo(Piece pieceUpper, bool isPromoted,
                                         int fromFile, int fromRank,
                                         int toFile, int toRank,
                                         bool blackToMove)
{
    const int df = toFile - fromFile;
    const int dr = toRank - fromRank;

    // 先手は上（段が小さくなる方向）へ、後手は下へ進む
    const int forward = blackToMove ? -1 : 1;

    switch (pieceUpper) {
    case Piece::BlackPawn: // 歩
        if (isPromoted) {
            // と金（金と同じ動き）
            if (qAbs(df) <= 1 && qAbs(dr) <= 1) {
                if (df == 0 && dr == -forward) return false; // 真後ろは不可
                if (qAbs(df) == 1 && dr == -forward) return false; // 斜め後ろは不可
                return true;
            }
            return false;
        }
        return (df == 0 && dr == forward);
        
    case Piece::BlackLance: // 香
        if (isPromoted) {
            // 成香（金と同じ動き）
            if (qAbs(df) <= 1 && qAbs(dr) <= 1) {
                if (df == 0 && dr == -forward) return false;
                if (qAbs(df) == 1 && dr == -forward) return false;
                return true;
            }
            return false;
        }
        return (df == 0 && dr * forward > 0); // 前方に直進
        
    case Piece::BlackKnight: // 桂
        if (isPromoted) {
            // 成桂（金と同じ動き）
            if (qAbs(df) <= 1 && qAbs(dr) <= 1) {
                if (df == 0 && dr == -forward) return false;
                if (qAbs(df) == 1 && dr == -forward) return false;
                return true;
            }
            return false;
        }
        return (qAbs(df) == 1 && dr == 2 * forward);
        
    case Piece::BlackSilver: // 銀
        if (isPromoted) {
            // 成銀（金と同じ動き）
            if (qAbs(df) <= 1 && qAbs(dr) <= 1) {
                if (df == 0 && dr == -forward) return false;
                if (qAbs(df) == 1 && dr == -forward) return false;
                return true;
            }
            return false;
        }
        // 銀の動き：前3方向と斜め後ろ2方向
        if (qAbs(df) <= 1 && qAbs(dr) <= 1) {
            if (df == 0 && dr == -forward) return false; // 真後ろは不可
            if (df == 0 && dr == 0) return false; // その場は不可
            if (qAbs(df) == 1 && dr == 0) return false; // 横は不可
            return true;
        }
        return false;
        
    case Piece::BlackGold: // 金
        if (qAbs(df) <= 1 && qAbs(dr) <= 1) {
            if (df == 0 && dr == -forward) return false; // 真後ろは不可
            if (qAbs(df) == 1 && dr == -forward) return false; // 斜め後ろは不可
            return true;
        }
        return false;
        
    case Piece::BlackBishop: // 角
        if (isPromoted) {
            // 馬（角の動き + 周囲1マス）
            if (qAbs(df) == qAbs(dr) && df != 0) return true;
            if (qAbs(df) <= 1 && qAbs(dr) <= 1 && (df != 0 || dr != 0)) return true;
            return false;
        }
        return (qAbs(df) == qAbs(dr) && df != 0);
        
    case Piece::BlackRook: // 飛
        if (isPromoted) {
            // 龍（飛車の動き + 斜め1マス）
            if ((df == 0 && dr != 0) || (df != 0 && dr == 0)) return true;
            if (qAbs(df) <= 1 && qAbs(dr) <= 1 && qAbs(df) == qAbs(dr) && df != 0) return true;
            return false;
        }
        return ((df == 0 && dr != 0) || (df != 0 && dr == 0));
        
    case Piece::BlackKing: // 玉
        return (qAbs(df) <= 1 && qAbs(dr) <= 1 && (df != 0 || dr != 0));
        
    default:
        return false;
    }
}

// ----------------------------------------------------------------------------
// isPathClear - 飛び駒の経路に他の駒がないか確認
// ----------------------------------------------------------------------------

static bool isPathClear(int fromFile, int fromRank, int toFile, int toRank,
                        const QString boardState[9][9])
{
    const int df = (toFile > fromFile) ? 1 : (toFile < fromFile) ? -1 : 0;
    const int dr = (toRank > fromRank) ? 1 : (toRank < fromRank) ? -1 : 0;
    
    int f = fromFile + df;
    int r = fromRank + dr;
    
    while (f != toFile || r != toRank) {
        // 棋譜座標から配列インデックスへ変換
        const int col = 9 - f;  // 9筋がcol 0
        const int row = r - 1;  // 1段がrow 0
        
        if (col < 0 || col > 8 || row < 0 || row > 8) break;
        
        if (!boardState[row][col].isEmpty()) {
            return false; // 途中に駒がある
        }
        
        f += df;
        r += dr;
    }
    
    return true;
}

// ----------------------------------------------------------------------------
// collectCandidates - 移動先に到達可能な候補駒を盤面から収集
// ----------------------------------------------------------------------------

QVector<Ki2ToSfenConverter::Candidate> Ki2ToSfenConverter::collectCandidates(
    Piece pieceUpper, bool moveIsPromoted,
    int toFile, int toRank,
    bool blackToMove,
    const QString boardState[9][9])
{
    QVector<Candidate> candidates;

    for (int r = 0; r < 9; ++r) {
        for (int f = 0; f < 9; ++f) {
            const QString& token = boardState[r][f];
            if (token.isEmpty()) continue;

            Piece tokenPiece = Piece::None;
            bool tokenPromoted, tokenBlack;
            if (!parseToken(token, tokenPiece, tokenPromoted, tokenBlack)) continue;
            if (tokenBlack != blackToMove) continue;
            if (tokenPiece != pieceUpper) continue;
            // KI2では「角」は未成りの角のみ、「馬」は成った角のみにマッチ
            if (tokenPromoted != moveIsPromoted) continue;

            const int candFile = 9 - f;  // col 0 = 9筋
            const int candRank = r + 1;  // row 0 = 1段

            if (!canPieceMoveTo(pieceUpper, tokenPromoted, candFile, candRank,
                                toFile, toRank, blackToMove))
                continue;

            // 飛び駒（飛車、角、香車）の経路確認
            if (pieceUpper == Piece::BlackRook || pieceUpper == Piece::BlackBishop ||
                pieceUpper == Piece::BlackLance) {
                if (!isPathClear(candFile, candRank, toFile, toRank, boardState))
                    continue;
            }

            candidates.push_back({candFile, candRank});
        }
    }

    return candidates;
}

// ----------------------------------------------------------------------------
// filterByDirection - 右/左/上/引/寄/直 の修飾語で候補を絞り込む
// ----------------------------------------------------------------------------

QVector<Ki2ToSfenConverter::Candidate> Ki2ToSfenConverter::filterByDirection(
    const QVector<Candidate>& candidates,
    const QString& modifier,
    bool blackToMove,
    int toFile, int toRank)
{
    if (modifier.isEmpty()) return candidates;

    const int forward = blackToMove ? -1 : 1;
    QVector<Candidate> filtered = candidates;

    // 「右」: 先手→筋が小さい側、後手→筋が大きい側
    if (modifier.contains(QChar(u'右'))) {
        int targetFile = blackToMove ? 9 : 0;
        for (const auto& c : std::as_const(filtered)) {
            if (blackToMove) {
                if (targetFile == 9 || c.file < targetFile) targetFile = c.file;
            } else {
                if (targetFile == 0 || c.file > targetFile) targetFile = c.file;
            }
        }
        QVector<Candidate> tmp;
        for (const auto& c : std::as_const(filtered)) {
            if (c.file == targetFile) tmp.push_back(c);
        }
        if (!tmp.isEmpty()) filtered = tmp;
    }

    // 「左」: 先手→筋が大きい側、後手→筋が小さい側
    if (modifier.contains(QChar(u'左'))) {
        int targetFile = blackToMove ? 0 : 9;
        for (const auto& c : std::as_const(filtered)) {
            if (blackToMove) {
                if (targetFile == 0 || c.file > targetFile) targetFile = c.file;
            } else {
                if (targetFile == 9 || c.file < targetFile) targetFile = c.file;
            }
        }
        QVector<Candidate> tmp;
        for (const auto& c : std::as_const(filtered)) {
            if (c.file == targetFile) tmp.push_back(c);
        }
        if (!tmp.isEmpty()) filtered = tmp;
    }

    // 「上」「行」: 前進（移動元が移動先より後方）
    if (modifier.contains(QChar(u'上')) || modifier.contains(QChar(u'行'))) {
        QVector<Candidate> tmp;
        for (const auto& c : std::as_const(filtered)) {
            if ((toRank - c.rank) * forward > 0) tmp.push_back(c);
        }
        if (!tmp.isEmpty()) filtered = tmp;
    }

    // 「引」: 後退（移動元が移動先より前方）
    if (modifier.contains(QChar(u'引'))) {
        QVector<Candidate> tmp;
        for (const auto& c : std::as_const(filtered)) {
            if ((toRank - c.rank) * forward < 0) tmp.push_back(c);
        }
        if (!tmp.isEmpty()) filtered = tmp;
    }

    // 「寄」: 横移動（同じ段からの移動）
    if (modifier.contains(QChar(u'寄'))) {
        QVector<Candidate> tmp;
        for (const auto& c : std::as_const(filtered)) {
            if (c.rank == toRank) tmp.push_back(c);
        }
        if (!tmp.isEmpty()) filtered = tmp;
    }

    // 「直」: 真っ直ぐ（同じ筋からの前進）
    if (modifier.contains(QChar(u'直'))) {
        QVector<Candidate> tmp;
        for (const auto& c : std::as_const(filtered)) {
            if (c.file == toFile) tmp.push_back(c);
        }
        if (!tmp.isEmpty()) filtered = tmp;
    }

    return filtered;
}

// ----------------------------------------------------------------------------
// generateModifier - 候補駒から修飾子を生成（KI2書き出し用）
// ----------------------------------------------------------------------------

QString Ki2ToSfenConverter::generateModifier(
    const QVector<Candidate>& candidates,
    int srcFile, int srcRank,
    int dstFile, int dstRank,
    bool blackToMove)
{
    if (candidates.size() <= 1) return QString();

    const int forward = blackToMove ? -1 : 1;

    // 単一修飾子の候補を列挙
    QStringList singleModifiers;

    // 右/左: 筋が異なる候補がある場合
    {
        bool hasDifferentFile = false;
        for (const auto& c : std::as_const(candidates)) {
            if (c.file != srcFile || c.rank != srcRank) {
                if (c.file != srcFile) {
                    hasDifferentFile = true;
                    break;
                }
            }
        }
        if (hasDifferentFile) {
            singleModifiers << QStringLiteral("右") << QStringLiteral("左");
        }
    }

    // 上/引/寄: 移動方向に基づく
    const int dr = dstRank - srcRank;
    if (dr * forward > 0) {
        // 前進
        singleModifiers << QStringLiteral("上");
        // 同筋なら「直」も候補
        if (srcFile == dstFile) {
            singleModifiers << QStringLiteral("直");
        }
    } else if (dr * forward < 0) {
        // 後退
        singleModifiers << QStringLiteral("引");
    } else {
        // 横移動
        singleModifiers << QStringLiteral("寄");
    }

    // 各単一修飾子で filterByDirection を呼び、一意に特定できるか試す
    for (const QString& mod : std::as_const(singleModifiers)) {
        const auto filtered = filterByDirection(candidates, mod, blackToMove, dstFile, dstRank);
        if (filtered.size() == 1 && filtered[0].file == srcFile && filtered[0].rank == srcRank) {
            return mod;
        }
    }

    // 単一で不十分な場合、組合せを試す
    static const QStringList combos = {
        QStringLiteral("右上"), QStringLiteral("右引"),
        QStringLiteral("左上"), QStringLiteral("左引"),
        QStringLiteral("右寄"), QStringLiteral("左寄"),
    };

    for (const QString& combo : combos) {
        const auto filtered = filterByDirection(candidates, combo, blackToMove, dstFile, dstRank);
        if (filtered.size() == 1 && filtered[0].file == srcFile && filtered[0].rank == srcRank) {
            return combo;
        }
    }

    // フォールバック: 修飾子なし
    return QString();
}

// ----------------------------------------------------------------------------
// convertPrettyMoveToKi2 - KIF形式の指し手をKI2形式に変換
// ----------------------------------------------------------------------------

QString Ki2ToSfenConverter::convertPrettyMoveToKi2(
    const QString& prettyMove,
    QString boardState[9][9],
    QMap<Piece, int>& blackHands,
    QMap<Piece, int>& whiteHands,
    bool blackToMove,
    int& prevToFile, int& prevToRank)
{
    // 1. 移動元 (FF) を正規表現で抽出
    static const QRegularExpression fromPosRe(QStringLiteral("\\(([0-9])([0-9])\\)$"));
    const QRegularExpressionMatch fromMatch = fromPosRe.match(prettyMove);

    int srcFile = 0, srcRank = 0;
    bool hasSource = false;
    if (fromMatch.hasMatch()) {
        srcFile = fromMatch.captured(1).at(0).toLatin1() - '0';
        srcRank = fromMatch.captured(2).at(0).toLatin1() - '0';
        hasSource = true;
    }

    // 2. 移動先座標を取得
    int dstFile = 0, dstRank = 0;
    bool isSame = false;
    if (!findDestination(prettyMove, dstFile, dstRank, isSame)) {
        // 解析できない場合はそのまま返す（(xx)だけ除去）
        QString result = prettyMove;
        result.remove(fromPosRe);
        return result;
    }
    if (isSame) {
        dstFile = prevToFile;
        dstRank = prevToRank;
    }

    // 3. 駒種を取得
    Piece pieceUpper = Piece::None;
    bool isPromoted = false;
    if (!getPieceTypeAndPromoted(prettyMove, pieceUpper, isPromoted)) {
        QString result = prettyMove;
        result.remove(fromPosRe);
        return result;
    }

    // 4. ki2Moveテキストを構築（移動元除去）
    QString ki2Move = prettyMove;
    ki2Move.remove(fromPosRe);

    // 5. 「打」の場合は修飾子不要
    bool isDrop = prettyMove.contains(QChar(u'打'));
    if (!isDrop && !hasSource) {
        // 移動元がない場合も打ちの可能性がある（盤上に移動可能な駒がない場合）
        isDrop = true;
    }

    if (!isDrop && hasSource) {
        // 6. collectCandidates で同種駒の候補を収集
        const auto candidates = collectCandidates(pieceUpper, isPromoted,
                                                   dstFile, dstRank,
                                                   blackToMove, boardState);

        // 7. 候補が2以上なら修飾子を生成
        if (candidates.size() >= 2) {
            const QString modifier = generateModifier(candidates, srcFile, srcRank,
                                                       dstFile, dstRank, blackToMove);
            if (!modifier.isEmpty()) {
                // 修飾子の挿入位置: 駒名の後、成/不成の前
                // ki2Move は "▲５二金" or "△５二金成" or "▲同　金" 等
                // 成/不成の直前に挿入する
                static const QRegularExpression promoteSuffixRe(QStringLiteral("(成|不成)$"));
                const QRegularExpressionMatch pm = promoteSuffixRe.match(ki2Move);
                if (pm.hasMatch()) {
                    // 成/不成がある場合: その前に挿入
                    const qsizetype pos = pm.capturedStart(1);
                    ki2Move.insert(pos, modifier);
                } else {
                    // 成/不成がない場合: 末尾に追加
                    ki2Move.append(modifier);
                }
            }
        }
    }

    // 8. USI文字列を構築して盤面を更新
    if (isDrop) {
        // 駒打ち
        const QString usi = NotationUtils::formatSfenDrop(pieceToChar(pieceUpper), dstFile, dstRank);
        applyMoveToBoard(usi, boardState, blackHands, whiteHands, blackToMove);
    } else if (hasSource) {
        // 盤上の移動
        const QString usi = NotationUtils::formatSfenMove(srcFile, srcRank, dstFile, dstRank, isPromotionMoveText(prettyMove));
        applyMoveToBoard(usi, boardState, blackHands, whiteHands, blackToMove);
    }

    // 9. prevToFile/prevToRank を更新
    prevToFile = dstFile;
    prevToRank = dstRank;

    return ki2Move;
}

// ----------------------------------------------------------------------------
// inferSourceSquare - 移動元座標を推測
// ----------------------------------------------------------------------------

bool Ki2ToSfenConverter::inferSourceSquare(Piece pieceUpper,
                                            bool moveIsPromoted,
                                            int toFile, int toRank,
                                            const QString& modifier,
                                            bool blackToMove,
                                            const QString boardState[9][9],
                                            int& outFromFile, int& outFromRank)
{
    const auto candidates = collectCandidates(pieceUpper, moveIsPromoted,
                                               toFile, toRank,
                                               blackToMove, boardState);
    if (candidates.isEmpty()) return false;

    if (candidates.size() == 1) {
        outFromFile = candidates[0].file;
        outFromRank = candidates[0].rank;
        return true;
    }

    // 複数候補 → 修飾語で絞り込む
    const auto filtered = filterByDirection(candidates, modifier,
                                             blackToMove, toFile, toRank);

    // filterByDirection は空を返さない（フィルタが効かない場合は元の候補を維持）
    outFromFile = filtered[0].file;
    outFromRank = filtered[0].rank;
    return true;
}

// ----------------------------------------------------------------------------
// convertKi2MoveToUsi - KI2形式の指し手をUSI形式に変換
// ----------------------------------------------------------------------------

QString Ki2ToSfenConverter::convertKi2MoveToUsi(const QString& moveText,
                                                 QString boardState[9][9],
                                                 QMap<Piece, int>& blackHands,
                                                 QMap<Piece, int>& whiteHands,
                                                 bool blackToMove,
                                                 int& prevToFile, int& prevToRank)
{
    // パス対応
    if (moveText.contains(QStringLiteral("パス"))) {
        return QStringLiteral("pass");
    }
    
    // 終局語判定
    QString term;
    if (isTerminalWord(moveText, &term)) {
        return QString(); // 終局は空文字列を返す
    }
    
    // 移動先座標を取得
    int toFile = 0, toRank = 0;
    bool isSame = false;
    if (!findDestination(moveText, toFile, toRank, isSame)) {
        return QString();
    }
    
    if (isSame) {
        toFile = prevToFile;
        toRank = prevToRank;
    }
    
    if (toFile < 1 || toFile > 9 || toRank < 1 || toRank > 9) {
        return QString();
    }
    
    // 駒種と成り状態を取得
    // 例：「角」→ pieceUpper=BlackBishop, moveIsPromoted=false
    //     「馬」→ pieceUpper=BlackBishop, moveIsPromoted=true
    Piece pieceUpper = Piece::None;
    bool moveIsPromoted = false;
    if (!getPieceTypeAndPromoted(moveText, pieceUpper, moveIsPromoted)) {
        return QString();
    }

    // 明示的な「打」があるかどうか
    bool explicitDrop = moveText.contains(QChar(u'打'));

    // 盤上に移動可能な駒があるか確認
    // moveIsPromotedを渡して、成駒と未成駒を区別する
    const QString modifier = extractMoveModifier(moveText);
    int fromFile = 0, fromRank = 0;
    bool canMoveFromBoard = inferSourceSquare(pieceUpper, moveIsPromoted, toFile, toRank, modifier, blackToMove, boardState, fromFile, fromRank);

    // 持ち駒にあるか確認（成駒は持ち駒にならない）
    QMap<Piece, int>& hands = blackToMove ? blackHands : whiteHands;
    bool hasInHand = !moveIsPromoted && (hands.value(pieceUpper, 0) > 0);

    // 打ちかどうかの判定
    // 1. 明示的に「打」がある場合
    // 2. 盤上に移動可能な駒がなく、持ち駒にある場合（成駒は打てない）
    bool isDrop = explicitDrop;

    if (!isDrop && !canMoveFromBoard && hasInHand) {
        // 盤上に移動可能な駒がないが持ち駒にある → 打ち
        isDrop = true;
    }

    QString usi;

    if (isDrop) {
        // 駒打ち
        if (!hasInHand) {
            qCWarning(lcKifu) << "No piece in hand for drop:" << pieceToChar(pieceUpper) << "move:" << moveText;
            return QString();
        }

        usi = NotationUtils::formatSfenDrop(pieceToChar(pieceUpper), toFile, toRank);
    } else {
        // 盤上の移動
        if (!canMoveFromBoard) {
            qCWarning(lcKifu) << "Cannot infer source square for:" << moveText;
            return QString();
        }

        usi = NotationUtils::formatSfenMove(fromFile, fromRank, toFile, toRank, isPromotionMoveText(moveText));
    }
    
    // 「同」のために保存
    prevToFile = toFile;
    prevToRank = toRank;
    
    return usi;
}

// ----------------------------------------------------------------------------
// detectInitialSfenFromFile - 初期SFENを検出
// ----------------------------------------------------------------------------

QString Ki2ToSfenConverter::detectInitialSfenFromFile(const QString& ki2Path, QString* detectedLabel)
{
    QString usedEnc, warn;
    QStringList lines;
    if (!KifReader::readLinesAuto(ki2Path, lines, &usedEnc, &warn)) {
        if (detectedLabel) *detectedLabel = QStringLiteral("平手(既定)");
        return NotationUtils::mapHandicapToSfen(QStringLiteral("平手"));
    }

    // BODを試す
    QString bodSfen;
    if (buildInitialSfenFromBod(lines, bodSfen, detectedLabel, &warn)) {
        qCDebug(lcKifu).noquote() << "BOD detected. sfen =" << bodSfen;
        return bodSfen;
    }

    // 「手合割」ベース
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

    QString label = found;
    label.remove(afterColonRe());
    label = label.trimmed();
    if (detectedLabel) *detectedLabel = label;
    return NotationUtils::mapHandicapToSfen(label);
}

// ----------------------------------------------------------------------------
// convertFile - USI手列を抽出
// ----------------------------------------------------------------------------

QStringList Ki2ToSfenConverter::convertFile(const QString& ki2Path, QString* errorMessage)
{
    QStringList out;

    QString usedEnc;
    QStringList lines;
    if (!KifReader::readLinesAuto(ki2Path, lines, &usedEnc, errorMessage)) {
        return out;
    }

    qCDebug(lcKifu).noquote() << QStringLiteral("convertFile: encoding = %1 , lines = %2")
                                    .arg(usedEnc).arg(lines.size());

    // 初期SFENを取得
    const QString initialSfen = detectInitialSfenFromFile(ki2Path);

    // 盤面を初期化
    QString boardState[9][9];
    QMap<Piece, int> blackHands, whiteHands;
    initBoardFromSfen(initialSfen, boardState, blackHands, whiteHands);

    // 手番を判定（SFENの第2フィールド）
    bool blackToMove = true;
    {
        const QStringList parts = initialSfen.split(QLatin1Char(' '));
        if (parts.size() >= 2) {
            blackToMove = (parts[1] == QStringLiteral("b"));
        }
    }

    int prevToFile = 0, prevToRank = 0;
    bool gameEnded = false;

    for (const QString& raw : std::as_const(lines)) {
        const QString lineStr = raw.trimmed();
        
        if (gameEnded) break;

        if (isCommentLine(lineStr) || isBookmarkLine(lineStr)) continue;
        
        // 「まで○手で...」行で終了
        if (isResultLine(lineStr)) {
            break;
        }
        
        if (lineStr.isEmpty() || isSkippableLine(lineStr) || isBoardHeaderOrFrame(lineStr)) continue;
        if (!isKi2MoveLine(lineStr)) continue;

        // 終局判定
        if (containsAnyTerminal(lineStr)) break;

        // 1行から複数の指し手を抽出
        const QStringList moves = extractMovesFromLine(lineStr);

        for (const QString& move : moves) {
            // 終局語判定
            QString term;
            if (isTerminalWord(move, &term)) {
                gameEnded = true;
                break;
            }

            const QString usi = convertKi2MoveToUsi(move, boardState, blackHands, whiteHands,
                                                     blackToMove, prevToFile, prevToRank);
            if (!usi.isEmpty()) {
                out << usi;
                qCDebug(lcKifu).noquote() << QStringLiteral("USI [%1] %2").arg(out.size()).arg(usi);

                // 盤面を更新
                applyMoveToBoard(usi, boardState, blackHands, whiteHands, blackToMove);
                blackToMove = !blackToMove;
            } else {
                if (errorMessage) *errorMessage += QStringLiteral("[skip ?] %1\n").arg(move);
            }
        }
    }

    qCDebug(lcKifu).noquote() << QStringLiteral("convertFile: moves = %1").arg(out.size());
    return out;
}

// ----------------------------------------------------------------------------
// extractMovesWithTimes - 指し手と時間を抽出
// ----------------------------------------------------------------------------

QList<KifDisplayItem> Ki2ToSfenConverter::extractMovesWithTimes(const QString& ki2Path,
                                                                 QString* errorMessage)
{
    QList<KifDisplayItem> out;

    QString usedEnc;
    QStringList lines;
    if (!KifReader::readLinesAuto(ki2Path, lines, &usedEnc, errorMessage)) {
        return out;
    }

    // 初期SFENを取得
    const QString initialSfen = detectInitialSfenFromFile(ki2Path);

    // 盤面を初期化
    QString boardState[9][9];
    QMap<Piece, int> blackHands, whiteHands;
    initBoardFromSfen(initialSfen, boardState, blackHands, whiteHands);

    // 手番を判定
    bool blackToMove = true;
    {
        const QStringList parts = initialSfen.split(QLatin1Char(' '));
        if (parts.size() >= 2) {
            blackToMove = (parts[1] == QStringLiteral("b"));
        }
    }

    QString openingCommentBuf;  // 開始局面用コメント
    QString openingBookmarkBuf; // 開始局面用しおり
    QString commentBuf;        // 指し手後のコメント
    int moveIndex = blackToMove ? 0 : 1;
    int prevToFile = 0, prevToRank = 0;
    bool gameEnded = false;
    bool firstMoveFound = false;

    for (const QString& raw : std::as_const(lines)) {
        const QString lineStr = raw.trimmed();
        
        if (gameEnded) break;

        // コメント行
        if (isCommentLine(lineStr)) {
            QString c = lineStr.mid(1).trimmed();
            if (!c.isEmpty()) {
                if (!firstMoveFound) {
                    // 最初の指し手の前のコメント → 開始局面用
                    if (!openingCommentBuf.isEmpty()) openingCommentBuf += QLatin1Char('\n');
                    openingCommentBuf += c;
                } else {
                    // 指し手後のコメント → 直前の指し手用
                    if (!commentBuf.isEmpty()) commentBuf += QLatin1Char('\n');
                    commentBuf += c;
                }
            }
            continue;
        }

        // しおり行 → bookmark フィールドに格納
        if (isBookmarkLine(lineStr)) {
            const QString name = lineStr.mid(1).trimmed();
            if (!name.isEmpty()) {
                if (firstMoveFound && out.size() > 1) {
                    QString& dst = out.last().bookmark;
                    if (!dst.isEmpty()) dst += QLatin1Char('\n');
                    dst += name;
                } else {
                    if (!openingBookmarkBuf.isEmpty()) openingBookmarkBuf += QLatin1Char('\n');
                    openingBookmarkBuf += name;
                }
            }
            continue;
        }

        // 「まで○手で...」行の処理
        if (isResultLine(lineStr)) {
            QString terminalWord;
            int resultMoveCount = 0;
            if (parseResultLine(lineStr, terminalWord, resultMoveCount)) {
                // 最初の指し手が見つかる前に終局語が来た場合
                if (!firstMoveFound) {
                    firstMoveFound = true;
                    KifDisplayItem openingItem;
                    openingItem.prettyMove = QString();
                    openingItem.timeText   = QStringLiteral("00:00/00:00:00");
                    openingItem.comment    = openingCommentBuf;
                    openingItem.bookmark   = openingBookmarkBuf;
                    openingItem.ply        = 0;
                    out.push_back(openingItem);
                }

                // 直前の指し手にコメントを付与
                if (!commentBuf.isEmpty() && out.size() > 1) {
                    QString& dst = out.last().comment;
                    if (!dst.isEmpty()) dst += QLatin1Char('\n');
                    dst += commentBuf;
                    commentBuf.clear();
                }

                ++moveIndex;
                const QString teban = (moveIndex % 2 != 0) ? QStringLiteral("▲") : QStringLiteral("△");

                KifDisplayItem item;
                item.prettyMove = teban + terminalWord;
                item.timeText = QStringLiteral("00:00/00:00:00");
                item.comment = QString();
                item.ply = moveIndex;

                out.push_back(item);
                gameEnded = true;
            }
            continue;
        }

        if (lineStr.isEmpty() || isSkippableLine(lineStr) || isBoardHeaderOrFrame(lineStr)) continue;
        if (!isKi2MoveLine(lineStr)) continue;

        // 1行から複数の指し手を抽出
        const QStringList moves = extractMovesFromLine(lineStr);

        for (const QString& move : moves) {
            // 終局語判定
            QString term;
            if (isTerminalWord(move, &term)) {
                // 最初の指し手が見つかる前に終局語が来た場合
                if (!firstMoveFound) {
                    firstMoveFound = true;
                    KifDisplayItem openingItem;
                    openingItem.prettyMove = QString();
                    openingItem.timeText   = QStringLiteral("00:00/00:00:00");
                    openingItem.comment    = openingCommentBuf;
                    openingItem.bookmark   = openingBookmarkBuf;
                    openingItem.ply        = 0;
                    out.push_back(openingItem);
                }

                // 直前の指し手にコメントを付与
                if (!commentBuf.isEmpty() && out.size() > 1) {
                    QString& dst = out.last().comment;
                    if (!dst.isEmpty()) dst += QLatin1Char('\n');
                    dst += commentBuf;
                    commentBuf.clear();
                }

                ++moveIndex;
                const QString teban = (moveIndex % 2 != 0) ? QStringLiteral("▲") : QStringLiteral("△");

                KifDisplayItem item;
                item.prettyMove = teban + term;
                item.timeText = QStringLiteral("00:00/00:00:00");
                item.comment = QString();
                item.ply = moveIndex;

                out.push_back(item);
                gameEnded = true;
                break;
            }

            // 最初の指し手が見つかった時に開始局面エントリを挿入
            if (!firstMoveFound) {
                firstMoveFound = true;
                KifDisplayItem openingItem;
                openingItem.prettyMove = QString();
                openingItem.timeText   = QStringLiteral("00:00/00:00:00");
                openingItem.comment    = openingCommentBuf;
                openingItem.bookmark   = openingBookmarkBuf;
                openingItem.ply        = 0;
                out.push_back(openingItem);
            }

            // 直前の指し手にコメントを付与
            if (!commentBuf.isEmpty() && out.size() > 1) {
                QString& dst = out.last().comment;
                if (!dst.isEmpty()) dst += QLatin1Char('\n');
                dst += commentBuf;
                commentBuf.clear();
            }

            // USI変換して移動元を取得
            const QString usi = convertKi2MoveToUsi(move, boardState, blackHands, whiteHands,
                                                     blackToMove, prevToFile, prevToRank);

            // 通常手
            ++moveIndex;
            const QString teban = (moveIndex % 2 != 0) ? QStringLiteral("▲") : QStringLiteral("△");

            // prettyMoveを構築（移動元座標を追加）
            QString prettyMove;
            if (!usi.isEmpty()) {
                // 移動先と駒種を取得
                int toFile = 0, toRank = 0;
                bool isSame = false;
                findDestination(move, toFile, toRank, isSame);
                
                // ▲/△を除いた指し手部分
                QString moveBody = move;
                moveBody.remove(QChar(u'▲'));
                moveBody.remove(QChar(u'△'));
                moveBody.remove(QChar(u'▽'));
                moveBody.remove(QChar(u'☗'));
                moveBody.remove(QChar(u'☖'));
                moveBody = moveBody.trimmed();
                
                if (usi.contains(QLatin1Char('*'))) {
                    // 駒打ち：「打」がなければ追加
                    if (!moveBody.contains(QChar(u'打'))) {
                        // 駒名の後に「打」を挿入
                        prettyMove = teban + moveBody + QStringLiteral("打");
                    } else {
                        prettyMove = teban + moveBody;
                    }
                } else {
                    // 盤上の移動：移動元座標を追加
                    // USIから移動元を取得（例: "7g7f" → from=7g）
                    if (usi.size() >= 4) {
                        const int fromFile = usi.at(0).toLatin1() - '0';
                        const int fromRank = usi.at(1).toLatin1() - 'a' + 1;
                        prettyMove = teban + moveBody + QStringLiteral("(%1%2)").arg(fromFile).arg(fromRank);
                    } else {
                        prettyMove = teban + moveBody;
                    }
                }
                
                // 盤面を更新
                applyMoveToBoard(usi, boardState, blackHands, whiteHands, blackToMove);
            } else {
                // USI変換失敗時はそのまま
                prettyMove = move;
                if (!prettyMove.startsWith(QChar(u'▲')) && !prettyMove.startsWith(QChar(u'△')) &&
                    !prettyMove.startsWith(QChar(u'▽')) &&
                    !prettyMove.startsWith(QChar(u'☗')) && !prettyMove.startsWith(QChar(u'☖'))) {
                    prettyMove = teban + prettyMove;
                }
            }

            KifDisplayItem item;
            item.prettyMove = prettyMove;
            item.timeText = QStringLiteral("00:00/00:00:00"); // KI2には時間情報がない
            item.comment = QString();  // コメントは次の指し手で付与される
            item.ply = moveIndex;

            out.push_back(item);
            
            // 手番を交代
            blackToMove = !blackToMove;
        }
    }

    // 最後の指し手の後に残っているコメントを付与
    if (!commentBuf.isEmpty() && !out.isEmpty()) {
        QString& dst = out.last().comment;
        if (!dst.isEmpty()) dst += QLatin1Char('\n');
        dst += commentBuf;
    }

    // 指し手が一つもなかった場合でも開始局面エントリを追加
    if (out.isEmpty()) {
        KifDisplayItem openingItem;
        openingItem.prettyMove = QString();
        openingItem.timeText   = QStringLiteral("00:00/00:00:00");
        openingItem.comment    = openingCommentBuf;
        openingItem.bookmark   = openingBookmarkBuf;
        openingItem.ply        = 0;
        out.push_back(openingItem);
    }

    return out;
}

// ----------------------------------------------------------------------------
// parseWithVariations - 本譜と変化を解析
// ----------------------------------------------------------------------------

bool Ki2ToSfenConverter::parseWithVariations(const QString& ki2Path,
                                              KifParseResult& out,
                                              QString* errorMessage)
{
    out = KifParseResult{};

    // 本譜
    QString teai;
    out.mainline.baseSfen = detectInitialSfenFromFile(ki2Path, &teai);
    out.mainline.disp = extractMovesWithTimes(ki2Path, errorMessage);
    out.mainline.usiMoves = convertFile(ki2Path, errorMessage);

    // KI2形式では変化の記載がないため、variations は空のまま
    // （変化が必要な場合は KIF 形式を使用）

    return true;
}

// ----------------------------------------------------------------------------
// extractGameInfo - 対局情報を抽出
// ----------------------------------------------------------------------------

QList<KifGameInfoItem> Ki2ToSfenConverter::extractGameInfo(const QString& filePath)
{
    QList<KifGameInfoItem> ordered;
    if (filePath.isEmpty()) return ordered;

    QString usedEnc, warn;
    QStringList lines;
    if (!KifReader::readLinesAuto(filePath, lines, &usedEnc, &warn)) {
        qCWarning(lcKifu).noquote() << "read failed:" << filePath << "warn:" << warn;
        return ordered;
    }

    static const QRegularExpression kHeaderLine(
        QStringLiteral("^\\s*([^：:]+?)\\s*[：:]\\s*(.*?)\\s*$")
    );
    static const QRegularExpression kLineIsComment(
        QStringLiteral("^\\s*[#＃\\*\\＊]")
    );

    auto isBodHeld = [](const QString& t) {
        return t.startsWith(QStringLiteral("先手の持駒")) ||
               t.startsWith(QStringLiteral("後手の持駒")) ||
               t.startsWith(QStringLiteral("先手の持ち駒")) ||
               t.startsWith(QStringLiteral("後手の持ち駒"));
    };

    for (const QString& rawLine : std::as_const(lines)) {
        const QString t = rawLine.trimmed();

        if (t.isEmpty()) continue;
        if (kLineIsComment.match(t).hasMatch()) continue;
        if (isKi2MoveLine(t)) break; // 指し手行に達したら終了
        if (isBodHeld(t)) continue;

        QRegularExpressionMatch m = kHeaderLine.match(rawLine);
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

// ----------------------------------------------------------------------------
// extractGameInfoMap
// ----------------------------------------------------------------------------

QMap<QString, QString> Ki2ToSfenConverter::extractGameInfoMap(const QString& filePath)
{
    QMap<QString, QString> m;
    const auto items = extractGameInfo(filePath);
    for (const auto& it : items) m.insert(it.key, it.value);
    return m;
}

// ----------------------------------------------------------------------------
// extractOpeningComment - 開始局面のコメントを抽出
// ----------------------------------------------------------------------------

QString Ki2ToSfenConverter::extractOpeningComment(const QString& filePath)
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
        if (isSkippableLine(t) || isBoardHeaderOrFrame(t)) continue;

        // 指し手の開始なら終了
        if (isKi2MoveLine(t)) break;

        // コメント行
        if (t.startsWith(QChar(u'*')) || t.startsWith(QChar(u'＊'))) {
            const QString c = t.mid(1).trimmed();
            if (!c.isEmpty()) {
                if (!buf.isEmpty()) buf += QLatin1Char('\n');
                buf += c;
            }
            continue;
        }

        // しおり - コメントには含めずスキップ
        if (t.startsWith(QLatin1Char('&'))) {
            continue;
        }
    }
    return buf;
}

// ----------------------------------------------------------------------------
// buildInitialSfenFromBod - BODから初期SFENを構築
// （KifToSfenConverterと同じ実装を流用）
// ----------------------------------------------------------------------------

bool Ki2ToSfenConverter::buildInitialSfenFromBod(const QStringList& lines,
                                                  QString& outSfen,
                                                  QString* detectedLabel,
                                                  QString* warn)
{
    // KifToSfenConverterの完全なBODパーサに委譲
    return KifToSfenConverter::buildInitialSfenFromBod(lines, outSfen, detectedLabel, warn);
}
