#include "kiftosfenconverter.h"
#include "kifreader.h"

#include <QRegularExpression>
#include <QDebug>
#include <QMap>

// 半角/全角の数字1桁 → int（0..9）。クラス外から使える軽量ラッパ
static inline int asciiDigitToInt_(QChar c) {
    const ushort u = c.unicode();
    return (u >= '0' && u <= '9') ? int(u - '0') : 0;
}
static inline int zenkakuDigitToInt_(QChar c) {
    static const QString z = QStringLiteral("０１２３４５６７８９");
    const int idx = z.indexOf(c);
    return (idx >= 0) ? idx : 0;
}

// 1桁（半角/全角）→ int。QString の range-for を使わず detach 回避。
static inline int flexDigitToInt_NoDetach_(QChar c)
{
    int v = asciiDigitToInt_(c);
    if (!v) v = zenkakuDigitToInt_(c);
    return v;
}

// 文字列に含まれる（半角/全角）数字を int へ。
// QString を range-for しない（detach 回避）。
static int flexDigitsToInt_NoDetach_(const QString& t)
{
    int v = 0;
    const int n = t.size();
    for (int i = 0; i < n; ++i) {
        const QChar ch = t.at(i);
        int d = asciiDigitToInt_(ch);
        if (!d) d = zenkakuDigitToInt_(ch);
        if (!d && ch != QChar(u'0') && ch != QChar(u'０')) continue;
        if (ch == QChar(u'0') || ch == QChar(u'０')) d = 0;
        v = v * 10 + d;
    }
    return v;
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
    static const QRegularExpression re(
        QStringLiteral("\\(\\s*([0-9０-９]{1,2})[:：]([0-9０-９]{2})(?:\\s*[／/]\\s*([0-9０-９]{1,3})[:：]([0-9０-９]{2})[:：]([0-9０-９]{2}))?\\s*\\)")
        );
    return re;
}

// 時間の正規化 "mm:ss/HH:MM:SS"
static inline QString normalizeTimeMatch_(const QRegularExpressionMatch& m)
{
    // 既存の flexDigitsToInt_NoDetach_ を想定（全角→半角も許容）
    const int mm = flexDigitsToInt_NoDetach_(m.captured(1));
    const int ss = flexDigitsToInt_NoDetach_(m.captured(2));
    const bool hasCum = m.lastCapturedIndex() >= 5 && m.captured(3).size();
    const int HH = hasCum ? flexDigitsToInt_NoDetach_(m.captured(3)) : 0;
    const int MM = hasCum ? flexDigitsToInt_NoDetach_(m.captured(4)) : 0;
    const int SS = hasCum ? flexDigitsToInt_NoDetach_(m.captured(5)) : 0;
    auto z2 = [](int x){ return QStringLiteral("%1").arg(x, 2, 10, QLatin1Char('0')); };
    return z2(mm) + QLatin1Char(':') + z2(ss) + QLatin1Char('/') +
           z2(HH) + QLatin1Char(':') + z2(MM) + QLatin1Char(':') + z2(SS);
}

// --- 終局語の判定（KIF仕様に準拠） ---
// 該当すれば normalized に表記をそのまま返す（例: "千日手"）
static inline bool isTerminalWord_(const QString& s, QString* normalized)
{
    static const QStringList kTerminals = {
        QStringLiteral("中断"),
        QStringLiteral("投了"),
        QStringLiteral("持将棋"),
        QStringLiteral("千日手"),
        QStringLiteral("切れ負け"),
        QStringLiteral("反則勝ち"),
        QStringLiteral("反則負け"),
        QStringLiteral("入玉勝ち"),
        QStringLiteral("不戦勝"),
        QStringLiteral("不戦敗"),
        QStringLiteral("詰み"),
        QStringLiteral("不詰")
    };
    const QString t = s.trimmed();
    for (const QString& w : kTerminals) {
        if (t == w) { if (normalized) *normalized = w; return true; }
    }
    return false;
}

} // namespace

namespace {

// KIF/BOD 共通：駒の漢字/略字 → (USI基底文字, 成りフラグ)
// 例: 「歩/と」「香/杏」「桂/圭」「銀/全」「角/馬」「飛/龍/竜」「玉/王」
static inline bool mapKanjiPiece(const QString& s, QChar& base, bool& promoted)
{
    promoted = false;

    if (s.contains(QChar(u'歩')) || s.contains(QChar(u'と'))) {
        base = QLatin1Char('P'); promoted = s.contains(QChar(u'と')); return true;
    }
    if (s.contains(QChar(u'香')) || s.contains(QChar(u'杏'))) {
        base = QLatin1Char('L'); promoted = s.contains(QChar(u'杏')); return true;
    }
    if (s.contains(QChar(u'桂')) || s.contains(QChar(u'圭'))) {
        base = QLatin1Char('N'); promoted = s.contains(QChar(u'圭')); return true;
    }
    if (s.contains(QChar(u'銀')) || s.contains(QChar(u'全'))) {
        base = QLatin1Char('S'); promoted = s.contains(QChar(u'全')); return true;
    }
    if (s.contains(QChar(u'金'))) {
        base = QLatin1Char('G'); return true;
    }
    if (s.contains(QChar(u'角')) || s.contains(QChar(u'馬'))) {
        base = QLatin1Char('B'); promoted = s.contains(QChar(u'馬')); return true;
    }
    if (s.contains(QChar(u'飛')) || s.contains(QChar(u'龍')) || s.contains(QChar(u'竜'))) {
        base = QLatin1Char('R'); promoted = (s.contains(QChar(u'龍')) || s.contains(QChar(u'竜'))); return true;
    }
    if (s.contains(QChar(u'玉')) || s.contains(QChar(u'王'))) {
        base = QLatin1Char('K'); return true;
    }
    return false;
}

} // namespace

