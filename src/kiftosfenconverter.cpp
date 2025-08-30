#include "kiftosfenconverter.h"

#include <QFile>
#include <QTextStream>
#include <QRegularExpression>

KifToSfenConverter::KifToSfenConverter()
    : m_lastDest(-1, -1)
{
}

QStringList KifToSfenConverter::convertFile(const QString& kifPath, QString* errorMessage)
{
    QStringList out;
    QFile f(kifPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) *errorMessage += QStringLiteral("[open fail] %1\n").arg(kifPath);
        return out;
    }
    QTextStream ts(&f);
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    ts.setEncoding(QStringConverter::Encoding::Utf8);
#else
    ts.setCodec("UTF-8");
#endif

    int lineNo = 0;
    while (!ts.atEnd()) {
        const QString raw = ts.readLine();
        ++lineNo;
        const QString line = raw.trimmed();
        if (line.isEmpty() || isSkippableLine(line)) continue;

        // ★ 終局/中断なら打ち切り
        if (containsAnyTerminal(line)) break;

        QString usi;
        if (convertMoveLine(line, usi)) {
            out << usi;
        } else if (errorMessage) {
            *errorMessage += QStringLiteral("[skip %1] %2\n").arg(lineNo).arg(line);
        }
    }
    return out;
}

bool KifToSfenConverter::convertMoveLine(const QString& lineIn, QString& usiMove)
{
    QString line = lineIn;
    line.remove(QRegularExpression(QStringLiteral("\\s+"))); // 空白類は除去して扱いやすく

    // 打ち手か？
    const bool isDrop = line.contains(QStringLiteral("打"));

    // 着手先を取得（「同」対応）
    int toFile = 0, toRank = 0;
    bool isSame = false;
    if (!findDestination(line, toFile, toRank, isSame)) {
        return false;
    }
    if (isSame) {
        if (m_lastDest.x() < 1 || m_lastDest.y() < 1) return false; // 「同」だが直前が不明
        toFile = m_lastDest.x();
        toRank = m_lastDest.y();
    }

    // 成りフラグ（「不成」が無く、かつ「成」を含む）
    bool isPromote = false;
    if (!line.contains(QStringLiteral("不成")) && line.contains(QStringLiteral("成"))) {
        // ただし、"成銀""成桂""成香" のように既に成っている駒の移動だけの場合もある。
        // 簡易対応として、"成" が「打」や "(" より前に現れるパターンでも成りとみなす。
        // （厳密に判定するには局面追跡が必要だが簡易変換では省略）
        isPromote = true;
    }

    // 直前手の着手先を更新（「同」のため）
    m_lastDest = QPoint(toFile, toRank);

    // USI の座標は file数字 + rank文字（1..9 → a..i）
    const QChar toRankLetter = rankNumToLetter(toRank);
    if (toRankLetter.isNull()) return false;

    if (isDrop) {
        const QChar dropLetter = pieceKanaToUsiDropLetter(line);
        if (dropLetter.isNull()) return false;
        usiMove = QStringLiteral("%1*%2%3").arg(dropLetter)
                      .arg(toFile)
                      .arg(toRankLetter);
        return true;
    }

    // 通常の移動: "(77)" などの移動元が必要
    int fromFile = 0, fromRank = 0;
    if (!parseOrigin(line, fromFile, fromRank)) {
        // KIF に移動元が無い場合の推論は未対応（簡易実装）
        return false;
    }
    const QChar fromRankLetter = rankNumToLetter(fromRank);
    if (fromRankLetter.isNull()) return false;

    usiMove = QStringLiteral("%1%2%3%4")
                  .arg(fromFile)
                  .arg(fromRankLetter)
                  .arg(toFile)
                  .arg(toRankLetter);

    if (isPromote) {
        usiMove += QLatin1Char('+');
    }
    return true;
}

// ========== ユーティリティ実装 ==========

int KifToSfenConverter::zenkakuDigitToInt(QChar ch)
{
    static const QString z = QStringLiteral("０１２３４５６７８９");
    int idx = z.indexOf(ch);
    if (idx < 0) return 0;
    return idx; // '０'→0, '１'→1 ...
}

