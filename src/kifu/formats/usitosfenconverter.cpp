/// @file usitosfenconverter.cpp
/// @brief USI形式棋譜コンバータクラスの実装

#include "usitosfenconverter.h"
#include "parsecommon.h"
#include "kifreader.h"

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

// 平手初期局面のSFEN
static const char* kHirateSfen = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1";

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

} // namespace

// ============================================================================
// 公開メソッド
// ============================================================================

QString UsiToSfenConverter::detectInitialSfenFromFile(const QString& usiPath, QString* detectedLabel)
{
    QString content;
    QString warn;
    if (!readUsiFile(usiPath, content, &warn)) {
        if (detectedLabel) *detectedLabel = QStringLiteral("平手(既定)");
        return QString::fromLatin1(kHirateSfen);
    }

    QString baseSfen;
    QStringList usiMoves;
    QString terminalCode;

    if (!parseUsiPositionString(content, baseSfen, usiMoves, &terminalCode, &warn)) {
        if (detectedLabel) *detectedLabel = QStringLiteral("平手(既定)");
        return QString::fromLatin1(kHirateSfen);
    }

    // 平手初期局面かどうかを判定
    if (baseSfen == QString::fromLatin1(kHirateSfen)) {
        if (detectedLabel) *detectedLabel = QStringLiteral("平手");
    } else {
        if (detectedLabel) *detectedLabel = QStringLiteral("局面指定");
    }

    return baseSfen;
}

QStringList UsiToSfenConverter::convertFile(const QString& usiPath, QString* errorMessage)
{
    QString content;
    if (!readUsiFile(usiPath, content, errorMessage)) {
        return QStringList();
    }

    QString baseSfen;
    QStringList usiMoves;
    QString terminalCode;

    if (!parseUsiPositionString(content, baseSfen, usiMoves, &terminalCode, errorMessage)) {
        return QStringList();
    }

    return usiMoves;
}

QList<KifDisplayItem> UsiToSfenConverter::extractMovesWithTimes(const QString& usiPath, QString* errorMessage)
{
    QList<KifDisplayItem> out;

    QString content;
    if (!readUsiFile(usiPath, content, errorMessage)) {
        return out;
    }

    QString baseSfen;
    QStringList usiMoves;
    QString terminalCode;

    if (!parseUsiPositionString(content, baseSfen, usiMoves, &terminalCode, errorMessage)) {
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

    QString content;
    if (!readUsiFile(usiPath, content, errorMessage)) {
        return false;
    }

    QString baseSfen;
    QStringList usiMoves;
    QString terminalCode;

    if (!parseUsiPositionString(content, baseSfen, usiMoves, &terminalCode, errorMessage)) {
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

bool UsiToSfenConverter::readUsiFile(const QString& filePath, QString& content, QString* warn)
{
    QStringList lines;
    QString usedEnc;
    if (!KifReader::readAllLinesAuto(filePath, lines, &usedEnc, warn)) {
        return false;
    }

    // 複数行を連結（空行や空白のみの行は無視）
    QStringList nonEmptyLines;
    for (const QString& line : std::as_const(lines)) {
        const QString trimmed = line.trimmed();
        if (!trimmed.isEmpty()) {
            nonEmptyLines.append(trimmed);
        }
    }

    // 最初の非空行を使用
    if (nonEmptyLines.isEmpty()) {
        if (warn) *warn += QStringLiteral("Empty USI file\n");
        return false;
    }

    content = nonEmptyLines.first();
    return true;
}

bool UsiToSfenConverter::parseUsiPositionString(const QString& usiStr,
                                                 QString& baseSfen,
                                                 QStringList& usiMoves,
                                                 QString* terminalCode,
                                                 QString* warn)
{
    Q_UNUSED(warn)

    QString str = usiStr.trimmed();

    // "position" キーワードがあれば除去
    if (str.startsWith(QStringLiteral("position"), Qt::CaseInsensitive)) {
        str = str.mid(8).trimmed();
    }

    // startpos または sfen で始まるか判定
    int sfenEndIndex = 0;

    if (str.startsWith(QStringLiteral("startpos"), Qt::CaseInsensitive)) {
        baseSfen = QString::fromLatin1(kHirateSfen);
        sfenEndIndex = 8;  // "startpos" の長さ
    } else if (str.startsWith(QStringLiteral("sfen"), Qt::CaseInsensitive)) {
        // SFEN形式: "sfen <board> <stm> <hands> <ply> [moves ...]"
        // "sfen " を除去
        const QString rest = str.mid(5).trimmed();

        // "moves" キーワードの位置を探す
        const qsizetype movesIdx = rest.indexOf(QStringLiteral(" moves "), Qt::CaseInsensitive);

        if (movesIdx >= 0) {
            // "moves" より前がSFEN
            baseSfen = rest.left(movesIdx).trimmed();
            sfenEndIndex = static_cast<int>(5 + movesIdx);  // "sfen " + SFEN部分
        } else {
            // "moves" がない場合、全体がSFEN（指し手なし）
            baseSfen = rest;
            sfenEndIndex = static_cast<int>(str.size());
        }
    } else {
        // 不明な形式 → デフォルトで平手初期局面
        if (warn) *warn += QStringLiteral("Unknown USI position format, defaulting to startpos\n");
        baseSfen = QString::fromLatin1(kHirateSfen);

        // "moves" があるか確認
        const qsizetype movesIdx = str.indexOf(QStringLiteral("moves"), Qt::CaseInsensitive);
        if (movesIdx >= 0) {
            sfenEndIndex = static_cast<int>(movesIdx);
        } else {
            sfenEndIndex = 0;
        }
    }

    // "moves" 以降を解析
    QString movesStr = str.mid(sfenEndIndex).trimmed();
    if (movesStr.startsWith(QStringLiteral("moves"), Qt::CaseInsensitive)) {
        movesStr = movesStr.mid(5).trimmed();  // "moves" を除去
    }

    // 指し手をスペースで分割
    usiMoves.clear();
    if (terminalCode) terminalCode->clear();

    if (!movesStr.isEmpty()) {
        static const QRegularExpression kWhitespaceRe(QStringLiteral("\\s+"));
        const QStringList tokens = movesStr.split(kWhitespaceRe, Qt::SkipEmptyParts);

        for (const QString& token : std::as_const(tokens)) {
            if (isTerminalCode(token)) {
                // 終局理由コード
                if (terminalCode) *terminalCode = token.toLower();
                break;  // 終局理由以降は無視
            } else if (isValidUsiMove(token)) {
                usiMoves.append(token);
            } else {
                // 不明なトークンは無視（または警告）
                if (warn) {
                    *warn += QStringLiteral("Skipping unknown token: %1\n").arg(token);
                }
            }
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
