/// @file usiexporter.cpp
/// @brief USI形式棋譜エクスポータクラスの実装

#include "usiexporter.h"
#include "logcategories.h"

// ========================================
// ヘルパ関数
// ========================================

// ヘルパ関数: 指し手から手番記号（▲△）を除去
static QString removeTurnMarker(const QString& move)
{
    QString result = move;
    if (result.startsWith(QStringLiteral("▲")) || result.startsWith(QStringLiteral("△"))) {
        result = result.mid(1);
    }
    return result;
}

// ヘルパ関数: 終局語を判定
static bool isTerminalMove(const QString& move)
{
    static const QStringList terminals = {
        QStringLiteral("投了"),
        QStringLiteral("中断"),
        QStringLiteral("持将棋"),
        QStringLiteral("千日手"),
        QStringLiteral("切れ負け"),
        QStringLiteral("時間切れ"),  // 対局で時間切れになったときの終局語
        QStringLiteral("反則勝ち"),
        QStringLiteral("反則負け"),
        QStringLiteral("入玉勝ち"),
        QStringLiteral("不戦勝"),
        QStringLiteral("不戦敗"),
        QStringLiteral("詰み"),
        QStringLiteral("不詰")
    };
    const QString stripped = removeTurnMarker(move);
    for (const QString& t : terminals) {
        if (stripped.contains(t)) return true;
    }
    return false;
}

static QString getUsiTerminalCode(const QString& terminalMove)
{
    const QString stripped = removeTurnMarker(terminalMove);

    // 投了 → resign
    if (stripped.contains(QStringLiteral("投了"))) {
        return QStringLiteral("resign");
    }
    // 以下の終局コードは棋譜ファイル用の拡張であり、USIのposition通信には含めない。
    if (stripped.contains(QStringLiteral("中断"))) {
        return QStringLiteral("break");
    }
    // 千日手 → rep_draw
    if (stripped.contains(QStringLiteral("千日手"))) {
        return QStringLiteral("rep_draw");
    }
    // 持将棋・引き分け → draw
    if (stripped.contains(QStringLiteral("持将棋")) || stripped.contains(QStringLiteral("引き分け"))) {
        return QStringLiteral("draw");
    }
    // 切れ負け（対局の記録では「時間切れ」） → timeout
    if (stripped.contains(QStringLiteral("切れ負け")) || stripped.contains(QStringLiteral("時間切れ"))) {
        return QStringLiteral("timeout");
    }
    // 入玉勝ち → win
    if (stripped.contains(QStringLiteral("入玉勝ち"))) {
        return QStringLiteral("win");
    }
    // 詰み（棋譜ファイル用の拡張）
    if (stripped.contains(QStringLiteral("詰み"))) {
        return QStringLiteral("checkmate");
    }
    if (stripped.contains(QStringLiteral("反則勝ち"))) {
        return QStringLiteral("illegal_win");
    }
    if (stripped.contains(QStringLiteral("反則負け"))) {
        return QStringLiteral("lose");
    }

    return QString();
}

// ========================================
// エクスポート
// ========================================

QStringList UsiExporter::exportLines(const GameRecordModel& model,
                                     const GameRecordModel::ExportContext& ctx,
                                     const QStringList& usiMoves)
{
    const QList<KifDisplayItem> disp = model.collectMainlineForExport();
    return exportPosition(ctx.startSfen, usiMoves, disp.isEmpty() ? QString() : disp.last().prettyMove);
}

QStringList UsiExporter::exportPosition(const QString& startSfen, const QStringList& usiMoves,
                                        const QString& terminalMove)
{
    QStringList out;

    // 1) 初期局面を判定
    // 平手初期局面のSFEN
    static const QString HIRATE_SFEN = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

    QString positionStr;

    // 初期SFENが空または平手初期局面の場合は "startpos" を使用
    if (startSfen.isEmpty() || startSfen == HIRATE_SFEN) {
        positionStr = QStringLiteral("position startpos");
    } else {
        // 元のSFEN手数も保持する。
        positionStr = QStringLiteral("position sfen %1").arg(startSfen);
    }

    // 2) USI moves
    QStringList mainlineUsi = usiMoves;

    // 3) 終局コードを取得
    QString terminalCode;
    if (isTerminalMove(terminalMove)) terminalCode = getUsiTerminalCode(terminalMove);

    // 4) position コマンド文字列を構築
    QString usiLine = positionStr;

    if (!mainlineUsi.isEmpty()) {
        usiLine += QStringLiteral(" moves");
        for (const QString& move : std::as_const(mainlineUsi)) {
            usiLine += QStringLiteral(" ") + move;
        }
    }

    // 5) 終局コードを追加（指し手の後にスペース区切りで）
    if (!terminalCode.isEmpty()) {
        usiLine += QStringLiteral(" ") + terminalCode;
    }

    out << usiLine;

    qCDebug(lcKifu).noquote() << "toUsiLines: generated USI position command with"
                              << mainlineUsi.size() << "moves,"
                              << "terminal:" << terminalCode;

    return out;
}
