/// @file csaformatter.cpp
/// @brief CSA形式の文字列生成ヘルパクラス・関数の実装

#include "csaformatter.h"

#include <QRegularExpression>

// ========================================
// CsaBoardTracker
// ========================================

CsaBoardTracker::CsaBoardTracker() {
    initHirate();
}

void CsaBoardTracker::initHirate() {
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

void CsaBoardTracker::initFromSfen(const QString& sfen) {
    for (int f = 0; f < 9; ++f)
        for (int r = 0; r < 9; ++r)
            board[f][r] = {EMPTY, true};

    const QStringList parts = sfen.trimmed().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.isEmpty()) { initHirate(); return; }

    const QStringList ranks = parts[0].split(QLatin1Char('/'));
    for (int rank = 0; rank < qMin(ranks.size(), qsizetype(9)); ++rank) {
        const QString& row = ranks[rank];
        int file = 9;
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

CsaBoardTracker::Square& CsaBoardTracker::at(int file, int rank) {
    return board[file - 1][rank - 1];
}

const CsaBoardTracker::Square& CsaBoardTracker::at(int file, int rank) const {
    return board[file - 1][rank - 1];
}

QString CsaBoardTracker::pieceToCSA(PieceType p) {
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

CsaBoardTracker::PieceType CsaBoardTracker::charToPiece(QChar c) {
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

CsaBoardTracker::PieceType CsaBoardTracker::promote(PieceType p) {
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

QString CsaBoardTracker::applyMove(const QString& usiMove, bool isSente) {
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

    at(toFile, toRank) = {piece, isSente};
    at(fromFile, fromRank) = {EMPTY, true};

    return pieceToCSA(piece);
}

// ========================================
// CsaFormatter namespace
// ========================================

namespace CsaFormatter {

QString removeTurnMarker(const QString& move)
{
    QString result = move;
    if (result.startsWith(QStringLiteral("▲")) || result.startsWith(QStringLiteral("△"))) {
        result = result.mid(1);
    }
    return result;
}

bool isTerminalMove(const QString& move)
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

int extractCsaTimeSeconds(const QString& timeText)
{
    QString text = timeText.trimmed();

    if (text.startsWith(QLatin1Char('('))) text = text.mid(1);
    if (text.endsWith(QLatin1Char(')'))) text.chop(1);

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

QString getCsaResultCode(const QString& terminalMove)
{
    const QString move = removeTurnMarker(terminalMove);

    if (move.contains(QStringLiteral("投了"))) return QStringLiteral("%TORYO");
    if (move.contains(QStringLiteral("中断"))) return QStringLiteral("%CHUDAN");
    if (move.contains(QStringLiteral("千日手"))) return QStringLiteral("%SENNICHITE");
    if (move.contains(QStringLiteral("持将棋"))) return QStringLiteral("%JISHOGI");
    if (move.contains(QStringLiteral("切れ負け")) || move.contains(QStringLiteral("時間切れ")))
        return QStringLiteral("%TIME_UP");
    if (move.contains(QStringLiteral("反則負け"))) return QStringLiteral("%ILLEGAL_MOVE");
    if (move.contains(QStringLiteral("先手の反則"))) return QStringLiteral("%+ILLEGAL_ACTION");
    if (move.contains(QStringLiteral("後手の反則"))) return QStringLiteral("%-ILLEGAL_ACTION");
    if (move.contains(QStringLiteral("反則"))) return QStringLiteral("%ILLEGAL_MOVE");
    if (move.contains(QStringLiteral("入玉勝ち")) || move.contains(QStringLiteral("宣言勝ち")))
        return QStringLiteral("%KACHI");
    if (move.contains(QStringLiteral("引き分け"))) return QStringLiteral("%HIKIWAKE");
    if (move.contains(QStringLiteral("最大手数"))) return QStringLiteral("%MAX_MOVES");
    if (move.contains(QStringLiteral("詰み"))) return QStringLiteral("%TSUMI");
    if (move.contains(QStringLiteral("不詰"))) return QStringLiteral("%FUZUMI");
    if (move.contains(QStringLiteral("エラー"))) return QStringLiteral("%ERROR");

    return QString();
}

QString convertToCsaDateTime(const QString& dateTimeStr)
{
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

    return dateTimeStr;
}

QString convertToCsaTime(const QString& timeStr)
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
        return QStringLiteral("$TIME:0+%1+0").arg(seconds);
    }

    return QStringLiteral("$TIME_LIMIT:%1").arg(timeStr);
}

} // namespace CsaFormatter
