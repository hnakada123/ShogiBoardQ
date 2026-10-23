#ifndef PVLINE_H
#define PVLINE_H

/// @file pvline.h
/// @brief 読み筋盤に出す読み筋（各手数の局面・手の表記・直前の手のハイライト）

#include <QList>
#include <QPoint>
#include <QString>
#include <QStringList>
#include <optional>

#include "pvtext.h"
#include "shogitypes.h"
#include "usimove.h"

/**
 * @brief 読み筋盤に出す読み筋1本
 *
 * 読み筋（PvText。指せない手の手前で打ち切ってある）の各手数の局面・手の表記・ハイライトする直前の手を作る。
 * 表示する手数は持たない（読み筋盤のビューアの番号を使う）。
 */
class PvLine
{
public:
    /// 直前の手のハイライト（盤の座標。駒打ちの移動元は打った側の駒台）
    struct Highlight {
        std::optional<QPoint> from;
        std::optional<QPoint> to;
    };

    explicit PvLine(const PvText& pv);

    /// 指せた手の数
    int moveCount() const { return m_pv.moveCount(); }
    /// 各手数の局面（0 が開始局面、i が i 手指した後）
    const QStringList& positions() const { return m_positions; }
    /// ply 手目の表記（ply は 1 から。日本語表記は「同」も付ける）
    QString moveText(int ply) const;
    /// 読み筋全体の表記（表記の設定に合わせる。fullOrigin なら移動元をすべて付ける）
    QString displayPv(bool fullOrigin = false) const;

    /**
     * @brief 開始局面に至った手を設定する（開始局面のハイライトに使う）
     * @param lastMove 至った手（USI）。分からなければ空
     * @param previousSfen 至る前の局面。lastMove が空なら、開始局面との盤の差分から手を決める
     */
    void setArrival(const QString& lastMove, const QString& previousSfen);

    /// ply 手目を指した後の局面で強調する直前の手（0 は開始局面に至った手）
    Highlight highlight(int ply) const;

private:
    Highlight arrivalHighlight() const;

    PvText m_pv;
    QStringList m_positions;  ///< [0] が開始局面、[i] が i 手指した後
    QString m_lastMove;
    QString m_previousSfen;
};

#endif // PVLINE_H
