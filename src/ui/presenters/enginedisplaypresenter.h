#ifndef ENGINEDISPLAYPRESENTER_H
#define ENGINEDISPLAYPRESENTER_H

/// @file enginedisplaypresenter.h
/// @brief エンジンの表示（思考・検討・通信ログ・評価値グラフの系列名）の持ち主

#include <QObject>
#include <QPointer>
#include <QString>
#include <array>

class AnalysisSession;
class ConsiderationPanel;
class CsaLogPanel;
class EngineLifecycleManager;
class EvaluationGraphPresenter;
class KifuAnalysisListModel;
class KifuDocument;
class ShogiEngineThinkingModel;
class ThinkingPanel;
class Usi;
class UsiCommLogModel;
class UsiLogPanel;
class UsiModelBinding;

/**
 * @brief エンジンの表示を1か所で受け持つ
 *
 * - 枠 E1・E2（EngineLifecycleManager）にエンジンが入るたびに UsiModelBinding を作り、思考の表・情報欄・
 *   通信ログのモデルと結ぶ
 * - 検討の表示（検討タブの実行の状態・経過時間・情報欄のモデル・検討の表への読み筋）は、検討のセッション
 *   （AnalysisSession）の知らせだけから決める。検討中かどうかの正本はセッション
 * - 対局の2つ目のエンジン欄と評価値グラフの系列名は、文書の対局者（KifuDocument::players）から、
 *   検討・詰み探索のときはそのエンジンから決める
 * - 棋譜を作り直すとき（新規・対局の開始前・読み込み前）の表示の消去（clearAll）
 */
class EngineDisplayPresenter : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        EngineLifecycleManager* engines = nullptr;        ///< エンジンの枠（必須）
        AnalysisSession* analysis = nullptr;              ///< 検討・詰み探索のセッション
        const KifuDocument* document = nullptr;           ///< 対局者（2つ目のエンジン欄・系列名）
        UsiCommLogModel* commLog1 = nullptr;
        UsiCommLogModel* commLog2 = nullptr;
        ShogiEngineThinkingModel* thinking1 = nullptr;
        ShogiEngineThinkingModel* thinking2 = nullptr;
        ShogiEngineThinkingModel* consideration = nullptr; ///< 検討の表
        KifuAnalysisListModel* analysisModel = nullptr;   ///< 棋譜解析の結果
        ThinkingPanel* thinkingPanel = nullptr;           ///< 思考ドック（2つ目のエンジン欄・エンジン名）
        ConsiderationPanel* considerationTab = nullptr; ///< 検討ドック
        EvaluationGraphPresenter* evalGraph = nullptr;    ///< 評価値グラフの系列名
        UsiLogPanel* usiLog = nullptr;
        CsaLogPanel* csaLog = nullptr;
    };

    explicit EngineDisplayPresenter(const Deps& deps, QObject* parent = nullptr);

    /// 前の対局・検討・解析のエンジンの表示を消す（通信ログ・思考情報・検討・解析結果・エンジン名）
    void clearAll();

private slots:
    void bindEngines();
    void clearEngineOutput();
    void onConsiderationSearchStarted();
    void onConsiderationWaitingStarted();
    void onConsiderationModeEnded();
    void clearConsideration();
    void onAnalysisEngineNamesChanged(const QString& engine1, const QString& engine2);
    /// 対局者が変わった（対局の開始・棋譜の置き換え）
    void onPlayersChanged();

private:
    void setSecondEngineVisible(bool visible);

    const Deps m_deps;
    std::array<QPointer<Usi>, 2> m_engines;               ///< 結んだエンジン（E1・E2）
    std::array<QPointer<UsiModelBinding>, 2> m_bindings;
};

#endif // ENGINEDISPLAYPRESENTER_H
