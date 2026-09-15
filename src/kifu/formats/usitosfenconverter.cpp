/// @file usitosfenconverter.cpp
/// @brief USI形式棋譜コンバータクラスの実装

#include "usitosfenconverter.h"
#include "parsecommon.h"
#include "kifreader.h"
#include "sfenutils.h"

#include <QFile>
#include <QRegularExpression>

// ============================================================================
// USI position コマンド文字列デコーダ
// ============================================================================
//
// USI仕様: https://shogidokoro2.stars.ne.jp/usi.html
//
// position コマンドの形式:
// - position [sfen <sfenstring> | startpos ] moves <move1> ... <movei>
//
// 対応フォーマット:
// 1. 平手初期局面から指し手列:
//    position startpos moves 2g2f 3c3d 7g7f ...
//
// 2. SFEN形式の任意局面から指し手列:
//    position sfen lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1 moves 2g2f 3c3d ...
//
// 3. position キーワードが省略されている場合:
//    startpos moves 2g2f 3c3d ...
//    sfen lnsgkgsnl/... b - 1 moves 2g2f 3c3d ...
//
// 4. 局面のみ（.sfen ファイルなど）。sfen 接頭辞と手数は省略可:
//    lnsgkgsnl/... b - 1
//
// 指し手の形式:
// - 通常移動: <from><to>     例: 7g7f (７七から７六へ)
// - 成る場合: <from><to>+    例: 8h2b+ (８八から２二へ成る)
// - 駒打ち:   <piece>*<to>   例: P*5e (歩を５五に打つ)
//
// 座標:
// - 筋: 1-9 (数字)
// - 段: a-i (アルファベット小文字, a=1段, i=9段)
//
// 終局理由コード (ShogiGUIなど):
// - resign    : 投了
// - break     : 中断
// - rep_draw  : 千日手
// - draw      : 引き分け
// - timeout   : 時間切れ
// - win       : 入玉勝ち
// ============================================================================

