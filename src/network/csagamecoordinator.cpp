#include "csagamecoordinator.h"
#include "shogigamecontroller.h"
#include "shogiview.h"
#include "shogiclock.h"
#include "shogiboard.h"
#include "boardinteractioncontroller.h"
#include "usi.h"
#include "usicommlogmodel.h"
#include "shogienginethinkingmodel.h"
#include "enginesettingsconstants.h"

#include <QDebug>
#include <QSettings>
#include <QRegularExpression>

using namespace EngineSettingsConstants;

// コンストラクタ
CsaGameCoordinator::CsaGameCoordinator(QObject* parent)
    : QObject(parent)
    , m_client(new CsaClient(this))
    , m_engine(nullptr)
    , m_engineCommLog(nullptr)
    , m_engineThinking(nullptr)
    , m_ownsCommLog(false)
    , m_ownsThinking(false)
    , m_gameState(GameState::Idle)
    , m_playerType(PlayerType::Human)
    , m_isBlackSide(true)
    , m_isMyTurn(false)
    , m_moveCount(0)
    , m_blackTotalTimeMs(0)
    , m_whiteTotalTimeMs(0)
    , m_prevToFile(0)
    , m_prevToRank(0)
    , m_initialTimeMs(0)
    , m_blackRemainingMs(0)
    , m_whiteRemainingMs(0)
    , m_resignConsumedTimeMs(0)
{
    // CSAクライアントのシグナル接続
    connect(m_client, &CsaClient::connectionStateChanged,
            this, &CsaGameCoordinator::onConnectionStateChanged);
    connect(m_client, &CsaClient::errorOccurred,
            this, &CsaGameCoordinator::onClientError);
    connect(m_client, &CsaClient::loginSucceeded,
            this, &CsaGameCoordinator::onLoginSucceeded);
    connect(m_client, &CsaClient::loginFailed,
            this, &CsaGameCoordinator::onLoginFailed);
    connect(m_client, &CsaClient::logoutCompleted,
            this, &CsaGameCoordinator::onLogoutCompleted);
    connect(m_client, &CsaClient::gameSummaryReceived,
            this, &CsaGameCoordinator::onGameSummaryReceived);
    connect(m_client, &CsaClient::gameStarted,
            this, &CsaGameCoordinator::onGameStarted);
    connect(m_client, &CsaClient::gameRejected,
            this, &CsaGameCoordinator::onGameRejected);
    connect(m_client, &CsaClient::moveReceived,
            this, &CsaGameCoordinator::onMoveReceived);
    connect(m_client, &CsaClient::moveConfirmed,
            this, &CsaGameCoordinator::onMoveConfirmed);
    connect(m_client, &CsaClient::gameEnded,
            this, &CsaGameCoordinator::onClientGameEnded);
    connect(m_client, &CsaClient::gameInterrupted,
            this, &CsaGameCoordinator::onGameInterrupted);
    connect(m_client, &CsaClient::rawMessageReceived,
            this, &CsaGameCoordinator::onRawMessageReceived);
    connect(m_client, &CsaClient::rawMessageSent,
            this, &CsaGameCoordinator::onRawMessageSent);
}

// デストラクタ
CsaGameCoordinator::~CsaGameCoordinator()
{
    cleanup();
}

// 依存オブジェクト設定
void CsaGameCoordinator::setDependencies(const Dependencies& deps)
{
    m_gameController = deps.gameController;
    m_view = deps.view;
    m_clock = deps.clock;
    m_boardController = deps.boardController;
    m_recordModel = deps.recordModel;
    m_sfenRecord = deps.sfenRecord;
    m_gameMoves = deps.gameMoves;
    
    // 外部から渡されたUSI通信ログモデルを使用
    if (deps.usiCommLog) {
        m_engineCommLog = deps.usiCommLog;
        m_ownsCommLog = false;  // 外部所有
    }
    
    // 外部から渡されたエンジン思考モデルを使用
    if (deps.engineThinking) {
        m_engineThinking = deps.engineThinking;
        m_ownsThinking = false;  // 外部所有
    }
}

// 対局開始
void CsaGameCoordinator::startGame(const StartOptions& options)
{
    if (m_gameState != GameState::Idle && m_gameState != GameState::Error) {
        emit errorOccurred(tr("対局中は新しい対局を開始できません"));
        return;
    }

    m_options = options;
    m_playerType = options.playerType;
    m_moveCount = 0;
    m_blackTotalTimeMs = 0;
    m_whiteTotalTimeMs = 0;
    m_prevToFile = 0;
    m_prevToRank = 0;
    m_resignConsumedTimeMs = 0;  // 投了時消費時間をリセット
    m_usiMoves.clear();
    if (m_sfenRecord) m_sfenRecord->clear();
    if (m_gameMoves) m_gameMoves->clear();

    // CSAバージョン設定
    m_client->setCsaVersion(options.csaVersion);

    // サーバーに接続
    setGameState(GameState::Connecting);
    emit logMessage(tr("サーバー %1:%2 に接続中...").arg(options.host).arg(options.port));
    m_client->connectToServer(options.host, options.port);
}

// 対局中断
void CsaGameCoordinator::stopGame()
{
    if (m_gameState == GameState::InGame) {
        // 投了送信前に時計を停止して経過時間を取得
        if (m_clock) {
            m_clock->stopClock();
            // 自分の手番の経過時間を保存
            m_resignConsumedTimeMs = m_isBlackSide
                ? m_clock->getPlayer1ConsiderationTimeMs()
                : m_clock->getPlayer2ConsiderationTimeMs();
            qDebug() << "[CSA] StopGame consumed time from clock:" << m_resignConsumedTimeMs << "ms";
        }
        m_client->resign();
    }

    cleanup();
    setGameState(GameState::Idle);
}

// 自分の手番かどうか
bool CsaGameCoordinator::isMyTurn() const
{
    return m_isMyTurn;
}

// 先手側かどうか
bool CsaGameCoordinator::isBlackSide() const
{
    return m_isBlackSide;
}

// 人間の指し手処理
void CsaGameCoordinator::onHumanMove(const QPoint& from, const QPoint& to, bool promote)
{
    qDebug() << "[CSA-DEBUG] onHumanMove called: from=" << from << "to=" << to << "promote=" << promote;
    qDebug() << "[CSA-DEBUG] gameState=" << static_cast<int>(m_gameState) 
             << "isMyTurn=" << m_isMyTurn 
             << "playerType=" << static_cast<int>(m_playerType)
             << "isBlackSide=" << m_isBlackSide;

    if (m_gameState != GameState::InGame) {
        qDebug() << "[CSA-DEBUG] Not in game state, returning";
        return;
    }

    if (!m_isMyTurn) {
        qDebug() << "[CSA-DEBUG] Not my turn, returning";
        emit errorOccurred(tr("相手の手番です"));
        return;
    }

    if (m_playerType != PlayerType::Human) {
        qDebug() << "[CSA-DEBUG] Not human player, returning";
        return;
    }

    QString csaMove = boardToCSA(from, to, promote);
    qDebug() << "[CSA-DEBUG] Generated CSA move:" << csaMove;
    
    m_client->sendMove(csaMove);
    qDebug() << "[CSA-DEBUG] Move sent to server";
}

