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

class EngineLifecycleManager;
class MatchTimeSource;
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
        /// 持ち時間を読む出どころ（非所有。サーバーが示した残り時間・決まり・遅延・余裕）
        MatchTimeSource* timeSource = nullptr;
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
    /// position（対局の開始局面から今までの手順）の手番の手を考えさせる
    void thinkAsync(const UsiPosition& position);
    /// 思考中なら stop を送り、その時点の最善手ですぐ指させる
    void stopThinking();
    /// gameover を送ってエンジンを終える
    void sendGameOver(bool win);
    /// エンジンを終える（引き分け・中断の終局）
    void sendQuit();
    void cleanup();

    Usi* engine() const;
    /// エンジンを起動して初期化（usi〜readyok）している
    bool isStarting() const;
    /// readyok を受け、思考を頼める（終局の gameover・quit の後は false。次局は起動し直す）
    bool isReady() const;

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
};

#endif // CSAENGINECONTROLLER_H
