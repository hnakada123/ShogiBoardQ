/// @file csaexporter.cpp
/// @brief CSA形式棋譜エクスポータクラスの実装

#include "csaexporter.h"
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
// ヘルパ関数（CSAエクスポート専用）
// ========================================

// 指し手から手番記号（▲△）を除去
static QString removeTurnMarker(const QString& move)
{
    QString result = move;
    if (result.startsWith(QStringLiteral("▲")) || result.startsWith(QStringLiteral("△"))) {
        result = result.mid(1);
    }
    return result;
}

// 終局語を判定
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

// 時間テキストからCSA形式の消費時間（秒）を抽出
static int extractCsaTimeSeconds(const QString& timeText)
{
    // timeText: "mm:ss/HH:MM:SS" または "(mm:ss/HH:MM:SS)"
    QString text = timeText.trimmed();

    // 括弧を除去
    if (text.startsWith(QLatin1Char('('))) text = text.mid(1);
    if (text.endsWith(QLatin1Char(')'))) text.chop(1);

    // "mm:ss/..." の部分を取得
    const qsizetype slashIdx = text.indexOf(QLatin1Char('/'));
    if (slashIdx < 0) return 0;

    const QString moveTime = text.left(slashIdx).trimmed();
    const QStringList parts = moveTime.split(QLatin1Char(':'));
    if (parts.size() != 2) return 0;

    bool ok1, ok2;
    const int minutes = parts[0].toInt(&ok1);
    const int seconds = parts[1].toInt(&ok2);
    if (!ok1 || !ok2) return 0;

    return minutes * 60 + seconds;
}

// CSA形式の終局コードを取得
static QString getCsaResultCode(const QString& terminalMove)
{
    const QString move = removeTurnMarker(terminalMove);

    if (move.contains(QStringLiteral("投了"))) return QStringLiteral("%TORYO");
    if (move.contains(QStringLiteral("中断"))) return QStringLiteral("%CHUDAN");
    if (move.contains(QStringLiteral("千日手"))) return QStringLiteral("%SENNICHITE");
    if (move.contains(QStringLiteral("持将棋"))) return QStringLiteral("%JISHOGI");
    if (move.contains(QStringLiteral("切れ負け")) || move.contains(QStringLiteral("時間切れ")))
        return QStringLiteral("%TIME_UP");
    if (move.contains(QStringLiteral("反則負け"))) return QStringLiteral("%ILLEGAL_MOVE");
    // V3.0: 反則行為（手番側の勝ちを表現）
    if (move.contains(QStringLiteral("先手の反則"))) return QStringLiteral("%+ILLEGAL_ACTION");
    if (move.contains(QStringLiteral("後手の反則"))) return QStringLiteral("%-ILLEGAL_ACTION");
    if (move.contains(QStringLiteral("反則"))) return QStringLiteral("%ILLEGAL_MOVE");
    if (move.contains(QStringLiteral("入玉勝ち")) || move.contains(QStringLiteral("宣言勝ち")))
        return QStringLiteral("%KACHI");
    if (move.contains(QStringLiteral("引き分け"))) return QStringLiteral("%HIKIWAKE");
    // V3.0で追加: 最大手数
    if (move.contains(QStringLiteral("最大手数"))) return QStringLiteral("%MAX_MOVES");
    if (move.contains(QStringLiteral("詰み"))) return QStringLiteral("%TSUMI");
    if (move.contains(QStringLiteral("不詰"))) return QStringLiteral("%FUZUMI");
    if (move.contains(QStringLiteral("エラー"))) return QStringLiteral("%ERROR");

    return QString();
}

// ========================================
// CSA出力用の盤面追跡クラス
// ========================================

class CsaBoardTracker {
public:
    // 駒の種類（CSA形式）
    enum PieceType {
        EMPTY = 0,
        FU, KY, KE, GI, KI, KA, HI, OU,  // 基本駒
        TO, NY, NK, NG, UM, RY           // 成駒
    };

    struct Square {
        PieceType piece = EMPTY;
        bool isSente = true;
    };

    Square board[9][9];  // board[file-1][rank-1], 1-indexed access via at()

    CsaBoardTracker() {
        initHirate();
    }