namespace {

// 終局理由コードのマッピング
struct TerminalInfo {
    const char* code;
    const char* japanese;
};

static const TerminalInfo kTerminalCodes[] = {
    {"resign",     "投了"},
    {"break",      "中断"},
    {"rep_draw",   "千日手"},
    {"draw",       "引き分け"},
    {"timeout",    "時間切れ"},
    {"win",        "入玉勝ち"},
    {"lose",       "反則負け"},
    {"sennichite", "千日手"},
    {"checkmate",  "詰み"},
    {nullptr, nullptr}
};

// 終局理由コードかどうかを判定（kTerminalCodesを参照して一貫性を保つ）
static bool isTerminalCode(const QString& str)
{
    const QString lower = str.toLower();
    for (int i = 0; kTerminalCodes[i].code != nullptr; ++i) {
        if (lower == QString::fromLatin1(kTerminalCodes[i].code)) {
            return true;
        }
    }
    return false;
}

// USI指し手として有効かどうかを簡易判定
static bool isValidUsiMove(const QString& str)
{
    if (str.isEmpty()) return false;

    // 駒打ち: P*5e (長さ4)
    if (str.size() == 4 && str.at(1) == QChar('*')) {
        const QChar piece = str.at(0).toUpper();
        if (QStringLiteral("PLNSGBR").contains(piece)) {
            const QChar toFile = str.at(2);
            const QChar toRank = str.at(3);
            if (toFile >= '1' && toFile <= '9' && toRank >= 'a' && toRank <= 'i') {
                return true;
            }
        }
        return false;
    }

    // 通常移動: 7g7f (長さ4) または 7g7f+ (長さ5)
    if (str.size() >= 4 && str.size() <= 5) {
        const QChar fromFile = str.at(0);
        const QChar fromRank = str.at(1);
        const QChar toFile = str.at(2);
        const QChar toRank = str.at(3);

        if (fromFile >= '1' && fromFile <= '9' &&
            fromRank >= 'a' && fromRank <= 'i' &&
            toFile >= '1' && toFile <= '9' &&
            toRank >= 'a' && toRank <= 'i') {
            if (str.size() == 5 && str.at(4) != QChar('+')) {
                return false;
            }
            return true;
        }
    }

    return false;
}

// ---- SFEN / position 行のトークン判定 ----

static bool equalsKeyword(const QString& token, const char* keyword)
{
    return token.compare(QLatin1String(keyword), Qt::CaseInsensitive) == 0;
}

// 盤面トークン: '/' 区切りの 9 段で、駒文字・数字・'+' のみ
static bool looksLikeSfenBoard(const QString& token)
{
    if (token.count(QLatin1Char('/')) != 8) return false;
    static const QString kAllowed = QStringLiteral("lnsgkrpbLNSGKRPB123456789+/");
    for (const QChar c : token) {
        if (!kAllowed.contains(c)) return false;
    }
    return true;
}

static bool isTurnToken(const QString& token)
{
    return equalsKeyword(token, "b") || equalsKeyword(token, "w");
}

// 持ち駒トークン: "-" または枚数付き駒文字の並び（例: "2P3p", "S"）
static bool isHandsToken(const QString& token)
{
    if (token == QStringLiteral("-")) return true;
    if (token.isEmpty()) return false;
    static const QString kAllowed = QStringLiteral("PLNSGBRplnsgbr0123456789");
    for (const QChar c : token) {
        if (!kAllowed.contains(c)) return false;
    }
    return true;
}

static bool isPlyToken(const QString& token)
{
    if (token.isEmpty()) return false;
    for (const QChar c : token) {
        if (c < QLatin1Char('0') || c > QLatin1Char('9')) return false;
    }
    return true;
}

static QStringList splitTokens(const QString& text)
{
    static const QRegularExpression kWhitespaceRe(QStringLiteral("\\s+"));
    return text.trimmed().split(kWhitespaceRe, Qt::SkipEmptyParts);
}

// 行が USI position / SFEN として解釈できそうか（ファイル内の対象行の選択に使う）
static bool looksLikePositionLine(const QString& line)
{
    const QStringList tokens = splitTokens(line);
    qsizetype i = 0;
    if (i < tokens.size() && equalsKeyword(tokens.at(i), "position")) ++i;
    if (i >= tokens.size()) return false;
    if (equalsKeyword(tokens.at(i), "startpos") || equalsKeyword(tokens.at(i), "sfen")) return true;
    return looksLikeSfenBoard(tokens.at(i));
}

} // namespace

// ============================================================================
// 公開メソッド
// ============================================================================

QString UsiToSfenConverter::detectInitialSfenFromFile(const QString& usiPath, QString* detectedLabel)
{
    QString baseSfen;
    QStringList usiMoves;
    QString terminalCode;
    QString warn;

    if (!parseUsiFile(usiPath, baseSfen, usiMoves, &terminalCode, &warn)) {
        if (detectedLabel) *detectedLabel = QStringLiteral("平手(既定)");
        return SfenUtils::hirateSfen();
    }

    // 平手初期局面かどうかを判定
    if (baseSfen == SfenUtils::hirateSfen()) {
        if (detectedLabel) *detectedLabel = QStringLiteral("平手");
    } else {
        if (detectedLabel) *detectedLabel = QStringLiteral("局面指定");
    }

    return baseSfen;
}

QStringList UsiToSfenConverter::convertFile(const QString& usiPath, QString* errorMessage)
{
    QString baseSfen;
    QStringList usiMoves;
    QString terminalCode;

    if (!parseUsiFile(usiPath, baseSfen, usiMoves, &terminalCode, errorMessage)) {
        return QStringList();
    }

    return usiMoves;
}

