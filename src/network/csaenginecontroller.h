#ifndef CSAENGINECONTROLLER_H
#define CSAENGINECONTROLLER_H

/// @file csaenginecontroller.h
/// @brief CSA通信対局用エンジンコントローラの定義

#include <QObject>
#include <QString>
#include <QPoint>
#include <QPointer>

#include "usimove.h"
#include "usitimingparams.h"

class Usi;
class UsiCommLogModel;
class ShogiEngineThinkingModel;

/**
 * @brief CSA通信対局用のUSIエンジンライフサイクル管理クラス
 *
 * エンジンの初期化・思考指示・終了を担当する。
 * CsaGameCoordinatorからエンジン関連の責務を分離。
 */
class CsaEngineController : public QObject
{
    Q_OBJECT

public:
    struct InitParams {
        QString enginePath;
        QString engineName;
        UsiCommLogModel* commLog = nullptr;
        ShogiEngineThinkingModel* thinkingModel = nullptr;
    };

    struct ThinkingParams {
        QString positionCmd;
        UsiTimingParams timing;  ///< 残り時間（USI の btime/wtime の意味）・決まり・遅延・余裕
    };

    struct ThinkingResult {
        UsiMove move;  ///< エンジンの指し手（成り・不成を含む）
        bool resign = false;
        bool valid = false;
        int scoreCp = 0;
        QString scoreMate;
    };

    explicit CsaEngineController(QObject* parent = nullptr);
    ~CsaEngineController() override;

    void initialize(const InitParams& params);
    void thinkAsync(const ThinkingParams& params);
    /// 思考中なら stop を送り、その時点の最善手ですぐ指させる
    void stopThinking();
    void sendGameOver(bool win);
    void sendQuit();
    void cleanup();

    Usi* engine() const { return m_engine; }
    bool isInitialized() const { return m_engine != nullptr; }

signals:
    void initialized();
    void engineError(const QString& message);
    void thinkingFinished(const CsaEngineController::ThinkingResult& result);
    void logMessage(const QString& message, bool isError = false);
    void resignRequested();
    void winDeclarationRequested();

private slots:
    void onEngineInitialized();
    void onEngineError(const QString& message);
    void onMatchMoveReady(const UsiMove& move, const QString& position, const QString& ponder);
    void onEngineResign();
    void onEngineWinDeclaration();

private:
    QString m_engineName;
    Usi* m_engine = nullptr;
    QString m_ponderPosition; ///< 次の手番まで保持する予測局面
    QPointer<UsiCommLogModel> m_engineCommLog;
    QPointer<ShogiEngineThinkingModel> m_engineThinking;
};

#endif // CSAENGINECONTROLLER_H