int KifToSfenConverter::asciiDigitToInt(QChar ch)
{
    if (ch >= QLatin1Char('0') && ch <= QLatin1Char('9'))
        return ch.toLatin1() - '0';
    return 0;
}

int KifToSfenConverter::kanjiDigitToInt(QChar ch)
{
    // 一二三四五六七八九 → 1..9
    static const QString ks = QStringLiteral("一二三四五六七八九");
    int idx = ks.indexOf(ch);
    if (idx < 0) return 0;
    return idx + 1;
}

QChar KifToSfenConverter::rankNumToLetter(int rank)
{
    // USI: 1→'a', 2→'b', ... 9→'i'
    if (rank < 1 || rank > 9) return QChar();
    return QChar(QLatin1Char('a' + (rank - 1)));
}

bool KifToSfenConverter::parseOrigin(const QString& src, int& fromFile, int& fromRank)
{
    // "(77)" または "（77）"。内側の数字は半角/全角どちらでもOKにする。
    // パターン例:  ...歩(77) / ...角成（９９）
    static const QRegularExpression re(
        QStringLiteral("[\\(（]\\s*([0-9０-９])\\s*([0-9０-９])\\s*[\\)）]"));
    const QRegularExpressionMatch m = re.match(src);
    if (!m.hasMatch()) return false;

    auto toIntFlex = [](QChar c)->int {
        int v = asciiDigitToInt(c);
        if (v == 0) v = zenkakuDigitToInt(c);
        return v;
    };

    fromFile = toIntFlex(m.capturedView(1).at(0));
    fromRank = toIntFlex(m.capturedView(2).at(0));

    return (fromFile >= 1 && fromFile <= 9 && fromRank >= 1 && fromRank <= 9);
}

bool KifToSfenConverter::findDestination(const QString& line, int& toFile, int& toRank, bool& isSameAsPrev)
{
    isSameAsPrev = false;

    // 「同」は直前手と同一地点
    if (line.contains(QStringLiteral("同"))) {
        isSameAsPrev = true;
        toFile = toRank = 0;
        return true;
    }

    // 先頭の ASCII 手数（例: "10"）だけを除去（全角数字は残す）
    int i = 0;
    while (i < line.size()) {
        const ushort u = line.at(i).unicode();
        if (u >= '0' && u <= '9') ++i; else break;
    }
    QString s = line.mid(i);

    // 最初の括弧の手前だけを見る（"(27)" や 時間 "(00:...)" を誤検出しない）
    const int paren = s.indexOf(QRegularExpression(QStringLiteral("[\\(（]")));
    const QString head = (paren >= 0) ? s.left(paren) : s;

    // 1) 「数字＋漢数字」優先
    static const QRegularExpression reDigitKanji(QStringLiteral("([1-9１-９])([一二三四五六七八九])"));
    QRegularExpressionMatch m = reDigitKanji.match(head);

    // 2) 無ければ「数字＋数字」（半角/全角）
    if (!m.hasMatch()) {
        static const QRegularExpression reDigitDigit(QStringLiteral("([1-9１-９])([1-9１-９])"));
        m = reDigitDigit.match(head);
        if (!m.hasMatch()) return false;
    }

    const QChar fch = m.capturedView(1).at(0);
    const QChar rch = m.capturedView(2).at(0);

    auto flexDigit = [](QChar c)->int {
        int v = asciiDigitToInt(c);
        if (v == 0) v = zenkakuDigitToInt(c);
        return v;
    };

    toFile = flexDigit(fch);
    int r = kanjiDigitToInt(rch);
    if (r == 0) r = flexDigit(rch); // 例: "76" のような数字段
    toRank = r;

    return (toFile >= 1 && toFile <= 9 && toRank >= 1 && toRank <= 9);
}