    void initHirate() {
        // 盤面クリア
        for (int f = 0; f < 9; ++f) {
            for (int r = 0; r < 9; ++r) {
                board[f][r] = {EMPTY, true};
            }
        }
        // 後手の駒（1段目）
        board[0][0] = {KY, false}; board[1][0] = {KE, false}; board[2][0] = {GI, false};
        board[3][0] = {KI, false}; board[4][0] = {OU, false}; board[5][0] = {KI, false};
        board[6][0] = {GI, false}; board[7][0] = {KE, false}; board[8][0] = {KY, false};
        // 後手の飛車・角（2段目）: 飛車は8筋(file=8)、角は2筋(file=2)
        board[7][1] = {HI, false}; board[1][1] = {KA, false};
        // 後手の歩（3段目）
        for (int f = 0; f < 9; ++f) board[f][2] = {FU, false};
        // 先手の歩（7段目）
        for (int f = 0; f < 9; ++f) board[f][6] = {FU, true};
        // 先手の飛車・角（8段目）: 飛車は2筋(file=2)、角は8筋(file=8)
        board[1][7] = {HI, true}; board[7][7] = {KA, true};
        // 先手の駒（9段目）
        board[0][8] = {KY, true}; board[1][8] = {KE, true}; board[2][8] = {GI, true};
        board[3][8] = {KI, true}; board[4][8] = {OU, true}; board[5][8] = {KI, true};
        board[6][8] = {GI, true}; board[7][8] = {KE, true}; board[8][8] = {KY, true};
    }

    // SFENから盤面を初期化（非標準局面用）
    void initFromSfen(const QString& sfen) {
        // 盤面クリア
        for (int f = 0; f < 9; ++f)
            for (int r = 0; r < 9; ++r)
                board[f][r] = {EMPTY, true};

        const QStringList parts = sfen.trimmed().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (parts.isEmpty()) { initHirate(); return; }

        const QStringList ranks = parts[0].split(QLatin1Char('/'));
        for (int rank = 0; rank < qMin(ranks.size(), qsizetype(9)); ++rank) {
            const QString& row = ranks[rank];
            int file = 9;  // 9筋から開始（左から右へ）
            bool promoted = false;
            for (int i = 0; i < row.size() && file >= 1; ++i) {
                const QChar ch = row.at(i);
                if (ch == QLatin1Char('+')) {
                    promoted = true;
                    continue;
                }
                if (ch.isDigit()) {
                    file -= ch.digitValue();
                    promoted = false;
                    continue;
                }
                const bool sente = ch.isUpper();
                PieceType piece = charToPiece(ch);
                if (promoted) piece = promote(piece);
                at(file, rank + 1) = {piece, sente};
                --file;
                promoted = false;
            }
        }
    }

    Square& at(int file, int rank) {
        return board[file - 1][rank - 1];
    }

    const Square& at(int file, int rank) const {
        return board[file - 1][rank - 1];
    }

    static QString pieceToCSA(PieceType p) {
        switch (p) {
            case FU: return QStringLiteral("FU");
            case KY: return QStringLiteral("KY");
            case KE: return QStringLiteral("KE");
            case GI: return QStringLiteral("GI");
            case KI: return QStringLiteral("KI");
            case KA: return QStringLiteral("KA");
            case HI: return QStringLiteral("HI");
            case OU: return QStringLiteral("OU");
            case TO: return QStringLiteral("TO");
            case NY: return QStringLiteral("NY");
            case NK: return QStringLiteral("NK");
            case NG: return QStringLiteral("NG");
            case UM: return QStringLiteral("UM");
            case RY: return QStringLiteral("RY");
            default: return QString();
        }
    }

    static PieceType charToPiece(QChar c) {
        switch (c.toUpper().toLatin1()) {
            case 'P': return FU;
            case 'L': return KY;
            case 'N': return KE;
            case 'S': return GI;
            case 'G': return KI;
            case 'B': return KA;
            case 'R': return HI;
            case 'K': return OU;
            default: return EMPTY;
        }
    }

    static PieceType promote(PieceType p) {
        switch (p) {
            case FU: return TO;
            case KY: return NY;
            case KE: return NK;
            case GI: return NG;
            case KA: return UM;
            case HI: return RY;
            default: return p;
        }
    }

