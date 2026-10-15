#ifndef JOSEKIWINDOWCONTROLLER_H
#define JOSEKIWINDOWCONTROLLER_H

/// @file josekiwindowcontroller.h
/// @brief 定跡ウィンドウの表示・更新と、定跡手の着手


#include <QObject>
#include <functional>
#include <utility>
#include <QString>
#include <QStringList>
#include <QPoint>


struct AppState;
class JosekiWindow;
class KifuNavigationState;
class QDockWidget;
class ShogiGameController;

/**
 * @brief 定跡ウィンドウを作って表示中の局面に合わせ、選んだ定跡手を盤への着手として要求する
 *
 * 責務:
 * - JosekiWindowの生成・表示管理
 * - 定跡手選択シグナルの処理（USI→QPoint変換）
 * - 棋譜データの収集と提供
 * - humanCanPlayの判定
 *
 * 本クラスは定跡ウィンドウ関連の処理を一元管理し、
 * MainWindowの肥大化を防ぐ。
 */
class JosekiWindowController : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 依存オブジェクト構造体
     */
    struct Dependencies {
        QWidget* parentWidget = nullptr;
        ShogiGameController* gameController = nullptr;
        const AppState* state = nullptr;  ///< 表示中の局面（非所有）
        /// 表示中の手順（分岐ツリー）。棋譜のマージに使う
        const KifuNavigationState* navState = nullptr;
    };

    /// 盤面クリックと同じ「人間が今指せるか」の判定
    void setHumanTurnCheck(std::function<bool()> isHumanTurnNow) { m_isHumanTurnNow = std::move(isHumanTurnNow); }

    /**
     * @brief コンストラクタ
     * @param deps 依存オブジェクト
     * @param parent 親オブジェクト
     */
    explicit JosekiWindowController(const Dependencies& deps, QObject* parent = nullptr);

    /**
     * @brief デストラクタ
     */
    ~JosekiWindowController() override = default;

    /**
     * @brief 定跡ウィンドウを更新する
     *
     * 現在の局面と手番情報を反映する。
     */
    void updateJosekiWindow();

    /**
     * @brief 定跡ウィンドウを取得
     * @return JosekiWindowへのポインタ（未作成時はnullptr）
     */
    JosekiWindow* josekiWindow() const { return m_josekiWindow; }

    /**
     * @brief JosekiWindowを確保する（未作成時のみ作成）
     */
    void ensureJosekiWindow();

    /**
     * @brief 定跡ウィンドウを入れたドックを登録する
     *
     * タブにまとめたドックは裏に回っても Qt では表示中のままなので、ドックの
     * visibilityChanged で前面にあるかを覚え、前面にある間だけ更新する。
     */
    void attachDock(QDockWidget* dock);

signals:
    /**
     * @brief 指し手実行を要求
     * @param from 移動元座標
     * @param to 移動先座標
     */
    void moveRequested(const QPoint& from, const QPoint& to);

    /**
     * @brief 強制成りモードを設定
     * @param forced 強制するかどうか
     * @param promote 成るかどうか
     */
    void forcedPromotionRequested(bool forced, bool promote);

private slots:
    /**
     * @brief 定跡手選択時の処理
     * @param usiMove USI形式の指し手
     */
    void onJosekiMoveSelected(const QString& usiMove);

    /**
     * @brief 棋譜データマージ要求時の処理
     */
    void onRequestKifuDataForMerge();

    /// ドックが前面に出た・裏に回った・閉じたときの処理
    void onDockVisibilityChanged(bool visible);

private:



    /**
     * @brief 人間が着手可能かどうかを判定
     * @return 着手可能時true
     */
    bool determineHumanCanPlay() const;


    // 依存オブジェクト
    QWidget* m_parentWidget = nullptr;
    ShogiGameController* m_gameController = nullptr;
    const AppState* m_state = nullptr;
    const KifuNavigationState* m_navState = nullptr;
    std::function<bool()> m_isHumanTurnNow;  ///< 人間が今指せるか

    // 内部オブジェクト
    JosekiWindow* m_josekiWindow = nullptr;
    /// 定跡ウィンドウを入れたドック（無ければ nullptr。単独のウィンドウとして使う場合）
    QDockWidget* m_dockWidget = nullptr;
    /// ドックが前面に出ているか（visibilityChanged で更新）
    bool m_dockShown = false;
};

#endif // JOSEKIWINDOWCONTROLLER_H