// 投了処理
void CsaGameCoordinator::onResign()
{
    if (m_gameState == GameState::InGame) {
        // 投了送信前に時計を停止して経過時間を取得
        if (m_clock) {
            m_clock->stopClock();
            // 自分の手番の経過時間を保存
            m_resignConsumedTimeMs = m_isBlackSide
                ? m_clock->getPlayer1ConsiderationTimeMs()
                : m_clock->getPlayer2ConsiderationTimeMs();
            qDebug() << "[CSA] Resign consumed time from clock:" << m_resignConsumedTimeMs << "ms";
        }
        m_client->resign();
    }
}

// 接続状態変化ハンドラ
void CsaGameCoordinator::onConnectionStateChanged(CsaClient::ConnectionState state)
{
    switch (state) {
    case CsaClient::ConnectionState::Connected:
        emit logMessage(tr("接続完了。ログイン中..."));
        setGameState(GameState::LoggingIn);
        m_client->login(m_options.username, m_options.password);
        break;

    case CsaClient::ConnectionState::Disconnected:
        if (m_gameState != GameState::Idle && m_gameState != GameState::GameOver) {
            emit logMessage(tr("サーバーから切断されました"), true);
            setGameState(GameState::Error);
        }
        break;

    default:
        break;
    }
}

// エラーハンドラ
void CsaGameCoordinator::onClientError(const QString& message)
{
    emit errorOccurred(message);
    emit logMessage(tr("エラー: %1").arg(message), true);
}

// ログイン成功ハンドラ
void CsaGameCoordinator::onLoginSucceeded()
{
    emit logMessage(tr("ログイン成功。対局待ち中..."));
    setGameState(GameState::WaitingForMatch);

    if (m_playerType == PlayerType::Engine) {
        initializeEngine();
    }
}

// ログイン失敗ハンドラ
void CsaGameCoordinator::onLoginFailed(const QString& reason)
{
    emit logMessage(tr("ログイン失敗: %1").arg(reason), true);
    emit errorOccurred(reason);
    setGameState(GameState::Error);
    m_client->disconnectFromServer();
}

// ログアウト完了ハンドラ
void CsaGameCoordinator::onLogoutCompleted()
{
    emit logMessage(tr("ログアウト完了"));
    setGameState(GameState::Idle);
}

// 対局情報受信ハンドラ
void CsaGameCoordinator::onGameSummaryReceived(const CsaClient::GameSummary& summary)
{
    m_gameSummary = summary;
    m_isBlackSide = summary.isBlackTurn();

    emit logMessage(tr("対局条件を受信しました"));
    emit logMessage(tr("先手: %1, 後手: %2").arg(summary.blackName, summary.whiteName));
    emit logMessage(tr("持時間: %1秒, 秒読み: %2秒")
                        .arg(summary.totalTime)
                        .arg(summary.byoyomi));

    emit gameSummaryReceived(summary);
    setGameState(GameState::WaitingForAgree);

    emit logMessage(tr("対局条件に同意します..."));
    m_client->agree(summary.gameId);
}

// 対局開始ハンドラ
void CsaGameCoordinator::onGameStarted(const QString& gameId)
{
    Q_UNUSED(gameId)

    emit logMessage(tr("対局開始！"));
    setGameState(GameState::InGame);

    setupInitialPosition();
    
    // ★ setupClock()の前にm_isMyTurnを設定
    m_isMyTurn = (m_gameSummary.myTurn == m_gameSummary.toMove);
    
    setupClock();

    emit gameStarted(m_gameSummary.blackName, m_gameSummary.whiteName);
    emit turnChanged(m_isMyTurn);

    if (m_playerType == PlayerType::Engine && m_isMyTurn) {
        startEngineThinking();
    }
}

// 対局拒否ハンドラ
void CsaGameCoordinator::onGameRejected(const QString& gameId, const QString& rejector)
{
    emit logMessage(tr("対局が拒否されました (ID: %1, 拒否者: %2)").arg(gameId, rejector), true);
    setGameState(GameState::WaitingForMatch);
}

// 相手の指し手受信ハンドラ
void CsaGameCoordinator::onMoveReceived(const QString& move, int consumedTimeMs)
{
    emit logMessage(tr("相手の指し手: %1 (消費時間: %2ms)").arg(move).arg(consumedTimeMs));

    // CSA形式から手番を判定（+なら先手、-なら後手）
    bool isBlackMove = (move.length() > 0 && move[0] == QLatin1Char('+'));

    // 累計消費時間を更新
    if (isBlackMove) {
        m_blackTotalTimeMs += consumedTimeMs;
        m_blackRemainingMs -= consumedTimeMs;
        if (m_blackRemainingMs < 0) m_blackRemainingMs = 0;
    } else {
        m_whiteTotalTimeMs += consumedTimeMs;
        m_whiteRemainingMs -= consumedTimeMs;
        if (m_whiteRemainingMs < 0) m_whiteRemainingMs = 0;
    }
    
    // ★ 相手の指し手を受信したので、時計の残り時間を更新し、自分の手番として時計を開始
    if (m_clock) {
        // 残り時間を更新（ミリ秒→秒）
        int blackRemainSec = m_blackRemainingMs / 1000;
        int whiteRemainSec = m_whiteRemainingMs / 1000;
        int byoyomiSec = m_gameSummary.byoyomi * m_gameSummary.timeUnitMs() / 1000;
        int incrementSec = m_gameSummary.increment * m_gameSummary.timeUnitMs() / 1000;
        
        m_clock->setPlayerTimes(blackRemainSec, whiteRemainSec,
                                byoyomiSec, byoyomiSec,
                                incrementSec, incrementSec,
                                true);
        
        // 次は自分の手番なので、自分側の時計を開始
        m_clock->setCurrentPlayer(m_isBlackSide ? 1 : 2);
        m_clock->startClock();
    }

    // CSA形式から座標を抽出（ハイライト用）
    QPoint from, to;
    int fromFile = 0, fromRank = 0;
    int toFile = 0, toRank = 0;
    if (move.length() >= 5) {
        fromFile = move[1].digitValue();
        fromRank = move[2].digitValue();
        toFile = move[3].digitValue();
        toRank = move[4].digitValue();

        if (fromFile == 0 && fromRank == 0) {
            // 駒打ちの場合、移動元は無効
            from = QPoint(-1, -1);
        } else {
            from = QPoint(fromFile, fromRank);
        }
        to = QPoint(toFile, toRank);
    }

    // 成り判定（盤面更新前に行う）
    bool isPromotion = false;
    if (move.length() >= 7 && fromFile != 0 && fromRank != 0) {
        QString destPiece = move.mid(5, 2);
        // 成り駒で指し手が来た場合、移動元の駒を確認
        static const QStringList promotedPieces = {
            QStringLiteral("TO"), QStringLiteral("NY"), QStringLiteral("NK"),
            QStringLiteral("NG"), QStringLiteral("UM"), QStringLiteral("RY")
        };
        if (promotedPieces.contains(destPiece)) {
            // 盤面から移動元の駒を取得して、未成駒かどうか確認
            if (m_gameController && m_gameController->board()) {
                QChar srcPieceChar = m_gameController->board()->getPieceCharacter(fromFile, fromRank);
                // 未成駒の文字（大文字小文字両方）
                static const QString unpromoted = QStringLiteral("PLNSBRplnsbr");
                if (unpromoted.contains(srcPieceChar)) {
                    isPromotion = true;
                }
            }
        }
    }

    if (!applyMoveToBoard(move)) {
        emit logMessage(tr("指し手の適用に失敗しました: %1").arg(move), true);
        return;
    }

    m_moveCount++;
    m_isMyTurn = true;
    emit turnChanged(true);

    // ハイライト更新を要求
    emit moveHighlightRequested(from, to);

    QString usiMove = csaToUsi(move);
    QString prettyMove = csaToPretty(move, isPromotion);

    // 前の移動先を更新（「同」判定用）
    m_prevToFile = toFile;
    m_prevToRank = toRank;

    emit moveMade(move, usiMove, prettyMove, consumedTimeMs);

    if (m_playerType == PlayerType::Engine) {
        startEngineThinking();
    }
}