    // USI形式の指し手を適用し、CSA形式の駒種を返す
    QString applyMove(const QString& usiMove, bool isSente) {
        if (usiMove.size() < 4) return QString();

        // 駒打ちの場合
        if (usiMove.at(1) == QLatin1Char('*')) {
            PieceType piece = charToPiece(usiMove.at(0));
            int toFile = usiMove.at(2).toLatin1() - '0';
            int toRank = usiMove.at(3).toLatin1() - 'a' + 1;

            if (toFile >= 1 && toFile <= 9 && toRank >= 1 && toRank <= 9) {
                at(toFile, toRank) = {piece, isSente};
            }
            return pieceToCSA(piece);
        }

        // 盤上移動の場合
        int fromFile = usiMove.at(0).toLatin1() - '0';
        int fromRank = usiMove.at(1).toLatin1() - 'a' + 1;
        int toFile = usiMove.at(2).toLatin1() - '0';
        int toRank = usiMove.at(3).toLatin1() - 'a' + 1;
        bool isPromo = usiMove.endsWith(QLatin1Char('+'));

        if (fromFile < 1 || fromFile > 9 || fromRank < 1 || fromRank > 9 ||
            toFile < 1 || toFile > 9 || toRank < 1 || toRank > 9) {
            return QString();
        }

        PieceType piece = at(fromFile, fromRank).piece;
        if (isPromo) {
            piece = promote(piece);
        }

        // 盤面を更新
        at(toFile, toRank) = {piece, isSente};
        at(fromFile, fromRank) = {EMPTY, true};

        return pieceToCSA(piece);
    }
};

// ========================================
// 日時・持ち時間変換ヘルパ
// ========================================

// 日時文字列をCSA V3.0形式 (YYYY/MM/DD HH:MM:SS) に変換
static QString convertToCsaDateTime(const QString& dateTimeStr)
{
    // 既にCSA形式の場合はそのまま返す
    if (dateTimeStr.contains(QLatin1Char('/')) &&
        (dateTimeStr.length() == 10 || dateTimeStr.length() == 19)) {
        return dateTimeStr;
    }

    // KIF形式1: "2024年05月05日（月）15時05分40秒"
    static const QRegularExpression reKif1(
        QStringLiteral("(\\d{4})年(\\d{1,2})月(\\d{1,2})日[^\\d]*(\\d{1,2})時(\\d{1,2})分(\\d{1,2})秒"));
    QRegularExpressionMatch m1 = reKif1.match(dateTimeStr);
    if (m1.hasMatch()) {
        return QStringLiteral("%1/%2/%3 %4:%5:%6")
            .arg(m1.captured(1),
                 m1.captured(2).rightJustified(2, QLatin1Char('0')),
                 m1.captured(3).rightJustified(2, QLatin1Char('0')),
                 m1.captured(4).rightJustified(2, QLatin1Char('0')),
                 m1.captured(5).rightJustified(2, QLatin1Char('0')),
                 m1.captured(6).rightJustified(2, QLatin1Char('0')));
    }

    // KIF形式2: "2024年05月05日" (時刻なし)
    static const QRegularExpression reKif2(
        QStringLiteral("(\\d{4})年(\\d{1,2})月(\\d{1,2})日"));
    QRegularExpressionMatch m2 = reKif2.match(dateTimeStr);
    if (m2.hasMatch()) {
        return QStringLiteral("%1/%2/%3")
            .arg(m2.captured(1),
                 m2.captured(2).rightJustified(2, QLatin1Char('0')),
                 m2.captured(3).rightJustified(2, QLatin1Char('0')));
    }

    // ISO形式: "2024-05-05T15:05:40" や "2024-05-05 15:05:40"
    static const QRegularExpression reIso(
        QStringLiteral("(\\d{4})-(\\d{2})-(\\d{2})[T ](\\d{2}):(\\d{2}):(\\d{2})"));
    QRegularExpressionMatch m3 = reIso.match(dateTimeStr);
    if (m3.hasMatch()) {
        return QStringLiteral("%1/%2/%3 %4:%5:%6")
            .arg(m3.captured(1), m3.captured(2), m3.captured(3),
                 m3.captured(4), m3.captured(5), m3.captured(6));
    }

    // ISO形式（日付のみ）: "2024-05-05"
    static const QRegularExpression reIsoDate(
        QStringLiteral("(\\d{4})-(\\d{2})-(\\d{2})"));
    QRegularExpressionMatch m4 = reIsoDate.match(dateTimeStr);
    if (m4.hasMatch()) {
        return QStringLiteral("%1/%2/%3")
            .arg(m4.captured(1), m4.captured(2), m4.captured(3));
    }

    // 変換できなかった場合はそのまま返す
    return dateTimeStr;
}

