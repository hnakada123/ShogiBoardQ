#ifndef TURNMANAGER_H
#define TURNMANAGER_H

/// @file turnmanager.h
/// @brief 手番の管理と各種表現形式への変換を担うクラス


#include <QObject>
#include "shogigamecontroller.h"
#include "shogitypes.h"

/**
 * @brief 手番の単一ソースとして、表現形式の変換とシグナル配信を行う
 *
 * GC・SFEN 間の手番表現を相互変換する。
 */
class TurnManager : public QObject
{
    Q_OBJECT
public:
    using Side = Turn; ///< 先手・後手

    explicit TurnManager(QObject* parent=nullptr);

    Side side() const;
    void set(Side s);
    void toggle();

    // --- SFEN "b"/"w" 変換 ---
    QString toSfenToken() const;
    void setFromSfenToken(const QString& bw);

signals:
    /// 手番変更を通知（→ MainWindow::onTurnManagerChanged）
    void changed(Turn now);

private:
    Side m_side; ///< 現在の手番
    bool m_sideSet = false; ///< 手番を一度でも設定したか
};

#endif // TURNMANAGER_H
