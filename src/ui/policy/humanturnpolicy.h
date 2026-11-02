#ifndef HUMANTURNPOLICY_H
#define HUMANTURNPOLICY_H

/// @file humanturnpolicy.h
/// @brief 盤を人間が操作できるか・棋譜を移って・変えてよいか・対局が進行中かの判定

class AppActivityState;
class CsaGameCoordinator;
class LiveGameSession;
class MatchCoordinator;
class ShogiBoard;

/**
 * @brief 盤を人間が操作できるか・棋譜を移って・変えてよいか・対局が進行中かを、アプリの動作状態と対局の司令塔から判定する
 *
 * 動作状態ごとの盤の手の行き先（boardInput）と、棋譜を移ってよいか（canBrowseRecord）・変えてよいか
 * （canEditRecord）はここだけで決める。棋譜を移る・変える入口（棋譜欄・分岐候補欄・分岐ツリー・評価値グラフ・
 * 定跡ウィンドウ・棋譜解析の結果・自動化 API）はすべてこれを通す。ローカル対局は MatchCoordinator
 * （どちらの手番が人間か・手を受け付けるか）、通信対局は CsaGameCoordinator に聞く。QObject 非継承。
 */
class HumanTurnPolicy
{
public:
    struct Deps {
        const AppActivityState* activity = nullptr;
        const ShogiBoard* board = nullptr;                 ///< 盤（手番）
        const MatchCoordinator* match = nullptr;
        const CsaGameCoordinator* csaGame = nullptr;     ///< 通信対局の司令塔
        /// 対局の棋譜を書いている間は、棋譜を変えない
        const LiveGameSession* liveSession = nullptr;
    };

    /// 盤で指した手の行き先
    enum class BoardInput {
        None,        ///< 指せない（相手の手番・終局後・検討中・終了処理の後など）
        Edit,        ///< 局面編集: 駒の配置を変える
        Variation,   ///< 待機中: 表示中の手の次の手（変化）として棋譜に記録する（canEditRecord のときだけ）
        Game,        ///< ローカル対局で人間の手番: 指して対局の司令塔へ渡す
        CsaServer,   ///< 通信対局で自分の手番: サーバーへ送る（盤は確認を受けてから動かす）
    };

    /// 依存が無いものは「操作できない」「対局していない」と答える
    explicit HumanTurnPolicy(const Deps& deps) : m_deps(deps) {}

    /// 今の動作状態で、盤で指した手をどう扱うか
    BoardInput boardInput() const;

    /**
     * @brief 利用者の操作で表示中の手を移ってよいか
     *
     * 盤と棋譜を見るだけの状態（待機中・検討中・詰み探索中。UiStatePolicyManager の表の WidgetNavigation の行）
     * だけ移れる。対局中・通信対局中・棋譜解析中・局面編集中は、表示中の手が対局の盤・解析・編集中の局面と
     * 結び付いているので移らない
     */
    bool canBrowseRecord() const;

    /**
     * @brief 棋譜（分岐ツリー）を変えてよいか（分岐の削除・本譜にする・並べ替え・定跡の展開・盤の手の記録）
     *
     * 棋譜を移ってよい状態で、対局の棋譜を書いていないとき。検討中・詰み探索中も変えてよい
     * （検討は表示中の局面が変われば送り直す）。盤で指した手を記録するのは待機中だけ（boardInput）
     */
    bool canEditRecord() const;

    /// 盤を人間が指せる手番か（対局していなければ、変化の入力を受け付けるか。局面編集は含まない）
    bool isHumanTurnNow() const;

    /// 対局が進行中（終局前）か
    bool isGameActivelyInProgress() const;

    /// 終了処理の後は、盤のクリックが届いても何も受け付けない
    void disable() { m_disabled = true; }

private:
    const Deps m_deps;
    bool m_disabled = false;
};

#endif // HUMANTURNPOLICY_H
