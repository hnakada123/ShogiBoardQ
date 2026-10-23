#ifndef SESSIONRESETCOORDINATOR_H
#define SESSIONRESETCOORDINATOR_H

/// @file sessionresetcoordinator.h
/// @brief 棋譜と表示の作り直し（対局の開始前・新規・局面編集・棋譜の読み込み前）

#include <QString>

#include "startoptions.h"

class EngineDisplayPresenter;
class EvaluationGraphPresenter;
class KifuBranchNode;
class KifuDocument;
class KifuLoadCoordinator;
class LiveGameSession;

/**
 * @brief 棋譜（文書）とエンジン・解析の表示を作り直す処理を1か所にまとめる
 *
 * 棋譜の作り直しは文書の置き換え（KifuDocument::startNew）で、対局情報・対局の記録・上書き保存先も一緒に消える。
 * 棋譜欄・分岐・盤・コメントは、ツリーと表示中の手が変わると各部品が自分で合わせ、ツリーから作られない表示
 * （評価値グラフと、EngineDisplayPresenter が持つエンジンの表示）は文書の置き換え（KifuDocument::replaced）を
 * 受けて自分で消す。ここで表示に触るのは、文書を置き換えずに現在局面から対局を続けるときだけ。
 */
class SessionResetCoordinator
{
public:
    struct Deps {
        KifuDocument* document = nullptr;
        LiveGameSession* liveSession = nullptr;
        KifuLoadCoordinator* kifuLoader = nullptr;
        EvaluationGraphPresenter* evalGraph = nullptr;
        EngineDisplayPresenter* engineDisplay = nullptr;   ///< 通信ログ・思考情報・検討・解析結果の表示
    };

    explicit SessionResetCoordinator(const Deps& deps) : m_deps(deps) {}

    /**
     * @brief 対局を始める局面に棋譜を整え、ライブ対局を始める
     *
     * 現在局面から始めるときは開始ノードまでの手順を残す（中断の行は除く）。それ以外は rootSfen だけの新しい棋譜にする。
     * @return 対局を始めたノード。現在局面のノードが無くなっていたら nullptr（何も変えない）
     */
    KifuBranchNode* prepareGameStart(const GameStartPreparation& preparation);

    /// 局面 sfen だけの新しい棋譜にする（新規・局面編集）
    void resetToNewRecord(const QString& sfen);

private:
    Deps m_deps;
};

#endif // SESSIONRESETCOORDINATOR_H
