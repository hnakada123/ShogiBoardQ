#include "kiftosfenconverter.h"
#include "kifreader.h"

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
  #include <utility>
#endif

#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QHash>
#include <QDebug>

namespace {

// -------------------- 小物ヘルパ（フリー関数） --------------------

inline bool isIdeographicSpace(QChar ch) {
    return ch == QChar(0x3000);
}
inline bool isSpaceLike(QChar ch) {
    return ch.isSpace() || isIdeographicSpace(ch);
}

int asciiDigitToInt_free(QChar c)
{
    const ushort u = c.unicode();
    return (u >= '0' && u <= '9') ? int(u - '0') : 0;
}
int zenkakuDigitToInt_free(QChar c)
{
    static const QString z = QStringLiteral("０１２３４５６７８９");
    const int idx = z.indexOf(c);
    return (idx >= 0) ? idx : 0;
}
int kanjiDigitToInt_free(QChar c)
{
    switch (c.unicode()) {
    case 0x4E00: return 1; // 一
    case 0x4E8C: return 2; // 二
    case 0x4E09: return 3; // 三
    case 0x56DB: return 4; // 四
    case 0x4E94: return 5; // 五
    case 0x516D: return 6; // 六
    case 0x4E03: return 7; // 七
    case 0x516B: return 8; // 八
    case 0x4E5D: return 9; // 九
    default: return 0;
    }
}
QChar rankToLetter(int rank1to9) {
    return QChar(QLatin1Char('a' + (rank1to9 - 1)));
}

bool containsAnyTerminal(const QString& s, QString* matched = nullptr)
{
    static const QStringList keywords = {
        QStringLiteral("投了"), QStringLiteral("中断"), QStringLiteral("持将棋"),
        QStringLiteral("千日手"), QStringLiteral("切れ負け"),
        QStringLiteral("反則勝ち"), QStringLiteral("反則負け"),
        QStringLiteral("入玉勝ち"), QStringLiteral("不戦勝"),
        QStringLiteral("不戦敗"), QStringLiteral("詰み"), QStringLiteral("不詰"),
    };
    for (const auto& kw : keywords) {
        if (s.contains(kw)) {
            if (matched) *matched = kw;
            return true;
        }
    }
    return false;
}

// 盤面/BOD の「スキップすべき行」判定を強化
bool isSkippableKifLine(const QString& raw)
{
    const QString s = raw.trimmed();
    if (s.isEmpty()) return true;

    // 盤面座標行（例: "９ ８ ７ ６ ５ ４ ３ ２ １" / "1 2 3 4 5 6 7 8 9"）
    static const QRegularExpression s_fileNumberRow(
        QStringLiteral(R"(^[ \t　]*[1-9１-９](?:[ \t　]+[1-9１-９]){8}[ \t　]*$)"));
    if (s_fileNumberRow.match(s).hasMatch()) return true;

    // 盤面の各行（例: "|v香 ・ … |一" などの枠付き）
    static const QRegularExpression s_boardRow(
        QStringLiteral(R"(^\|.*\|[一二三四五六七八九]$)"));
    if (s_boardRow.match(s).hasMatch()) return true;

    // 枠線
    static const QRegularExpression s_border(QStringLiteral(R"(^[+]?[-=ー－─━]+[+]?$)"));
    if (s_border.match(s).hasMatch()) return true;

    // 表のヘッダ
    static const QRegularExpression s_tableHdr(
        QStringLiteral(R"(^(手数[-ー－]{2,}指手[-ー－]{2,}消費時間))"));
    if (s_tableHdr.match(s).hasMatch()) return true;

    // メタ情報（持駒行も含む）
    static const QRegularExpression s_metaLine(
        QStringLiteral(R"(^(開始日時|終了日時|対局日|棋戦|場所|持ち時間|消費時間|手合|手合割|先手|後手|戦型).*[：:])"));
    if (s_metaLine.match(s).hasMatch()) return true;

    // 「手数＝N ... まで」行
    static const QRegularExpression s_te_su(QStringLiteral(R"(^手数\s*[=＝]\s*[0-9０-９]+)"));
    if (s_te_su.match(s).hasMatch()) return true;

    // 「先手番」「後手番」
    if (s.contains(QStringLiteral("先手番")) || s.contains(QStringLiteral("後手番")))
        return true;

    // コメント・変化
    if (s.startsWith(QLatin1Char('*'))) return true;
    static const QRegularExpression s_branch(QStringLiteral(R"(^(変化[:：]|変化\s))"));
    if (s_branch.match(s).hasMatch()) return true;

    // 「まで◯手で…」の終局行
    static const QRegularExpression s_resultLine(
        QStringLiteral(R"(まで[0-9０-９]+手で)"));
    if (s_resultLine.match(s).hasMatch()) return true;

    // 先頭が手数（半/全角）なら「指し手候補」として扱う
    static const QRegularExpression s_moveHeadRe(QStringLiteral(R"(^[0-9０-９]+)"));
    if (s_moveHeadRe.match(s).hasMatch())
        return false;

    // 終局/中断キーワードは別経路で拾うので、ここではスキップ解除しない
    static const QStringList s_terminals = {
        QStringLiteral("投了"), QStringLiteral("中断"), QStringLiteral("持将棋"),
        QStringLiteral("千日手"), QStringLiteral("切れ負け"),
        QStringLiteral("反則勝ち"), QStringLiteral("反則負け"),
        QStringLiteral("入玉勝ち"), QStringLiteral("不戦勝"),
        QStringLiteral("不戦敗"), QStringLiteral("詰み"), QStringLiteral("不詰"),
    };
    for (const auto& kw : s_terminals)
        if (s.contains(kw)) return false;

    return false; // 既定ではスキップしない（＝指し手候補）
}

// -------------------- BOD 解析 --------------------

// 盤面行（"|" ... "|一"）の中身から 9 マスのトークンを抽出
static bool splitBoardCells(const QString& rowBody, QStringList& outNine)
{
    outNine.clear();
    const int n = rowBody.size();
    for (int i = 0; i < n;) {
        while (i < n && isSpaceLike(rowBody.at(i))) ++i;
        if (i >= n) break;

        QChar ch = rowBody.at(i);
        if (ch == QLatin1Char('v')) {
            ++i;
            while (i < n && isSpaceLike(rowBody.at(i))) ++i;
            if (i >= n) return false;
            QChar pc = rowBody.at(i++);
            outNine << (QStringLiteral("v") + pc);
        } else if (ch == QChar(0x30FB) /* ・ */) {
            ++i;
            outNine << QStringLiteral("・");
        } else {
            QChar pc = ch;
            ++i;
            outNine << QString(pc);
        }
        if (outNine.size() == 9) break;
    }
    return (outNine.size() == 9);
}

// 駒漢字 → SFEN トークン（先手=Upper, 後手=Lower, 成駒は +X）
// 杏(成香)=+L, 圭(成桂)=+N, 全(成銀)=+S, と=+P, 馬=+B, 龍/竜=+R
static QString kanjiToSfenToken(const QString& tok)
{
    bool gote = false;
    QChar k;
    if (tok.startsWith(QLatin1Char('v'))) {
        gote = true;
        if (tok.size() < 2) return QString();
        k = tok.at(1);
    } else {
        k = tok.isEmpty() ? QChar() : tok.at(0);
    }

    auto up = [](QChar c){ return QString(c); };
    auto dn = [](QChar c){ return QString(c.toLower()); };

    // promoted/simple
    if (k == QChar(0x3068)) { // と
        return gote ? QStringLiteral("+p") : QStringLiteral("+P");
    }
    if (k == QChar(0x99AC)) { // 馬
        return gote ? QStringLiteral("+b") : QStringLiteral("+B");
    }
    if (k == QChar(0x9F8D) || k == QChar(0x7ADC)) { // 龍 / 竜
        return gote ? QStringLiteral("+r") : QStringLiteral("+R");
    }
    if (k == QChar(0x674F)) { // 杏 (成香)
        return gote ? QStringLiteral("+l") : QStringLiteral("+L");
    }
    if (k == QChar(0x572D)) { // 圭 (成桂)
        return gote ? QStringLiteral("+n") : QStringLiteral("+N");
    }
    if (k == QChar(0x5168)) { // 全 (成銀)
        return gote ? QStringLiteral("+s") : QStringLiteral("+S");
    }

    // normal pieces
    if (k == QChar(0x6B69)) return gote ? dn('P') : up('P'); // 歩
    if (k == QChar(0x9999)) return gote ? dn('L') : up('L'); // 香
    if (k == QChar(0x6842)) return gote ? dn('N') : up('N'); // 桂
    if (k == QChar(0x9280)) return gote ? dn('S') : up('S'); // 銀
    if (k == QChar(0x91D1)) return gote ? dn('G') : up('G'); // 金
    if (k == QChar(0x89D2)) return gote ? dn('B') : up('B'); // 角
    if (k == QChar(0x98DB)) return gote ? dn('R') : up('R'); // 飛
    if (k == QChar(0x738B) || k == QChar(0x7389)) // 王 / 玉
        return gote ? dn('K') : up('K');

    if (k == QChar(0x30FB)) return QString(); // ・ 空き

    // 不明文字は空扱い（=空マス）
    return QString();
}

// BOD の 9 行から SFEN の盤面配置文字列を生成
static bool boardRowsToSfen(const QStringList& nineRows, QString& outBoardSfen)
{
    QStringList sfenRanks;
    sfenRanks.reserve(9);

    for (const QString& row : nineRows) {
        int a = row.indexOf('|');
        int b = row.lastIndexOf('|');
        if (a < 0 || b <= a) return false;
        const QString body = row.mid(a + 1, b - a - 1);

        QStringList cells;
        if (!splitBoardCells(body, cells)) return false;

        QString rank;
        int emptyRun = 0;
        for (const QString& cellTok : cells) {
            const QString s = kanjiToSfenToken(cellTok);
            if (s.isEmpty()) {
                ++emptyRun;
            } else {
                if (emptyRun > 0) { rank += QString::number(emptyRun); emptyRun = 0; }
                rank += s;
            }
        }
        if (emptyRun > 0) rank += QString::number(emptyRun);
        sfenRanks << rank;
    }

    if (sfenRanks.size() != 9) return false;
    outBoardSfen = sfenRanks.join(QLatin1Char('/'));
    return true;
}

// 「◯手の持駒：...」行 → SFEN持駒文字列へ（先手=Upper, 後手=Lower）
static int kanjiNumToInt(const QString& s)
{
    if (s.isEmpty()) return 1;
    bool ok=false; int v = s.toInt(&ok);
    if (ok) return v;

    int ten = s.count(QChar(0x5341)); // 十
    if (ten > 0) {
        int left = 0, right = 0;
        int pos = s.indexOf(QChar(0x5341));
        QString L = s.left(pos);
        QString R = s.mid(pos + 1);
        if (!L.isEmpty()) {
            left = kanjiDigitToInt_free(L.at(0));
            if (left == 0) { left = L.toInt(&ok); left = ok ? left : 0; }
        } else {
            left = 1;
        }
        if (!R.isEmpty()) {
            right = kanjiDigitToInt_free(R.at(0));
            if (right == 0) { right = R.toInt(&ok); right = ok ? right : 0; }
        }
        return left * 10 + right;
    }
    return kanjiDigitToInt_free(s.at(0));
}

static QChar pieceKanjiToLetter(QChar k, bool gote)
{
    auto up = [](QChar c){ return c; };
    auto dn = [](QChar c){ return QChar(c.toLower()); };

    if (k == QChar(0x6B69)) return gote ? dn('P') : up('P'); // 歩
    if (k == QChar(0x9999)) return gote ? dn('L') : up('L'); // 香
    if (k == QChar(0x6842)) return gote ? dn('N') : up('N'); // 桂
    if (k == QChar(0x9280)) return gote ? dn('S') : up('S'); // 銀
    if (k == QChar(0x91D1)) return gote ? dn('G') : up('G'); // 金
    if (k == QChar(0x89D2)) return gote ? dn('B') : up('B'); // 角
    if (k == QChar(0x98DB)) return gote ? dn('R') : up('R'); // 飛
    if (k == QChar(0x738B) || k == QChar(0x7389)) return gote ? dn('K') : up('K'); // 王/玉
    // 成駒表記が持駒に出た場合は元駒へ丸める（捕獲されれば元駒に戻るため）
    if (k == QChar(0x3068)) return gote ? dn('P') : up('P'); // と -> P
    if (k == QChar(0x99AC)) return gote ? dn('B') : up('B'); // 馬 -> B
    if (k == QChar(0x9F8D) || k == QChar(0x7ADC)) return gote ? dn('R') : up('R'); // 龍/竜 -> R
    if (k == QChar(0x674F)) return gote ? dn('L') : up('L'); // 杏(成香) -> L
    if (k == QChar(0x572D)) return gote ? dn('N') : up('N'); // 圭(成桂) -> N
    if (k == QChar(0x5168)) return gote ? dn('S') : up('S'); // 全(成銀) -> S
    return QChar(); // 不明
}

static QString parseHandLineToHands(const QString& line, bool gote)
{
    int pos = line.indexOf(QChar(0xFF1A)); // ：
    if (pos < 0) pos = line.indexOf(QLatin1Char(':'));
    if (pos < 0) return QString();

    QString tail = line.mid(pos + 1).trimmed();
    if (tail.contains(QStringLiteral("なし"))) return QString();

    tail.replace(QChar(0x3000), QLatin1Char(' '));
    tail.replace(QChar(0x3001), QLatin1Char(' '));
    tail.replace(QChar(0xFF0C), QLatin1Char(' '));
    while (tail.contains(QStringLiteral("  "))) tail.replace(QStringLiteral("  "), QStringLiteral(" "));

    QString hands;
    const QStringList toks = tail.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString& t : toks) {
        if (t.isEmpty()) continue;
        const QChar k = t.at(0);
        QChar letter = pieceKanjiToLetter(k, gote);
        if (letter.isNull()) continue;

        int cnt = 1;
        if (t.size() > 1) {
            const QString num = t.mid(1);
            cnt = qMax(1, kanjiNumToInt(num));
        }
        hands += QString(cnt, letter);
    }
    return hands;
}