// 持ち時間文字列をCSA V3.0形式 ($TIME:秒+秒読み+フィッシャー) に変換
static QString convertToCsaTime(const QString& timeStr)
{
    // V2.2形式: "HH:MM+SS" → V3.0形式: "$TIME:秒+秒読み+0"
    static const QRegularExpression reV22(
        QStringLiteral("(\\d+):(\\d{2})\\+(\\d+)"));
    QRegularExpressionMatch m = reV22.match(timeStr);
    if (m.hasMatch()) {
        int hours = m.captured(1).toInt();
        int minutes = m.captured(2).toInt();
        int byoyomi = m.captured(3).toInt();
        int totalSeconds = hours * 3600 + minutes * 60;
        return QStringLiteral("$TIME:%1+%2+0").arg(totalSeconds).arg(byoyomi);
    }

    // 既にV3.0形式: "秒+秒読み+フィッシャー"
    static const QRegularExpression reV30(
        QStringLiteral("^(\\d+(?:\\.\\d+)?)\\+(\\d+(?:\\.\\d+)?)\\+(\\d+(?:\\.\\d+)?)$"));
    QRegularExpressionMatch m2 = reV30.match(timeStr);
    if (m2.hasMatch()) {
        return QStringLiteral("$TIME:%1").arg(timeStr);
    }

    // "○分" や "○秒" 形式
    static const QRegularExpression reMinSec(
        QStringLiteral("(\\d+)分(?:(\\d+)秒)?"));
    QRegularExpressionMatch m3 = reMinSec.match(timeStr);
    if (m3.hasMatch()) {
        int minutes = m3.captured(1).toInt();
        int seconds = m3.captured(2).isEmpty() ? 0 : m3.captured(2).toInt();
        int totalSeconds = minutes * 60 + seconds;
        return QStringLiteral("$TIME:%1+0+0").arg(totalSeconds);
    }

    // 秒読み形式: "○秒"
    static const QRegularExpression reSec(QStringLiteral("(\\d+)秒"));
    QRegularExpressionMatch m4 = reSec.match(timeStr);
    if (m4.hasMatch()) {
        int seconds = m4.captured(1).toInt();
        // 秒読みのみの場合
        return QStringLiteral("$TIME:0+%1+0").arg(seconds);
    }

    // 変換できなかった場合は旧形式で出力
    return QStringLiteral("$TIME_LIMIT:%1").arg(timeStr);
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
    // まず棋譜情報から対局者名を探す（CSAファイル読み込み時はここに保存される）
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
    // 棋譜情報になければresolvePlayerNamesから取得
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
    // 既に出力済みの項目を追跡
    bool hasStartTime = false;
    Q_UNUSED(hasStartTime);  // 現在未使用だが将来の拡張用に残す
    bool hasEndTime = false;
    Q_UNUSED(hasEndTime);
    bool hasTime = false;
    Q_UNUSED(hasTime);

    for (const auto& it : header) {
        const QString key = it.key.trimmed();
        const QString val = it.value.trimmed();
        if (key.isEmpty() || val.isEmpty()) continue;

        // 対局者名は既に出力済みなのでスキップ
        if (key == QStringLiteral("先手") || key == QStringLiteral("後手")) {
            continue;
        }

        if (key == QStringLiteral("棋戦")) {
            out << QStringLiteral("$EVENT:%1").arg(val);
        } else if (key == QStringLiteral("場所")) {
            out << QStringLiteral("$SITE:%1").arg(val);
        } else if (key == QStringLiteral("開始日時")) {
            // 日時形式を CSA V3.0 形式に変換
            QString csaDateTime = convertToCsaDateTime(val);
            out << QStringLiteral("$START_TIME:%1").arg(csaDateTime);
            hasStartTime = true;
        } else if (key == QStringLiteral("終了日時")) {
            QString csaDateTime = convertToCsaDateTime(val);
            out << QStringLiteral("$END_TIME:%1").arg(csaDateTime);
            hasEndTime = true;
        } else if (key == QStringLiteral("持ち時間")) {
            // V3.0形式: $TIME:秒+秒読み+フィッシャー
            // 入力値の形式を判定して適切な形式で出力
            QString timeVal = convertToCsaTime(val);
            out << timeVal;
            hasTime = true;
        } else if (key == QStringLiteral("戦型")) {
            out << QStringLiteral("$OPENING:%1").arg(val);
        } else if (key == QStringLiteral("最大手数")) {
            // V3.0で追加
            out << QStringLiteral("$MAX_MOVES:%1").arg(val);
        } else if (key == QStringLiteral("持将棋")) {
            // V3.0で追加
            out << QStringLiteral("$JISHOGI:%1").arg(val);
        } else if (key == QStringLiteral("備考")) {
            // V3.0で追加: \は\\に、改行は\nに変換
            QString noteVal = val;
            noteVal.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));  // 先に\をエスケープ
            noteVal.replace(QStringLiteral("\n"), QStringLiteral("\\n"));   // 次に改行を変換
            out << QStringLiteral("$NOTE:%1").arg(noteVal);
        }
    }

    // 5-2) 棋譜情報がない場合のデフォルト生成
    // 開始日時がなければctxから取得、なければ現在日時を使用
    if (!hasStartTime) {
        QString startTimeStr;
        if (ctx.gameStartDateTime.isValid()) {
            startTimeStr = ctx.gameStartDateTime.toString(QStringLiteral("yyyy/MM/dd HH:mm:ss"));
        } else {
            startTimeStr = QDateTime::currentDateTime().toString(QStringLiteral("yyyy/MM/dd HH:mm:ss"));
        }
        out << QStringLiteral("$START_TIME:%1").arg(startTimeStr);
    }

    // 終了日時がなければ現在日時を使用（棋譜コピー時点で対局は終了しているはず）
    if (!hasEndTime) {
        const QString nowStr = QDateTime::currentDateTime().toString(QStringLiteral("yyyy/MM/dd HH:mm:ss"));
        out << QStringLiteral("$END_TIME:%1").arg(nowStr);
    }

    // 持ち時間情報がなければctxから生成
    if (!hasTime && ctx.hasTimeControl) {
        // ミリ秒から秒に変換（小数点以下は切り捨て）
        int initialSec = ctx.initialTimeMs / 1000;
        int byoyomiSec = ctx.byoyomiMs / 1000;
        int fischerSec = ctx.fischerIncrementMs / 1000;
        out << QStringLiteral("$TIME:%1+%2+%3").arg(initialSec).arg(byoyomiSec).arg(fischerSec);
    }

    // 6) 開始局面
    // startSfenがデフォルト（平手）かどうかを判定
    const QString defaultSfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    const bool isHirate = ctx.startSfen.isEmpty() || ctx.startSfen == defaultSfen
                          || ctx.startSfen.startsWith(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"));

    if (isHirate) {
        // 平手初期配置（簡易表記）
        out << QStringLiteral("PI");
        out << QStringLiteral("+");  // 先手番
    } else {
        bool converted = false;
        QString convErr;
        const QStringList csaPos =
            SfenCsaPositionConverter::toCsaPositionLines(ctx.startSfen, &converted, &convErr);
        if (converted && !csaPos.isEmpty()) {
            out << csaPos;
            // 非標準局面: 盤面追跡を開始SFENで初期化
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

    // デバッグ: disp の確認
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

    // デバッグ: ループ開始前（詳細版）
    qCDebug(lcKifu).noquote() << "toCsaLines: ========== CSA変換ループ開始 ==========";
    qCDebug(lcKifu).noquote() << "toCsaLines: startIdx =" << startIdx
                              << ", disp.size() =" << disp.size()
                              << ", usiMoves.size() =" << usiMoves.size()
                              << ", ループ回数予定 =" << (disp.size() - startIdx);

    // dispの内容を出力
    for (qsizetype dbg = 0; dbg < qMin(disp.size(), qsizetype(25)); ++dbg) {
        qCDebug(lcKifu).noquote() << "toCsaLines: disp[" << dbg << "].prettyMove =" << disp[dbg].prettyMove.trimmed()
                                  << ", timeText =" << disp[dbg].timeText;
    }

    // usiMovesの内容を出力
    for (qsizetype dbg = 0; dbg < qMin(usiMoves.size(), qsizetype(25)); ++dbg) {
        qCDebug(lcKifu).noquote() << "toCsaLines: usiMoves[" << dbg << "] =" << usiMoves[dbg];
    }

    // 8) 各指し手を出力
    // 手番をSFENから判定（非標準局面は後手番の場合がある）
    int moveNo = 1;
    bool isSente = true;
    if (!ctx.startSfen.isEmpty()) {
        const QStringList sfenParts = ctx.startSfen.trimmed().split(QLatin1Char(' '));
        if (sfenParts.size() >= 2 && sfenParts[1] == QStringLiteral("w"))
            isSente = false;
    }
    QString terminalMove;
    int processedMoves = 0;
    int skippedEmpty = 0;
    int skippedNoUsi = 0;

    for (qsizetype i = startIdx; i < disp.size(); ++i) {
        const auto& it = disp[i];
        const QString moveText = it.prettyMove.trimmed();

        if (moveText.isEmpty()) {
            ++skippedEmpty;
            continue;
        }

        // 終局語の判定
        if (isTerminalMove(moveText)) {
            qCDebug(lcKifu).noquote() << "toCsaLines: 終局語検出: moveText =" << moveText;
            terminalMove = moveText;
            const QString resultCode = getCsaResultCode(moveText);
            if (!resultCode.isEmpty()) {
                // 終局コードを出力
                out << resultCode;
                // 消費時間を別行で出力
                const int timeSec = extractCsaTimeSeconds(it.timeText);
                out << QStringLiteral("T%1").arg(timeSec);
            }
            break;
        }

        // USI指し手を取得（インデックスは moveNo-1）
        QString usiMove;
        if (moveNo - 1 < usiMoves.size()) {
            usiMove = usiMoves.at(moveNo - 1);
        }

        // デバッグ: 各指し手の詳細（全ての手を出力）
        qCDebug(lcKifu).noquote() << "toCsaLines: LOOP: i=" << i
                                  << " moveNo=" << moveNo
                                  << " isSente=" << isSente
                                  << " moveText='" << moveText << "'"
                                  << " usiMove='" << usiMove << "'"
                                  << " (usiMoves.size=" << usiMoves.size() << ")";

        // CSA形式の指し手を構築
        QString csaMove;
        const QString sign = isSente ? QStringLiteral("+") : QStringLiteral("-");

        if (!usiMove.isEmpty()) {
            // 駒打ちの場合
            if (usiMove.size() >= 4 && usiMove.at(1) == QLatin1Char('*')) {
                const QString toSquare = usiMove.mid(2, 2);

                const int toFile = toSquare.at(0).toLatin1() - '0';
                const int toRank = toSquare.at(1).toLatin1() - 'a' + 1;

                // 盤面追跡で駒種を取得（盤面も更新される）
                const QString csaPiece = boardTracker.applyMove(usiMove, isSente);

                if (!csaPiece.isEmpty()) {
                    csaMove = QStringLiteral("%1%2%3%4%5")
                        .arg(sign, QStringLiteral("00"), QString::number(toFile),
                             QString::number(toRank), csaPiece);
                }
            } else if (usiMove.size() >= 4) {
                // 盤上移動の場合
                const QString fromSquare = usiMove.left(2);
                const QString toSquare = usiMove.mid(2, 2);

                const int fromFile = fromSquare.at(0).toLatin1() - '0';
                const int fromRank = fromSquare.at(1).toLatin1() - 'a' + 1;
                const int toFile = toSquare.at(0).toLatin1() - '0';
                const int toRank = toSquare.at(1).toLatin1() - 'a' + 1;

                // 盤面追跡で駒種を取得（成りも考慮、盤面も更新される）
                const QString csaPiece = boardTracker.applyMove(usiMove, isSente);

                if (!csaPiece.isEmpty()) {
                    csaMove = QStringLiteral("%1%2%3%4%5%6")
                        .arg(sign, QString::number(fromFile), QString::number(fromRank),
                             QString::number(toFile), QString::number(toRank), csaPiece);
                }
            }
        }

        if (csaMove.isEmpty()) {
            // USI指し手がない場合はスキップ
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
        // 消費時間を別行で出力（常に出力、0秒でもT0を付加）
        const int timeSec = extractCsaTimeSeconds(it.timeText);
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
                              << ", skippedEmpty =" << skippedEmpty
                              << ", skippedNoUsi =" << skippedNoUsi;
    qCDebug(lcKifu).noquote() << "toCsaLines: generated"
                              << out.size() << "lines,"
                              << (moveNo - 1) << "moves";

    return out;
}
