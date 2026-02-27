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
    };

    explicit CsaEngineController(QObject* parent = nullptr);
    ~CsaEngineController() override;

    void initialize(const InitParams& params);
    ThinkingResult think(const ThinkingParams& params);
    void sendGameOver(bool win);
    void sendQuit();
    void cleanup();

    Usi* engine() const { return m_engine; }
    bool isInitialized() const { return m_engine != nullptr; }

signals:
    void logMessage(const QString& message, bool isError = false);
    void resignRequested();

private slots:
    void onBestMoveReceived();
    void onEngineResign();

private:
    Usi* m_engine = nullptr;
    QPointer<ShogiGameController> m_gameController;
    UsiCommLogModel* m_engineCommLog = nullptr;
    ShogiEngineThinkingModel* m_engineThinking = nullptr;
    bool m_ownsCommLog = false;
    bool m_ownsThinking = false;
};

#endif // CSAENGINECONTROLLER_H