QChar KifToSfenConverter::pieceKanaToUsiDropLetter(const QString& line)
{
    // USI ドロップは成駒ではなく元の駒種を使う
    // KIF 内の駒名をざっくり検出（最初に見つかった基本駒を採用）
    struct Map { const QChar kana; const QChar usi; };
    static const Map table[] = {
                                { QChar(u'歩'), QChar(u'P') },
                                { QChar(u'香'), QChar(u'L') },
                                { QChar(u'桂'), QChar(u'N') },
                                { QChar(u'銀'), QChar(u'S') },
                                { QChar(u'金'), QChar(u'G') },
                                { QChar(u'角'), QChar(u'B') },
                                { QChar(u'飛'), QChar(u'R') },
                                { QChar(u'玉'), QChar(u'K') },
                                { QChar(u'王'), QChar(u'K') },
                                };
    for (const auto& m : table) {
        if (line.contains(m.kana)) return m.usi;
    }
    return QChar(); // 不明
}

// 先に提示した isSkippableLine を少し強化（ヘッダ類を弾く）
bool KifToSfenConverter::isSkippableLine(const QString& line)
{
    if (line.isEmpty()) return true;
    if (line.startsWith(QLatin1Char('*'))) return true; // コメント

    // 見出し・メタ情報
    static const QStringList keys = {
        QStringLiteral("手数="), QStringLiteral("開始日時"), QStringLiteral("終了日時"),
        QStringLiteral("先手："), QStringLiteral("後手："),
        QStringLiteral("上手："), QStringLiteral("下手："),
        QStringLiteral("棋戦："), QStringLiteral("場所："),
        QStringLiteral("持ち時間："), QStringLiteral("消費時間："),
        QStringLiteral("戦型："), QStringLiteral("手合割"),
        QStringLiteral("手数----指手---------消費時間--")
    };
    for (const auto& k : keys) if (line.contains(k)) return true;

    // 集計行「まで◯◯手で…」はスキップ
    if (line.contains(QStringLiteral("まで")) && line.contains(QStringLiteral("手で"))) return true;

    // ★ 終局/中断系はスキップしない
    QString matched;
    if (containsAnyTerminal(line, &matched)) return false;

    // 「投了」「中断」等でない普通の行はここで false（=スキップしない）
    return false;
}