// 与えられた行列から BOD（盤面図）を検出して SFEN を構築
static bool detectBodAndBuildSfen(const QStringList& lines, QString& outSfen)
{
    int startRow = -1;
    for (int i = 0; i < lines.size(); ++i) {
        const QString s = lines.at(i).trimmed();
        static const QRegularExpression s_row(QStringLiteral(R"(^\|.*\|[一二三四五六七八九]$)"));
        if (s_row.match(s).hasMatch()) {
            if (s.endsWith(QChar(0x4E00))) { // 一
                if (i + 8 < lines.size()) {
                    startRow = i;
                    break;
                }
            }
        }
    }
    if (startRow < 0) return false;

    QStringList nineRows;
    for (int r = 0; r < 9; ++r) {
        const QString row = lines.at(startRow + r).trimmed();
        static const QRegularExpression s_row(QStringLiteral(R"(^\|.*\|[一二三四五六七八九]$)"));
        if (!s_row.match(row).hasMatch())
            return false;
        nineRows << row;
    }

    QString boardSfen;
    if (!boardRowsToSfen(nineRows, boardSfen)) return false;

    QString goteLine, senteLine;
    for (int i = startRow - 3; i >= 0 && i >= startRow - 6; --i) {
        const QString s = lines.at(i).trimmed();
        if (s.contains(QStringLiteral("後手の持駒"))) { goteLine = s; break; }
    }
    for (int i = startRow + 9; i < lines.size() && i <= startRow + 14; ++i) {
        const QString s = lines.at(i).trimmed();
        if (s.contains(QStringLiteral("先手の持駒"))) { senteLine = s; break; }
    }
    if (goteLine.isEmpty()) {
        for (const QString& s : lines) if (s.contains(QStringLiteral("後手の持駒"))) { goteLine = s; break; }
    }
    if (senteLine.isEmpty()) {
        for (const QString& s : lines) if (s.contains(QStringLiteral("先手の持駒"))) { senteLine = s; break; }
    }
    const QString goteHands = goteLine.isEmpty() ? QString() : parseHandLineToHands(goteLine, /*gote=*/true);
    const QString senteHands = senteLine.isEmpty() ? QString() : parseHandLineToHands(senteLine, /*gote=*/false);

    QString hands;
    if (!senteHands.isEmpty() || !goteHands.isEmpty()) {
        hands = senteHands + goteHands;
    } else {
        hands = QStringLiteral("-");
    }

    bool blackToMove = true;
    int moveCount = 1;
    for (const QString& s : lines) {
        if (s.contains(QStringLiteral("先手番"))) { blackToMove = true; break; }
        if (s.contains(QStringLiteral("後手番"))) { blackToMove = false; break; }
    }
    if (!lines.isEmpty()) {
        static const QRegularExpression s_moves(QStringLiteral(R"(手数\s*[=＝]\s*([0-9０-９]+))"));
        for (const QString& s : lines) {
            auto m = s_moves.match(s);
            if (m.hasMatch()) {
                const QString num = m.captured(1);
                int n = 0;
                for (QChar c : num) {
                    int a = zenkakuDigitToInt_free(c);
                    if (a == 0) a = asciiDigitToInt_free(c);
                    n = n*10 + a;
                }
                if (n > 0) {
                    moveCount = n + 1;
                    if (!s.contains(QStringLiteral("先手番")) && !s.contains(QStringLiteral("後手番"))) {
                        blackToMove = (n % 2 == 0);
                    }
                }
                break;
            }
        }
    }

    outSfen = boardSfen
            + QLatin1Char(' ')
            + (blackToMove ? QLatin1Char('b') : QLatin1Char('w'))
            + QLatin1Char(' ')
            + hands
            + QLatin1Char(' ')
            + QString::number(moveCount);
    return true;
}

} // namespace

