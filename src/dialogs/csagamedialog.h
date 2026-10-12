#ifndef CSAGAMEDIALOG_H
#define CSAGAMEDIALOG_H

/// @file csagamedialog.h
/// @brief CSA対局ダイアログクラスの定義


#include <QDialog>
#include <QList>

#include <memory>

#include "fontsizehelper.h"

QT_BEGIN_NAMESPACE
namespace Ui { class CsaGameDialog; }
QT_END_NAMESPACE

class QComboBox;

/**
 * @brief CSA通信対局ダイアログクラス
 *
 * CSAプロトコルを使用した通信対局の設定を行うダイアログ。
 * 「基本」タブでサーバー接続情報（ホスト、ポート、ID、パスワード）と
 * こちら側の対局者（人間またはエンジン）を、「詳細」タブで接続の維持、
 * 対局数と再接続、棋譜の自動保存、盤の反転、shogi-server のゲーム名を設定する。
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
    };

    /// 対局数の決め方
    enum class GameCountMode {
        Single = 0,     ///< 1局で終了
        Count = 1,      ///< 指定回数
        Unlimited = 2   ///< 無制限
    };

    /// shogi-server に伝える手番の希望
    enum class TurnPreference {
        None = 0,       ///< 指定なし
        Black = 1,      ///< 先手
        White = 2       ///< 後手
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
     * @brief パスワードを取得（入力されたままの値）
     * @return パスワード
     */
    QString password() const;

    /**
     * @brief LOGIN コマンドに載せるパスワード欄の文字列を取得
     *
     * shogi-server のゲーム名が指定されていれば「ゲーム名[-b|-w],パスワード」の形にする。
     */
    QString loginPassword() const;

    // ========== 対局者情報の取得 ==========

    /**
     * @brief こちら側が人間かどうかを取得
     * @return 人間の場合true
     */
    bool isHuman() const;

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

    // ========== 詳細設定の取得 ==========

    bool keepAliveEnabled() const { return m_keepAliveEnabled; }
    int keepAliveIntervalSec() const { return m_keepAliveIntervalSec; }
    bool tcpKeepAliveEnabled() const { return m_tcpKeepAliveEnabled; }
    int thinkingMarginSec() const { return m_thinkingMarginSec; }
    /// 対局数の上限（無制限は 0）
    int gameLimit() const;
    bool reconnectAfterGame() const { return m_reconnectAfterGame; }
    bool autoSaveEnabled() const { return m_autoSaveEnabled; }
    QString autoSaveDir() const { return m_autoSaveDir; }
    /// 自動保存の形式（拡張子: kif/kifu/ki2/csa/jkf/usi）
    QString autoSaveFormat() const { return m_autoSaveFormat; }
    bool autoFlipBoard() const { return m_autoFlipBoard; }
    QString gameName() const { return m_gameName; }
    TurnPreference turnPreference() const { return m_turnPreference; }

    /**
     * @brief 登録エンジンと接続履歴を設定ファイルから読み直す（入力中の値は残す）
     *
     * ダイアログは使い回すため、表示のたびに呼んで前回以降の変更を反映する。
     */
    void refreshChoices();

private slots:
    void updateFormState();
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
     * @brief 自動保存先の「参照...」ボタンが押された時の処理
     */
    void onAutoSaveDirBrowse();

    /**
     * @brief フォントサイズを大きくする
     */
    void onFontIncrease();

    /**
     * @brief フォントサイズを小さくする
     */
    void onFontDecrease();

private:
    QString validationMessage() const;
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

    /// 「詳細」タブの設定を NetworkSettings から読み込む
    void loadAdvancedSettings();

    /// 「詳細」タブの設定を NetworkSettings に保存する
    void saveAdvancedSettings();

    /// 自動保存の形式コンボボックスに選択肢を入れる
    void populateAutoSaveFormats();

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
     * @brief ダイアログ全体にフォントサイズを適用する
     */
    void applyFontSize();

private:
    std::unique_ptr<Ui::CsaGameDialog> ui; ///< UIオブジェクト
    QList<Engine> m_engineList;         ///< エンジンリスト
    QList<ServerHistory> m_serverHistory; ///< サーバー接続履歴
    FontSizeHelper m_fontHelper;          ///< フォントサイズヘルパー

    // 設定値のキャッシュ
    bool m_isHuman = true;  ///< こちら側が人間かどうか
    QString m_engineName;   ///< 選択されたエンジン名
    int m_engineNumber = 0; ///< 選択されたエンジン番号
    QString m_host;         ///< 接続先ホスト
    int m_port = 4081;      ///< ポート番号
    QString m_loginId;      ///< ログインID
    QString m_password;     ///< パスワード

    // 詳細設定のキャッシュ
    bool m_keepAliveEnabled = true;
    int m_keepAliveIntervalSec = 40;
    bool m_tcpKeepAliveEnabled = true;
    int m_thinkingMarginSec = 1;
    GameCountMode m_gameCountMode = GameCountMode::Single;
    int m_gameCount = 2;
    bool m_reconnectAfterGame = false;
    bool m_autoSaveEnabled = false;
    QString m_autoSaveDir;
    QString m_autoSaveFormat = QStringLiteral("kif");
    bool m_autoFlipBoard = true;
    QString m_gameName;
    TurnPreference m_turnPreference = TurnPreference::None;
};

#endif // CSAGAMEDIALOG_H
