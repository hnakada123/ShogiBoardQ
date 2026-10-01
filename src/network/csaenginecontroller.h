#ifndef CSAENGINECONTROLLER_H
#define CSAENGINECONTROLLER_H

/// @file csaenginecontroller.h
/// @brief CSA通信対局用エンジンコントローラの定義

#include <QObject>
#include <QString>
#include <QPoint>
#include <QPointer>

class Usi;
class UsiCommLogModel;
class ShogiEngineThinkingModel;
class ShogiGameController;

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
        int engineNumber = 0;
        ShogiGameController* gameController = nullptr;
        UsiCommLogModel* commLog = nullptr;
        ShogiEngineThinkingModel* thinkingModel = nullptr;
    };

    struct ThinkingParams {
        QString positionCmd;
        int byoyomiMs = 0;
        QString btimeStr;
        QString wtimeStr;
        int bincMs = 0;
        int wincMs = 0;
        bool useByoyomi = false;
    };

    struct ThinkingResult {
        QPoint from;
        QPoint to;
        bool promote = false;
        bool resign = false;
        bool valid = false;
        int scoreCp = 0;
        QString scoreMate;
    };

    explicit CsaEngineController(QObject* parent = nullptr);
    ~CsaEngineController() override;

    void initialize(const InitParams& params);
    void thinkAsync(const ThinkingParams& params);
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

private slots:
    void onEngineInitialized();
    void onEngineError(const QString& message);
    void onMatchMoveReady(const QPoint& from, const QPoint& to, const QString& position, const QString& ponder);
    void onEngineResign();

private:
    QString m_engineName;
    Usi* m_engine = nullptr;
    QString m_ponderPosition; ///< 次の手番まで保持する予測局面
    QPointer<ShogiGameController> m_gameController;
    QPointer<UsiCommLogModel> m_engineCommLog;
    QPointer<ShogiEngineThinkingModel> m_engineThinking;
};

#endif // CSAENGINECONTROLLER_H
