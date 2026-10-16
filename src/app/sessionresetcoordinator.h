#ifndef SESSIONRESETCOORDINATOR_H
#define SESSIONRESETCOORDINATOR_H

/// @file sessionresetcoordinator.h
/// @brief 棋譜と表示の作り直し（対局の開始前・新規・局面編集・棋譜の読み込み前）

#include <QString>

#include "startoptions.h"

class ConsiderationTabManager;
class CsaLogPanel;
class ThinkingPanel;
class UsiLogPanel;
class EvaluationGraphPresenter;
class KifuAnalysisListModel;
class KifuBranchNode;
class KifuDocument;
class KifuLoadCoordinator;
class LiveGameSession;
class ShogiEngineThinkingModel;
class UsiCommLogModel;

/**
 * @brief 棋譜（文書）とエンジン・解析の表示を作り直す処理を1か所にまとめる
 *
 * 棋譜の作り直しは文書の置き換え（KifuDocument::startNew）で、対局情報・対局の記録・上書き保存先も一緒に消える。
 * 棋譜欄・分岐・盤・コメントは、ツリーと表示中の手が変わると各部品が自分で合わせるので、ここでは触らない。
 * ここで消すのは、ツリーから作られない表示（通信ログ・思考情報・検討・解析結果・評価値グラフ）だけ。
 */
class SessionResetCoordinator
{
public:
    struct Deps {
        KifuDocument* document = nullptr;
        LiveGameSession* liveSession = nullptr;
        KifuLoadCoordinator* kifuLoader = nullptr;
        EvaluationGraphPresenter* evalGraph = nullptr;
        UsiCommLogModel* commLog1 = nullptr;
        UsiCommLogModel* commLog2 = nullptr;
        ShogiEngineThinkingModel* thinking1 = nullptr;
        ShogiEngineThinkingModel* thinking2 = nullptr;
        ShogiEngineThinkingModel* consideration = nullptr;
        KifuAnalysisListModel* analysisModel = nullptr;
        ThinkingPanel* thinkingPanel = nullptr;              ///< エンジン名の表示
        ConsiderationTabManager* considerationTab = nullptr; ///< 検討のエンジン名・経過時間
        UsiLogPanel* usiLog = nullptr;
        CsaLogPanel* csaLog = nullptr;
    };

    explicit SessionResetCoordinator(const Deps& deps) : m_deps(deps) {}

    /**
     * @brief 対局を始める局面に棋譜を整え、ライブ対局を始める
     *
     * 現在局面から始めるときは開始ノードまでの手順を残す（中断の行は除く）。それ以外は rootSfen だけの新しい棋譜にする。
     * @return 対局を始めたノード。現在局面のノードが無くなっていたら nullptr（何も変えない）
     */
    KifuBranchNode* prepareGameStart(const GameStartPreparation& preparation);

    /// 局面 sfen だけの新しい棋譜にする（新規・局面編集・通信対局の開始）。
    /// keepHeader なら、利用者が対局情報に書きかけた内容を残す（通信対局の開始）
    void resetToNewRecord(const QString& sfen, bool keepHeader = false);

    /// 棋譜を読み込む前に、前の対局の通信ログ・思考情報・評価値グラフを消す
    void clearBeforeKifuLoad();

private:
    void clearEngineDisplays();

    Deps m_deps;
};

#endif // SESSIONRESETCOORDINATOR_H
