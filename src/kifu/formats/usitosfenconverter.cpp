/// @file usitosfenconverter.cpp
/// @brief USI形式棋譜コンバータクラスの実装

#include "usitosfenconverter.h"
#include "shogiposition.h"
#include "terminalcodes.h"
#include "kiflinebuilder.h"
#include "kifutextdecoder.h"
#include "usiposition.h"

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

// 行が USI position / SFEN として読めるか（ファイル内の対象行の選択に使う。語の分け方は UsiPosition::split）
bool isPositionLine(const QString& line)
{
    const std::optional<UsiPosition::Words> words = UsiPosition::split(line);
    return words && Position::fromSfen(words->completeStart()).has_value();
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
    QString warn;
    if (!selectPositionLine(KifuTextDecoder::splitLines(text.toString()), content, &warn)) {
        outcome.warning = warn;
        return outcome;
    }
    // 開始局面と後の語の分け方は position コマンドと同じ（UsiPosition::split）
    const std::optional<UsiPosition::Words> words = UsiPosition::split(content);
    const std::optional<Position> start = words ? Position::fromSfen(words->completeStart()) : std::nullopt;
    if (!start) {
        // 入力の中身は文に入れない（棋譜でないファイルの先頭を、エラー文で読み出せてしまう。セキュリティ監査 V7）
        outcome.warning = QStringLiteral("Unknown USI position format\n");
        return outcome;
    }
    outcome.startKind = *start == Position::hirate() ? StartKind::Handicap : StartKind::Specified;
    outcome.record.mainline.startSfen = start->toSfen();
    // 指し手のない局面（.sfen ファイルなど）は局面だけを読む
    if (words->moves.isEmpty()) {
        outcome.success = true;
        outcome.positionOnly = true;
        return outcome;
    }

    // 指し手の列。終局理由のコードの後は読まない。読めない語・指せない手は、その手の手前で打ち切る（全形式で同じ。
    // 読めない語を飛ばして続けると、後の手が1手ずれる）
    KifLineBuilder builder(*start);
    for (const QString& token : words->moves) {
        if (TerminalCodes::isUsiCode(token)) {
            builder.addTerminal(TerminalCodes::fromUsi(token), builder.sideToMove());
            break;
        }
        const int ply = builder.nextPly();
        const std::optional<UsiMove> move = UsiMove::parse(token);
        if (!move || !builder.addMove(*move)) {
            report.rejectMove(ply, move ? builder.cannotRestore(QStringLiteral("USI"), ply)
                                        : QStringLiteral("USI: Invalid USI move %1: %2").arg(ply).arg(token));
            builder.stop();
            break;
        }
        builder.warnIrregularity(report, QStringLiteral("USI"), ply);
    }
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
        if (isPositionLine(trimmed)) {
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