QList<KifDisplayItem> UsiToSfenConverter::extractMovesWithTimes(const QString& usiPath, QString* errorMessage)
{
    QList<KifDisplayItem> out;

    QString baseSfen;
    QStringList usiMoves;
    QString terminalCode;

    if (!parseUsiFile(usiPath, baseSfen, usiMoves, &terminalCode, errorMessage)) {
        return out;
    }

    // 開始局面エントリ
    out.push_back(KifuParseCommon::createOpeningDisplayItem(QString(), QString()));

    // 共通パイプラインで指し手アイテムを構築
    int plyNumber = KifuParseCommon::buildUsiMoveDisplayItems(usiMoves, baseSfen, 1, out);

    // 終局理由があれば追加
    if (!terminalCode.isEmpty()) {
        out.push_back(KifuParseCommon::createTerminalDisplayItem(
            plyNumber + 1, terminalCodeToJapanese(terminalCode)));
    }

    return out;
}

bool UsiToSfenConverter::parseWithVariations(const QString& usiPath,
                                              KifParseResult& out,
                                              QString* errorMessage)
{
    out = KifParseResult{};

    QString baseSfen;
    QStringList usiMoves;
    QString terminalCode;

    if (!parseUsiFile(usiPath, baseSfen, usiMoves, &terminalCode, errorMessage)) {
        return false;
    }

    // 本譜のメタデータを設定
    out.mainline.baseSfen = baseSfen;
    out.mainline.startPly = 1;
    out.mainline.usiMoves = usiMoves;

    // 開始局面エントリ
    out.mainline.disp.push_back(KifuParseCommon::createOpeningDisplayItem(QString(), QString()));

    // 共通パイプラインで指し手アイテムを構築
    int plyNumber = KifuParseCommon::buildUsiMoveDisplayItems(
        usiMoves, baseSfen, 1, out.mainline.disp);

    // 終局理由があれば追加
    if (!terminalCode.isEmpty()) {
        out.mainline.disp.push_back(KifuParseCommon::createTerminalDisplayItem(
            plyNumber + 1, terminalCodeToJapanese(terminalCode)));
        out.mainline.endsWithTerminal = true;
    }

    // USI形式では変化(分岐)は通常含まれないため、variationsは空のまま

    return true;
}

QList<KifGameInfoItem> UsiToSfenConverter::extractGameInfo(const QString& filePath)
{
    Q_UNUSED(filePath)
    // USI形式はメタ情報を含まないため空を返す
    return QList<KifGameInfoItem>();
}

QMap<QString, QString> UsiToSfenConverter::extractGameInfoMap(const QString& filePath)
{
    Q_UNUSED(filePath)
    // USI形式はメタ情報を含まないため空を返す
    return QMap<QString, QString>();
}

QString UsiToSfenConverter::terminalCodeToJapanese(const QString& code)
{
    for (int i = 0; kTerminalCodes[i].code != nullptr; ++i) {
        if (code.compare(QString::fromLatin1(kTerminalCodes[i].code), Qt::CaseInsensitive) == 0) {
            return QString::fromUtf8(kTerminalCodes[i].japanese);
        }
    }
    // 不明なコードはそのまま返す
    return code;
}

// ============================================================================
// 非公開メソッド
// ============================================================================

bool UsiToSfenConverter::parseUsiFile(const QString& usiPath,
                                      QString& baseSfen,
                                      QStringList& usiMoves,
                                      QString* terminalCode,
                                      QString* warn)
{
    QString content;
    if (!readUsiFile(usiPath, content, warn)) {
        return false;
    }
    return parseUsiPositionString(content, baseSfen, usiMoves, terminalCode, warn);
}

