#ifndef TURNMANAGER_H
#define TURNMANAGER_H

#include <QObject>
#include "shogigamecontroller.h"

// 薄い手番マネージャ：手番の単一ソース＋表現変換＋シグナル配信
class TurnManager : public QObject
{
    Q_OBJECT
public:
    using Side = ShogiGameController::Player; // Player1 / Player2 / NoPlayer

    explicit TurnManager(QObject* parent=nullptr);

    Side side() const;
    void set(Side s);
    void toggle();

    // ---- 変換：SFEN "b"/"w"
    QString toSfenToken() const;
    void setFromSfenToken(const QString& bw);

    // ---- 変換：Clock 1/2
    int  toClockPlayer() const;
    void setFromClockPlayer(int p);

    // ---- 変換：GC
    ShogiGameController::Player toGc() const;
    void setFromGc(ShogiGameController::Player p);

signals:
    void changed(ShogiGameController::Player now);

private:
    Side m_side;
};

#endif // TURNMANAGER_H
