#ifndef DOCKLAYOUTMANAGER_H
#define DOCKLAYOUTMANAGER_H

/// @file docklayoutmanager.h
/// @brief ドックレイアウト管理クラスの定義


#include <QObject>
#include <QList>
#include <QDockWidget>

class DockRegistry;
class QMainWindow;
class QMenu;

/**
 * @brief ドックウィジェットのレイアウト管理を担当するクラス
 *
 * 責務:
 * - デフォルトレイアウトへのリセット（DockRegistry の表の配置）
 * - レイアウトの保存/復元
 * - 起動時レイアウトの設定
 * - ドックのロック/アンロック
 * - 保存済みレイアウトメニューの更新
 */
class DockLayoutManager : public QObject
{
    Q_OBJECT

public:
    /// docks のドックの配置を扱う（ドックの表・規定の配置・状態の保存は DockRegistry）
    DockLayoutManager(QMainWindow* mainWindow, DockRegistry* docks, QObject* parent = nullptr);

    /**
     * @brief 保存済みレイアウトのメニューを設定する（項目はここが作ってつなぐ）
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
     * @brief ドックのロック状態を切り替えて保存する（作ったときは保存した状態にする）
     * @param locked true=ロック（移動・フローティング禁止）
     */
    void setDocksLocked(bool locked);
    /// ドックを固定しているか（「ドックを固定」のチェックの初期値）
    bool docksLocked() const;

    /**
     * @brief すべてのドックの状態（浮動/可視/ジオメトリ）を保存
     */
    void saveDockStates();

private:
    /**
     * @brief すべてのドックを表示状態にする（復元前の準備）
     */
    void showAllDocks();

    /// sender() の QAction::data() からレイアウト名を取得して restoreLayout を呼ぶ
    void onRestoreLayoutTriggered();
    /// sender() の QAction::data() からレイアウト名を取得して setAsStartupLayout を呼ぶ
    void onSetStartupLayoutTriggered();
    /// sender() の QAction::data() からレイアウト名を取得して deleteLayout を呼ぶ
    void onDeleteLayoutTriggered();

    QMainWindow* m_mainWindow = nullptr;
    QMenu* m_savedLayoutsMenu = nullptr;
    DockRegistry* m_docks = nullptr;
};

#endif // DOCKLAYOUTMANAGER_H