// 自分の指し手確認ハンドラ
void CsaGameCoordinator::onMoveConfirmed(const QString& move, int consumedTimeMs)
{
    emit logMessage(tr("指し手確認: %1 (消費時間: %2ms)").arg(move).arg(consumedTimeMs));

    // CSA形式から手番を判定（+なら先手、-なら後手）
    bool isBlackMove = (move.length() > 0 && move[0] == QLatin1Char('+'));

    // 累計消費時間を更新
    if (isBlackMove) {
        m_blackTotalTimeMs += consumedTimeMs;
        m_blackRemainingMs -= consumedTimeMs;
        if (m_blackRemainingMs < 0) m_blackRemainingMs = 0;
    } else {
        m_whiteTotalTimeMs += consumedTimeMs;
        m_whiteRemainingMs -= consumedTimeMs;
        if (m_whiteRemainingMs < 0) m_whiteRemainingMs = 0;
    }
    
    // ★ 自分の指し手が確認されたので、残り時間を更新し、相手側の時計を開始
    if (m_clock) {
        // 残り時間を更新（ミリ秒→秒）
        int blackRemainSec = m_blackRemainingMs / 1000;
        int whiteRemainSec = m_whiteRemainingMs / 1000;
        int byoyomiSec = m_gameSummary.byoyomi * m_gameSummary.timeUnitMs() / 1000;
        int incrementSec = m_gameSummary.increment * m_gameSummary.timeUnitMs() / 1000;
        
        m_clock->setPlayerTimes(blackRemainSec, whiteRemainSec,
                                byoyomiSec, byoyomiSec,
                                incrementSec, incrementSec,
                                true);
        
        // 次は相手の手番なので、相手側の時計を開始
        // （相手の時間もGUI上でカウントダウン表示する）
        m_clock->setCurrentPlayer(m_isBlackSide ? 2 : 1);
        m_clock->startClock();
    }

    // 自分の指し手は既にGUI側またはstartEngineThinkingで盤面が更新されているので、
    // applyMoveToBoardは呼ばない。
    // USI指し手リストとSFEN記録は更新する必要がある。
    QString usiMove = csaToUsi(move);
    if (!usiMove.isEmpty()) {
        m_usiMoves.append(usiMove);
    }

    // SFEN記録を更新
    // 注意: ShogiBoard::currentPlayer()はsetSfen()でのみ更新されるため、
    //       ShogiGameController::currentPlayer()から手番を取得する
    if (m_gameController && m_gameController->board() && m_sfenRecord) {
        QString boardSfen = m_gameController->board()->convertBoardToSfen();
        QString standSfen = m_gameController->board()->convertStandToSfen();
        // ShogiGameController::currentPlayer()から手番を取得（Player1=先手="b", Player2=後手="w"）
        QString currentPlayerStr = (m_gameController->currentPlayer() == ShogiGameController::Player1)
                                   ? QStringLiteral("b") : QStringLiteral("w");
        QString fullSfen = QString("%1 %2 %3 %4")
                               .arg(boardSfen, currentPlayerStr, standSfen)
                               .arg(m_moveCount + 1);
        m_sfenRecord->append(fullSfen);
    }

    // CSA形式から座標を抽出（ハイライト用）
    QPoint from, to;
    int toFile = 0, toRank = 0;
    if (move.length() >= 5) {
        int fromFile = move[1].digitValue();
        int fromRank = move[2].digitValue();
        toFile = move[3].digitValue();
        toRank = move[4].digitValue();

        if (fromFile == 0 && fromRank == 0) {
            // 駒打ちの場合、移動元は無効
            from = QPoint(-1, -1);
        } else {
            from = QPoint(fromFile, fromRank);
        }
        to = QPoint(toFile, toRank);
    }

    m_moveCount++;
    m_isMyTurn = false;
    emit turnChanged(false);

    // ハイライト更新を要求（自分の指し手でも確認後に再度更新）
    emit moveHighlightRequested(from, to);

    // 自分の指し手の成りフラグはGameControllerから取得
    bool isPromotion = m_gameController ? m_gameController->promote() : false;
    QString prettyMove = csaToPretty(move, isPromotion);

    // 前の移動先を更新（「同」判定用）
    m_prevToFile = toFile;
    m_prevToRank = toRank;

    emit moveMade(move, usiMove, prettyMove, consumedTimeMs);
}

// 対局終了ハンドラ
void CsaGameCoordinator::onClientGameEnded(CsaClient::GameResult result, CsaClient::GameEndCause cause, int consumedTimeMs)
{
    // サーバーからの消費時間が0で、自分が投了した場合は、保存した経過時間を使う
    int actualConsumedTimeMs = consumedTimeMs;
    if (consumedTimeMs == 0 && cause == CsaClient::GameEndCause::Resign && 
        result == CsaClient::GameResult::Lose && m_resignConsumedTimeMs > 0) {
        actualConsumedTimeMs = m_resignConsumedTimeMs;
        qDebug() << "[CSA] Using saved resign consumed time:" << actualConsumedTimeMs << "ms";
    }

    QString resultStr;
    switch (result) {
    case CsaClient::GameResult::Win:
        resultStr = tr("勝ち");
        break;
    case CsaClient::GameResult::Lose:
        resultStr = tr("負け");
        break;
    case CsaClient::GameResult::Draw:
        resultStr = tr("引き分け");
        break;
    case CsaClient::GameResult::Censored:
        resultStr = tr("打ち切り");
        break;
    case CsaClient::GameResult::Chudan:
        resultStr = tr("中断");
        break;
    default:
        resultStr = tr("不明");
        break;
    }

    QString causeStr;
    switch (cause) {
    case CsaClient::GameEndCause::Resign:
        causeStr = tr("投了");
        break;
    case CsaClient::GameEndCause::TimeUp:
        causeStr = tr("時間切れ");
        break;
    case CsaClient::GameEndCause::IllegalMove:
        causeStr = tr("反則");
        break;
    case CsaClient::GameEndCause::Sennichite:
        causeStr = tr("千日手");
        break;
    case CsaClient::GameEndCause::OuteSennichite:
        causeStr = tr("連続王手の千日手");
        break;
    case CsaClient::GameEndCause::Jishogi:
        causeStr = tr("入玉宣言");
        break;
    case CsaClient::GameEndCause::MaxMoves:
        causeStr = tr("手数制限");
        break;
    case CsaClient::GameEndCause::Chudan:
        causeStr = tr("中断");
        break;
    case CsaClient::GameEndCause::IllegalAction:
        causeStr = tr("不正行為");
        break;
    default:
        causeStr = tr("不明");
        break;
    }

    emit logMessage(tr("対局終了: %1 (%2)").arg(resultStr, causeStr));
    setGameState(GameState::GameOver);
    emit gameEnded(resultStr, causeStr, actualConsumedTimeMs);

    if (m_clock) {
        m_clock->stopClock();
    }
    
    // エンジンにgameoverとquitコマンドを送信
    if (m_engine) {
        switch (result) {
        case CsaClient::GameResult::Win:
            m_engine->sendGameOverWinAndQuitCommands();
            break;
        case CsaClient::GameResult::Lose:
            m_engine->sendGameOverLoseAndQuitCommands();
            break;
        case CsaClient::GameResult::Draw:
        default:
            // 引き分け・中断等の場合はquitのみ送信
            m_engine->sendQuitCommand();
            break;
        }
    }
}