static QString mapHandicapToSfen(const QString& labelNorm) {
    // 比較は包含で行う。より具体的なものを先に判定
    auto has = [&](const QString& s){ return labelNorm.contains(s, Qt::CaseInsensitive); };

    if (has(QStringLiteral("右香落ち"))) return QStringLiteral("1nsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
    if (has(QStringLiteral("左七枚落ち"))) return QStringLiteral("2sgkg3/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
    if (has(QStringLiteral("右七枚落ち"))) return QStringLiteral("3gkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
    if (has(QStringLiteral("左五枚落ち"))) return QStringLiteral("1nsgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");

    if (has(QStringLiteral("飛香落ち"))) return QStringLiteral("lnsgkgsn1/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
    if (has(QStringLiteral("三枚落ち"))) return QStringLiteral("lnsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
    if (has(QStringLiteral("四枚落ち"))) return QStringLiteral("1nsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
    if (has(QStringLiteral("五枚落ち"))) return QStringLiteral("2sgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
    if (has(QStringLiteral("六枚落ち"))) return QStringLiteral("2sgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
    if (has(QStringLiteral("八枚落ち"))) return QStringLiteral("3gkg3/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
    if (has(QStringLiteral("十枚落ち"))) return QStringLiteral("4k4/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");

    if (has(QStringLiteral("香落ち")))  return QStringLiteral("lnsgkgsn1/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
    if (has(QStringLiteral("角落ち")))  return QStringLiteral("lnsgkgsnl/1r7/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
    if (has(QStringLiteral("飛車落ち"))) return QStringLiteral("lnsgkgsnl/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
    if (has(QStringLiteral("二枚落ち"))) return QStringLiteral("lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");

    if (has(QStringLiteral("その他")))  return QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    // 既定: 平手
    return QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
}

QString KifToSfenConverter::detectInitialSfenFromFile(const QString& kifPath, QString* detectedLabel)
{
    QFile f(kifPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (detectedLabel) *detectedLabel = QStringLiteral("平手(既定)");
        return mapHandicapToSfen(QStringLiteral("平手"));
    }
    QTextStream ts(&f);
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    ts.setEncoding(QStringConverter::Encoding::Utf8);
#else
    ts.setCodec("UTF-8");
#endif
    QString found; // 行全体
    while (!ts.atEnd()) {
        const QString line = ts.readLine().trimmed();
        if (line.contains(QStringLiteral("手合")) || line.contains(QStringLiteral("手合割"))) {
            found = line;
            break;
        }
    }
    if (found.isEmpty()) {
        if (detectedLabel) *detectedLabel = QStringLiteral("平手(既定)");
        return mapHandicapToSfen(QStringLiteral("平手"));
    }
    // 例: "手合割：平手" / "手合割：香落ち"
    QString label = found;
    label.remove(QRegularExpression(QStringLiteral("^.*[:：]"))); // "："より前を削除
    label = label.trimmed();
    if (detectedLabel) *detectedLabel = label;
    return mapHandicapToSfen(label);
}

// 追加：KIFから「指し手＋消費時間」を抽出
QList<KifDisplayItem> KifToSfenConverter::extractMovesWithTimes(const QString& kifPath,
                                                                QString* errorMessage)
{
    QList<KifDisplayItem> out;

    QFile f(kifPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) *errorMessage += QStringLiteral("[open fail] %1\n").arg(kifPath);
        return out;
    }
    QTextStream ts(&f);
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    ts.setEncoding(QStringConverter::Encoding::Utf8);
#else
    ts.setCodec("UTF-8");
#endif

    int moveIndex = 0;
    const QRegularExpression timeRe(
        QStringLiteral("\\(\\s*(\\d{1,2}:\\d{2}/\\d{2}:\\d{2}:\\d{2})\\s*\\)")
        );

    while (!ts.atEnd()) {
        const QString raw = ts.readLine();
        const QString line = raw.trimmed();
        if (isSkippableLine(line)) continue;
        if (line.isEmpty()) continue;

        // 手数（半/全角）を読む
        int i = 0, digits = 0;
        while (i < line.size()) {
            const QChar ch = line.at(i);
            const ushort u = ch.unicode();
            const bool ascii = (u >= '0' && u <= '9');
            const bool zenk  = QStringLiteral("０１２３４５６７８９").contains(ch);
            if (ascii || zenk) { ++i; ++digits; } else break;
        }
        if (digits == 0) { if (errorMessage) *errorMessage += QStringLiteral("[skip ?] %1\n").arg(line); continue; }
        while (i < line.size() && line.at(i).isSpace()) ++i;
        if (i >= line.size()) continue;

        const QString rest = line.mid(i);
        QRegularExpressionMatch tm = timeRe.match(rest);
        QString moveText = rest.trimmed();
        QString timeText;

        if (tm.hasMatch()) {
            timeText = tm.captured(1).trimmed();
            moveText = rest.left(tm.capturedStart(0)).trimmed();
        }

        if (moveText.isEmpty()) { if (errorMessage) *errorMessage += QStringLiteral("[skip ?] %1\n").arg(line); continue; }

        ++moveIndex;
        const bool black = (moveIndex % 2 == 1);
        const QString teban = black ? QStringLiteral("▲") : QStringLiteral("△");

        // 終局/中断キーワード処理
        QString matched;
        if (containsAnyTerminal(moveText, &matched)) {
            KifDisplayItem item;
            item.prettyMove = teban + matched;

            // 仕様：千日手の消費時間は 0
            if (matched == QStringLiteral("千日手")) {
                item.timeText = QStringLiteral("00:00/00:00:00");
            } else {
                item.timeText = timeText.isEmpty() ? QStringLiteral("00:00/00:00:00") : timeText;
            }
            out.push_back(item);

            // 終局/中断以降は通常は終了（ここで break しても良い）
            // break;
            continue;
        }

        // 通常の指し手
        KifDisplayItem item;
        item.prettyMove = teban + moveText;                  // 例: "▲２六歩(27)"
        item.timeText   = timeText;                          // 例: "00:00/00:00:00"
        out.push_back(item);
    }
    return out;
}
