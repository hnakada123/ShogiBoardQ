#ifndef ENGINEDISPLAYPRESENTER_H
#define ENGINEDISPLAYPRESENTER_H

/// @file enginedisplaypresenter.h
/// @brief エンジンの表示（思考・検討・通信ログ・評価値グラフの系列名）の持ち主

#include <QObject>
#include <QPointer>
#include <QString>
#include <array>
#include <optional>

#include "shogitypes.h"

class AnalysisSession;
class AppActivityState;
class ConsiderationPanel;
class CsaLogPanel;
class EngineSlots;
class EvaluationGraphPresenter;
class KifuAnalysisListModel;
class KifuDocument;
class ShogiEngineThinkingModel;
class ThinkingPanel;
class Usi;
class EngineStatusModel;
class UsiLogPanel;
class UsiModelBinding;

/**
 * @brief エンジンの表示を1か所で受け持つ
 *
 * - 枠 E1・E2（EngineSlots）にエンジンが入るたびに UsiModelBinding を作り、思考の表・情報欄・
 *   通信ログのモデルと結ぶ
 * - 検討の表示（検討タブの実行の状態・経過時間・情報欄のモデル・検討の表への読み筋）は、検討のセッション
 *   （AnalysisSession）の知らせだけから決める。検討中かどうかの正本はセッション
 * - 対局の2つ目のエンジン欄と評価値グラフの系列名は、文書の対局者（KifuDocument::players）から、
 *   検討・詰み探索のときはそのエンジンから、通信対局は司令塔が知らせる自分のエンジン（showMatchEngine）から決める
 * - 思考ドックの情報帯の手番の印（▲・△）も同じく文書の対局者と showMatchEngine から決め、E1 を片方の側の対局者と
 *   して使わない動作（検討・詰み探索・棋譜解析）が始まったら、次の対局まで出さない
 * - 棋譜を作り直すとき（新規・対局の開始前・読み込み前）の表示の消去（clearAll）。棋譜に結び付く表示
 *   （思考・検討・解析結果）だけを消し、通信ログと結んでいるエンジンの名前は残す
 * - 対局を始めるとき（ローカル対局の開始・通信対局の接続の前）の通信ログの消去（clearEngineLogs）。通信対局は
 *   エンジンの起動とログインの後に棋譜を作り直すので、棋譜の作り直しでは消さない
 */
class EngineDisplayPresenter : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        EngineSlots* engines = nullptr;        ///< エンジンの枠（必須）
        AnalysisSession* analysis = nullptr;              ///< 検討・詰み探索のセッション
        const AppActivityState* activity = nullptr;       ///< 動作状態（手番の印を消す）
        const KifuDocument* document = nullptr;           ///< 対局者（2つ目のエンジン欄・系列名）
        EngineStatusModel* commLog1 = nullptr;
        EngineStatusModel* commLog2 = nullptr;
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

    /// 前の対局・検討・解析の、棋譜に結び付くエンジンの表示を消す（思考情報・検討・解析結果。
    /// 結んでいないエンジンの名前も消す）。通信ログは消さない
    void clearAll();

public slots:
    /// 前の対局の通信ログ（USI・CSA）を消す（対局を始めるとき）
    void clearEngineLogs();
    /// 通信対局の自分のエンジン（side が指す）の名前を評価値グラフの系列名に、side を情報帯の手番の印にする
    /// （文書の対局者はサーバーの名前）
    void showMatchEngine(Turn side, const QString& engineName);

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
    /// 動作状態が変わった（検討・詰み探索・棋譜解析の開始で手番の印を消す）
    void onActivityChanged();

private:
    void setSecondEngineVisible(bool visible);
    /// 情報帯の手番の印（first は E1 のエンジン、second は E2 のエンジンが指す側。nullopt は印なし）
    void setEngineSides(std::optional<Turn> first, std::optional<Turn> second);

    const Deps m_deps;
    std::array<QPointer<Usi>, 2> m_engines;               ///< 結んだエンジン（E1・E2）
    std::array<QPointer<UsiModelBinding>, 2> m_bindings;
};

#endif // ENGINEDISPLAYPRESENTER_H
