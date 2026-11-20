#ifndef P2PSETTINGSPANEL_H
#define P2PSETTINGSPANEL_H

/// @file p2psettingspanel.h
/// @brief P2P対局のウィンドウの「設定」タブ（表示・プレイヤーIDとプレイヤー鍵・使用期限・防げないこと）

#include <QWidget>

class QCheckBox;
class QLabel;

/**
 * @brief 接続のしかた以外の設定と、プレイヤー鍵・このビルド・既知の限界の説明
 *
 * 相手とのつなぎ方（協力ノード・直接つなぐか中継だけか）は「接続」タブにある。ここには、表示の設定、自分の
 * プレイヤーIDとプレイヤー鍵の保管、ビルドの使用期限、P2P対局で防げないことを置く。値を出すだけで、保存と操作は
 * 受け手（P2pWindow）が行う。
 */
class P2pSettingsPanel : public QWidget
{
    Q_OBJECT
public:
    explicit P2pSettingsPanel(QWidget* parent = nullptr);

    bool hideUnknownNames() const;
    /// 保存した設定に合わせる（知らせは出さない）
    void setHideUnknownNames(bool hide);
    void setPlayerId(const QString& text);
    /// プレイヤー鍵の保管の方法と、通信プロセスの権限の制限（行ごと）
    void setIdentityInfo(const QString& text);
    void setBuildInfo(const QString& text);

signals:
    void hideUnknownNamesToggled(bool hide);
    void copyPlayerIdRequested();
    void recreateIdentityRequested();
    /// 「接続」タブを開く
    void connectionTabRequested();

private:
    QCheckBox* m_hideUnknown = nullptr;
    QLabel* m_playerId = nullptr;
    QLabel* m_identityInfo = nullptr;
    QLabel* m_buildInfo = nullptr;
};

#endif // P2PSETTINGSPANEL_H
