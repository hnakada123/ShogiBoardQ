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
    // 中断 → break (将棋所の仕様)
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
    // 切れ負け → timeout
    if (stripped.contains(QStringLiteral("切れ負け"))) {
        return QStringLiteral("timeout");
    }
    // 入玉勝ち → win
    if (stripped.contains(QStringLiteral("入玉勝ち"))) {
        return QStringLiteral("win");
    }
    // 詰み → mate (カスタム)
    if (stripped.contains(QStringLiteral("詰み"))) {
        return QStringLiteral("mate");
    }
    // 反則勝ち/反則負け → illegal (カスタム)
    if (stripped.contains(QStringLiteral("反則"))) {
        return QStringLiteral("illegal");
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
    QStringList out;

    // 1) 初期局面を判定
    // 平手初期局面のSFEN（手数部分は省略して比較）
    static const QString HIRATE_SFEN = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b -");

    QString positionStr;

    // 初期SFENが空または平手初期局面の場合は "startpos" を使用
    if (ctx.startSfen.isEmpty()) {
        positionStr = QStringLiteral("position startpos");
    } else {
        // SFENの手数部分（最後のスペースと数字）を除いて比較
        QString sfenWithoutMoveNum = ctx.startSfen;
        const qsizetype lastSpace = ctx.startSfen.lastIndexOf(QLatin1Char(' '));
        if (lastSpace > 0) {
            const QString lastPart = ctx.startSfen.mid(lastSpace + 1);
            bool isNumber = false;
            lastPart.toInt(&isNumber);
            if (isNumber) {
                sfenWithoutMoveNum = ctx.startSfen.left(lastSpace);
            }
        }

        if (sfenWithoutMoveNum == HIRATE_SFEN) {
            positionStr = QStringLiteral("position startpos");
        } else {
            // 駒落ち等の場合は sfen を使用
            positionStr = QStringLiteral("position sfen %1").arg(ctx.startSfen);
        }
    }

    // 2) USI moves
    QStringList mainlineUsi = usiMoves;

    // 3) 終局コードを取得
    QString terminalCode;
    const QList<KifDisplayItem> disp = model.collectMainlineForExport();
    if (!disp.isEmpty()) {
        const QString& lastMove = disp.last().prettyMove;
        if (isTerminalMove(lastMove)) {
            terminalCode = getUsiTerminalCode(lastMove);
        }
    }

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
