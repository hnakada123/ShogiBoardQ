/// @file csaexporter.cpp
/// @brief CSA形式棋譜エクスポータクラスの実装

#include "csaexporter.h"
#include "csaformatter.h"
#include "gamerecordmodel.h"
#include "kifdisplayitem.h"
#include "kifparsetypes.h"
#include "logcategories.h"
#include "sfencsapositionconverter.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QStringList>

static const QRegularExpression& newlineRe()
{
    static const QRegularExpression re(QStringLiteral("\r?\n"));
    return re;
}

// ========================================
// CsaExporter 本体
// ========================================

QStringList CsaExporter::exportLines(const GameRecordModel& model,
                                     const GameRecordModel::ExportContext& ctx,
                                     const QStringList& usiMoves)
{
    QStringList out;

    // デバッグ: 入力データの確認
    qCDebug(lcKifu).noquote() << "toCsaLines: ========== CSA出力開始 ==========";
    qCDebug(lcKifu).noquote() << "toCsaLines: usiMoves.size() =" << usiMoves.size();
    if (!usiMoves.isEmpty()) {
        qCDebug(lcKifu).noquote() << "toCsaLines: usiMoves[0..min(5,size)] ="
                                  << usiMoves.mid(0, qMin(5, usiMoves.size()));
    }

    // 盤面追跡用オブジェクト
    CsaBoardTracker boardTracker;

    // 1) 文字コード宣言 (V3.0で追加)
    out << QStringLiteral("'CSA encoding=UTF-8");

    // 2) バージョン
    out << QStringLiteral("V3.0");

    // 3) 棋譜情報を先に収集（対局者名もここから取得）
    const QList<KifGameInfoItem> header = GameRecordModel::collectGameInfo(ctx);

    // 4) 対局者名
    QString blackPlayer, whitePlayer;
    for (const auto& it : header) {
        const QString key = it.key.trimmed();
        const QString val = it.value.trimmed();
        if (key == QStringLiteral("先手") && !val.isEmpty()) {
            blackPlayer = val;
        } else if (key == QStringLiteral("後手") && !val.isEmpty()) {
            whitePlayer = val;
        }
    }
    if (blackPlayer.isEmpty() || whitePlayer.isEmpty()) {
        QString resolvedBlack, resolvedWhite;
        GameRecordModel::resolvePlayerNames(ctx, resolvedBlack, resolvedWhite);
        if (blackPlayer.isEmpty()) blackPlayer = resolvedBlack;
        if (whitePlayer.isEmpty()) whitePlayer = resolvedWhite;
    }
    if (!blackPlayer.isEmpty()) {
        out << QStringLiteral("N+%1").arg(blackPlayer);
    }
    if (!whitePlayer.isEmpty()) {
        out << QStringLiteral("N-%1").arg(whitePlayer);
    }

    // 5) 棋譜情報（対局者名以外）
    bool hasStartTime = false;
    Q_UNUSED(hasStartTime);
    bool hasEndTime = false;
    Q_UNUSED(hasEndTime);
    bool hasTime = false;
    Q_UNUSED(hasTime);

    for (const auto& it : header) {
        const QString key = it.key.trimmed();
        const QString val = it.value.trimmed();
        if (key.isEmpty() || val.isEmpty()) continue;

        if (key == QStringLiteral("先手") || key == QStringLiteral("後手")) {
            continue;
        }

        if (key == QStringLiteral("棋戦")) {
            out << QStringLiteral("$EVENT:%1").arg(val);
        } else if (key == QStringLiteral("場所")) {
            out << QStringLiteral("$SITE:%1").arg(val);
        } else if (key == QStringLiteral("開始日時")) {
            QString csaDateTime = CsaFormatter::convertToCsaDateTime(val);
            out << QStringLiteral("$START_TIME:%1").arg(csaDateTime);
            hasStartTime = true;
        } else if (key == QStringLiteral("終了日時")) {
            QString csaDateTime = CsaFormatter::convertToCsaDateTime(val);
            out << QStringLiteral("$END_TIME:%1").arg(csaDateTime);
            hasEndTime = true;
        } else if (key == QStringLiteral("持ち時間")) {
            QString timeVal = CsaFormatter::convertToCsaTime(val);
            out << timeVal;
            hasTime = true;
        } else if (key == QStringLiteral("戦型")) {
            out << QStringLiteral("$OPENING:%1").arg(val);
        } else if (key == QStringLiteral("最大手数")) {
            out << QStringLiteral("$MAX_MOVES:%1").arg(val);
        } else if (key == QStringLiteral("持将棋")) {
            out << QStringLiteral("$JISHOGI:%1").arg(val);
        } else if (key == QStringLiteral("備考")) {
            QString noteVal = val;
            noteVal.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
            noteVal.replace(QStringLiteral("\n"), QStringLiteral("\\n"));
            out << QStringLiteral("$NOTE:%1").arg(noteVal);
        }
    }

    // 5-2) 棋譜情報がない場合のデフォルト生成
    if (!hasStartTime) {
        QString startTimeStr;
        if (ctx.gameStartDateTime.isValid()) {
            startTimeStr = ctx.gameStartDateTime.toString(QStringLiteral("yyyy/MM/dd HH:mm:ss"));
        } else {
            startTimeStr = QDateTime::currentDateTime().toString(QStringLiteral("yyyy/MM/dd HH:mm:ss"));
        }
        out << QStringLiteral("$START_TIME:%1").arg(startTimeStr);
    }

    if (!hasEndTime) {
        const QString nowStr = QDateTime::currentDateTime().toString(QStringLiteral("yyyy/MM/dd HH:mm:ss"));
        out << QStringLiteral("$END_TIME:%1").arg(nowStr);
    }

    if (!hasTime && ctx.hasTimeControl) {
        int initialSec = ctx.initialTimeMs / 1000;
        int byoyomiSec = ctx.byoyomiMs / 1000;
        int fischerSec = ctx.fischerIncrementMs / 1000;
        out << QStringLiteral("$TIME:%1+%2+%3").arg(initialSec).arg(byoyomiSec).arg(fischerSec);
    }

    // 6) 開始局面
    const QString defaultSfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    const bool isHirate = ctx.startSfen.isEmpty() || ctx.startSfen == defaultSfen
                          || ctx.startSfen.startsWith(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"));

    if (isHirate) {
        out << QStringLiteral("PI");
        out << QStringLiteral("+");
    } else {
        QString convErr;
        const auto csaPos =
            SfenCsaPositionConverter::toCsaPositionLines(ctx.startSfen, &convErr);
        if (csaPos && !csaPos->isEmpty()) {
            out << *csaPos;
            boardTracker.initFromSfen(ctx.startSfen);
        } else {
            qCWarning(lcKifu).noquote()
                << "SFEN->CSA position conversion failed. fallback to PI. error=" << convErr;
            out << QStringLiteral("PI");
            out << QStringLiteral("+");
        }
    }

    // 7) 本譜の指し手を収集
    const QList<KifDisplayItem> disp = model.collectMainlineForExport();

    qCDebug(lcKifu).noquote() << "toCsaLines: disp.size() =" << disp.size();
    for (qsizetype dbgIdx = 0; dbgIdx < qMin(qsizetype(10), disp.size()); ++dbgIdx) {
        qCDebug(lcKifu).noquote() << "toCsaLines: disp[" << dbgIdx << "].prettyMove ="
                                  << disp[dbgIdx].prettyMove;
    }

    // 開始局面のコメントを出力
    int startIdx = 0;
    if (!disp.isEmpty() && disp[0].prettyMove.trimmed().isEmpty()) {
        const QString cmt = disp[0].comment.trimmed();
        if (!cmt.isEmpty()) {
            const QStringList lines = cmt.split(newlineRe(), Qt::KeepEmptyParts);
            for (const QString& raw : lines) {
                QString t = raw.trimmed();
                if (t.isEmpty()) continue;

                if (t.startsWith(QLatin1Char('\''))) {
                    out << t;
                } else {
                    out << (QStringLiteral("'*") + t);
                }
            }
        }
        startIdx = 1;
        qCDebug(lcKifu).noquote() << "toCsaLines: 開始局面エントリあり、startIdx = 1";
    }

    // デバッグ: ループ開始前
    qCDebug(lcKifu).noquote() << "toCsaLines: ========== CSA変換ループ開始 ==========";
    qCDebug(lcKifu).noquote() << "toCsaLines: startIdx =" << startIdx
                              << ", disp.size() =" << disp.size()
                              << ", usiMoves.size() =" << usiMoves.size();

    // 8) 各指し手を出力
    int moveNo = 1;
    bool isSente = true;
    if (!ctx.startSfen.isEmpty()) {
        const QStringList sfenParts = ctx.startSfen.trimmed().split(QLatin1Char(' '));
        if (sfenParts.size() >= 2 && sfenParts[1] == QStringLiteral("w"))
            isSente = false;
    }
    QString terminalMove;
    int processedMoves = 0;
    int skippedNoUsi = 0;

    for (qsizetype i = startIdx; i < disp.size(); ++i) {
        const auto& it = disp[i];
        const QString moveText = it.prettyMove.trimmed();

        if (moveText.isEmpty()) {
            continue;
        }

        // 終局語の判定
        if (CsaFormatter::isTerminalMove(moveText)) {
            qCDebug(lcKifu).noquote() << "toCsaLines: 終局語検出: moveText =" << moveText;
            terminalMove = moveText;
            const QString resultCode = CsaFormatter::getCsaResultCode(moveText);
            if (!resultCode.isEmpty()) {
                out << resultCode;
                const int timeSec = CsaFormatter::extractCsaTimeSeconds(it.timeText);
                out << QStringLiteral("T%1").arg(timeSec);
            }
            break;
        }

        // USI指し手を取得
        QString usiMove;
        if (moveNo - 1 < usiMoves.size()) {
            usiMove = usiMoves.at(moveNo - 1);
        }

        qCDebug(lcKifu).noquote() << "toCsaLines: LOOP: i=" << i
                                  << " moveNo=" << moveNo
                                  << " isSente=" << isSente
                                  << " moveText='" << moveText << "'"
                                  << " usiMove='" << usiMove << "'";

        // CSA形式の指し手を構築
        QString csaMove;
        const QString sign = isSente ? QStringLiteral("+") : QStringLiteral("-");

        if (!usiMove.isEmpty()) {
            if (usiMove.size() >= 4 && usiMove.at(1) == QLatin1Char('*')) {
                const QString toSquare = usiMove.mid(2, 2);
                const int toFile = toSquare.at(0).toLatin1() - '0';
                const int toRank = toSquare.at(1).toLatin1() - 'a' + 1;
                const QString csaPiece = boardTracker.applyMove(usiMove, isSente);

                if (!csaPiece.isEmpty()) {
                    csaMove = QStringLiteral("%1%2%3%4%5")
                        .arg(sign, QStringLiteral("00"), QString::number(toFile),
                             QString::number(toRank), csaPiece);
                }
            } else if (usiMove.size() >= 4) {
                const QString fromSquare = usiMove.left(2);
                const QString toSquare = usiMove.mid(2, 2);
                const int fromFile = fromSquare.at(0).toLatin1() - '0';
                const int fromRank = fromSquare.at(1).toLatin1() - 'a' + 1;
                const int toFile = toSquare.at(0).toLatin1() - '0';
                const int toRank = toSquare.at(1).toLatin1() - 'a' + 1;
                const QString csaPiece = boardTracker.applyMove(usiMove, isSente);

                if (!csaPiece.isEmpty()) {
                    csaMove = QStringLiteral("%1%2%3%4%5%6")
                        .arg(sign, QString::number(fromFile), QString::number(fromRank),
                             QString::number(toFile), QString::number(toRank), csaPiece);
                }
            }
        }

        if (csaMove.isEmpty()) {
            ++skippedNoUsi;
            if (skippedNoUsi <= 5) {
                qCDebug(lcKifu).noquote() << "toCsaLines: スキップ(USIなし): i=" << i
                                          << " moveNo=" << moveNo
                                          << " moveText=" << moveText;
            }
            ++moveNo;
            isSente = !isSente;
            continue;
        }

        // 指し手を出力
        out << csaMove;
        const int timeSec = CsaFormatter::extractCsaTimeSeconds(it.timeText);
        out << QStringLiteral("T%1").arg(timeSec);
        ++processedMoves;

        // コメント出力
        const QString cmt = it.comment.trimmed();
        if (!cmt.isEmpty()) {
            const QStringList lines = cmt.split(newlineRe(), Qt::KeepEmptyParts);
            for (const QString& raw : lines) {
                QString t = raw.trimmed();
                if (t.isEmpty()) continue;

                if (t.startsWith(QLatin1Char('\''))) {
                    out << t;
                } else {
                    out << (QStringLiteral("'*") + t);
                }
            }
        }

        ++moveNo;
        isSente = !isSente;
    }

    // デバッグ: 結果サマリ
    qCDebug(lcKifu).noquote() << "toCsaLines: ========== CSA出力終了 ==========";
    qCDebug(lcKifu).noquote() << "toCsaLines: processedMoves =" << processedMoves
                              << ", skippedNoUsi =" << skippedNoUsi;
    qCDebug(lcKifu).noquote() << "toCsaLines: generated"
                              << out.size() << "lines,"
                              << (moveNo - 1) << "moves";

    return out;
}
