#ifndef PVLINE_H
#define PVLINE_H

/// @file pvline.h
/// @brief 読み筋盤に出す読み筋（各手数の局面・手の表記・直前の手のハイライト）

#include <QList>
#include <QPoint>
#include <QString>
#include <QStringList>
#include <optional>

#include "shogitypes.h"
#include "usimove.h"

/**
 * @brief 読み筋1本の値
 *
 * 開始局面から読み筋の手を順に指し（指せない手の手前で打ち切る）、各手数の局面・手の表記・
 * ハイライトする直前の手を作る。表示する手数は持たない（読み筋盤のビューアの番号を使う）。
 */
class PvLine
{
public:
    /// 直前の手のハイライト（盤の座標。駒打ちの移動元は打った側の駒台）
    struct Highlight {
        std::optional<QPoint> from;
        std::optional<QPoint> to;
    };

    PvLine(const QString& baseSfen, const QStringList& pvMoves);

    /// 指せた手の数
    int moveCount() const { return static_cast<int>(m_steps.size()); }
    /// 各手数の局面（0 が開始局面、i が i 手指した後）
    QStringList positions() const;
    /// ply 手目の表記（ply は 1 から。日本語表記は「同」も付ける）
    QString moveText(int ply) const;
    /// 読み筋全体の表記。日本語表記で japanese（エンジンの読み筋の漢字表記）があればそれを使う
    QString displayPv(const QString& japanese, bool fullOrigin = false) const;

    /**
     * @brief 開始局面に至った手を設定する（開始局面のハイライトに使う）
     * @param lastMove 至った手（USI）。分からなければ空
     * @param previousSfen 至る前の局面。lastMove が空なら、開始局面との盤の差分から手を決める
     */
    void setArrival(const QString& lastMove, const QString& previousSfen);

    /// ply 手目を指した後の局面で強調する直前の手（0 は開始局面に至った手）
    Highlight highlight(int ply) const;

private:
    struct Step {
        UsiMove move;
        Turn mover = Turn::Black;
        QString sfen;     ///< 指した後の局面
        QString japanese; ///< 日本語の表記
    };

    Highlight arrivalHighlight() const;

    QString m_baseSfen;
    QStringList m_moves;
    QList<Step> m_steps;
    QString m_lastMove;
    QString m_previousSfen;
};

#endif // PVLINE_H
