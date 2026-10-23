/// @file usitosfenconverter.cpp
/// @brief USI形式棋譜コンバータクラスの実装

#include "usitosfenconverter.h"
#include "shogiposition.h"
#include "terminalcodes.h"
#include "kiflinebuilder.h"
#include "kifreader.h"
#include "sfenutils.h"

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

// ---- SFEN / position 行のトークン判定 ----

static bool equalsKeyword(const QString& token, const char* keyword)
{
    return token.compare(QLatin1String(keyword), Qt::CaseInsensitive) == 0;
}

// 盤面トークン: SFEN の盤の欄として読める
static bool looksLikeSfenBoard(const QString& token)
{
    return Position::fromSfen(token + QStringLiteral(" b - 1")).has_value();
}

static bool isTurnToken(const QString& token)
{
    return equalsKeyword(token, "b") || equalsKeyword(token, "w");
}

// 持ち駒トークン: SFEN の持ち駒の欄として読める（"-" または枚数付き駒文字の並び。例: "2P3p", "S"）
static bool isHandsToken(const QString& token)
{
    return !token.isEmpty() && Position::fromSfen(QStringLiteral("9/9/9/9/9/9/9/9/9 b %1 1").arg(token)).has_value();
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
// 解析
// ============================================================================


KifuParseOutcome UsiToSfenConverter::parse(QStringView text, const ParseOptions& options)
{
    KifuParseOutcome outcome;
    ParseReport report(options);
    QString content;
    QString baseSfen;
    QStringList usiMoves;
    QString terminalCode;
    QString warn;
    if (!selectPositionLine(KifReader::splitLines(text.toString()), content, &warn)
        || !parseUsiPositionString(content, baseSfen, usiMoves, &terminalCode, &warn)) {
        outcome.warning = warn;
        return outcome;
    }
    if (!warn.isEmpty()) report.warn(warn);
    const std::optional<Position> start = Position::fromSfen(baseSfen);
    if (!start) {
        outcome.warning = QStringLiteral("Unknown USI position format: %1\n").arg(content.left(80));
        return outcome;
    }
    outcome.startKind = *start == Position::hirate() ? StartKind::Handicap : StartKind::Specified;
    outcome.record.mainline.startSfen = start->toSfen();
    // 指し手のない局面（.sfen ファイルなど）は局面だけを読む
    if (usiMoves.isEmpty() && terminalCode.isEmpty()) {
        outcome.success = true;
        outcome.positionOnly = true;
        outcome.warning = report.warnings();
        return outcome;
    }

    KifLineBuilder builder(*start);
    for (const QString& moveText : std::as_const(usiMoves)) {
        const int ply = builder.nextPly();
        const std::optional<UsiMove> move = UsiMove::parse(moveText);
        if (!move || !builder.addMove(*move)) {
            report.rejectMove(ply, move ? builder.cannotRestore(QStringLiteral("USI"), ply)
                                        : QStringLiteral("USI: Invalid USI move %1: %2").arg(ply).arg(moveText));
            builder.stop();
            break;
        }
    }
    if (!terminalCode.isEmpty()) builder.addTerminal(TerminalCodes::fromUsi(terminalCode), builder.sideToMove());
    outcome.warning = report.warnings();
    if (report.failed()) return outcome;
    outcome.record.mainline = builder.take();
    outcome.success = true;
    return outcome;
}

// ============================================================================
// 非公開メソッド
// ============================================================================

bool UsiToSfenConverter::selectPositionLine(const QStringList& lines, QString& content, QString* warn)
{
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
        if (TerminalCodes::isUsiCode(token)) {
            // 終局理由コード。以降は無視
            if (terminalCode) *terminalCode = token.toLower();
            break;
        }
        if (UsiMove::parse(token)) {
            usiMoves.append(token);
        } else if (warn) {
            *warn += QStringLiteral("Skipping unknown token: %1\n").arg(token);
        }
    }

    return true;
}
