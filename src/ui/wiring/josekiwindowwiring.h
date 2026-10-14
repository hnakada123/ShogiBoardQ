#ifndef JOSEKIWINDOWWIRING_H
#define JOSEKIWINDOWWIRING_H

/// @file josekiwindowwiring.h
/// @brief 定跡ウィンドウ配線クラスの定義


#include <QObject>
#include <QString>
#include <QStringList>
#include <QPoint>


class JosekiWindow;
class KifuNavigationState;
class PlayModePolicyService;
class QDockWidget;
class ShogiGameController;

/**
 * @brief 定跡ウィンドウとMainWindowの間のUI配線を担当するクラス
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
class JosekiWindowWiring : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 依存オブジェクト構造体
     */
    struct Dependencies {
        QWidget* parentWidget = nullptr;
        ShogiGameController* gameController = nullptr;
        QString* currentSfenStr = nullptr;
        /// 表示中の手順（分岐ツリー）。棋譜のマージに使う
        const KifuNavigationState* navState = nullptr;
    };

    /// 盤面クリックと同じ「人間が今指せるか」の判定元（対局モードの判定サービスは後で作られる）
    void setPlayModePolicy(const PlayModePolicyService* policy) { m_playModePolicy = policy; }

    /**
     * @brief コンストラクタ
     * @param deps 依存オブジェクト
     * @param parent 親オブジェクト
     */
    explicit JosekiWindowWiring(const Dependencies& deps, QObject* parent = nullptr);

    /**
     * @brief デストラクタ
     */
    ~JosekiWindowWiring() override = default;

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
    QString* m_currentSfenStr = nullptr;
    const KifuNavigationState* m_navState = nullptr;
    const PlayModePolicyService* m_playModePolicy = nullptr;

    // 内部オブジェクト
    JosekiWindow* m_josekiWindow = nullptr;
    /// 定跡ウィンドウを入れたドック（無ければ nullptr。単独のウィンドウとして使う場合）
    QDockWidget* m_dockWidget = nullptr;
    /// ドックが前面に出ているか（visibilityChanged で更新）
    bool m_dockShown = false;
};

#endif // JOSEKIWINDOWWIRING_H
