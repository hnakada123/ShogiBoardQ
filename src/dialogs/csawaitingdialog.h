#ifndef CSAWAITINGDIALOG_H
#define CSAWAITINGDIALOG_H

/// @file csawaitingdialog.h
/// @brief CSA接続待機ダイアログクラスの定義


#include <QDialog>
#include "csagamecoordinator.h"

QT_BEGIN_NAMESPACE
class QLabel;
class QPushButton;
class QToolButton;
class QProgressBar;
class QPlainTextEdit;
class QLineEdit;
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

    /**
     * @brief コマンド入力でEnterが押された時の処理
     */
    void onCommandEntered();

    /**
     * @brief ログウィンドウのフォントサイズを大きくする
     */
    void onLogFontIncrease();

    /**
     * @brief ログウィンドウのフォントサイズを小さくする
     */
    void onLogFontDecrease();

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
    QString getStateMessage(CsaGameCoordinator::GameState state) const;

    /**
     * @brief 通信ログウィンドウを作成する
     */
    void createLogWindow();

    /**
     * @brief ログウィンドウのフォントサイズを更新する
     * @param delta 変更量（+1または-1）
     */
    void updateLogFontSize(int delta);

private:
    CsaGameCoordinator* m_coordinator;  ///< CSA通信対局コーディネータ
    QLabel* m_statusLabel;              ///< 状態表示ラベル
    QLabel* m_detailLabel;              ///< 詳細メッセージラベル
    QProgressBar* m_progressBar;        ///< プログレスバー（待機中表示用）
    QPushButton* m_cancelButton;        ///< キャンセルボタン
    QPushButton* m_showLogButton;       ///< 通信ログ表示ボタン

    // 通信ログウィンドウ
    QDialog* m_logWindow;               ///< 通信ログウィンドウ
    QPlainTextEdit* m_logTextEdit;      ///< 通信ログテキスト表示

    // フォントサイズ調整ボタン（ログウィンドウ）
    QToolButton* m_btnLogFontIncrease;  ///< ログA+ボタン
    QToolButton* m_btnLogFontDecrease;  ///< ログA-ボタン
    int m_logFontSize;                   ///< ログウィンドウのフォントサイズ

    // コマンド入力UI
    QPushButton* m_btnSendToServer;     ///< CSAサーバーへ送信ボタン
    QLineEdit* m_commandInput;          ///< コマンド入力欄
};

#endif // CSAWAITINGDIALOG_H