// 中断ハンドラ
void CsaGameCoordinator::onGameInterrupted()
{
    emit logMessage(tr("対局が中断されました"));
    setGameState(GameState::GameOver);
}

// 生メッセージ受信ハンドラ
void CsaGameCoordinator::onRawMessageReceived(const QString& message)
{
    emit logMessage(tr("[RECV] %1").arg(message));
    // CSA通信ログに追記（受信は ▶ で表示：サーバーからGUIへ）
    emit csaCommLogAppended(QStringLiteral("▶ ") + message);
}

// 生メッセージ送信ハンドラ
void CsaGameCoordinator::onRawMessageSent(const QString& message)
{
    emit logMessage(tr("[SEND] %1").arg(message));
    // CSA通信ログに追記（送信は ◀ で表示、パスワードはマスク：GUIからサーバーへ）
    QString displayMsg = message;
    if (displayMsg.startsWith(QStringLiteral("LOGIN "))) {
        // パスワード部分をマスク（"LOGIN username password" 形式）
        qsizetype commaPos = displayMsg.indexOf(QLatin1Char(','));
        if (commaPos > 0) {
            displayMsg = displayMsg.left(commaPos + 1) + QStringLiteral("*****");
        }
    }
    emit csaCommLogAppended(QStringLiteral("◀ ") + displayMsg);
}

// エンジンのbestmoveハンドラ
// 注意: handleEngineVsHumanOrEngineMatchCommunicationがブロッキングで処理するため、
// このスロットは startEngineThinking() が完了した後に呼ばれる。
// 主に非同期で使う場合のためのスタブとして残す。
void CsaGameCoordinator::onEngineBestMoveReceived_()
{
    // handleEngineVsHumanOrEngineMatchCommunication がブロッキングで処理するため、
    // このスロットが呼ばれる時点では既に startEngineThinking() 内で処理済み。
    // ここでは追加のログのみ出力
    qDebug() << "[CSA-DEBUG] onEngineBestMoveReceived_ called (handled in startEngineThinking)";
}

// エンジン投了ハンドラ
void CsaGameCoordinator::onEngineResign()
{
    if (m_gameState == GameState::InGame) {
        emit logMessage(tr("エンジンが投了を選択しました"));
        // 投了送信前に時計を停止して経過時間を取得
        if (m_clock) {
            m_clock->stopClock();
            // 自分の手番の経過時間を保存
            m_resignConsumedTimeMs = m_isBlackSide
                ? m_clock->getPlayer1ConsiderationTimeMs()
                : m_clock->getPlayer2ConsiderationTimeMs();
            qDebug() << "[CSA] Engine resign consumed time from clock:" << m_resignConsumedTimeMs << "ms";
        }
        m_client->resign();
    }
}

// 対局状態設定
void CsaGameCoordinator::setGameState(GameState state)
{
    if (m_gameState != state) {
        m_gameState = state;
        emit gameStateChanged(state);
    }
}

// CSA→USI変換
QString CsaGameCoordinator::csaToUsi(const QString& csaMove) const
{
    if (csaMove.length() < 7) {
        return QString();
    }

    int fromFile = csaMove[1].digitValue();
    int fromRank = csaMove[2].digitValue();
    int toFile = csaMove[3].digitValue();
    int toRank = csaMove[4].digitValue();
    QString piece = csaMove.mid(5, 2);

    QString usiMove;

    if (fromFile == 0 && fromRank == 0) {
        // 駒打ち
        QString usiPiece = csaPieceToUsi(piece);
        usiMove = QString("%1*%2%3")
                      .arg(usiPiece)
                      .arg(toFile)
                      .arg(QChar('a' + toRank - 1));
    } else {
        // 通常の移動
        usiMove = QString("%1%2%3%4")
                      .arg(fromFile)
                      .arg(QChar('a' + fromRank - 1))
                      .arg(toFile)
                      .arg(QChar('a' + toRank - 1));

        // 成り判定
        static const QStringList promotedPieces = {
            QStringLiteral("TO"), QStringLiteral("NY"), QStringLiteral("NK"),
            QStringLiteral("NG"), QStringLiteral("UM"), QStringLiteral("RY")
        };
        if (promotedPieces.contains(piece)) {
            usiMove += QStringLiteral("+");
        }
    }

    return usiMove;
}

// USI→CSA変換
QString CsaGameCoordinator::usiToCsa(const QString& usiMove, bool isBlack) const
{
    if (usiMove.isEmpty()) {
        return QString();
    }

    QChar turnSign = isBlack ? QLatin1Char('+') : QLatin1Char('-');
    QString csaMove;

    if (usiMove.contains(QLatin1Char('*'))) {
        // 駒打ち: P*7f
        QChar usiPiece = usiMove[0];
        int toFile = usiMove[2].digitValue();
        QChar toRankChar = usiMove[3];
        int toRank = toRankChar.toLatin1() - 'a' + 1;

        QString csaPiece = usiPieceToCsa(QString(usiPiece), false);
        csaMove = QString("%1%2%3%4%5%6")
                      .arg(turnSign)
                      .arg(0)
                      .arg(0)
                      .arg(toFile)
                      .arg(toRank)
                      .arg(csaPiece);
    } else {
        // 通常の移動
        int fromFile = usiMove[0].digitValue();
        QChar fromRankChar = usiMove[1];
        int fromRank = fromRankChar.toLatin1() - 'a' + 1;
        int toFile = usiMove[2].digitValue();
        QChar toRankChar = usiMove[3];
        int toRank = toRankChar.toLatin1() - 'a' + 1;
        bool promote = usiMove.endsWith(QLatin1Char('+'));

        QString csaPiece;
        if (m_gameController && m_gameController->board()) {
            // ShogiBoardはgetPieceCharacter()でQCharを返す
            // 内部座標系: file=1-9, rank=1-9
            QChar pieceChar = m_gameController->board()->getPieceCharacter(fromFile, fromRank);
            csaPiece = pieceCharToCsa(pieceChar, promote);
        }

        if (csaPiece.isEmpty()) {
            csaPiece = QStringLiteral("FU");
        }

        csaMove = QString("%1%2%3%4%5%6")
                      .arg(turnSign)
                      .arg(fromFile)
                      .arg(fromRank)
                      .arg(toFile)
                      .arg(toRank)
                      .arg(csaPiece);
    }

    return csaMove;
}

