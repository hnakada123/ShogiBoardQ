#ifndef CSAFORMATTER_H
#define CSAFORMATTER_H

/// @file csaformatter.h
/// @brief CSA形式の文字列生成ヘルパクラス・関数の定義

#include <QChar>
#include <QString>
#include <QStringList>

/**
 * @brief CSA出力用の盤面追跡クラス
 *
 * USI形式の指し手を逐次適用しながら盤面状態を追跡し、
 * CSA形式の駒種文字列を返す。
 */
class CsaBoardTracker {
public:
    enum PieceType {
        EMPTY = 0,
        FU, KY, KE, GI, KI, KA, HI, OU,
        TO, NY, NK, NG, UM, RY
    };

    struct Square {
        PieceType piece = EMPTY;
        bool isSente = true;
    };

    Square board[9][9];

    CsaBoardTracker();

    void initHirate();
    void initFromSfen(const QString& sfen);

    Square& at(int file, int rank);
    const Square& at(int file, int rank) const;

    static QString pieceToCSA(PieceType p);
    static PieceType charToPiece(QChar c);
    static PieceType promote(PieceType p);

    QString applyMove(const QString& usiMove, bool isSente);
};

/**
 * @brief CSAエクスポート用フォーマット変換ヘルパ関数群
 */
namespace CsaFormatter {

QString removeTurnMarker(const QString& move);
bool isTerminalMove(const QString& move);
int extractCsaTimeSeconds(const QString& timeText);
QString getCsaResultCode(const QString& terminalMove);
QString convertToCsaDateTime(const QString& dateTimeStr);
QString convertToCsaTime(const QString& timeStr);

} // namespace CsaFormatter

#endif // CSAFORMATTER_H