// ===================== KifToSfenConverter 本体 =====================

KifToSfenConverter::KifToSfenConverter()
    : m_lastDest(-1, -1)
{}

bool KifToSfenConverter::findDestination(const QString& line, int& toFile, int& toRank, bool& isSameAsPrev)
{
    isSameAsPrev = false;

    if (line.contains(QStringLiteral("同"))) {
        isSameAsPrev = true;
        toFile = toRank = 0;
        return true;
    }

    int i = 0;
    while (i < line.size()) {
        const ushort u = line.at(i).unicode();
        if (u >= '0' && u <= '9') ++i; else break;
    }
    QString s = line.mid(i);

    const int pAscii = s.indexOf(QLatin1Char('('));
    const int pZenk  = s.indexOf(QChar(0xFF08));
    int paren = -1;
    if (pAscii >= 0 && pZenk >= 0)      paren = qMin(pAscii, pZenk);
    else if (pAscii >= 0)               paren = pAscii;
    else                                paren = pZenk;

    const QString head = (paren >= 0) ? s.left(paren) : s;

    static const QRegularExpression reDigitKanji(QStringLiteral("([1-9１-９])([一二三四五六七八九])"));
    QRegularExpressionMatch m = reDigitKanji.match(head);

    if (!m.hasMatch()) {
        static const QRegularExpression reDigitDigit(QStringLiteral("([1-9１-９])([1-9１-９])"));
        m = reDigitDigit.match(head);
        if (!m.hasMatch()) return false;
    }

    const QChar fch = m.capturedView(1).at(0);
    const QChar rch = m.capturedView(2).at(0);

    auto flexDigit = [](QChar c)->int {
        int v = KifToSfenConverter::asciiDigitToInt(c);
        if (v == 0) v = KifToSfenConverter::zenkakuDigitToInt(c);
        return v;
    };

    toFile = flexDigit(fch);
    int r = KifToSfenConverter::kanjiDigitToInt(rch);
    if (r == 0) r = flexDigit(rch);
    toRank = r;

    return (toFile >= 1 && toFile <= 9 && toRank >= 1 && toRank <= 9);
}