// 盤面座標→CSA変換
QString CsaGameCoordinator::boardToCSA(const QPoint& from, const QPoint& to, bool promote) const
{
    qDebug() << "[CSA-DEBUG] boardToCSA: from=" << from << "to=" << to << "promote=" << promote;
    qDebug() << "[CSA-DEBUG] isBlackSide=" << m_isBlackSide;

    QChar turnSign = m_isBlackSide ? QLatin1Char('+') : QLatin1Char('-');

    // GUI座標: QPoint(file, rank) where file=1-9, rank=1-9
    // CSA形式: +FFRRFFRRPP where FF=fromFile, RR=fromRank (1-9)
    // つまりGUI座標はそのままCSA座標として使える
    int fromFile = from.x();
    int fromRank = from.y();
    int toFile = to.x();
    int toRank = to.y();

    qDebug() << "[CSA-DEBUG] Calculated: fromFile=" << fromFile << "fromRank=" << fromRank
             << "toFile=" << toFile << "toRank=" << toRank;

    bool isDrop = (from.x() >= 10);  // 駒台からの打ち駒 (file=10 or 11)
    if (isDrop) {
        fromFile = 0;
        fromRank = 0;
        qDebug() << "[CSA-DEBUG] This is a drop move";
    }

    QString csaPiece;
    if (m_gameController && m_gameController->board()) {
        QChar pieceChar;
        if (isDrop) {
            // 駒台からの打ち駒の場合: from.x()=10(先手駒台) or 11(後手駒台)
            // from.y() は駒種インデックス (1=P, 2=L, 3=N, 4=S, 5=G, 6=B, 7=R, 8=K)
            pieceChar = m_gameController->board()->getPieceCharacter(from.x(), from.y());
            qDebug() << "[CSA-DEBUG] Got pieceChar from stand:" << pieceChar
                     << "at file=" << from.x() << "rank=" << from.y();
        } else {
            // 通常の移動: validateAndMoveで既に盤面が更新されているので、
            // 移動先(to)から駒を取得する
            pieceChar = m_gameController->board()->getPieceCharacter(toFile, toRank);
            qDebug() << "[CSA-DEBUG] Got pieceChar from board (at destination):" << pieceChar
                     << "at file=" << toFile << "rank=" << toRank;
        }
        csaPiece = pieceCharToCsa(pieceChar, promote);
        qDebug() << "[CSA-DEBUG] Converted to CSA piece:" << csaPiece;
    } else {
        qDebug() << "[CSA-DEBUG] WARNING: gameController or board is null!";
    }
    if (csaPiece.isEmpty()) {
        csaPiece = QStringLiteral("FU");
        qDebug() << "[CSA-DEBUG] csaPiece was empty, defaulting to FU";
    }

    QString result = QString("%1%2%3%4%5%6")
        .arg(turnSign)
        .arg(fromFile)
        .arg(fromRank)
        .arg(toFile)
        .arg(toRank)
        .arg(csaPiece);

    qDebug() << "[CSA-DEBUG] Final CSA move string:" << result;
    return result;
}

// 駒文字→CSA駒名変換
QString CsaGameCoordinator::pieceCharToCsa(QChar pieceChar, bool promote) const
{
    // ShogiBoardの駒表現:
    // 先手（大文字）: P=歩, L=香, N=桂, S=銀, G=金, B=角, R=飛, K=玉
    //               Q=と金, M=成香, O=成桂, T=成銀, C=馬, U=龍
    // 後手（小文字）: p=歩, l=香, n=桂, s=銀, g=金, b=角, r=飛, k=玉
    //               q=と金, m=成香, o=成桂, t=成銀, c=馬, u=龍

    char c = pieceChar.toLatin1();

    // 成り駒の判定（大文字・小文字両方に対応）
    switch (c) {
    // 先手の成り駒
    case 'Q': return QStringLiteral("TO");  // と金
    case 'M': return QStringLiteral("NY");  // 成香
    case 'O': return QStringLiteral("NK");  // 成桂
    case 'T': return QStringLiteral("NG");  // 成銀
    case 'C': return QStringLiteral("UM");  // 馬
    case 'U': return QStringLiteral("RY");  // 龍

    // 後手の成り駒
    case 'q': return QStringLiteral("TO");  // と金
    case 'm': return QStringLiteral("NY");  // 成香
    case 'o': return QStringLiteral("NK");  // 成桂
    case 't': return QStringLiteral("NG");  // 成銀
    case 'c': return QStringLiteral("UM");  // 馬
    case 'u': return QStringLiteral("RY");  // 龍

    // 先手の未成駒
    case 'P': return promote ? QStringLiteral("TO") : QStringLiteral("FU");
    case 'L': return promote ? QStringLiteral("NY") : QStringLiteral("KY");
    case 'N': return promote ? QStringLiteral("NK") : QStringLiteral("KE");
    case 'S': return promote ? QStringLiteral("NG") : QStringLiteral("GI");
    case 'G': return QStringLiteral("KI");
    case 'B': return promote ? QStringLiteral("UM") : QStringLiteral("KA");
    case 'R': return promote ? QStringLiteral("RY") : QStringLiteral("HI");
    case 'K': return QStringLiteral("OU");

    // 後手の未成駒
    case 'p': return promote ? QStringLiteral("TO") : QStringLiteral("FU");
    case 'l': return promote ? QStringLiteral("NY") : QStringLiteral("KY");
    case 'n': return promote ? QStringLiteral("NK") : QStringLiteral("KE");
    case 's': return promote ? QStringLiteral("NG") : QStringLiteral("GI");
    case 'g': return QStringLiteral("KI");
    case 'b': return promote ? QStringLiteral("UM") : QStringLiteral("KA");
    case 'r': return promote ? QStringLiteral("RY") : QStringLiteral("HI");
    case 'k': return QStringLiteral("OU");

    default:
        qWarning() << "[CSA-DEBUG] Unknown piece character:" << pieceChar;
        return QStringLiteral("FU");
    }
}