namespace {

// 手合 → 初期SFEN（ユーザー提示の固定マップ）
// 手合 → 初期SFEN（ユーザー提示の固定マップ）
static QString mapHandicapToSfenImpl(const QString& label) {
    static const char* kEven = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1";
    struct Pair { const char* key; const char* sfen; };
    static const Pair tbl[] = {
                               {"平手",   kEven},
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
                               };
    for (const auto& p : tbl) {
        if (label.contains(QString::fromUtf8(p.key))) return QString::fromUtf8(p.sfen);
    }
    return QString::fromUtf8(kEven);
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

    // ★ 先に BOD を試す
    QString bodSfen;
    if (buildInitialSfenFromBod(lines, bodSfen, detectedLabel, &warn)) {
        qDebug().noquote() << "[detectInitialSfenFromFile] BOD detected. sfen =" << bodSfen;
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
        return mapHandicapToSfenImpl(QStringLiteral("平手"));
    }

    static const QRegularExpression s_afterColon(QStringLiteral("^.*[:：]"));
    QString label = found;
    label.remove(s_afterColon);
    label = label.trimmed();
    if (detectedLabel) *detectedLabel = label;
    return mapHandicapToSfenImpl(label);
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

    QString commentBuf;
    int moveIndex = 0; // 手数管理

    // --- ヘッダ走査（手番/手合割の確認） ---
    for (const QString& raw : std::as_const(lines)) {
        const QString line = raw.trimmed();
        if (startsWithMoveNumber(line)) break;

        if (line.contains(QStringLiteral("後手番"))) {
            moveIndex = 1;
            break;
        }
        if (line.startsWith(QStringLiteral("手合割")) || line.startsWith(QStringLiteral("手合"))) {
            if (!line.contains(QStringLiteral("平手"))) {
                moveIndex = 1;
            }
        }
    }

    for (const QString& raw : std::as_const(lines)) {
        QString lineStr = raw.trimmed();

        // 行頭コメント・しおり
        if (isCommentLine(lineStr)) {
            QString c = lineStr.mid(1).trimmed();
            if (!c.isEmpty()) {
                if (!commentBuf.isEmpty()) commentBuf += QLatin1Char('\n');
                commentBuf += c;
            }
            continue;
        }
        if (isBookmarkLine(lineStr)) {
            const QString name = lineStr.mid(1).trimmed();
            if (!name.isEmpty()) {
                if (!commentBuf.isEmpty()) commentBuf += QLatin1Char('\n');
                commentBuf += QStringLiteral("【しおり】") + name;
            }
            continue;
        }

        if (lineStr.isEmpty() || isSkippableLine(lineStr) || isBoardHeaderOrFrame(lineStr)) continue;

        // --- 1行に含まれる指し手をループ処理 ---
        while (!lineStr.isEmpty()) {
            // 手数で始まるか確認
            int digits = 0;
            if (!startsWithMoveNumber(lineStr, &digits)) {
                break; // 次の指し手が見つからない場合は行の残りを無視して次行へ
            }

            // 手数部分をスキップ
            int i = digits;
            while (i < lineStr.size() && lineStr.at(i).isSpace()) ++i;
            if (i >= lineStr.size()) break;

            QString rest = lineStr.mid(i).trimmed();

            // --- 時間抽出 "(...)" ---
            QRegularExpressionMatch tm = kifTimeRe().match(rest);
            QString timeText;
            int nextMoveStartIdx = -1;

            if (tm.hasMatch()) {
                // 時間が見つかった場合、そこまでが指し手
                timeText = normalizeTimeMatch_(tm);

                // 次の指し手が始まる位置（時間の末尾以降）
                nextMoveStartIdx = tm.capturedEnd(0);
                rest = rest.left(tm.capturedStart(0)).trimmed();
            } else {
                // 時間がない場合、次の数字（手数）の手前、または行末までを指し手とする
                static const QRegularExpression s_nextNum(QStringLiteral("\\s+[0-9０-９]"));
                QRegularExpressionMatch nextM = s_nextNum.match(rest);
                if (nextM.hasMatch()) {
                    nextMoveStartIdx = nextM.capturedStart();
                    rest = rest.left(nextMoveStartIdx).trimmed();
                } else {
                    nextMoveStartIdx = -1; // 行末まで
                }
            }

            // --- インラインコメント除去 (指し手の後ろに *コメント がある場合) ---
            int commentIdx = rest.indexOf(QChar(u'*'));
            if (commentIdx < 0) commentIdx = rest.indexOf(QChar(u'＊'));
            if (commentIdx >= 0) {
                QString inlineC = rest.mid(commentIdx + 1).trimmed();
                if (!inlineC.isEmpty()) {
                    if (!commentBuf.isEmpty()) commentBuf += QLatin1Char('\n');
                    commentBuf += inlineC;
                }
                rest = rest.left(commentIdx).trimmed();
            }

            // --- 処理済みの部分を行文字列から削除 ---
            if (nextMoveStartIdx != -1) {
                // 時間があった場合
                if (tm.hasMatch()) {
                    int consumed = i + tm.capturedEnd(0);
                    lineStr = lineStr.mid(consumed).trimmed();
                } else {
                    // 時間がなく、次の手が見つかった場合
                    // インラインコメントがあった場合は、コメントも含めて消費する必要がある
                    // 簡易的に、現在解析した位置(i)以降で、次の「数字」を探してそこまで進める
                    static const QRegularExpression s_next(QStringLiteral("\\s+[0-9０-９]"));
                    QRegularExpressionMatch nm = s_next.match(lineStr, i);
                    if (nm.hasMatch()) {
                        lineStr = lineStr.mid(nm.capturedStart()).trimmed();
                    } else {
                        lineStr.clear();
                    }
                }
            } else {
                lineStr.clear();
            }

            // --- 終局語? ---
            QString term;
            if (isTerminalWord_(rest, &term)) {
                ++moveIndex;
                const QString teban = (moveIndex % 2 != 0) ? QStringLiteral("▲") : QStringLiteral("△");

                // 累計時間
                auto z2 = [](int x){ return QStringLiteral("%1").arg(x, 2, 10, QLatin1Char('0')); };
                QString cum = QStringLiteral("00:00:00");
                if (tm.hasMatch()) {
                    const bool hasCum = tm.lastCapturedIndex() >= 5 && tm.captured(3).size();
                    if (hasCum) {
                        const int HH = flexDigitsToInt_NoDetach_(tm.captured(3));
                        const int MM = flexDigitsToInt_NoDetach_(tm.captured(4));
                        const int SS = flexDigitsToInt_NoDetach_(tm.captured(5));
                        cum = z2(HH) + QLatin1Char(':') + z2(MM) + QLatin1Char(':') + z2(SS);
                    }
                }

                KifDisplayItem item;
                item.prettyMove = teban + term;
                if (term == QStringLiteral("千日手")) {
                    item.timeText = QStringLiteral("00:00/") + cum;
                } else {
                    item.timeText = timeText.isEmpty() ? (QStringLiteral("00:00/") + cum) : timeText;
                }
                item.comment = commentBuf;
                commentBuf.clear();
                out.push_back(item);

                lineStr.clear();
                break;
            }

            // --- 通常手 ---
            if (!rest.isEmpty()) {
                ++moveIndex;
                const QString teban = (moveIndex % 2 != 0) ? QStringLiteral("▲") : QStringLiteral("△");

                KifDisplayItem item;
                item.prettyMove = teban + rest;
                item.timeText   = timeText.isEmpty() ? QStringLiteral("00:00/00:00:00") : timeText;
                item.comment    = commentBuf;
                commentBuf.clear();

                out.push_back(item);
            }
        }
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

    qDebug().noquote() << QStringLiteral("[convertFile] encoding = %1 , lines = %2")
                              .arg(usedEnc).arg(lines.size());

    auto isCommentLine = [](const QString& s)->bool {
        if (s.isEmpty()) return false;
        const QChar ch = s.front();
        return (ch == QChar(u'*') || ch == QChar(u'＊'));
    };
    auto isBookmarkLine = [](const QString& s)->bool {
        return s.startsWith(QLatin1Char('&'));
    };

    int prevToFile = 0, prevToRank = 0;

    for (const QString& raw : std::as_const(lines)) {
        QString lineStr = raw.trimmed();

        if (isCommentLine(lineStr) || isBookmarkLine(lineStr)) continue;
        if (lineStr.isEmpty() || isSkippableLine(lineStr) || isBoardHeaderOrFrame(lineStr)) continue;

        if (containsAnyTerminal(lineStr)) break;

        // --- 1行複数指し手ループ ---
        while (!lineStr.isEmpty()) {
            int digits = 0;
            if (!startsWithMoveNumber(lineStr, &digits)) break;

            int i = digits;
            while (i < lineStr.size() && lineStr.at(i).isSpace()) ++i;
            if (i >= lineStr.size()) break;

            QString rest = lineStr.mid(i).trimmed();

            // 時間除去・次手位置特定
            QRegularExpressionMatch tm = kifTimeRe().match(rest);
            if (tm.hasMatch()) {
                rest = rest.left(tm.capturedStart(0)).trimmed();
            } else {
                static const QRegularExpression s_nextNum(QStringLiteral("\\s+[0-9０-９]"));
                QRegularExpressionMatch nm = s_nextNum.match(rest);
                if (nm.hasMatch()) {
                    rest = rest.left(nm.capturedStart()).trimmed();
                }
            }

            // インラインコメント除去
            int cIdx = rest.indexOf(QChar(u'*'));
            if (cIdx < 0) cIdx = rest.indexOf(QChar(u'＊'));
            if (cIdx >= 0) {
                rest = rest.left(cIdx).trimmed();
            }

            // USI変換
            QString usi;
            if (convertMoveLine(rest, usi, prevToFile, prevToRank)) {
                out << usi;
                qDebug().noquote() << QStringLiteral("[USI %1] %2").arg(out.size()).arg(usi);
            } else {
                if (errorMessage) *errorMessage += QStringLiteral("[skip ?] %1\n").arg(raw);
            }

            // lineStrを進める
            if (tm.hasMatch()) {
                int consumed = i + tm.capturedEnd(0);
                lineStr = lineStr.mid(consumed).trimmed();
            } else {
                static const QRegularExpression s_next(QStringLiteral("\\s+[0-9０-９]"));
                QRegularExpressionMatch nm = s_next.match(lineStr, i);
                if (nm.hasMatch()) lineStr = lineStr.mid(nm.capturedStart()).trimmed();
                else lineStr.clear();
            }
        }
    }

    qDebug().noquote() << QStringLiteral("[convertFile] moves = %1").arg(out.size());
    return out;
}

bool KifToSfenConverter::parseWithVariations(const QString& kifPath,
                                             KifParseResult& out,
                                             QString* errorMessage)
{
    out = KifParseResult{};

    // 本譜
    QString teai;
    out.mainline.baseSfen = detectInitialSfenFromFile(kifPath, &teai);
    out.mainline.disp     = extractMovesWithTimes(kifPath, errorMessage);
    out.mainline.usiMoves = convertFile(kifPath, errorMessage);

    // 変化抽出
    QString usedEnc;
    QStringList lines;
    if (!KifReader::readLinesAuto(kifPath, lines, &usedEnc, errorMessage)) {
        return false;
    }

    static const QRegularExpression sVarHead(QStringLiteral("^\\s*変化[:：]\\s*([0-9０-９]+)手"));
    QVector<KifVariation> vars;
    int i = 0;

    while (i < lines.size()) {
        QString l = lines.at(i).trimmed();
        if (l.isEmpty() || isSkippableLine(l) || isBoardHeaderOrFrame(l)) { ++i; continue; }

        QRegularExpressionMatch m = sVarHead.match(l);
        if (!m.hasMatch()) { ++i; continue; }

        const int startPly = flexDigitsToInt_NoDetach_(m.captured(1));
        KifVariation var; var.startPly = startPly;

        ++i;
        QStringList block;
        for (; i < lines.size(); ++i) {
            const QString s = lines.at(i).trimmed();
            if (s.isEmpty()) break;
            if (s.startsWith(QStringLiteral("変化"))) break;
            block << s;
        }

        // 変化ブロック解析
        {
            int prevToFile = 0, prevToRank = 0;
            int moveIndex = var.startPly - 1;
            QString commentBuf;

            for (const QString& raw : std::as_const(block)) {
                QString lineStr = raw;

                if (isCommentLine(lineStr)) {
                    QString c = lineStr.mid(1).trimmed();
                    if (!c.isEmpty()) {
                        if (!commentBuf.isEmpty()) commentBuf += QLatin1Char('\n');
                        commentBuf += c;
                    }
                    continue;
                }
                if (isBookmarkLine(lineStr)) continue;
                if (lineStr.isEmpty() || isSkippableLine(lineStr) || isBoardHeaderOrFrame(lineStr)) continue;

                // --- 1行複数手ループ ---
                while (!lineStr.isEmpty()) {
                    int digits2 = 0;
                    if (!startsWithMoveNumber(lineStr, &digits2)) break;

                    int j = digits2;
                    while (j < lineStr.size() && lineStr.at(j).isSpace()) ++j;
                    if (j >= lineStr.size()) break;

                    QString rest = lineStr.mid(j).trimmed();

                    // 時間抽出
                    QRegularExpressionMatch tm = kifTimeRe().match(rest);
                    QString timeText;
                    if (tm.hasMatch()) {
                        timeText = normalizeTimeMatch_(tm);
                        rest = rest.left(tm.capturedStart(0)).trimmed();
                    } else {
                        static const QRegularExpression s_nextNum(QStringLiteral("\\s+[0-9０-９]"));
                        QRegularExpressionMatch nm = s_nextNum.match(rest);
                        if (nm.hasMatch()) rest = rest.left(nm.capturedStart()).trimmed();
                    }

                    // インラインコメント除去
                    int cIdx = rest.indexOf(QChar(u'*'));
                    if (cIdx < 0) cIdx = rest.indexOf(QChar(u'＊'));
                    if (cIdx >= 0) {
                        QString ic = rest.mid(cIdx+1).trimmed();
                        if (!ic.isEmpty()) {
                            if (!commentBuf.isEmpty()) commentBuf += QLatin1Char('\n');
                            commentBuf += ic;
                        }
                        rest = rest.left(cIdx).trimmed();
                    }

                    // 終局語？
                    QString term;
                    bool isTerm = isTerminalWord_(rest, &term);

                    if (isTerm) {
                        ++moveIndex;
                        const QString teban = (moveIndex % 2 != 0) ? QStringLiteral("▲") : QStringLiteral("△");
                        KifDisplayItem item;
                        item.prettyMove = teban + term;
                        item.timeText   = (term == QStringLiteral("千日手"))
                                            ? QStringLiteral("00:00/00:00:00")
                                            : (timeText.isEmpty() ? QStringLiteral("00:00/00:00:00") : timeText);
                        item.comment    = commentBuf; commentBuf.clear();
                        var.line.disp.push_back(item);
                        lineStr.clear();
                        break;
                    }

                    // 通常手
                    ++moveIndex;
                    const QString teban = (moveIndex % 2 != 0) ? QStringLiteral("▲") : QStringLiteral("△");
                    KifDisplayItem item;
                    item.prettyMove = teban + rest;
                    item.timeText   = timeText.isEmpty() ? QStringLiteral("00:00/00:00:00") : timeText;
                    item.comment    = commentBuf; commentBuf.clear();
                    var.line.disp.push_back(item);

                    QString usi;
                    if (convertMoveLine(rest, usi, prevToFile, prevToRank)) {
                        var.line.usiMoves << usi;
                    }

                    // ループ継続用 lineStr 更新
                    if (tm.hasMatch()) {
                        int consumed = j + tm.capturedEnd(0);
                        lineStr = lineStr.mid(consumed).trimmed();
                    } else {
                        static const QRegularExpression s_next(QStringLiteral("\\s+[0-9０-９]"));
                        QRegularExpressionMatch nm = s_next.match(lineStr, j);
                        if (nm.hasMatch()) lineStr = lineStr.mid(nm.capturedStart()).trimmed();
                        else lineStr.clear();
                    }
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

    // 代表的なヘッダ/表形式見出し
    static const QStringList keys = {
        QStringLiteral("開始日時"), QStringLiteral("終了日時"), QStringLiteral("対局日"),
        QStringLiteral("棋戦"), QStringLiteral("戦型"), QStringLiteral("持ち時間"),
        QStringLiteral("秒読み"), QStringLiteral("消費時間"), QStringLiteral("場所"),
        QStringLiteral("表題"),   QStringLiteral("掲載"),   QStringLiteral("記録係"),
        QStringLiteral("備考"),   QStringLiteral("図"),     QStringLiteral("振り駒"),
        QStringLiteral("先手省略名"), QStringLiteral("後手省略名"),
        QStringLiteral("手数----指手---------消費時間--"),
        QStringLiteral("手数――指手――――――――消費時間――"),
        QStringLiteral("先手："), QStringLiteral("後手："),
        QStringLiteral("先手番"), QStringLiteral("後手番"),
        QStringLiteral("手合割"), QStringLiteral("手合"),
        QStringLiteral("手数＝") // BODの手数行
    };
    for (const auto& k : keys) {
        if (line.contains(k)) return true;
    }

    // 「しおり」行はスキップ対象（USI/構造には不要）
    if (line.startsWith(QLatin1Char('&'))) return true;

    // 「まで◯手」行
    static const QRegularExpression s_made(QStringLiteral("^\\s*まで[0-9０-９]+手"));
    if (s_made.match(line).hasMatch()) return true;

    return false;
}

bool KifToSfenConverter::isBoardHeaderOrFrame(const QString& line)
{
    if (line.trimmed().isEmpty()) return false;

    // 使い回す定数群（毎回 QStringLiteral を生成しない）
    static const QString kDigitsZ   = QStringLiteral("１２３４５６７８９");
    static const QString kKanjiRow  = QStringLiteral("一二三四五六七八九");
    static const QString kBoxChars  = QStringLiteral("┌┬┐┏┳┓└┴┘┗┻┛│┃─━┼");

    // 先頭「９ ８ ７ … １」（半角/全角/混在・空白区切りを許容）
    {
        int digitCount = 0;
        bool onlyDigitsAndSpace = true;
        for (int i = 0; i < line.size(); ++i) {
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

    // 罫線：ASCII（+,-,|）/ Unicode（┌┬┐│└┴┘┏┳┓┃┗┻┛━─┼）
    {
        const QString s = line.trimmed();
        if ((s.startsWith(QLatin1Char('+')) && s.endsWith(QLatin1Char('+'))) ||
            (s.startsWith(QChar(u'|')) && s.endsWith(QChar(u'|')))) {
            return true;
        }
        int boxCount = 0;
        for (int i = 0; i < s.size(); ++i) if (kBoxChars.contains(s.at(i))) ++boxCount;
        if (boxCount >= qMax(3, s.size() / 2)) return true; // 行の半分以上が罫線
    }

    // 持駒見出し
    if (line.contains(QStringLiteral("先手の持駒")) ||
        line.contains(QStringLiteral("後手の持駒")))
        return true;

    // 下部見出し（"一二三…九"）だけの行
    {
        const QString t = line.trimmed();
        if (!t.isEmpty()) {
            bool ok = true; int kanjiCount = 0;
            for (int i = 0; i < t.size(); ++i) {
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

bool KifToSfenConverter::containsAnyTerminal(const QString& s, QString* matched)
{
    static const QStringList kWords = {
        // 主要
        QStringLiteral("投了"), QStringLiteral("中断"), QStringLiteral("持将棋"),
        QStringLiteral("千日手"), QStringLiteral("切れ負け"), QStringLiteral("時間切れ"),
        QStringLiteral("反則勝ち"), QStringLiteral("反則負け"),
        QStringLiteral("不戦勝"), QStringLiteral("不戦敗"),

        // 詰関連
        QStringLiteral("詰み"), QStringLiteral("詰"), QStringLiteral("不詰"),

        // 宣言系（KIF記述のゆらぎを吸収）
        QStringLiteral("宣言勝ち"), QStringLiteral("入玉勝ち"), QStringLiteral("入玉宣言勝ち")
    };
    for (const QString& w : kWords) {
        if (s.contains(w)) { if (matched) *matched = w; return true; }
    }
    return false;
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

    static const QRegularExpression s_paren(QStringLiteral("[\\(（]"));
    const int paren = line.indexOf(s_paren);
    const QString head = (paren >= 0) ? line.left(paren) : line;

    static const QRegularExpression s_digitKanji(QStringLiteral("([1-9１-９])([一二三四五六七八九])"));
    static const QRegularExpression s_digitDigit(QStringLiteral("([1-9１-９])([1-9１-９])"));

    QRegularExpressionMatch m = s_digitKanji.match(head);
    if (!m.hasMatch()) m = s_digitDigit.match(head);
    if (!m.hasMatch()) return false;

    const QChar fch = m.capturedView(1).at(0);
    const QChar rch = m.capturedView(2).at(0);

    toFile = flexDigitToInt_NoDetach_(fch);
    int r  = KifToSfenConverter::kanjiDigitToInt(rch); // 漢数字を優先
    if (r == 0) r = flexDigitToInt_NoDetach_(rch);     // 半/全角数字ならこちら
    toRank = r;

    return (toFile >= 1 && toFile <= 9 && toRank >= 1 && toRank <= 9);
}

QChar KifToSfenConverter::pieceKanjiToUsiUpper(const QString& s)
{
    QChar base; bool promoted = false;
    if (!mapKanjiPiece(s, base, promoted)) return QChar();
    // ドロップでは成り情報は不要。USIの基底文字（先手=大文字）を返す
    return base;
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
    int p = head.indexOf(QRegularExpression(QStringLiteral("[\\(（]")));
    if (p >= 0) head = head.left(p);
    int u = head.indexOf(QChar(u'打'));
    if (u >= 0) head = head.left(u);

    // 装飾語を除去（右/左/上/引/寄/直/行）
    head.remove(QRegularExpression(QStringLiteral("[右左上下引寄直行]+")));
    head.remove(QChar(u'▲'));
    head.remove(QChar(u'△'));
    head = head.trimmed();

    // 「歩/香/桂/銀/角/飛 + 成」で終わっていればプロモーション
    static const QRegularExpression kPromoteSuffix(QStringLiteral("(歩|香|桂|銀|角|飛)成$"));
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
    static const QRegularExpression kMods(QStringLiteral("[右左上下引寄直行]+"));
    QString line = moveText;
    line.remove(kMods);
    line = line.trimmed();

    // 目的地
    int toF = 0, toR = 0; bool same = false;
    if (!findDestination(line, toF, toR, same)) return false;
    if (same) { toF = prevToFile; toR = prevToRank; }
    if (!(toF >= 1 && toF <= 9 && toR >= 1 && toR <= 9)) return false;
    const QChar toRankLetter = rankNumToLetter(toR);

    // 打ち？
    const bool isDrop = line.contains(QStringLiteral("打"));

    // from は "(xy)"（半角/全角）から読む（なければ 0）
    int fromF = 0, fromR = 0;
    {
        static const QRegularExpression kParenAscii(QStringLiteral("\\(([0-9０-９])([0-9０-９])\\)"));
        static const QRegularExpression kParenZenkaku(QStringLiteral("（([0-9０-９])([0-9０-９])）"));
        QRegularExpressionMatch m = kParenAscii.match(line);
        if (!m.hasMatch()) m = kParenZenkaku.match(line);
        if (m.hasMatch() && m.lastCapturedIndex() >= 2) {
            const QChar a = m.capturedView(1).at(0);
            const QChar b = m.capturedView(2).at(0);

            fromF = flexDigitToInt_NoDetach_(a);
            fromR = flexDigitToInt_NoDetach_(b);
        }
    }

    if (isDrop) {
        // 駒種を拾う
        const QChar usiPiece = pieceKanjiToUsiUpper(line);
        if (usiPiece.isNull()) return false;
        usi = QStringLiteral("%1*%2%3").arg(usiPiece).arg(toF).arg(toRankLetter);
    } else {
        if (!(fromF >= 1 && fromF <= 9 && fromR >= 1 && fromR <= 9)) return false;
        const QChar fromRankLetter = rankNumToLetter(fromR);
        usi = QStringLiteral("%1%2%3%4").arg(fromF).arg(fromRankLetter).arg(toF).arg(toRankLetter);
        if (isPromotionMoveText(line)) usi += QLatin1Char('+');
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
        qWarning().noquote() << "[KIF] read failed:" << filePath << "warn:" << warn;
        return ordered;
    }
    qDebug().noquote()
        << QStringLiteral("[KIF] encoding = %1 , lines = %2").arg(usedEnc).arg(lines.size());

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

bool KifToSfenConverter::buildInitialSfenFromBod(const QStringList& lines,
                                                 QString& outSfen,
                                                 QString* detectedLabel,
                                                 QString* warn)
{
    // 1) ── ヘルパ群（この関数内のみで使用） ───────────────────────────────
    auto isSpaceLike = [](QChar ch)->bool {
        return ch.isSpace() || ch == QChar(0x3000); // 全角スペース
    };

    // 1行の盤面文字列（枠線の有無を問わず）から9マスの駒トークンを抽出する
    // hasFrame=true の場合は "| ... |一" の形式を想定（ランク文字も抽出）
    // hasFrame=false の場合は "香桂銀..." の形式を想定（ランク文字はなし）
    auto parseBodRowCommon = [&](const QString& line, QStringList& outTokens, QChar& outRankKanji, bool hasFrame)->bool {
        outTokens.clear();
        QString inner;

        if (hasFrame) {
            static const QRegularExpression rowRe(QStringLiteral("^\\s*\\|(.+)\\|\\s*([一二三四五六七八九])\\s*$"));
            QRegularExpressionMatch m = rowRe.match(line);
            if (!m.hasMatch()) return false;
            inner = m.captured(1);
            outRankKanji = m.captured(2).at(0);
        } else {
            // 枠なし：行全体を解析対象とする（コメント等は事前に除去されている前提）
            inner = line.trimmed();
            outRankKanji = QChar(); // 呼び出し元でインデックス管理する
        }

        const QString promotedSingles = QStringLiteral("と杏圭全馬龍竜");
        const QString baseSingles = QStringLiteral("歩香桂銀金角飛玉王") + promotedSingles;

        int i = 0, n = inner.size();
        while (i < n && outTokens.size() < 9) {
            while (i < n && isSpaceLike(inner.at(i))) ++i;
            if (i >= n) break;

            QChar ch = inner.at(i);

            // 特殊空白・プレフィックス処理
            if (ch == QChar(0xFF65)) { outTokens << QStringLiteral("・"); ++i; continue; } // ･
            if (ch == QChar(0xFF56)) { ch = QLatin1Char('v'); } // ｖ

            if (ch == QChar(0x30FB)) { // '・'
                outTokens << QStringLiteral("・");
                ++i;
                continue;
            }

            // 後手駒 (v + 駒文字)
            if (ch == QLatin1Char('v')) {
                ++i;
                while (i < n && isSpaceLike(inner.at(i))) ++i;
                if (i < n) {
                    QChar p = inner.at(i);
                    if (baseSingles.contains(p)) {
                        outTokens << (QStringLiteral("v") + p);
                        ++i;
                    } else {
                        outTokens << QStringLiteral("・");
                    }
                } else {
                    outTokens << QStringLiteral("・");
                }
                continue;
            }

            // 先手駒 (単独文字)
            if (baseSingles.contains(ch)) {
                outTokens << QString(ch);
                ++i;
                continue;
            }

            // どれにも当てはまらない文字は読み飛ばす
            ++i;
        }

        // 9マスに満たない場合は空白で埋める（仕様上の安全策）
        while (outTokens.size() < 9) outTokens << QStringLiteral("・");
        return true;
    };

    // SFEN行生成（空点を数字に圧縮）
    auto rowTokensToSfen = [&](const QStringList& tokens)->QString {
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

            QChar base; bool promoted = false;
            if (!mapKanjiPiece(body, base, promoted)) {
                ++empty; ++used;
                continue;
            }

            flushEmpty();
            if (promoted) row += QLatin1Char('+');
            row += (gote ? base.toLower() : base.toUpper());
            ++used;
        }
        while (used < 9) { ++empty; ++used; }
        flushEmpty();
        return row;
    };

    // 持駒解析用ヘルパ
    auto kanjiDigit = [](QChar c)->int {
        switch (c.unicode()) {
        case u'〇': case u'零': return 0;
        case u'一': return 1; case u'二': return 2; case u'三': return 3;
        case u'四': return 4; case u'五': return 5; case u'六': return 6;
        case u'七': return 7; case u'八': return 8; case u'九': return 9;
        default: return -1;
        }
    };
    auto parseKanjiNumberString = [&](const QString& s)->int {
        if (s.isEmpty()) return -1;
        int idx = s.indexOf(QChar(u'十'));
        if (idx >= 0) {
            int tens = 1;
            if (idx > 0) {
                int d = kanjiDigit(s.at(0));
                if (d <= 0) return -1;
                tens = d;
            }
            int ones = 0;
            if (idx + 1 < s.size()) {
                int d = kanjiDigit(s.at(idx + 1));
                if (d < 0) return -1;
                ones = d;
            }
            return tens * 10 + ones;
        }
        if (s.size() == 1) {
            int d = kanjiDigit(s.at(0));
            return (d >= 0) ? d : -1;
        }
        return -1;
    };
    auto parseCountSuffixFlexible = [&](const QString& token)->int {
        if (token.size() <= 1) return 1;
        QString rest = token.mid(1).trimmed();
        if (rest.isEmpty()) return 1;
        static const QRegularExpression reAscii(QStringLiteral("^(?:[×x*]?)(\\d+)$"));
        QRegularExpressionMatch m = reAscii.match(rest);
        if (m.hasMatch()) return m.captured(1).toInt();
        int n = parseKanjiNumberString(rest);
        return (n > 0) ? n : 1;
    };

    auto parseHandsLine = [&](const QString& line, QMap<QChar,int>& outCounts, bool isBlack) {
        static const QString prefixB = QStringLiteral("先手の持駒");
        static const QString prefixW = QStringLiteral("後手の持駒");
        QString t = line.trimmed();
        if (!t.startsWith(prefixB) && !t.startsWith(prefixW)) return;

        const bool sideBlack = t.startsWith(prefixB);
        if (sideBlack != isBlack) return;

        int idx = t.indexOf(QChar(u'：')); if (idx < 0) idx = t.indexOf(QLatin1Char(':'));
        if (idx < 0) return;

        QString rhs = t.mid(idx+1).trimmed();
        if (rhs == QStringLiteral("なし")) return;

        rhs.replace(QChar(u'、'), QLatin1Char(' '));
        rhs.replace(QChar(0x3000), QLatin1Char(' '));
        const QStringList toks = rhs.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);

        for (QString tok : toks) {
            tok = tok.trimmed();
            if (tok.isEmpty()) continue;

            QChar head = tok.at(0);
            QChar base; bool promoted=false;
            if (!mapKanjiPiece(QString(head), base, promoted)) continue;
            if (base == QLatin1Char('K')) continue;

            int cnt = parseCountSuffixFlexible(tok);
            outCounts[base] += cnt;
        }
    };

    auto buildHandsSfen = [](const QMap<QChar,int>& black, const QMap<QChar,int>& white)->QString {
        auto emitSide = [](const QMap<QChar,int>& m, bool gote)->QString{
            const char order[] = {'R','B','G','S','N','L','P'};
            QString s;
            for (char c : order) {
                const QChar key = QLatin1Char(c);
                int cnt = m.value(key, 0);
                if (cnt <= 0) continue;
                if (cnt > 1) s += QString::number(cnt);
                s += (gote ? key.toLower() : key);
            }
            return s;
        };
        QString s = emitSide(black, false) + emitSide(white, true);
        if (s.isEmpty()) s = QStringLiteral("-");
        return s;
    };

    auto zenk2ascii = [](QString s)->QString{
        static const QString z = QStringLiteral("０１２３４５６７８９");
        for (int i=0;i<s.size();++i) {
            int idx = z.indexOf(s.at(i));
            if (idx >= 0) s[i] = QChar('0' + idx);
        }
        return s;
    };

    // 2) ── 盤面情報の収集 ────────────────────────────────────────────────
    // モード判別：「配置：」ヘッダがあるか、通常のBOD枠があるか
    QMap<QChar, QString> rowByRank;
    bool foundPlacementHeader = false;
    int placementStartLineIndex = -1;

    for (int i = 0; i < lines.size(); ++i) {
        if (lines.at(i).contains(QStringLiteral("配置：")) || lines.at(i).contains(QStringLiteral("配置:"))) {
            foundPlacementHeader = true;
            placementStartLineIndex = i;
            break;
        }
    }

    if (foundPlacementHeader) {
        // --- 「配置：」形式 ---
        // ヘッダの次の行から9行分を読む
        static const QChar ranks_1to9[9] = {u'一',u'二',u'三',u'四',u'五',u'六',u'七',u'八',u'九'};
        int currentRankIdx = 0;

        for (int i = placementStartLineIndex + 1; i < lines.size() && currentRankIdx < 9; ++i) {
            const QString& raw = lines.at(i);
            if (raw.trimmed().isEmpty()) continue; // 空行スキップ

            QStringList toks;
            QChar dummyRank; // 使わない
            if (parseBodRowCommon(raw, toks, dummyRank, false /*no frame*/)) {
                rowByRank[ranks_1to9[currentRankIdx]] = rowTokensToSfen(toks);
                currentRankIdx++;
            }
        }
        if (detectedLabel) *detectedLabel = QStringLiteral("配置");

    } else {
        // --- 通常BOD形式（枠あり） ---
        for (const QString& raw : std::as_const(lines)) {
            QStringList toks;
            QChar rank;
            if (parseBodRowCommon(raw, toks, rank, true /*has frame*/)) {
                rowByRank[rank] = rowTokensToSfen(toks);
            }
        }
        if (detectedLabel && !rowByRank.isEmpty()) *detectedLabel = QStringLiteral("BOD");
    }

    if (rowByRank.size() != 9) {
        // BODが見つからない場合は単にfalseを返す（平手などの判定へ進むため）
        return false;
    }

    // 3) ── 持駒 ──────────────────────────────────────────────────────────────
    QMap<QChar,int> handB, handW;
    for (const QString& line : std::as_const(lines)) {
        parseHandsLine(line, handB, true);
        parseHandsLine(line, handW, false);
    }

    // 4) ── 手番 ──────────────────────────────────────────────────────────────
    QChar turn = QLatin1Char('b'); // 既定: 先手番
    for (const QString& l : std::as_const(lines)) {
        const QString t = l.trimmed();
        if (t.contains(QStringLiteral("先手番"))) { turn = QLatin1Char('b'); break; }
        if (t.contains(QStringLiteral("後手番"))) { turn = QLatin1Char('w'); break; }
    }

    // 5) ── 手数 ────────────────────────────────────────────────────────────
    int moveNumber = 1;
    static const QRegularExpression reTeSu(QStringLiteral("手数\\s*[=＝]\\s*([0-9０-９]+)"));
    for (const QString& l : std::as_const(lines)) {
        auto mt = reTeSu.match(l);
        if (mt.hasMatch()) {
            moveNumber = zenk2ascii(mt.captured(1)).toInt() + 1;
            break;
        }
    }

    // 6) ── SFEN組み立て ─────────────────────────────────────────────────────
    static const QChar ranks_1to9[9] = {u'一',u'二',u'三',u'四',u'五',u'六',u'七',u'八',u'九'};
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
