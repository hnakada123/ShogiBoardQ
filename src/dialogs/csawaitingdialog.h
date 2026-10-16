#ifndef CSAWAITINGDIALOG_H
#define CSAWAITINGDIALOG_H

/// @file csawaitingdialog.h
/// @brief CSA接続待機ダイアログクラスの定義

#include <QDialog>
#include "csagamecoordinator.h"

QT_BEGIN_NAMESPACE
class CsaLogPanel;
class QLabel;
class QPushButton;
class QToolButton;
class QProgressBar;
QT_END_NAMESPACE

/**
 * @brief CSA通信対局待機ダイアログクラス
 *
 * CSA通信対局の開始を待機している間に表示されるダイアログ。
 * 接続状態やログイン状態、対局待ちの状態を表示し、
 * ユーザーが対局をキャンセルできるボタンを提供する。
 *
 * 対局が開始されたら自動的に閉じる。
 */
class CsaWaitingDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief コンストラクタ
     * @param coordinator CSA通信対局コーディネータ
     * @param parent 親ウィジェット
     */
    explicit CsaWaitingDialog(CsaGameCoordinator* coordinator, QWidget* parent = nullptr);

    /**
     * @brief デストラクタ
     */
    ~CsaWaitingDialog() override;

signals:
    /**
     * @brief 対局がキャンセルされた時に発行
     */
    void cancelRequested();

private slots:
    /**
     * @brief 対局状態が変化した時の処理
     * @param state 新しい状態
     */
    void onGameStateChanged(CsaGameCoordinator::GameState state);

    /**
     * @brief エラーが発生した時の処理
     * @param message エラーメッセージ
     */
    void onErrorOccurred(const QString& message);

    /**
     * @brief ログメッセージを受信した時の処理
     * @param message ログメッセージ
     * @param isError エラーかどうか
     */
    void onLogMessage(const QString& message, bool isError);

    /**
     * @brief キャンセルボタンが押された時の処理
     */
    void onCancelClicked();

    /**
     * @brief 通信ログボタンが押された時の処理
     */
    void onShowLogClicked();

    /**
     * @brief CSA通信ログを受信した時の処理
     * @param line ログ行
     */
    void onCsaCommLogAppended(const QString& line);

    /// 通信ログの窓で入力したコマンドをサーバーへ送る
    void onRawCommandRequested(const QString& command);
    /// 接続中だけコマンドを送れるようにする
    void updateCommandState();

private:
    /**
     * @brief UIを初期化する
     */
    void setupUi();

    /**
     * @brief シグナル・スロットの接続を行う
     */
    void connectSignalsAndSlots();

    /**
     * @brief 状態に応じたメッセージを取得する
     * @param state 対局状態
     * @return 状態メッセージ
     */
    QString stateMessage(CsaGameCoordinator::GameState state) const;

    /**
     * @brief 通信ログウィンドウを作成する
     */
    void createLogWindow();

private:
    CsaGameCoordinator* m_coordinator = nullptr;  ///< CSA通信対局コーディネータ
    QLabel* m_statusLabel = nullptr;              ///< 状態表示ラベル
    QLabel* m_detailLabel = nullptr;              ///< 詳細メッセージラベル
    QProgressBar* m_progressBar = nullptr;        ///< プログレスバー（待機中表示用）
    QPushButton* m_cancelButton = nullptr;        ///< キャンセルボタン
    QPushButton* m_showLogButton = nullptr;       ///< 通信ログ表示ボタン

    // フォントサイズ調整ボタン（ダイアログ本体）
    QToolButton* m_btnFontIncrease = nullptr;    ///< A+ボタン
    QToolButton* m_btnFontDecrease = nullptr;    ///< A-ボタン

    // 通信ログウィンドウ（中身は CSA 通信ログのドックと同じ部品。ダイアログを作ったときから受信を記録する）
    QDialog* m_logWindow = nullptr;
    CsaLogPanel* m_logPanel = nullptr;
};

#endif // CSAWAITINGDIALOG_H
