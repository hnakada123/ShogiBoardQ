#ifndef CSAGAMEDIALOG_H
#define CSAGAMEDIALOG_H

/// @file csagamedialog.h
/// @brief CSA対局ダイアログクラスの定義

#include "enginelistsettings.h"
#include <QDialog>
#include "networksettings.h"
#include "settingsformbinding.h"
#include <QList>

#include <memory>

#include "csagamecoordinator.h"

QT_BEGIN_NAMESPACE
namespace Ui { class CsaGameDialog; }
QT_END_NAMESPACE

class EngineSelector;
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
    /// 登録済みエンジン（名前・実行ファイル・作者）
    using Engine = EngineListSettings::EngineEntry;

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

    /// 画面で選んだ条件（接続・対局の進め方と、棋譜の自動保存・盤の向き）
    struct Selection {
        /// 接続と対局の進め方（パスワード欄はゲーム名と手番の希望を含む。終局した棋譜の自動保存も）
        CsaGameCoordinator::StartOptions start;
        bool autoFlipBoard = false;
    };
    /// 画面の値から条件を作る
    Selection selection() const;

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

private:
    /// 対局数の決め方（画面の選択）
    GameCountMode gameCountMode() const;
    /// TLS で接続するか（画面の選択）
    bool useTls() const;
    QString validationMessage() const;
    /**
     * @brief シグナル・スロットの接続を行う
     */
    void connectSignalsAndSlots();

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

    /// 自動保存の形式コンボボックスに選択肢を入れる
    void populateAutoSaveFormats();

    /**
     * @brief サーバー履歴の表示文字列を生成する
     * @param history サーバー履歴
     * @return 表示用文字列（例: "172.17.0.2:4000 user_id"）
     */
    QString formatServerHistoryDisplay(const NetworkSettings::CsaServerHistoryEntry& history) const;

    /**
     * @brief 選んだエンジンの設定ダイアログを表示する
     */
    void showEngineSettingsDialog(const EngineListSettings::EngineEntry& engine);

private:
    std::unique_ptr<Ui::CsaGameDialog> ui; ///< UIオブジェクト
    EngineSelector* m_engineSelector = nullptr; ///< エンジンの選択（QObject parent 所有）
    QList<NetworkSettings::CsaServerHistoryEntry> m_serverHistory; ///< サーバー接続履歴
    SettingsFormBinding m_form;   ///< 入力欄と設定の対応（「対局」と「詳細」のタブ）

    // 設定値のキャッシュ

    // 詳細設定のキャッシュ
};

#endif // CSAGAMEDIALOG_H
