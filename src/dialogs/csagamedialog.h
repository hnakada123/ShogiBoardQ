#ifndef CSAGAMEDIALOG_H
#define CSAGAMEDIALOG_H

/// @file csagamedialog.h
/// @brief CSA対局ダイアログクラスの定義


#include <QDialog>
#include <QList>

QT_BEGIN_NAMESPACE
namespace Ui { class CsaGameDialog; }
QT_END_NAMESPACE

class QComboBox;

/**
 * @brief CSA通信対局ダイアログクラス
 *
 * CSAプロトコルを使用した通信対局の設定を行うダイアログ。
 * サーバー接続情報（ホスト、ポート、ID、パスワード）と
 * こちら側の対局者（人間またはエンジン）の設定を行う。
 */
class CsaGameDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief エンジン情報を格納する構造体
     */
    struct Engine
    {
        QString name;   ///< エンジン名
        QString path;   ///< エンジンの実行ファイルパス
    };

    /**
     * @brief サーバー接続履歴を格納する構造体
     */
    struct ServerHistory
    {
        QString host;       ///< 接続先ホスト
        int port;           ///< ポート番号
        QString id;         ///< ログインID
        QString password;   ///< パスワード
        QString version;    ///< CSAプロトコルバージョン
    };

    /**
     * @brief コンストラクタ
     * @param parent 親ウィジェット
     */
    explicit CsaGameDialog(QWidget *parent = nullptr);

    /**
     * @brief デストラクタ
     */
    ~CsaGameDialog() override;

    // ========== サーバー接続情報の取得 ==========

    /**
     * @brief 接続先ホストを取得
     * @return 接続先ホスト名またはIPアドレス
     */
    QString host() const;

    /**
     * @brief ポート番号を取得
     * @return ポート番号
     */
    int port() const;

    /**
     * @brief ログインIDを取得
     * @return ログインID
     */
    QString loginId() const;

    /**
     * @brief パスワードを取得
     * @return パスワード
     */
    QString password() const;

    /**
     * @brief CSAプロトコルバージョンを取得
     * @return バージョン文字列
     */
    QString csaVersion() const;

    // ========== 対局者情報の取得 ==========

    /**
     * @brief こちら側が人間かどうかを取得
     * @return 人間の場合true
     */
    bool isHuman() const;

    /**
     * @brief こちら側がエンジンかどうかを取得
     * @return エンジンの場合true
     */
    bool isEngine() const;

    /**
     * @brief 選択されているエンジン名を取得
     * @return エンジン名
     */
    QString engineName() const;

    /**
     * @brief 選択されているエンジン番号を取得
     * @return エンジン番号（コンボボックスのインデックス）
     */
    int engineNumber() const;

    /**
     * @brief エンジンリストを取得
     * @return エンジンリストへの参照
     */
    const QList<Engine>& engineList() const;

private slots:
    /**
     * @brief 対局開始ボタンが押された時の処理
     */
    void onAccepted();

    /**
     * @brief エンジン設定ボタンが押された時の処理
     */
    void onEngineSettingsClicked();

    /**
     * @brief サーバー履歴が選択された時の処理
     * @param index 選択されたインデックス
     */
    void onServerHistoryChanged(int index);

    /**
     * @brief パスワード表示チェックボックスの状態が変化した時の処理
     * @param checked チェック状態
     */
    void onShowPasswordToggled(bool checked);

    /**
     * @brief フォントサイズを大きくする
     */
    void onFontIncrease();

    /**
     * @brief フォントサイズを小さくする
     */
    void onFontDecrease();

private:
    /**
     * @brief シグナル・スロットの接続を行う
     */
    void connectSignalsAndSlots();

    /**
     * @brief 設定ファイルからエンジン情報を読み込む
     */
    void loadEngineConfigurations();

    /**
     * @brief UIにエンジン設定を反映する
     */
    void populateUIWithEngines();

    /**
     * @brief 設定ファイルからサーバー接続履歴を読み込む
     */
    void loadServerHistory();

    /**
     * @brief 現在の設定をサーバー接続履歴として保存する
     */
    void saveServerHistory();

    /**
     * @brief UIにサーバー履歴を反映する
     */
    void populateUIWithServerHistory();

    /**
     * @brief 設定ファイルから対局設定を読み込む
     */
    void loadGameSettings();

    /**
     * @brief 対局設定を設定ファイルに保存する
     */
    void saveGameSettings();

    /**
     * @brief サーバー履歴の表示文字列を生成する
     * @param history サーバー履歴
     * @return 表示用文字列（例: "172.17.0.2:4000 user_id"）
     */
    QString formatServerHistoryDisplay(const ServerHistory& history) const;

    /**
     * @brief エンジン設定ダイアログを表示する
     * @param comboBox エンジン選択コンボボックス
     */
    void showEngineSettingsDialog(QComboBox* comboBox);

    /**
     * @brief フォントサイズを更新する
     * @param delta 変更量（+1または-1）
     */
    void updateFontSize(int delta);

    /**
     * @brief ダイアログ全体にフォントサイズを適用する
     */
    void applyFontSize();

private:
    Ui::CsaGameDialog* ui;              ///< UIオブジェクト
    QList<Engine> m_engineList;         ///< エンジンリスト
    QList<ServerHistory> m_serverHistory; ///< サーバー接続履歴
    int m_fontSize;                     ///< フォントサイズ

    // 設定値のキャッシュ
    bool m_isHuman;         ///< こちら側が人間かどうか
    bool m_isEngine;        ///< こちら側がエンジンかどうか
    QString m_engineName;   ///< 選択されたエンジン名
    int m_engineNumber;     ///< 選択されたエンジン番号
    QString m_host;         ///< 接続先ホスト
    int m_port;             ///< ポート番号
    QString m_loginId;      ///< ログインID
    QString m_password;     ///< パスワード
    QString m_csaVersion;   ///< CSAプロトコルバージョン
};

#endif // CSAGAMEDIALOG_H