// CSA→表示用変換
QString CsaGameCoordinator::csaToPretty(const QString& csaMove, bool isPromotion) const
{
    if (csaMove.length() < 7) {
        return csaMove;
    }

    QChar turnSign = csaMove[0];
    QString turnMark = (turnSign == QLatin1Char('+')) ? QStringLiteral("▲") : QStringLiteral("△");

    int fromFile = csaMove[1].digitValue();
    int fromRank = csaMove[2].digitValue();
    int toFile = csaMove[3].digitValue();
    int toRank = csaMove[4].digitValue();
    QString piece = csaMove.mid(5, 2);

    static const QString zenFile[] = {
        QString(), QStringLiteral("１"), QStringLiteral("２"), QStringLiteral("３"),
        QStringLiteral("４"), QStringLiteral("５"), QStringLiteral("６"),
        QStringLiteral("７"), QStringLiteral("８"), QStringLiteral("９")
    };
    static const QString kanjiRank[] = {
        QString(), QStringLiteral("一"), QStringLiteral("二"), QStringLiteral("三"),
        QStringLiteral("四"), QStringLiteral("五"), QStringLiteral("六"),
        QStringLiteral("七"), QStringLiteral("八"), QStringLiteral("九")
    };

    // 成りの場合は成る前の駒名に変換
    QString displayPiece = piece;
    if (isPromotion) {
        // 成り駒→元の駒への変換マップ
        static const QHash<QString, QString> unpromoteMap = {
            {QStringLiteral("TO"), QStringLiteral("FU")},  // と→歩
            {QStringLiteral("NY"), QStringLiteral("KY")},  // 成香→香
            {QStringLiteral("NK"), QStringLiteral("KE")},  // 成桂→桂
            {QStringLiteral("NG"), QStringLiteral("GI")},  // 成銀→銀
            {QStringLiteral("UM"), QStringLiteral("KA")},  // 馬→角
            {QStringLiteral("RY"), QStringLiteral("HI")}   // 龍→飛
        };
        displayPiece = unpromoteMap.value(piece, piece);
    }

    QString pieceKanji = csaPieceToKanji(displayPiece);

    QString moveStr = turnMark;

    // 「同」の判定（前の指し手の移動先と同じ場合）
    if (toFile == m_prevToFile && toRank == m_prevToRank && m_moveCount > 0) {
        moveStr += QStringLiteral("同　") + pieceKanji;
    } else {
        moveStr += zenFile[toFile] + kanjiRank[toRank] + pieceKanji;
    }

    // 成る手の場合は「成」を追加
    if (isPromotion) {
        moveStr += QStringLiteral("成");
    }

    // 駒打ちの場合
    if (fromFile == 0 && fromRank == 0) {
        moveStr += QStringLiteral("打");
    } else {
        // 移動元座標を追加
        moveStr += QStringLiteral("(") + QString::number(fromFile) + QString::number(fromRank) + QStringLiteral(")");
    }

    return moveStr;
}

// 初期局面セットアップ
void CsaGameCoordinator::setupInitialPosition()
{
    if (!m_gameController || !m_gameController->board()) {
        return;
    }

    // 平手初期局面のSFEN
    const QString hiratePosition = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

    if (m_gameSummary.positionLines.isEmpty()) {
        m_gameController->board()->setSfen(hiratePosition);
    } else {
        // TODO: CSA局面行からのSFEN変換
        m_gameController->board()->setSfen(hiratePosition);
    }

    // SFENを生成（盤面 + 持ち駒 + 手番）
    QString boardSfen = m_gameController->board()->convertBoardToSfen();
    QString standSfen = m_gameController->board()->convertStandToSfen();
    QString currentPlayer = m_gameController->board()->currentPlayer();
    m_startSfen = QString("%1 %2 %3 1").arg(boardSfen, currentPlayer, standSfen);

    if (m_sfenRecord) {
        m_sfenRecord->clear();
        m_sfenRecord->append(m_startSfen);
    }

    m_positionStr = QStringLiteral("position startpos");
    m_usiMoves.clear();

    for (const QString& move : std::as_const(m_gameSummary.moves)) {
        applyMoveToBoard(move);
    }

    if (m_view) {
        m_view->update();
    }
}

// 時計初期化
void CsaGameCoordinator::setupClock()
{
    if (!m_clock) {
        return;
    }

    int timeUnitMs = m_gameSummary.timeUnitMs();
    int totalTimeSec = m_gameSummary.totalTime * timeUnitMs / 1000;
    int byoyomiSec = m_gameSummary.byoyomi * timeUnitMs / 1000;
    int incrementSec = m_gameSummary.increment * timeUnitMs / 1000;

    // setPlayerTimes(p1秒, p2秒, byoyomi1秒, byoyomi2秒, binc秒, winc秒, isLimitedTime)
    m_clock->setPlayerTimes(totalTimeSec, totalTimeSec,
                            byoyomiSec, byoyomiSec,
                            incrementSec, incrementSec,
                            true);
    
    // 初期残り時間を記録（ミリ秒）
    m_initialTimeMs = totalTimeSec * 1000;
    m_blackRemainingMs = m_initialTimeMs;
    m_whiteRemainingMs = m_initialTimeMs;
    
    // CSA通信対局では toMove で最初の手番が決まる（通常は先手"+"から）
    // 最初の手番を設定して時計を開始
    // （自分/相手どちらの手番でもGUI上でカウントダウン表示する）
    bool blackToMove = (m_gameSummary.toMove == QStringLiteral("+"));
    m_clock->setCurrentPlayer(blackToMove ? 1 : 2);
    m_clock->startClock();
}

// エンジン初期化
void CsaGameCoordinator::initializeEngine()
{
    if (m_playerType != PlayerType::Engine) {
        return;
    }

    if (m_engine) {
        m_engine->sendQuitCommand();
        m_engine->deleteLater();
        m_engine = nullptr;
    }

    QSettings settings(SettingsFileName, QSettings::IniFormat);
    settings.beginReadArray("Engines");
    settings.setArrayIndex(m_options.engineNumber);
    QString enginePath = settings.value("path").toString();
    settings.endArray();

    if (enginePath.isEmpty()) {
        enginePath = m_options.enginePath;
    }

    if (enginePath.isEmpty()) {
        emit logMessage(tr("エンジンパスが指定されていません"), true);
        return;
    }

    // USI通信ログモデル：外部から渡されていなければ内部で作成
    if (!m_engineCommLog) {
        m_engineCommLog = new UsiCommLogModel(this);
        m_ownsCommLog = true;
    }
    
    // エンジン思考モデル：外部から渡されていなければ内部で作成
    if (!m_engineThinking) {
        m_engineThinking = new ShogiEngineThinkingModel(this);
        m_ownsThinking = true;
    }

    // PlayModeを取得するためにstaticなダミーを使用
    static PlayMode dummyPlayMode = CsaNetworkMode;

    m_engine = new Usi(m_engineCommLog, m_engineThinking,
                       m_gameController.data(), dummyPlayMode, this);

    connect(m_engine, &Usi::bestMoveReceived,
            this, &CsaGameCoordinator::onEngineBestMoveReceived_);
    connect(m_engine, &Usi::bestMoveResignReceived,
            this, &CsaGameCoordinator::onEngineResign);

    QString engineFile = enginePath;
    QString engineName = m_options.engineName;
    
    // ログ識別子を設定（[E1/CSA]形式で表示される）
    m_engine->setLogIdentity(QStringLiteral("[E1]"), QStringLiteral("CSA"), engineName);
    
    m_engine->startAndInitializeEngine(engineFile, engineName);

    emit logMessage(tr("エンジン %1 を起動しました").arg(m_options.engineName));
}

