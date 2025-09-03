#include "kiftosfenconverter.h"
#include "kifreader.h"

#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>
#include <algorithm>

// 先頭が「全角/半角の 1..9 が空白区切りで並ぶだけ」の行かどうかを判定。
// 例: " ９ ８ ７ … １" / "9 8 7 6 5 4 3 2 1" など。
// マッチした場合は outDigits に空白を除いた数字列（例 "987654321"）を返す。
static inline bool isLeadingBoardDigits(const QString& line, QString* outDigits)
{
    if (outDigits) outDigits->clear();

    const QString s = line.trimmed();
    if (s.isEmpty()) return false;

    auto is19 = [](QChar ch) -> bool {
        const ushort u = ch.unicode();
        // ASCII '1'..'9' または 全角 '１'(0xFF11)..'９'(0xFF19)
        return (u >= u'1' && u <= u'9') || (u >= 0xFF11 && u <= 0xFF19);
    };

    int tokenCount = 0;
    int i = 0, n = s.size();

    while (i < n) {
        // 空白をスキップ
        while (i < n && s.at(i).isSpace()) ++i;
        if (i >= n) break;

        // 次の空白までを1トークンとする
        const int start = i;
        while (i < n && !s.at(i).isSpace()) ++i;
        const QString token = s.mid(start, i - start);

        // 各トークンは 1 文字で 1..9 であること
        if (token.size() != 1 || !is19(token.at(0))) return false;

        if (outDigits) outDigits->append(token);
        ++tokenCount;
    }

    // 少なくとも 2 個以上の数字が並んでいること（盤面ヘッダ想定）
    return tokenCount >= 2;
}

// 個数だけ欲しい場合のオーバーロード
static inline bool isLeadingBoardDigits(const QString& line, int* outCount)
{
    QString digits;
    const bool ok = isLeadingBoardDigits(line, &digits); // 上の QString* 版を呼ぶ
    if (ok && outCount) *outCount = digits.size();
    return ok;
}

