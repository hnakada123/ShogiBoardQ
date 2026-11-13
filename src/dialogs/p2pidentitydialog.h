#ifndef P2PIDENTITYDIALOG_H
#define P2PIDENTITYDIALOG_H

/// @file p2pidentitydialog.h
/// @brief P2P対局のプレイヤー鍵を作る・読み込むダイアログ（初回の準備と、パスフレーズの入力。実装仕様 5.2節・11章）

#include <QDialog>

#include "p2pidentitystore.h"

class QButtonGroup;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QRadioButton;

/**
 * @brief プレイヤー鍵の準備
 *
 * 作るとき（Create）は保管の方法を選んでもらい、既知の限界を説明する。暗号化しないファイルを選ぶと、
 * ファイルを読める者が鍵を使えることを示す。読み込むとき（Unlock）はパスフレーズを入れてもらう。
 * パスフレーズは鍵の読み込みにだけ使い、保存しない。鍵の導出は GUI スレッドの外で行う（P2pIdentityStore）。
 */
class P2pIdentityDialog : public QDialog
{
    Q_OBJECT
public:
    enum class Mode { Create, Unlock };

    P2pIdentityDialog(Mode mode, P2pIdentityStore* store, QWidget* parent = nullptr);

    /// 既知の限界の説明（実装仕様 1.2節。条件確認画面と P2P対局のウィンドウでも使う）
    static QString knownLimitsText();

public slots:
    void accept() override;

private slots:
    void onStoreFinished(P2pIdentityStore::Failure failure);
    void updateState();

private:
    /// 入力欄の文字を、共有している領域ごとその場で消してから空にする
    static void wipeText(QLineEdit* edit);
    QString failureText(P2pIdentityStore::Failure failure) const;

    Mode m_mode;
    P2pIdentityStore* m_store = nullptr;   ///< 非所有
    QButtonGroup* m_methods = nullptr;
    QRadioButton* m_keychain = nullptr;
    QRadioButton* m_passphraseFile = nullptr;
    QRadioButton* m_plainFile = nullptr;
    QLineEdit* m_passphrase = nullptr;
    QLineEdit* m_confirm = nullptr;
    QLabel* m_status = nullptr;
    QDialogButtonBox* m_buttons = nullptr;
    bool m_working = false;
};

#endif // P2PIDENTITYDIALOG_H
