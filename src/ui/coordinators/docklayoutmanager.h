#ifndef DOCKLAYOUTMANAGER_H
#define DOCKLAYOUTMANAGER_H

/// @file docklayoutmanager.h
/// @brief ドックレイアウト管理クラスの定義


#include <QObject>
#include <QList>
#include <QDockWidget>

class QMainWindow;
class QMenu;

/**
 * @brief ドックウィジェットのレイアウト管理を担当するクラス
 *
 * MainWindowからドックレイアウト管理の責務を分離:
 * - デフォルトレイアウトへのリセット
 * - レイアウトの保存/復元
 * - 起動時レイアウトの設定
 * - ドックのロック/アンロック
 * - 保存済みレイアウトメニューの更新
 */
class DockLayoutManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief ドックの種類を示す列挙型
     */
    enum class DockType {
        Menu,            // メニューウィンドウ
        Joseki,          // 定跡ウィンドウ
        Record,          // 棋譜
        GameInfo,        // 対局情報
        Thinking,        // 思考
        Consideration,   // 検討
        UsiLog,          // USIログ
        CsaLog,          // CSAログ
        Comment,         // コメント
        BranchTree,      // 分岐ツリー
        EvalChart,       // 評価値グラフ
        AnalysisResults, // 棋譜解析結果
        Count            // ドックの数
    };

    explicit DockLayoutManager(QMainWindow* mainWindow, QObject* parent = nullptr);

    /**
     * @brief ドックウィジェットを登録する
     * @param type ドックの種類
     * @param dock ドックウィジェット
     */
    void registerDock(DockType type, QDockWidget* dock);

    /**
     * @brief 保存済みレイアウトメニューを設定する
     * @param menu メニュー
     */
    void setSavedLayoutsMenu(QMenu* menu);

    /**
     * @brief デフォルトのドックレイアウトにリセットする
     */
    void resetToDefault();

    /**
     * @brief 現在のレイアウトを名前を付けて保存する
     * ダイアログを表示して名前を入力させる
     */
    void saveLayoutAs();

    /**
     * @brief 保存済みレイアウトを復元する
     * @param name レイアウト名
     */
    void restoreLayout(const QString& name);

    /**
     * @brief 保存済みレイアウトを削除する
     * @param name レイアウト名
     */
    void deleteLayout(const QString& name);

    /**
     * @brief 指定したレイアウトを起動時のレイアウトに設定する
     * @param name レイアウト名
     */
    void setAsStartupLayout(const QString& name);

    /**
     * @brief 起動時レイアウト設定をクリアする
     */
    void clearStartupLayout();

    /**
     * @brief 起動時レイアウトが設定されていれば復元する
     */
    void restoreStartupLayoutIfSet();

    /**
     * @brief 保存済みレイアウトメニューを更新する
     */
    void updateSavedLayoutsMenu();

    /**
     * @brief メニュー項目の接続を一括で行う
     * @param menuActions 「表示」メニュー配下のアクション群
     */
    void wireMenuActions(QAction* resetLayout,
                         QAction* saveLayoutAs,
                         QAction* clearStartupLayout,
                         QAction* lockDocks,
                         QMenu* savedLayoutsMenu);

    /**
     * @brief ドックのロック状態を切り替える
     * @param locked true=ロック（移動・フローティング禁止）
     */
    void setDocksLocked(bool locked);

    /**
     * @brief ドックの状態（浮動/可視/ジオメトリ）を設定サービスへ保存
     */
    void saveDockStates();

signals:
    /**
     * @brief レイアウトが変更されたとき
     */
    void layoutChanged();

private:
    /**
     * @brief すべてのドックを表示状態にする（復元前の準備）
     */
    void showAllDocks();

    /**
     * @brief 指定したドックを取得する
     * @param type ドックの種類
     * @return ドックウィジェット（未登録の場合はnullptr）
     */
    QDockWidget* dock(DockType type) const;

    QMainWindow* m_mainWindow = nullptr;
    QMenu* m_savedLayoutsMenu = nullptr;
    QList<QDockWidget*> m_docks;
};

#endif // DOCKLAYOUTMANAGER_H