bool KifToSfenConverter::convertMoveLine(const QString& lineIn, QString& usiMove)
{
    QString line = lineIn;
    line.remove(QRegularExpression(QStringLiteral("\\s+")));

    const bool isDrop = line.contains(QStringLiteral("打"));

    int toFile = 0, toRank = 0;
    bool isSame = false;
    if (!findDestination(line, toFile, toRank, isSame)) {
        return false;
    }
    if (isSame) {
        if (m_lastDest.x() < 0 || m_lastDest.y() < 0) return false;
        toFile = m_lastDest.x() + 1;
        toRank = m_lastDest.y() + 1;
    }

    const QChar toFileChar = QChar('0' + toFile);
    const QChar toRankChar = rankToLetter(toRank);

    const bool hasNari    = line.contains(QStringLiteral("成"));
    const bool hasFunari  = line.contains(QStringLiteral("不成"));
    const bool isPromote  = (hasNari && !hasFunari);

    if (isDrop) {
        QChar kind = QLatin1Char('P');
        if (line.contains(QStringLiteral("香"))) kind = QLatin1Char('L');
        else if (line.contains(QStringLiteral("桂"))) kind = QLatin1Char('N');
        else if (line.contains(QStringLiteral("銀"))) kind = QLatin1Char('S');
        else if (line.contains(QStringLiteral("金"))) kind = QLatin1Char('G');
        else if (line.contains(QStringLiteral("角"))) kind = QLatin1Char('B');
        else if (line.contains(QStringLiteral("飛"))) kind = QLatin1Char('R');

        usiMove = QString(kind) + QStringLiteral("*") + QString(toFileChar) + QString(toRankChar);
        m_lastDest = QPoint(toFile - 1, toRank - 1);
        return true;
    }

    static const QRegularExpression reParen(QStringLiteral(R"(\(([0-9０-９])([0-9０-９])\))"));
    QRegularExpressionMatch m = reParen.match(line);
    if (!m.hasMatch()) {
        return false;
    }
    const int f1 = (KifToSfenConverter::asciiDigitToInt(m.capturedView(1).at(0)) ?
                    KifToSfenConverter::asciiDigitToInt(m.capturedView(1).at(0)) :
                    KifToSfenConverter::zenkakuDigitToInt(m.capturedView(1).at(0)));
    const int r1 = (KifToSfenConverter::asciiDigitToInt(m.capturedView(2).at(0)) ?
                    KifToSfenConverter::asciiDigitToInt(m.capturedView(2).at(0)) :
                    KifToSfenConverter::zenkakuDigitToInt(m.capturedView(2).at(0)));

    if (f1 < 1 || f1 > 9 || r1 < 1 || r1 > 9) return false;

    const QChar fromFileChar = QChar('0' + f1);
    const QChar fromRankChar = rankToLetter(r1);

    usiMove = QString(fromFileChar) + QString(fromRankChar)
            + QString(toFileChar) + QString(toRankChar);
    if (isPromote) usiMove += QLatin1Char('+');

    m_lastDest = QPoint(toFile - 1, toRank - 1);
    return true;
}