bool UsiToSfenConverter::readUsiFile(const QString& filePath, QString& content, QString* warn)
{
    QStringList lines;
    QString usedEnc;
    if (!KifReader::readAllLinesAuto(filePath, lines, &usedEnc, warn)) {
        return false;
    }

    // 最初の position/SFEN らしき行を対象にする（コメント行などは読み飛ばす）。
    // 見つからなければ最初の非空行を返し、形式エラーは解析側で報告する。
    QString firstNonEmpty;
    for (const QString& line : std::as_const(lines)) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) continue;
        if (looksLikePositionLine(trimmed)) {
            content = trimmed;
            return true;
        }
        if (firstNonEmpty.isEmpty()) firstNonEmpty = trimmed;
    }

    if (firstNonEmpty.isEmpty()) {
        if (warn) *warn += QStringLiteral("Empty USI file\n");
        return false;
    }

    content = firstNonEmpty;
    return true;
}

bool UsiToSfenConverter::parseUsiPositionString(const QString& usiStr,
                                                 QString& baseSfen,
                                                 QStringList& usiMoves,
                                                 QString* terminalCode,
                                                 QString* warn)
{
    baseSfen.clear();
    usiMoves.clear();
    if (terminalCode) terminalCode->clear();

    const QStringList tokens = splitTokens(usiStr);
    qsizetype i = 0;

    // "position" は省略可
    if (i < tokens.size() && equalsKeyword(tokens.at(i), "position")) ++i;

    // 開始局面: startpos | [sfen] <board> <turn> <hands> [<ply>]
    if (i < tokens.size() && equalsKeyword(tokens.at(i), "startpos")) {
        baseSfen = SfenUtils::hirateSfen();
        ++i;
    } else {
        if (i < tokens.size() && equalsKeyword(tokens.at(i), "sfen")) ++i;

        const bool hasSfenFields = (i + 3 <= tokens.size())
                                   && looksLikeSfenBoard(tokens.at(i))
                                   && isTurnToken(tokens.at(i + 1))
                                   && isHandsToken(tokens.at(i + 2));
        if (!hasSfenFields) {
            if (warn) {
                *warn += QStringLiteral("Unknown USI position format: %1\n")
                             .arg(usiStr.trimmed().left(80));
            }
            return false;
        }

        const QString board = tokens.at(i);
        const QString turn = tokens.at(i + 1).toLower();
        const QString hands = tokens.at(i + 2);
        i += 3;

        // 手数は省略可（既定 1）
        QString ply = QStringLiteral("1");
        if (i < tokens.size() && isPlyToken(tokens.at(i))) {
            ply = tokens.at(i);
            ++i;
        }
        baseSfen = QStringLiteral("%1 %2 %3 %4").arg(board, turn, hands, ply);
    }

    // 指し手列（"moves" キーワードは省略可）
    if (i < tokens.size() && equalsKeyword(tokens.at(i), "moves")) ++i;

    for (; i < tokens.size(); ++i) {
        const QString& token = tokens.at(i);
        if (isTerminalCode(token)) {
            // 終局理由コード。以降は無視
            if (terminalCode) *terminalCode = token.toLower();
            break;
        }
        if (isValidUsiMove(token)) {
            usiMoves.append(token);
        } else if (warn) {
            *warn += QStringLiteral("Skipping unknown token: %1\n").arg(token);
        }
    }

    return true;
}

void UsiToSfenConverter::buildKifLine(const QStringList& usiMoves,
                                       const QString& baseSfen,
                                       int startPly,
                                       const QString& terminalCode,
                                       KifLine& outLine,
                                       QString* warn)
{
    Q_UNUSED(warn)

    outLine.startPly = startPly;
    outLine.usiMoves = usiMoves;
    outLine.disp.clear();

    // 共通パイプラインで指し手アイテムを構築
    int plyNumber = KifuParseCommon::buildUsiMoveDisplayItems(
        usiMoves, baseSfen, startPly, outLine.disp);

    // 終局理由
    if (!terminalCode.isEmpty()) {
        outLine.disp.push_back(KifuParseCommon::createTerminalDisplayItem(
            plyNumber + 1, terminalCodeToJapanese(terminalCode)));
        outLine.endsWithTerminal = true;
    }
}
