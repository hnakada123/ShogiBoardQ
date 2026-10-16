#ifndef USI_H
#define USI_H

/// @file usi.h
/// @brief GUI から使う USI エンジン1台の窓口

#include <QObject>
#include <QString>

#include "shogitypes.h"
#include "usimatchhandler.h"
#include "usiprotocolhandler.h"
#include "usitimingparams.h"

class EngineProcessManager;
class ShogiClock;
class ThinkingInfoFormatter;
class UsiEngineSession;
struct UsiSearchRequest;

/**
 * @brief GUI から使う USI エンジン1台の窓口
 *
 * 対局・検討・棋譜解析・詰み探索の要求を受け、次の部品に頼む:
 * - UsiEngineSession: プロセス・状態機械（UsiProtocolHandler）・探索1回の依頼
 * - UsiMatchHandler: 対局の position/go/bestmove（先読みを含む）
 * - ThinkingInfoFormatter: info を思考の表の行と情報欄の値にする
 *
 * 思考の表・通信ログのモデルには触れない。表示する側は thinking()（整えた行と情報欄の値）と
 * process()（送受信した行）のシグナルを受ける（UsiModelBinding）。
 * 送信は用途ごとの型付きメソッドを使う。任意の文字列を送れるのは、利用者が入力する
 * USI コンソール（sendConsoleCommand）だけにする。
 */
class Usi : public QObject
{
    Q_OBJECT

public:
    explicit Usi(QObject* parent = nullptr);
    ~Usi() override;

    // --- 識別 ---

    /// ログに出す識別（engineTag は "E1"・"E2"、sideTag は "black"・"white"・"CSA" など）
    void setLogIdentity(const QString& engineTag, const QString& sideTag, const QString& engineName);
    QString engineTag() const { return m_engineTag; }
    /// 表示するエンジン名（無ければ "Engine"）
    QString engineName() const { return m_engineName; }

    // --- 起動・終了 ---

    /**
     * @brief エンジンを起動して初期化（usi〜readyok）を始める
     * @return 起動の要求を受け付けたか。初期化の完了は engineInitialized、失敗は errorOccurred で通知する
     */
    [[nodiscard]] bool startEngine(const QString& engineFile, const QString& engineName);
    bool isInitializing() const;
    /// プロセスが動いているか
    bool isRunning() const;
    /// プロセスを止める（quit を送り、応答がなければ強制終了する）。思考情報を消すかを選べる
    void quit(bool clearThinking = true);
    /// 探索・初期化の結果を待つのをやめる（遅れて届いた結果は知らせない）
    void cancelCurrentOperation();

    // --- 対局 ---

    void setMatchClock(ShogiClock* clock);
    void requestMatchMove(const QString& position, const QString& ponder, const UsiTimingParams& timing);
    /// 対局中の「すぐ指させる」。準備中に頼まれた探索は残し、それを送った後で stop を送る
    void requestImmediateMove();
    void sendGameOverCommand(GameOverResult result);

    // --- 検討・棋譜解析・詰み探索 ---

    /// 検討: 候補手の数を送って position + go infinite を送り、byoyomiMilliSec 後に stop を送る（0 なら送らない）
    void startConsideration(const QString& positionStr, int byoyomiMilliSec, int multiPV);
    /// 探索1回を頼む（棋譜解析の1局面など）
    void search(const UsiSearchRequest& request);
    /// 詰み探索（go mate）
    void startMateSearch(const QString& positionStr, int timeLimitMs);
    /// 探索を止める（初期化中なら、まだ送っていない探索を取りやめる）
    void stop();
    /// setoption name <name> value <value> を送る（初期化中なら完了後に送る）
    void sendSetOptionCommand(const QString& name, const QString& value);
    /// 利用者が USI コンソールに入力したコマンドをそのまま送る。アプリの処理は型付きの送信メソッドを使う
    void sendConsoleCommand(const QString& command);

    // --- 読み筋の表示 ---

    /// 直前の指し手の行き先（読み筋の最初の手の「同」表記に使う。無ければ {0, 0}）
    void setPreviousMove(BoardSquare to);
    /// 読み筋を表示する局面に至った指し手（USI 形式。読み筋の盤の強調）
    void setLastUsiMove(const QString& move);
    const UsiProtocolHandler::SearchCandidate& searchCandidate() const;

    /// 整えた思考の行・情報欄の値を知らせる部品（表示する側が受ける）
    const ThinkingInfoFormatter* thinking() const { return m_formatter; }
    /// 送受信した行を知らせる部品（通信ログ）
    const EngineProcessManager* process() const;
    /// 探索の状態と結果を知らせる部品（bestmove・checkmate・info）
    const UsiProtocolHandler* protocol() const;

signals:
    void engineInitialized();
    void errorOccurred(const QString& message);
    void searchCandidateChanged();
    /// 探索（詰み探索など）が打ち切りの後も答えなかった
    void searchTimedOut();
    /// 対局で指した（UsiMatchHandler::moveReady）
    void matchMoveReady(const UsiMatchMove& move);
    /// 対局の手番の探索に投了で答えた
    void resigned(Turn side);
    /// 対局の手番の探索に入玉宣言勝ちで答えた
    void declaredWin(Turn side);

private slots:
    void onSessionError(const QString& message);
    void onMatchTimeout();

private:
    UsiEngineSession* m_session = nullptr;      ///< QObject parent 所有
    UsiMatchHandler* m_matchHandler = nullptr;  ///< QObject parent 所有
    ThinkingInfoFormatter* m_formatter = nullptr; ///< QObject parent 所有
    QString m_engineTag;
    QString m_engineName = QStringLiteral("Engine");
};

#endif // USI_H