QStringList KifToSfenConverter::convertFile(const QString& kifPath, QString* errorMessage)
{
    QStringList out;

    QString usedEnc;
    QStringList lines;
    if (!KifReader::readLinesAuto(kifPath, lines, &usedEnc, errorMessage)) {
        return out;
    }

    qDebug().noquote() << "[convertFile] encoding =" << usedEnc
                       << ", lines =" << lines.size();

    auto isTerminalLine = [](const QString& s)->bool {
        QString matched;
        return containsAnyTerminal(s, &matched);
    };

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    for (const QString& raw : std::as_const(lines))
#else
    for (const QString& raw : lines)
#endif
    {
        const QString line = raw.trimmed();
        if (line.isEmpty() || isSkippableKifLine(line)) continue;

        if (isTerminalLine(line)) break;

        QString usi;
        if (convertMoveLine(line, usi)) {
            out << usi;
            qDebug().noquote() << QString("[USI %1] %2").arg(out.size()).arg(usi);
        } else if (errorMessage) {
            *errorMessage += QStringLiteral("[skip ?] %1\n").arg(line);
        }
    }

    qDebug().noquote() << "[convertFile] moves =" << out.size();
    return out;
}

QList<KifDisplayItem> KifToSfenConverter::extractMovesWithTimes(const QString& kifPath,
                                                                QString* errorMessage)
{
    QList<KifDisplayItem> out;

    QString usedEnc;
    QStringList lines;
    if (!KifReader::readLinesAuto(kifPath, lines, &usedEnc, errorMessage)) {
        qDebug().noquote() << "[extractMovesWithTimes] readLinesAuto failed.";
        return out;
    }

    qDebug().noquote() << "[extractMovesWithTimes] encoding =" << usedEnc
                       << ", lines =" << lines.size();

    int moveIndex = 0;

    static const QRegularExpression s_timeRe(
        QStringLiteral("\\(\\s*(\\d{1,2}:\\d{2}/\\d{2}:\\d{2}:\\d{2})\\s*\\)")
    );
    static const QRegularExpression s_onlyNumbersSpaced(
        QStringLiteral(R"(^[1-9１-９](?:[ \t　]+[1-9１-９]){1,8}$)"));

    for (auto it = lines.cbegin(); it != lines.cend(); ++it) {
        const QString raw = *it;
        const QString line = raw.trimmed();
        if (isSkippableKifLine(line) || line.isEmpty()) continue;

        int i = 0, digits = 0;
        while (i < line.size()) {
            const QChar ch = line.at(i);
            const ushort u = ch.unicode();
            const bool ascii = (u >= '0' && u <= '9');
            const bool zenk  = QStringLiteral("０１２３４５６７８９").contains(ch);
            if (ascii || zenk) { ++i; ++digits; } else break;
        }
        if (digits == 0) {
            if (errorMessage) *errorMessage += QStringLiteral("[skip ?] %1\n").arg(line);
            qDebug().noquote() << "[extractMovesWithTimes] skip(not a move num):" << line.left(80);
            continue;
        }
        while (i < line.size() && line.at(i).isSpace()) ++i;
        if (i >= line.size()) continue;

        const QString rest = line.mid(i);
        QRegularExpressionMatch tm = s_timeRe.match(rest);
        QString moveText = rest.trimmed();
        QString timeText;

        if (tm.hasMatch()) {
            timeText = tm.captured(1).trimmed();
            moveText = rest.left(tm.capturedStart(0)).trimmed();
        }
        if (moveText.isEmpty()) {
            if (errorMessage) *errorMessage += QStringLiteral("[skip ?] %1\n").arg(line);
            qDebug().noquote() << "[extractMovesWithTimes] skip(empty moveText):" << line.left(80);
            continue;
        }
        if (s_onlyNumbersSpaced.match(moveText).hasMatch()) {
            qDebug().noquote() << "[extractMovesWithTimes] skip(file-number row):" << moveText;
            continue;
        }

        ++moveIndex;
        const bool black = (moveIndex % 2 == 1);
        const QString teban = black ? QStringLiteral("▲") : QStringLiteral("△");

        QString matched;
        if (containsAnyTerminal(moveText, &matched)) {
            KifDisplayItem item;
            item.prettyMove = teban + matched;
            if (matched == QStringLiteral("千日手")) {
                item.timeText = QStringLiteral("00:00/00:00:00");
            } else {
                item.timeText = timeText.isEmpty()
                                ? QStringLiteral("00:00/00:00:00")
                                : timeText;
            }
            out.push_back(item);
            qDebug().noquote() << "[extractMovesWithTimes] terminal:"
                               << item.prettyMove << "," << item.timeText;
            continue;
        }

        KifDisplayItem item;
        item.prettyMove = teban + moveText;
        item.timeText   = timeText;
        out.push_back(item);

        qDebug().noquote() << "[extractMovesWithTimes] move:"
                           << item.prettyMove << "," << (item.timeText.isEmpty() ? "-" : item.timeText);
    }

    qDebug().noquote() << "[extractMovesWithTimes] total moves extracted =" << out.size();
    return out;
}