namespace {

// 手合 → 初期SFEN（ユーザー提示の固定マップ）
QString mapHandicapToSfenImpl(const QString& label) {
    const QString even = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    struct Pair { const char* key; const char* sfen; };
    static const Pair tbl[] = {
        {"平手",   "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"},
        {"香落ち", "lnsgkgsn1/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"右香落ち", "1nsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"角落ち", "lnsgkgsnl/1r7/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"飛車落ち", "lnsgkgsnl/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"飛香落ち", "lnsgkgsn1/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"二枚落ち", "lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"三枚落ち", "lnsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"四枚落ち", "1nsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"五枚落ち", "2sgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"左五枚落ち", "1nsgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"六枚落ち", "2sgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"左七枚落ち", "2sgkg3/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"右七枚落ち", "3gkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"八枚落ち", "3gkg3/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"十枚落ち", "4k4/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"その他", even.toUtf8().constData()}
    };
    for (const auto& p : tbl) {
        if (label.contains(QString::fromUtf8(p.key))) {
            return QString::fromUtf8(p.sfen);
        }
    }
    return even;
}

bool isTerminalWord(const QString& s, QString* matched=nullptr) {
    static const QStringList kWords = {
        QStringLiteral("投了"), QStringLiteral("中断"), QStringLiteral("持将棋"),
        QStringLiteral("千日手"), QStringLiteral("切れ負け"),
        QStringLiteral("反則勝ち"), QStringLiteral("反則負け"),
        QStringLiteral("入玉勝ち"), QStringLiteral("不戦勝"),
        QStringLiteral("不戦敗"), QStringLiteral("詰み"), QStringLiteral("不詰")
    };
    for (const QString& w : kWords) {
        if (s.contains(w)) { if (matched) *matched = w; return true; }
    }
    return false;
}

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

// BODの盤面/枠/見出しをスキップ判定
// BODの盤面/枠/見出しをスキップ判定（正規表現は使わずに実装）
bool isBoardHeaderOrFrameImpl(const QString& line) {
    // 1) 盤上部の「９ ８ ７ … １」行か？（全角/半角混在・空白区切りを許容）
    int leadingDigits = 0;
    if (isLeadingBoardDigits(line, &leadingDigits)) {
        // 行全体が「空白と（全角/半角）数字のみ」で構成されているかをチェック
        int totalDigits = 0;
        bool ok = true;
        for (QChar ch : line) {
            if (ch.isSpace()) continue;
            bool ascii = (ch.unicode() >= QChar(u'1').unicode() && ch.unicode() <= QChar(u'9').unicode());
            bool zenk  = QStringLiteral("１２３４５６７８９").contains(ch);
            if (ascii || zenk) {
                ++totalDigits;
            } else {
                ok = false;
                break;
            }
        }
        if (!ok) {
            // fix boolean literal capitalization just in case
            ok = false;
        }
        if (ok && totalDigits >= 5) { // 典型は9個。保守的に5個以上あれば見出しとみなす
            return true;
        }
    }

    // 2) 罫線（+-----+ / | ... |）
    if (line.startsWith(QLatin1Char('+')) && line.endsWith(QLatin1Char('+'))) return true;
    if (line.startsWith(QChar(u'|')) && line.endsWith(QChar(u'|'))) return true;

    // 3) 持駒の見出し
    if (line.contains(QStringLiteral("先手の持駒")) || line.contains(QStringLiteral("後手の持駒")))
        return true;

    return false;
}
} // namespace

// ===== public =====

QString KifToSfenConverter::mapHandicapToSfen(const QString& label) {
    return mapHandicapToSfenImpl(label);
}

QString KifToSfenConverter::detectInitialSfenFromFile(const QString& kifPath, QString* detectedLabel)
{
    QString usedEnc, warn;
    QStringList lines;
    if (!KifReader::readLinesAuto(kifPath, lines, &usedEnc, &warn)) {
        if (detectedLabel) *detectedLabel = QStringLiteral("平手(既定)");
        return mapHandicapToSfenImpl(QStringLiteral("平手"));
    }
    qDebug().noquote() << QStringLiteral("[detectInitialSfenFromFile] encoding = %1 , lines = %2")
                          .arg(usedEnc).arg(lines.size());

    QString found;
    for (auto it = lines.cbegin(); it != lines.cend(); ++it) {
        const QString line = it->trimmed();
        if (line.contains(QStringLiteral("手合割")) || line.contains(QStringLiteral("手合"))) {
            found = line; break;
        }
    }
    if (found.isEmpty()) {
        if (detectedLabel) *detectedLabel = QStringLiteral("平手(既定)");
        qDebug().noquote() << "[detectInitialSfenFromFile] handicap line not found. Use 平手 -> "
                           << mapHandicapToSfenImpl(QStringLiteral("平手"));
        return mapHandicapToSfenImpl(QStringLiteral("平手"));
    }

    static const QRegularExpression s_afterColon(QStringLiteral("^.*[:：]"));
    QString label = found;
    label.remove(s_afterColon);
    label = label.trimmed();
    if (detectedLabel) *detectedLabel = label;
    const QString sfen = mapHandicapToSfenImpl(label);
    qDebug().noquote() << QStringLiteral("[detectInitialSfenFromFile] found = %1 , label = %2 , mapped SFEN = %3")
                          .arg(found, label, sfen);
    return sfen;
}

QList<KifDisplayItem> KifToSfenConverter::extractMovesWithTimes(const QString& kifPath, QString* errorMessage)
{
    QList<KifDisplayItem> out;
    QString usedEnc;
    QStringList lines;
    if (!KifReader::readLinesAuto(kifPath, lines, &usedEnc, errorMessage)) {
        return out;
    }
    qDebug().noquote() << QStringLiteral("[extractMovesWithTimes] encoding = %1 , lines = %2")
                          .arg(usedEnc).arg(lines.size());

    static const QRegularExpression s_timeRe(
        QStringLiteral("\\(\\s*(\\d{1,2}:\\d{2}/\\d{2}:\\d{2}:\\d{2})\\s*\\)")
    );

    QString commentBuf;
    int moveIndex = 0;

    for (auto it = lines.cbegin(); it != lines.cend(); ++it) {
        const QString raw = *it;
        const QString line = raw.trimmed();

        if (line.startsWith(QLatin1Char('*'))) {
            // コメントは次の手に付与
            QString c = line.mid(1).trimmed();
            if (!c.isEmpty()) {
                if (!commentBuf.isEmpty()) commentBuf += QLatin1Char('\n');
                commentBuf += c;
            }
            continue;
        }
        if (line.isEmpty() || isSkippableLine(line) || isBoardHeaderOrFrame(line))
            continue;

        int digits = 0;
        if (!startsWithMoveNumber(line, &digits)) {
            if (errorMessage) *errorMessage += QStringLiteral("[skip ?] %1\n").arg(line);
            continue;
        }
        int i = digits;
        while (i < line.size() && line.at(i).isSpace()) ++i;
        if (i >= line.size()) continue;

        QString rest = line.mid(i).trimmed();
        // 時間
        QRegularExpressionMatch tm = s_timeRe.match(rest);
        QString timeText;
        if (tm.hasMatch()) {
            timeText = tm.captured(1).trimmed();
            rest = rest.left(tm.capturedStart(0)).trimmed();
        }

        // 終局/中断？
        QString term;
        if (containsAnyTerminal(rest, &term)) {
            ++moveIndex;
            const QString teban = (moveIndex % 2 == 1) ? QStringLiteral("▲") : QStringLiteral("△");
            KifDisplayItem item;
            item.prettyMove = teban + term;
            item.timeText = (term == QStringLiteral("千日手"))
                            ? QStringLiteral("00:00/00:00:00")
                            : (timeText.isEmpty() ? QStringLiteral("00:00/00:00:00") : timeText);
            item.comment = commentBuf;
            commentBuf.clear();
            out.push_back(item);
            qDebug().noquote() << QStringLiteral("[extractMovesWithTimes] terminal: %1 , %2")
                                  .arg(item.prettyMove, item.timeText);
            continue;
        }

        // 通常の指し手
        ++moveIndex;
        const QString teban = (moveIndex % 2 == 1) ? QStringLiteral("▲") : QStringLiteral("△");
        KifDisplayItem item;
        item.prettyMove = teban + rest;
        item.timeText   = timeText;
        item.comment    = commentBuf;
        commentBuf.clear();
        out.push_back(item);
        qDebug().noquote() << QStringLiteral("[extractMovesWithTimes] move: %1 , %2")
                              .arg(item.prettyMove, item.timeText.isEmpty() ? QStringLiteral("-") : item.timeText);
    }

    qDebug().noquote() << QStringLiteral("[extractMovesWithTimes] total moves extracted = %1").arg(out.size());
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
    qDebug().noquote() << QStringLiteral("[convertFile] encoding = %1 , lines = %2")
                          .arg(usedEnc).arg(lines.size());

    int prevToFile = 0, prevToRank = 0;

    for (auto it = lines.cbegin(); it != lines.cend(); ++it) {
        QString raw = *it;
        QString line = raw.trimmed();

        if (line.startsWith(QLatin1Char('*'))) continue;
        if (line.isEmpty() || isSkippableLine(line) || isBoardHeaderOrFrame(line)) continue;

        // 終局/中断で打ち切り
        if (containsAnyTerminal(line)) break;

        // 先頭の手数を外す
        int digits = 0;
        if (!startsWithMoveNumber(line, &digits)) {
            if (errorMessage) *errorMessage += QStringLiteral("[skip ?] %1\n").arg(line);
            continue;
        }
        int i = digits;
        while (i < line.size() && line.at(i).isSpace()) ++i;
        if (i >= line.size()) continue;

        QString moveText = line.mid(i).trimmed();
        QString usi;
        if (convertMoveLine(moveText, usi, prevToFile, prevToRank)) {
            out << usi;
            qDebug().noquote() << QStringLiteral("[USI %1] %2").arg(out.size()).arg(usi);
        } else {
            if (errorMessage) *errorMessage += QStringLiteral("[skip ?] %1\n").arg(line);
        }
    }
    qDebug().noquote() << QStringLiteral("[convertFile] moves = %1").arg(out.size());
    return out;
}

bool KifToSfenConverter::parseWithVariations(const QString& kifPath, KifParseResult& out, QString* errorMessage)
{
    out = KifParseResult{};

    // まず本譜を既存関数で取得
    QString label;
    out.mainline.baseSfen = detectInitialSfenFromFile(kifPath, &label);
    out.mainline.disp     = extractMovesWithTimes(kifPath, errorMessage);
    out.mainline.usiMoves = convertFile(kifPath, errorMessage);

    // 変化をスキャン
    QString usedEnc;
    QStringList lines;
    if (!KifReader::readLinesAuto(kifPath, lines, &usedEnc, errorMessage)) {
        return false;
    }

    static const QRegularExpression s_varHead(QStringLiteral("^\\s*変化[:：]\\s*([0-9０-９]+)手"));
    QVector<KifVariation> vars;

    int i = 0;
    while (i < lines.size()) {
        QString l = lines.at(i).trimmed();
        QRegularExpressionMatch m = s_varHead.match(l);
        if (!m.hasMatch()) { ++i; continue; }

        // 開始手数
        QString num = m.captured(1);
        int startPly = 0;
        for (QChar ch : num) {
            int v = asciiDigitToInt(ch);
            if (!v) v = zenkakuDigitToInt(ch);
            if (!v) continue;
            startPly = startPly * 10 + v;
        }
        ++i; // 変化ヘッダの次の行から

        // このブロックの指し手を集める（次の「変化：」または空行/EOFまで）
        KifVariation var;
        var.startPly = (startPly <= 0 ? 1 : startPly);

        QStringList block;
        while (i < lines.size()) {
            QString s = lines.at(i).trimmed();
            if (s.isEmpty()) break;
            if (s.startsWith(QStringLiteral("変化"))) break;
            block << s;
            ++i;
        }

        // ブロックを既存ロジックで解析（USI/表示）
        // 単純化：行頭の手数は既に含まれているはずなのでそのまま使う
        QString tmpWarn;
        // 表示
        {
            QString enc;
            // 疑似ファイルではないので、ここでは簡易実装でパース
            // blockの各行を extractMovesWithTimes と同じルールで処理
            int prevToFile = 0, prevToRank = 0;
            int moveIndex = var.startPly - 1;
            static const QRegularExpression s_timeRe(
                QStringLiteral("\\(\\s*(\\d{1,2}:\\d{2}/\\d{2}:\\d{2}:\\d{2})\\s*\\)")
            );
            QString commentBuf;
            for (const QString& raw : block) {
                const QString line = raw;
                if (line.startsWith(QLatin1Char('*'))) {
                    QString c = line.mid(1).trimmed();
                    if (!c.isEmpty()) {
                        if (!commentBuf.isEmpty()) commentBuf += QLatin1Char('\n');
                        commentBuf += c;
                    }
                    continue;
                }
                if (line.isEmpty() || isSkippableLine(line) || isBoardHeaderOrFrame(line)) continue;

                int digits = 0;
                if (!startsWithMoveNumber(line, &digits)) continue;
                int j = digits;
                while (j < line.size() && line.at(j).isSpace()) ++j;
                if (j >= line.size()) continue;
                QString rest = line.mid(j).trimmed();

                QRegularExpressionMatch tm = s_timeRe.match(rest);
                QString timeText;
                if (tm.hasMatch()) {
                    timeText = tm.captured(1).trimmed();
                    rest = rest.left(tm.capturedStart(0)).trimmed();
                }

                QString term;
                ++moveIndex;
                const QString teban = (moveIndex % 2 == 1) ? QStringLiteral("▲") : QStringLiteral("△");
                KifDisplayItem item;

                if (containsAnyTerminal(rest, &term)) {
                    item.prettyMove = teban + term;
                    item.timeText = (term == QStringLiteral("千日手"))
                                    ? QStringLiteral("00:00/00:00:00")
                                    : (timeText.isEmpty() ? QStringLiteral("00:00/00:00:00") : timeText);
                    item.comment = commentBuf; commentBuf.clear();
                    var.line.disp.push_back(item);
                    break;
                } else {
                    item.prettyMove = teban + rest;
                    item.timeText   = timeText;
                    item.comment    = commentBuf; commentBuf.clear();
                    var.line.disp.push_back(item);
                }

                QString usi;
                if (convertMoveLine(rest, usi, prevToFile, prevToRank)) {
                    var.line.usiMoves << usi;
                }
            }
        }

        vars.push_back(var);
    }

    out.variations = vars;
    return true;
}

// ===== private helpers =====

bool KifToSfenConverter::isSkippableLine(const QString& line)
{
    if (line.isEmpty()) return true;
    if (line.startsWith(QLatin1Char('#'))) return true;
    static const QStringList keys = {
        QStringLiteral("開始日時"), QStringLiteral("終了日時"), QStringLiteral("対局日"),
        QStringLiteral("棋戦"), QStringLiteral("戦型"), QStringLiteral("持ち時間"),
        QStringLiteral("秒読み"), QStringLiteral("消費時間"), QStringLiteral("場所"),
        QStringLiteral("備考"), QStringLiteral("図"), QStringLiteral("振り駒"),
        QStringLiteral("先手省略名"), QStringLiteral("後手省略名"),
        QStringLiteral("手数----指手---------消費時間--"),
        QStringLiteral("手数――指手――――――――消費時間――"),
        QStringLiteral("先手："), QStringLiteral("後手："),
        QStringLiteral("先手番"), QStringLiteral("後手番"),
        QStringLiteral("手合割"), QStringLiteral("手合")
    };
    for (const auto& k : keys) if (line.contains(k)) return true;

    // 「まで◯手」行もスキップ
    static const QRegularExpression s_made(QStringLiteral("^\\s*まで[0-9０-９]+手"));
    if (s_made.match(line).hasMatch()) return true;

    return false;
}

bool KifToSfenConverter::isBoardHeaderOrFrame(const QString& line)
{
    return isBoardHeaderOrFrameImpl(line);
}

bool KifToSfenConverter::containsAnyTerminal(const QString& s, QString* matched)
{
    return isTerminalWord(s, matched);
}

int KifToSfenConverter::asciiDigitToInt(QChar c)
{
    ushort u = c.unicode();
    if (u >= '0' && u <= '9') return int(u - '0');
    return 0;
}

int KifToSfenConverter::zenkakuDigitToInt(QChar c)
{
    const QString z = QStringLiteral("０１２３４５６７８９");
    int idx = z.indexOf(c);
    return (idx >= 0) ? idx : 0;
}

int KifToSfenConverter::kanjiDigitToInt(QChar c)
{
    switch (c.unicode()) {
    case u'一': return 1;
    case u'二': return 2;
    case u'三': return 3;
    case u'四': return 4;
    case u'五': return 5;
    case u'六': return 6;
    case u'七': return 7;
    case u'八': return 8;
    case u'九': return 9;
    default: return 0;
    }
}

QChar KifToSfenConverter::rankNumToLetter(int r)
{
    if (r < 1 || r > 9) return QChar();
    return QChar(QLatin1Char('a' + (r - 1)));
}

bool KifToSfenConverter::findDestination(const QString& line, int& toFile, int& toRank, bool& isSameAsPrev)
{
    isSameAsPrev = false;
    if (line.contains(QStringLiteral("同"))) {
        toFile = toRank = 0;
        isSameAsPrev = true;
        return true;
    }
    // 最初の括弧の手前のみ参照（"(27)" や 時間括弧と衝突しないように）
    static const QRegularExpression s_paren(QStringLiteral("[\\(（]"));
    int paren = line.indexOf(s_paren);
    QString head = (paren >= 0) ? line.left(paren) : line;

    // 目的地は行頭近くの「数字＋漢数字」or「数字＋数字」
    static const QRegularExpression s_digitKanji(QStringLiteral("([1-9１-９])([一二三四五六七八九])"));
    static const QRegularExpression s_digitDigit(QStringLiteral("([1-9１-９])([1-9１-９])"));
    QRegularExpressionMatch m = s_digitKanji.match(head);
    if (!m.hasMatch()) m = s_digitDigit.match(head);
    if (!m.hasMatch()) return false;

    auto flexDigit = [](QChar c)->int {
        int v = asciiDigitToInt(c);
        if (v == 0) v = zenkakuDigitToInt(c);
        return v;
    };

    QChar fch = m.capturedView(1).at(0);
    QChar rch = m.capturedView(2).at(0);
    toFile = flexDigit(fch);
    int r = kanjiDigitToInt(rch);
    if (r == 0) r = flexDigit(rch);
    toRank = r;
    return (toFile >= 1 && toFile <= 9 && toRank >= 1 && toRank <= 9);
}

QChar KifToSfenConverter::pieceKanjiToUsiUpper(const QString& s)
{
    // 代表1文字のみ見ればほぼ十分
    if (s.contains(QStringLiteral("歩")) || s.contains(QStringLiteral("と"))) return QLatin1Char('P');
    if (s.contains(QStringLiteral("香")) || s.contains(QStringLiteral("杏"))) return QLatin1Char('L');
    if (s.contains(QStringLiteral("桂")) || s.contains(QStringLiteral("圭"))) return QLatin1Char('N');
    if (s.contains(QStringLiteral("銀")) || s.contains(QStringLiteral("全"))) return QLatin1Char('S');
    if (s.contains(QStringLiteral("金"))) return QLatin1Char('G');
    if (s.contains(QStringLiteral("角")) || s.contains(QStringLiteral("馬"))) return QLatin1Char('B');
    if (s.contains(QStringLiteral("飛")) || s.contains(QStringLiteral("龍")) || s.contains(QStringLiteral("竜"))) return QLatin1Char('R');
    if (s.contains(QStringLiteral("玉")) || s.contains(QStringLiteral("王"))) return QLatin1Char('K');
    return QChar();
}

bool KifToSfenConverter::isPromotionMoveText(const QString& line)
{
    if (line.contains(QStringLiteral("不成"))) return false;
    return line.contains(QStringLiteral("成"));
}

bool KifToSfenConverter::convertMoveLine(const QString& moveText,
                                         QString& usi,
                                         int& prevToFile, int& prevToRank)
{
    // 目的地
    int toF=0, toR=0; bool same=false;
    if (!findDestination(moveText, toF, toR, same)) return false;
    if (same) { toF = prevToFile; toR = prevToRank; }
    if (!(toF>=1 && toF<=9 && toR>=1 && toR<=9)) return false;

    // 打ち？
    const bool isDrop = moveText.contains(QStringLiteral("打"));
    const QChar toRankLetter = rankNumToLetter(toR);

    // from は "(xy)" から読む（なければ 0）
    int fromF=0, fromR=0;
    {
        static const QRegularExpression s_par(QStringLiteral("[\\(（]\\s*([0-9１-９])([0-9１-９])\\s*[\\)）]"));
        QRegularExpressionMatch m = s_par.match(moveText);
        if (m.hasMatch()) {
            QChar a = m.capturedView(1).at(0);
            QChar b = m.capturedView(2).at(0);
            fromF = asciiDigitToInt(a); if (!fromF) fromF = zenkakuDigitToInt(a);
            fromR = asciiDigitToInt(b); if (!fromR) fromR = zenkakuDigitToInt(b);
        }
    }

    if (isDrop) {
        // 駒種を拾う（例: "角打", "香打"）
        QChar usiPiece = pieceKanjiToUsiUpper(moveText);
        if (usiPiece.isNull()) return false;
        usi = QStringLiteral("%1*%2%3").arg(usiPiece).arg(toF).arg(toRankLetter);
    } else {
        if (!(fromF>=1 && fromF<=9 && fromR>=1 && fromR<=9)) return false;
        const QChar fromRankLetter = rankNumToLetter(fromR);
        usi = QStringLiteral("%1%2%3%4").arg(fromF).arg(fromRankLetter).arg(toF).arg(toRankLetter);
        if (isPromotionMoveText(moveText)) usi += QLatin1Char('+');
    }

    // 「同」のために保存
    prevToFile = toF;
    prevToRank = toR;
    return true;
}

// ====== Game Info Extraction (file-order, duplicates kept as separate rows) ======
static inline QString normalizeKey(const QString& raw) {
    QString k = raw.trimmed();
    if (k.endsWith(u'：') || k.endsWith(u':')) k.chop(1);
    return k.trimmed();
}
static inline QString normalizeValue(QString v) {
    v = v.trimmed();
    v.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
    return v;
}

// 全角コロンのみ許容（半角コロンは無視）かつ「行頭（先頭空白は許容）」でのヘッダ判定
static const QRegularExpression kHeaderLine(
    QStringLiteral("^\\s*([^：]+?)\\s*：\\s*(.*?)\\s*$")
    );
// 「*」「＊」で始まるコメント行
static const QRegularExpression kLineIsComment(
    QStringLiteral("^\\s*[\\*\\uFF0A]")
    );
// 指し手行（手数で始まる）。半角/全角数字どちらにもマッチ
static const QRegularExpression kLineLooksLikeMoveNo(
    QStringLiteral("^\\s*[0-9０-９]+\\s")
    );
// 変化ヘッダ「変化：◯手」は対局情報から除外
static const QRegularExpression kVariationHead(
    QStringLiteral("^\\s*変化[：:]\\s*[0-9０-９]+手")
    );

QList<KifGameInfoItem> KifToSfenConverter::extractGameInfo(const QString& filePath)
{
    QList<KifGameInfoItem> ordered;
    if (filePath.isEmpty()) return ordered;

    // Auto-detect encoding (Shift-JIS / UTF-8 etc.)
    QString usedEnc, warn;
    QStringList lines;
    if (!KifReader::readLinesAuto(filePath, lines, &usedEnc, &warn)) {
        qWarning() << "[KIF] read failed:" << filePath << "warn:" << warn;
        return ordered;
    }
    qDebug().noquote() << QStringLiteral("[KIF] encoding = %1 , lines = %2")
                              .arg(usedEnc).arg(lines.size());

    for (const QString& rawLine : lines) {
        const QString line = rawLine;
        const QString t = line.trimmed();

        // 1) コメント/指し手/棋譜表ヘッダ/全角コロン無し/変化ヘッダ を早期フィルタ
        if (kLineIsComment.match(t).hasMatch()) continue;
        if (kLineLooksLikeMoveNo.match(t).hasMatch()) continue;
        if (t.startsWith(QStringLiteral("手数"))) continue;
        if (!t.contains(QChar(0xFF1A))) continue; // 全角コロン「：」が無い
        if (kVariationHead.match(t).hasMatch()) continue; // 変化：◯手

        // 2) 全角コロン限定で key/value 抽出（行頭のみ許容）
        QRegularExpressionMatch m = kHeaderLine.match(line);
        if (!m.hasMatch()) continue;

        const QString key = normalizeKey(m.captured(1));
        if (key.isEmpty()) continue;
        const QString val = normalizeValue(m.captured(2));

        // 3) 出現順のまま格納（重複キーも別行として push_back）
        ordered.push_back({ key, val });
    }

    // ※ 並べ替えも集約も行わない：KIFファイルの先頭からの出現順をそのまま維持
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
