#ifndef P2PCONFIRMDIALOG_H
#define P2PCONFIRMDIALOG_H

/// @file p2pconfirmdialog.h
/// @brief P2P対局の条件確認画面（相手・先後・持ち時間・ルール・記録に残る内容。実装仕様 5.4節・11章）

#include <QDialog>
#include <QPointer>

class P2pGameCoordinator;
class P2pKnownPeers;
class QLabel;
class QPushButton;
class QTimer;

/**
 * @brief 条件を確かめて開始を押す画面（モードレス）
 *
 * ハッシュ値だけを見せて同意を求めない。相手の開始署名を確かめるまで、相手のプレイヤーID は申告として出し、
 * 手元で付けた名前と確認済みの印は「申告された ID に付けてある名前」として出す。表示名は相手が自由に書ける文
 * なので、かぎ括弧で囲み、PlainText で出し、ウィンドウの題名とボタンに使わない。
 * プレイヤー鍵で開始合意に署名するのは、ここで開始を押したときだけ（司令塔の confirmStart）。
 */
class P2pConfirmDialog : public QDialog
{
    Q_OBJECT
public:
    P2pConfirmDialog(P2pGameCoordinator* game, P2pKnownPeers* knownPeers, bool hideUnknownNames,
                     QWidget* parent = nullptr);

public slots:
    /// 交渉の内容を出し直す
    void refresh();
    /// 閉じたら交渉を取りやめる
    void reject() override;

private slots:
    void onStart();
    void onCancel();
    void onTick();

private:
    QPointer<P2pGameCoordinator> m_game;
    P2pKnownPeers* m_knownPeers = nullptr;   ///< 非所有
    bool m_hideUnknownNames = false;
    QLabel* m_opponent = nullptr;
    QLabel* m_identity = nullptr;
    QLabel* m_warning = nullptr;
    QLabel* m_conditions = nullptr;
    QLabel* m_remaining = nullptr;
    QLabel* m_playerId = nullptr;
    QPushButton* m_start = nullptr;
    QPushButton* m_cancel = nullptr;
    QTimer* m_timer = nullptr;
};

#endif // P2PCONFIRMDIALOG_H