QString KifToSfenConverter::detectInitialSfenFromFile(const QString& kifPath, QString* detectedLabel)
{
    QString usedEnc, err;
    QStringList lines;
    if (!KifReader::readLinesAuto(kifPath, lines, &usedEnc, &err)) {
        if (detectedLabel) *detectedLabel = QStringLiteral("平手(既定)");
        const QString fallback = KifToSfenConverter::mapHandicapToSfen(QStringLiteral("平手"));
        qDebug().noquote() << "[detectInitialSfenFromFile] readLinesAuto failed. Fallback 平手. error:" << err;
        return fallback;
    }

    qDebug().noquote() << "[detectInitialSfenFromFile] encoding =" << usedEnc
                       << ", lines =" << lines.size();

    QString bodSfen;
    if (detectBodAndBuildSfen(lines, bodSfen)) {
        if (detectedLabel) *detectedLabel = QStringLiteral("BOD(途中局面)");
        qDebug().noquote() << "[detectInitialSfenFromFile] BOD detected. SFEN =" << bodSfen;
        return bodSfen;
    }

    QString found;
    for (auto it = lines.cbegin(); it != lines.cend(); ++it) {
        const QString line = it->trimmed();
        if (line.contains(QStringLiteral("手合割")) || line.contains(QStringLiteral("手合"))) {
            found = line;
            break;
        }
    }

    if (found.isEmpty()) {
        if (detectedLabel) *detectedLabel = QStringLiteral("平手(既定)");
        const QString sfen = KifToSfenConverter::mapHandicapToSfen(QStringLiteral("平手"));
        qDebug().noquote() << "[detectInitialSfenFromFile] handicap line not found. Use 平手 ->" << sfen;
        return sfen;
    }

    static const QRegularExpression s_afterColon(QStringLiteral("^.*[:：]"));
    QString label = found;
    label.remove(s_afterColon);
    label = label.trimmed();

    if (detectedLabel)
        *detectedLabel = label.isEmpty() ? QStringLiteral("平手(既定)") : label;

    const QString mapped = KifToSfenConverter::mapHandicapToSfen(label.isEmpty()
                           ? QStringLiteral("平手") : label);

    qDebug().noquote() << "[detectInitialSfenFromFile] found =" << found
                       << ", label =" << label
                       << ", mapped SFEN =" << mapped;

    return mapped;
}

