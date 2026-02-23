#ifndef TURNMANAGER_H
#define TURNMANAGER_H

/// @file turnmanager.h
/// @brief 手番の管理と各種表現形式への変換を担うクラス


#include <QObject>
#include "shogigamecontroller.h"
#include "shogitypes.h"

/**
 * @brief 手番の単一ソースとして、表現形式の変換とシグナル配信を行う
 * @todo remove コメントスタイルガイド適用済み
 *
 * GC・SFEN・Clock 間の手番表現を相互変換する。
 */
class TurnManager : public QObject
{
    Q_OBJECT
public:
    using Side = ShogiGameController::Player; ///< Player1 / Player2 / NoPlayer

    explicit TurnManager(QObject* parent=nullptr);

    Side side() const;
    void set(Side s);
    void toggle();

    // --- Turn enum 変換 ---
    Turn toTurn() const;
    void setFromTurn(Turn t);

    // --- SFEN "b"/"w" 変換 ---
    QString toSfenToken() const;
    void setFromSfenToken(const QString& bw);

    // --- Clock 1/2 変換 ---
    int  toClockPlayer() const;
    void setFromClockPlayer(int p);

    // --- GameController 変換 ---
    ShogiGameController::Player toGc() const;
    void setFromGc(ShogiGameController::Player p);

signals:
    /// 手番変更を通知（→ MainWindow::onTurnManagerChanged）
    void changed(ShogiGameController::Player now);

private:
    Side m_side; ///< 現在の手番
};

#endif // TURNMANAGER_H
