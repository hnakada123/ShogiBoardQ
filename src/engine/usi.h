#ifndef USI_H
#define USI_H

/// @file usi.h
/// @brief GUI から使う USI エンジン1台の窓口

#include <QObject>
#include <QString>

#include "shogitypes.h"
#include "usimatchhandler.h"
#include "usiprotocolhandler.h"


class EngineProcessManager;
class MatchTimeSource;
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
     *
     * 初期化の完了は engineInitialized、失敗は errorOccurred で知らせる。失敗は同期では知らせない
     * （この呼び出しから戻った後に届く）ので、呼び出し側は起動の途中の再入に備えなくてよい。
     */
    void startEngine(const QString& engineFile, const QString& engineName);
    bool isInitializing() const;
    /// プロセスが動いているか
    bool isRunning() const;
    /// プロセスを止める（quit を送り、応答がなければ強制終了する）。思考情報を消すかを選べる
    void quit(bool clearThinking = true);
    /// 探索・初期化の結果を待つのをやめる（遅れて届いた結果は知らせない）
    void cancelCurrentOperation();

    // --- 対局 ---

    /// 対局で持ち時間を読む出どころ（非所有。ローカル対局は対局時計、通信対局はサーバーの残り時間）
    void setMatchTimeSource(MatchTimeSource* source);
    /// 対局の手番の探索を頼む（先読みしていれば、その当たり・外れはこの部品が判断する）
    void requestMatchMove(const UsiPosition& position);
    /// 対局の手番で定跡手を指す。探索はさせず、delayMs 後に matchMoveReady で知らせる。
    /// label は情報欄の「探索手」に出す表示（例「▲７六歩（定跡 82%）」）
    void requestBookMove(const UsiPosition& position, const UsiMove& move, int delayMs, const QString& label);
    /// 先読みの局面を捨てる（待った・中断で局面が戻ったとき）
    void forgetPonder();
    /// 対局中の「すぐ指させる」。定跡手を待っていればすぐ指す。準備中に頼まれた探索は残し、それを送った後で stop を送る
    void requestImmediateMove();
    void sendGameOverCommand(GameOverResult result);

    // --- 検討・棋譜解析・詰み探索 ---

    /// 検討: 候補手の数を送って position + go infinite を送り、byoyomiMilliSec 後に stop を送る（0 なら送らない）
    void startConsideration(const UsiPosition& position, int byoyomiMilliSec, int multiPV);
    /// 探索1回を頼む（棋譜解析の1局面など）
    void search(const UsiSearchRequest& request);
    /// 詰み探索（go mate）
    void startMateSearch(const UsiPosition& position, int timeLimitMs);
    /// 探索を止める（初期化中なら、まだ送っていない探索を取りやめる）
    void stop();
    /// setoption name <name> value <value> を送る（初期化中なら完了後に送る）
    void sendSetOptionCommand(const QString& name, const QString& value);
    /// 利用者が USI コンソールに入力したコマンドをそのまま送る。アプリの処理は型付きの送信メソッドを使う
    void sendConsoleCommand(const QString& command);

    // --- 読み筋の表示 ---

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