QString KifToSfenConverter::mapHandicapToSfen(const QString& label)
{
    const QString in = label.trimmed();

    static const QHash<QString, QString> kMap = {
        { QStringLiteral("平手"),
          QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1") },
        { QStringLiteral("香落ち"),
          QStringLiteral("lnsgkgsn1/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1") },
        { QStringLiteral("右香落ち"),
          QStringLiteral("1nsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1") },
        { QStringLiteral("角落ち"),
          QStringLiteral("lnsgkgsnl/1r7/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1") },
        { QStringLiteral("飛車落ち"),
          QStringLiteral("lnsgkgsnl/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1") },
        { QStringLiteral("飛香落ち"),
          QStringLiteral("lnsgkgsn1/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1") },
        { QStringLiteral("二枚落ち"),
          QStringLiteral("lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1") },
        { QStringLiteral("三枚落ち"),
          QStringLiteral("lnsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1") },
        { QStringLiteral("四枚落ち"),
          QStringLiteral("1nsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1") },
        { QStringLiteral("五枚落ち"),
          QStringLiteral("2sgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1") },
        { QStringLiteral("左五枚落ち"),
          QStringLiteral("1nsgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1") },
        { QStringLiteral("六枚落ち"),
          QStringLiteral("2sgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1") },
        { QStringLiteral("左七枚落ち"),
          QStringLiteral("2sgkg3/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1") },
        { QStringLiteral("右七枚落ち"),
          QStringLiteral("3gkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1") },
        { QStringLiteral("八枚落ち"),
          QStringLiteral("3gkg3/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1") },
        { QStringLiteral("十枚落ち"),
          QStringLiteral("4k4/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1") },
        { QStringLiteral("その他"),
          QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1") },
    };

    if (const auto it = kMap.find(in); it != kMap.end()) {
        return *it;
    }
    for (auto it = kMap.cbegin(); it != kMap.cend(); ++it) {
        if (in.contains(it.key())) return it.value();
    }
    return kMap.value(QStringLiteral("平手"));
}

// ===================== KifToSfenConverter: static helpers (linkage fix) =====================

int KifToSfenConverter::asciiDigitToInt(QChar c)
{
    const ushort u = c.unicode();
    return (u >= '0' && u <= '9') ? int(u - '0') : 0;
}

int KifToSfenConverter::zenkakuDigitToInt(QChar c)
{
    static const QString z = QStringLiteral("０１２３４５６７８９");
    const int idx = z.indexOf(c);
    return (idx >= 0) ? idx : 0;
}

int KifToSfenConverter::kanjiDigitToInt(QChar c)
{
    switch (c.unicode()) {
    case 0x4E00: return 1; // 一
    case 0x4E8C: return 2; // 二
    case 0x4E09: return 3; // 三
    case 0x56DB: return 4; // 四
    case 0x4E94: return 5; // 五
    case 0x516D: return 6; // 六
    case 0x4E03: return 7; // 七
    case 0x516B: return 8; // 八
    case 0x4E5D: return 9; // 九
    default: return 0;
    }
}
