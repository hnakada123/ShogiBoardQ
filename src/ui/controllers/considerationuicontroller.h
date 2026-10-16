#ifndef CONSIDERATIONUICONTROLLER_H
#define CONSIDERATIONUICONTROLLER_H

/// @file considerationuicontroller.h
/// @brief 検討タブの操作と、検討中の表示（経過時間・候補手の数・矢印）

#include <QObject>
#include <QString>

class QWidget;
class ConsiderationTabManager;
class EngineInfoWidget;
class CandidateArrowPresenter;
class AnalysisSession;
class KifuNavigationState;
class ShogiEngineThinkingModel;
class UsiCommLogModel;

/**
 * @brief 検討タブの操作を検討のセッションへ伝え、検討の状態を検討タブと盤へ出す
 *
 * - 検討タブの開始・中止・エンジンの変更・エンジン設定・候補手の数の変更を受け付ける
 * - 開始は検討タブの設定を1回読んで、表示中の局面で検討のセッションを始める
 * - 検討を始めると検討タブに思考情報モデルと通信ログをつなぎ、ボタンを「検討中止」にする
 * - 検討の終了・待機でタイマーとボタンを戻し、盤の候補手の矢印を更新する
 */
class ConsiderationUiController : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        QWidget* parentWidget = nullptr;                             ///< エンジン設定ダイアログの親
        ConsiderationTabManager* considerationTabManager = nullptr;  ///< 検討タブ
        EngineInfoWidget* thinkingInfo1 = nullptr;                   ///< 思考タブの1つ目のエンジン欄
        CandidateArrowPresenter* arrows = nullptr;                   ///< 候補手の矢印
        AnalysisSession* session = nullptr;                          ///< 検討のセッション
        const KifuNavigationState* navState = nullptr;               ///< 検討する局面（表示中の手）
        ShogiEngineThinkingModel* considerationModel = nullptr;     ///< 検討タブの思考情報（空なら検討タブから取る）
        UsiCommLogModel* commLogModel = nullptr;                     ///< E1 の通信ログ
    };

    explicit ConsiderationUiController(const Deps& deps, QObject* parent = nullptr);

public slots:
    /// 検討タブの設定で、表示中の局面の検討を始める
    void startConsideration();
    /// 検討タブからのエンジン設定の要求
    void onEngineSettingsRequested(int engineNumber, const QString& engineName);
    /// 検討中にエンジンが変わった（中止して新しいエンジンで始め直す）
    void onEngineChanged(int engineIndex, const QString& engineName);
    /// 矢印表示のチェックが変わった
    void onShowArrowsChanged(bool checked);

private slots:
    void stopConsideration();
    void onModeEnded();
    /// 時間切れの後、次の局面を待っている（タイマーは止めるが、ボタンは「検討中止」のまま）
    void onWaitingStarted();
    /// 検討中に候補手の数が変わった
    void onMultiPVChanged(int value);

private:
    /// 検討タブと思考タブに、検討の思考情報モデルと通信ログをつなぐ
    void attachModels();
    void updateArrows();

    QWidget* m_parentWidget = nullptr;
    ConsiderationTabManager* m_considerationTabManager = nullptr;
    EngineInfoWidget* m_thinkingInfo1 = nullptr;
    CandidateArrowPresenter* m_arrows = nullptr;
    AnalysisSession* m_session = nullptr;
    const KifuNavigationState* m_navState = nullptr;
    ShogiEngineThinkingModel* m_considerationModel = nullptr;
    UsiCommLogModel* m_commLogModel = nullptr;

    bool m_considerationActive = false;  ///< 中止後は保持した読み筋から矢印を作り直さない
    bool m_showArrows = true;            ///< 矢印表示の設定
};

#endif // CONSIDERATIONUICONTROLLER_H