// エンジン思考開始
void CsaGameCoordinator::startEngineThinking()
{
    if (!m_engine || m_playerType != PlayerType::Engine) {
        return;
    }

    QString positionCmd = m_positionStr;
    if (!m_usiMoves.isEmpty()) {
        positionCmd += QStringLiteral(" moves ") + m_usiMoves.join(QLatin1Char(' '));
    }

    int byoyomiMs = m_gameSummary.byoyomi * m_gameSummary.timeUnitMs();
    int totalTimeMs = m_gameSummary.totalTime * m_gameSummary.timeUnitMs();

    // 残り時間を計算
    int blackRemainMs = totalTimeMs - m_blackTotalTimeMs;
    int whiteRemainMs = totalTimeMs - m_whiteTotalTimeMs;
    if (blackRemainMs < 0) blackRemainMs = 0;
    if (whiteRemainMs < 0) whiteRemainMs = 0;

    QString btimeStr = QString::number(blackRemainMs);
    QString wtimeStr = QString::number(whiteRemainMs);

    // 増加時間
    int incMs = m_gameSummary.increment * m_gameSummary.timeUnitMs();

    // ponderの文字列（現在はダミー）
    QString ponderStr;

    // 思考に使う情報
    QPoint from, to;
    m_gameController->setPromote(false);

    emit logMessage(tr("エンジンが思考中..."));

    // handleEngineVsHumanOrEngineMatchCommunicationを使用してエンジンに思考させる
    m_engine->handleEngineVsHumanOrEngineMatchCommunication(
        positionCmd,        // position文字列
        ponderStr,          // ponder用文字列
        from, to,           // 出力：移動元、移動先
        byoyomiMs,          // 秒読み時間
        btimeStr,           // 先手残り時間
        wtimeStr,           // 後手残り時間
        incMs,              // 先手増加時間
        incMs,              // 後手増加時間
        (byoyomiMs > 0)     // 秒読み使用フラグ
    );

    // 投了チェック
    if (m_engine->isResignMove()) {
        emit logMessage(tr("エンジンが投了しました"));
        onEngineResign();
        return;
    }

    // 有効な指し手かチェック
    bool isValidMove = (to.x() >= 1 && to.x() <= 9 && to.y() >= 1 && to.y() <= 9);
    if (!isValidMove) {
        emit logMessage(tr("エンジンが有効な指し手を返しませんでした"), true);
        return;
    }

    // 成りフラグを取得
    bool promote = m_gameController->promote();

    emit logMessage(tr("エンジンの指し手: from=(%1,%2) to=(%3,%4) promote=%5")
                        .arg(from.x()).arg(from.y())
                        .arg(to.x()).arg(to.y())
                        .arg(promote ? "true" : "false"));

    // 盤面を更新（validateAndMoveの代わりに直接盤面操作）
    // from.x() が10以上の場合は駒台からの打ち駒
    ShogiBoard* board = m_gameController->board();
    if (!board) {
        emit logMessage(tr("盤面が取得できませんでした"), true);
        return;
    }

    // 盤面更新前に駒情報を取得してCSA形式の指し手を生成
    int fromFile = from.x();
    int fromRank = from.y();
    int toFile = to.x();
    int toRank = to.y();
    
    QChar turnSign = m_isBlackSide ? QLatin1Char('+') : QLatin1Char('-');
    QString csaPiece;
    
    bool isDrop = (fromFile >= 10);
    if (isDrop) {
        // 駒打ちの場合: fromFile=10(先手駒台) or 11(後手駒台)
        // fromRank は駒種インデックス
        QChar pieceChar = board->getPieceCharacter(fromFile, fromRank);
        csaPiece = pieceCharToCsa(pieceChar, false);
        // CSA駒打ちは fromFile=0, fromRank=0
        fromFile = 0;
        fromRank = 0;
    } else {
        // 通常移動: 盤面更新前なので移動元から駒を取得
        QChar pieceChar = board->getPieceCharacter(fromFile, fromRank);
        csaPiece = pieceCharToCsa(pieceChar, promote);
    }
    
    QString csaMove = QString("%1%2%3%4%5%6")
        .arg(turnSign)
        .arg(fromFile)
        .arg(fromRank)
        .arg(toFile)
        .arg(toRank)
        .arg(csaPiece);
    
    emit logMessage(tr("CSA形式の指し手: %1").arg(csaMove));

    // 盤面を更新（movePieceToSquareを使用）
    if (!isDrop) {
        // 盤上の駒を移動
        QChar pieceChar = board->getPieceCharacter(from.x(), from.y());
        board->movePieceToSquare(pieceChar, from.x(), from.y(), toFile, toRank, promote);
    } else {
        // 駒打ち
        QChar pieceChar = board->getPieceCharacter(from.x(), from.y());
        board->movePieceToSquare(pieceChar, 0, 0, toFile, toRank, false);
    }

    // 手番を変更
    m_gameController->changeCurrentPlayer();

    // ビューを更新
    if (m_view) {
        m_view->update();
    }

    // サーバーに送信
    m_client->sendMove(csaMove);
}

// 指し手を盤面に適用
bool CsaGameCoordinator::applyMoveToBoard(const QString& csaMove)
{
    if (!m_gameController || !m_gameController->board()) {
        return false;
    }

    // CSA形式: +7776FU（移動）または +0077FU（打ち）
    if (csaMove.length() < 7) {
        return false;
    }

    QChar turnSign = csaMove[0];
    int fromFile = csaMove[1].digitValue();
    int fromRank = csaMove[2].digitValue();
    int toFile = csaMove[3].digitValue();
    int toRank = csaMove[4].digitValue();
    QString piece = csaMove.mid(5, 2);

    // 成り駒かどうか判定
    bool isPromoted = (piece == QStringLiteral("TO") || piece == QStringLiteral("NY") ||
                       piece == QStringLiteral("NK") || piece == QStringLiteral("NG") ||
                       piece == QStringLiteral("UM") || piece == QStringLiteral("RY"));

    // 盤面座標に変換（CSAは1-9、内部は0-8）
    // CSA: file=1が右端、rank=1が上端
    // 内部座標: x=0が左端（9筋）、y=0が上端（1段）
    QPoint from, to;

    if (fromFile == 0 && fromRank == 0) {
        // 駒打ち: 駒台からの移動
        // 駒台の座標は慣例的に x=10（先手）または x=11（後手）とする
        if (turnSign == QLatin1Char('+')) {
            from = QPoint(10, pieceTypeFromCsa(piece));  // 先手駒台
        } else {
            from = QPoint(11, pieceTypeFromCsa(piece));  // 後手駒台
        }
    } else {
        // 通常の移動: CSA座標→内部座標
        from = QPoint(9 - fromFile, fromRank - 1);
    }
    to = QPoint(9 - toFile, toRank - 1);

    // ShogiBoard::movePieceToSquareを使用
    QChar selectedPiece = m_gameController->board()->getPieceCharacter(
        fromFile == 0 ? 0 : fromFile, fromRank == 0 ? 0 : fromRank);

    // 盤面を直接更新
    if (fromFile != 0 || fromRank != 0) {
        // 通常移動
        m_gameController->board()->movePieceToSquare(
            selectedPiece, fromFile, fromRank, toFile, toRank, isPromoted);
    } else {
        // 駒打ち
        // 駒台から駒を減らし、盤面に配置
        QChar pieceChar = csaPieceToSfenChar(piece, turnSign == QLatin1Char('+'));
        m_gameController->board()->movePieceToSquare(
            pieceChar, 0, 0, toFile, toRank, false);
    }

    // 手番を変更
    m_gameController->changeCurrentPlayer();

    // ビューを更新
    if (m_view) {
        m_view->update();
    }

    // USI形式の指し手をリストに追加
    QString usiMove = csaToUsi(csaMove);
    if (!usiMove.isEmpty()) {
        m_usiMoves.append(usiMove);
    }

    // SFEN記録を更新
    // 注意: ShogiBoard::currentPlayer()はsetSfen()でのみ更新されるため、
    //       ShogiGameController::currentPlayer()から手番を取得する
    QString boardSfen = m_gameController->board()->convertBoardToSfen();
    QString standSfen = m_gameController->board()->convertStandToSfen();
    // ShogiGameController::currentPlayer()から手番を取得（Player1=先手="b", Player2=後手="w"）
    QString currentPlayerStr = (m_gameController->currentPlayer() == ShogiGameController::Player1)
                               ? QStringLiteral("b") : QStringLiteral("w");
    QString fullSfen = QString("%1 %2 %3 %4")
                           .arg(boardSfen, currentPlayerStr, standSfen)
                           .arg(m_moveCount + 1);
    if (m_sfenRecord) {
        m_sfenRecord->append(fullSfen);
    }

    return true;
}

