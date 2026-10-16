#ifndef CSAENGINECONTROLLER_H
#define CSAENGINECONTROLLER_H

/// @file csaenginecontroller.h
/// @brief CSA通信対局用エンジンコントローラの定義

#include <QObject>
#include <QString>
#include <QPoint>
#include <QPointer>

#include "usimove.h"
#include "usimatchhandler.h"
#include "usitimingparams.h"

class EngineLifecycleManager;
class Usi;

/**
 * @brief CSA通信対局で自分の手を指すエンジン
 *
 * エンジンはローカルの対局・検討と同じ枠 E1（EngineLifecycleManager）に置く。思考情報・通信ログの表示は
 * 枠のエンジンを結ぶ側（EngineDisplayPresenter）が行う。手番は持たせない（投了・宣言勝ちはここで受け取り、
 * ローカルの対局の司令塔には届かない）。
 */
class CsaEngineController : public QObject
{
    Q_OBJECT

public:
    struct InitParams {
        QString enginePath;
        QString engineName;
    };

    struct ThinkingParams {
        QString positionCmd;
        UsiTimingParams timing;  ///< 残り時間（USI の btime/wtime の意味）・決まり・遅延・余裕
    };

    struct ThinkingResult {
        UsiMove move;  ///< エンジンの指し手（成り・不成を含む）
        bool resign = false;
        bool valid = false;
        UsiScore score;  ///< 自分から見た評価値
    };

    explicit CsaEngineController(EngineLifecycleManager* engines, QObject* parent = nullptr);
    ~CsaEngineController() override;

    void initialize(const InitParams& params);
    void thinkAsync(const ThinkingParams& params);
    /// 思考中なら stop を送り、その時点の最善手ですぐ指させる
    void stopThinking();
    void sendGameOver(bool win);
    void sendQuit();
    void cleanup();

    Usi* engine() const;
    bool isInitialized() const { return !m_engine.isNull(); }

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
    void onMatchMoveReady(const UsiMatchMove& move);
    void onEngineResign();
    void onEngineWinDeclaration();

private:
    EngineLifecycleManager* m_engines = nullptr;  ///< 非所有（エンジンは枠が持つ）
    QString m_engineName;
    QPointer<Usi> m_engine;
    QString m_ponderPosition; ///< 次の手番まで保持する予測局面
};

#endif // CSAENGINECONTROLLER_H