// CSA駒種→駒台インデックス変換
int CsaGameCoordinator::pieceTypeFromCsa(const QString& csaPiece) const
{
    if (csaPiece == QStringLiteral("FU") || csaPiece == QStringLiteral("TO")) return 1;
    if (csaPiece == QStringLiteral("KY") || csaPiece == QStringLiteral("NY")) return 2;
    if (csaPiece == QStringLiteral("KE") || csaPiece == QStringLiteral("NK")) return 3;
    if (csaPiece == QStringLiteral("GI") || csaPiece == QStringLiteral("NG")) return 4;
    if (csaPiece == QStringLiteral("KI")) return 5;
    if (csaPiece == QStringLiteral("KA") || csaPiece == QStringLiteral("UM")) return 6;
    if (csaPiece == QStringLiteral("HI") || csaPiece == QStringLiteral("RY")) return 7;
    if (csaPiece == QStringLiteral("OU")) return 8;
    return 0;
}

// CSA駒種→SFEN駒文字変換
QChar CsaGameCoordinator::csaPieceToSfenChar(const QString& csaPiece, bool isBlack) const
{
    QChar c;
    if (csaPiece == QStringLiteral("FU")) c = QLatin1Char('P');
    else if (csaPiece == QStringLiteral("KY")) c = QLatin1Char('L');
    else if (csaPiece == QStringLiteral("KE")) c = QLatin1Char('N');
    else if (csaPiece == QStringLiteral("GI")) c = QLatin1Char('S');
    else if (csaPiece == QStringLiteral("KI")) c = QLatin1Char('G');
    else if (csaPiece == QStringLiteral("KA")) c = QLatin1Char('B');
    else if (csaPiece == QStringLiteral("HI")) c = QLatin1Char('R');
    else if (csaPiece == QStringLiteral("OU")) c = QLatin1Char('K');
    else c = QLatin1Char('P');

    return isBlack ? c.toUpper() : c.toLower();
}

// CSA駒記号→USI駒記号
QString CsaGameCoordinator::csaPieceToUsi(const QString& csaPiece) const
{
    static const QHash<QString, QString> map = {
        {QStringLiteral("FU"), QStringLiteral("P")},
        {QStringLiteral("KY"), QStringLiteral("L")},
        {QStringLiteral("KE"), QStringLiteral("N")},
        {QStringLiteral("GI"), QStringLiteral("S")},
        {QStringLiteral("KI"), QStringLiteral("G")},
        {QStringLiteral("KA"), QStringLiteral("B")},
        {QStringLiteral("HI"), QStringLiteral("R")},
        {QStringLiteral("OU"), QStringLiteral("K")},
        {QStringLiteral("TO"), QStringLiteral("+P")},
        {QStringLiteral("NY"), QStringLiteral("+L")},
        {QStringLiteral("NK"), QStringLiteral("+N")},
        {QStringLiteral("NG"), QStringLiteral("+S")},
        {QStringLiteral("UM"), QStringLiteral("+B")},
        {QStringLiteral("RY"), QStringLiteral("+R")}
    };
    return map.value(csaPiece, QStringLiteral("P"));
}

// USI駒記号→CSA駒記号
QString CsaGameCoordinator::usiPieceToCsa(const QString& usiPiece, bool promoted) const
{
    static const QHash<QString, QString> map = {
        {QStringLiteral("P"), QStringLiteral("FU")},
        {QStringLiteral("L"), QStringLiteral("KY")},
        {QStringLiteral("N"), QStringLiteral("KE")},
        {QStringLiteral("S"), QStringLiteral("GI")},
        {QStringLiteral("G"), QStringLiteral("KI")},
        {QStringLiteral("B"), QStringLiteral("KA")},
        {QStringLiteral("R"), QStringLiteral("HI")},
        {QStringLiteral("K"), QStringLiteral("OU")}
    };
    static const QHash<QString, QString> promotedMap = {
        {QStringLiteral("P"), QStringLiteral("TO")},
        {QStringLiteral("L"), QStringLiteral("NY")},
        {QStringLiteral("N"), QStringLiteral("NK")},
        {QStringLiteral("S"), QStringLiteral("NG")},
        {QStringLiteral("B"), QStringLiteral("UM")},
        {QStringLiteral("R"), QStringLiteral("RY")}
    };

    if (promoted) {
        return promotedMap.value(usiPiece, QStringLiteral("TO"));
    }
    return map.value(usiPiece, QStringLiteral("FU"));
}

// CSA駒記号→漢字
QString CsaGameCoordinator::csaPieceToKanji(const QString& csaPiece) const
{
    static const QHash<QString, QString> map = {
        {QStringLiteral("FU"), QStringLiteral("歩")},
        {QStringLiteral("KY"), QStringLiteral("香")},
        {QStringLiteral("KE"), QStringLiteral("桂")},
        {QStringLiteral("GI"), QStringLiteral("銀")},
        {QStringLiteral("KI"), QStringLiteral("金")},
        {QStringLiteral("KA"), QStringLiteral("角")},
        {QStringLiteral("HI"), QStringLiteral("飛")},
        {QStringLiteral("OU"), QStringLiteral("玉")},
        {QStringLiteral("TO"), QStringLiteral("と")},
        {QStringLiteral("NY"), QStringLiteral("成香")},
        {QStringLiteral("NK"), QStringLiteral("成桂")},
        {QStringLiteral("NG"), QStringLiteral("成銀")},
        {QStringLiteral("UM"), QStringLiteral("馬")},
        {QStringLiteral("RY"), QStringLiteral("龍")}
    };
    return map.value(csaPiece, csaPiece);
}

// クリーンアップ
void CsaGameCoordinator::cleanup()
{
    if (m_engine) {
        m_engine->sendQuitCommand();
        m_engine->deleteLater();
        m_engine = nullptr;
    }

    // 内部で作成したモデルのみ削除
    if (m_engineCommLog && m_ownsCommLog) {
        m_engineCommLog->deleteLater();
        m_engineCommLog = nullptr;
    }

    if (m_engineThinking && m_ownsThinking) {
        m_engineThinking->deleteLater();
        m_engineThinking = nullptr;
    }

    m_client->disconnectFromServer();
}

// GUI側からCSAサーバーへコマンドを送信
void CsaGameCoordinator::sendRawCommand(const QString& command)
{
    if (!m_client) {
        qWarning() << "[CSA] sendRawCommand: client is null";
        return;
    }

    qInfo().noquote() << "[CSA] Sending raw command:" << command;
    m_client->sendRawCommand(command);
}
