#ifndef SHOGIGAMECONTROLLER_H
#define SHOGIGAMECONTROLLER_H

/// @file shogigamecontroller.h
/// @brief 将棋の対局進行・盤面更新・合法手検証を管理するコントローラ


#include <QObject>
#include "fastmovevalidator.h"
#include "playmode.h"
#include "shogitypes.h"

class ShogiBoard;

/**
 * @brief 将棋の対局全体を管理し、盤面の初期化・指し手処理・合法手検証・対局状態管理を行う
 *
 * @todo remove コメントスタイルガイド適用済み
 */
class ShogiGameController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(Player currentPlayer READ currentPlayer NOTIFY currentPlayerChanged)

public:
    // --- 列挙型 ---

    /// 対局結果
    enum Result {
        NoResult,    ///< 未決
        Player1Wins, ///< 先手（下手）勝ち
        Draw,        ///< 引き分け
        Player2Wins  ///< 後手（上手）勝ち
    };
    Q_ENUM(Result)

    /// 対局者（手番）
    enum Player {
        NoPlayer, ///< 未設定
        Player1,  ///< 先手（下手）
        Player2   ///< 後手（上手）
    };
    Q_ENUM(Player)

    // --- 生成・取得 ---

    explicit ShogiGameController(QObject* parent = nullptr);

    ShogiBoard* board() const;
    Result result() const { return m_result; }
    Player currentPlayer() const { return m_currentPlayer; }
    void setCurrentPlayer(const Player);

    // --- 指し手処理 ---

    /**
     * @brief 指し手を検証し、合法手であれば盤面を更新する
     * @return 合法手で盤面更新できた場合 true
     */
    bool validateAndMove(QPoint& outFrom, QPoint& outTo, QString& record, PlayMode& playMode, int& moveNumber,
                         QStringList* m_sfenHistory, QVector<ShogiMove>& gameMoves);

    /// 局面編集モードで駒を移動し、盤面を更新する
    bool editPosition(const QPoint& outFrom, const QPoint& outTo);

    /// 局面編集モードで右クリックによる成/不成を切り替える
    void switchPiecePromotionStatusOnRightClick(const int fileFrom, const int rankFrom) const;

    // --- SFEN・手番 ---

    void changeCurrentPlayer();

    // --- 成り制御 ---

    void setPromote(bool newPromote);
    bool promote() const;

    /**
     * @brief 強制成りモードを設定する（定跡手からの着手時に使用）
     * @param force true=強制モード有効、false=無効（通常のダイアログ表示）
     * @param promote 強制モード時の成り/不成（true=成り、false=不成）
     */
    void setForcedPromotion(bool force, bool promote = false);

    // --- 対局結果制御 ---

    /// 時間切れによる敗北を適用する（clockPlayer: 1=先手負け, 2=後手負け）
    void applyTimeoutLossFor(int clockPlayer);

    /// 結果未確定の場合に最終決定する
    void finalizeGameResult();

    /// 対局結果をNoResultにリセットする（新規時用）
    void resetResult();

public slots:
    /**
     * @brief SFEN文字列から新規対局を準備する
     *
     * 盤面・駒台を初期化し、指定SFENの配置に設定する。
     * 対局結果はNoResult、手番はNoPlayerにリセットされる。
     */
    void newGame(QString& startsfenstr);

signals:
    /// 盤面が変更された（→ ShogiView）
    void boardChanged(ShogiBoard *);

    /// 対局結果が確定した（→ MatchCoordinator）
    void gameOver(ShogiGameController::Result);

    /// 成り/不成の選択ダイアログ表示を要求する（→ PromotionFlow）
    void showPromotionDialog();

    /// 手番が変更された（→ TurnManager）
    void currentPlayerChanged(ShogiGameController::Player);

    /// 駒のドラッグ終了を通知する（→ ShogiView）
    void endDragSignal();

    /// 着手が確定した（→ TurnSyncBridge）
    void moveCommitted(ShogiGameController::Player mover, int ply);

private:
    // --- メンバー変数 ---

    ShogiBoard* m_board = nullptr; ///< 将棋盤（所有）
    Result m_result = NoResult;    ///< 対局結果
    Player m_currentPlayer = NoPlayer; ///< 現在の手番

    bool m_promote = false;              ///< 成り/不成フラグ
    bool m_forcedPromotionMode = false;  ///< 強制成りモード有効フラグ（定跡手用）
    bool m_forcedPromotionValue = false; ///< 強制成りモード時の成り/不成の値

    int previousFileTo = 0;  ///< 1手前の移動先の筋（「同」表記判定用）
    int previousRankTo = 0;  ///< 1手前の移動先の段（「同」表記判定用）

    // --- 内部処理 ---

    void setupBoard();
    void setBoard(ShogiBoard* board);

    /// 指し手を漢字の棋譜文字列に変換する（例: "▲７六歩(77)"）
    QString convertMoveToKanjiStr(const QString piece, const int fileFrom, const int rankFrom, const int fileTo, const int rankTo);

    /// 投了時などの対局結果を設定する（現手番の相手を勝ちにする）
    void gameResult();

    /// 歩・桂・香の行き所のない駒に対して成りフラグを強制設定する
    void setMandatoryPromotionFlag(const int fileTo, const int rankTo, const QChar source);

    // --- 禁じ手チェック ---

    bool checkTwoPawn(const QChar source, const int fileFrom, const int fileTo) const;
    bool checkWhetherAllyPiece(const QChar source, const QChar dest, const int fileFrom, const int fileTo) const;
    bool checkNumberStandPiece(const QChar source, const int fileFrom) const;
    bool checkFromPieceStandToPieceStand(const QChar source, const QChar dest, const int fileFrom, const int fileTo) const;
    bool checkGetKingOpponentPiece(const QChar source, const QChar dest) const;
    bool checkMovePiece(const QChar source, const QChar dest, const int fileFrom, const int fileTo) const;
    void setResult(Result);

    /// アルファベットの駒文字を漢字表記に変換する
    QString getPieceKanji(const QChar& piece);

    /// 指定された駒が成り可能な駒かを判定する
    bool isPromotablePiece(QChar& piece);

    /// 成り/不成の選択ダイアログを表示して結果を反映する
    void decidePromotionByDialog(QChar &piece, int &rankFrom, int &rankTo);

    /// 現在の手番が人間操作かどうかを判定する
    bool isCurrentPlayerHumanControlled(PlayMode& playMode);

    /**
     * @brief 人間の着手時に成り/不成を判定・決定する
     * @return 合法手として成立する場合 true
     */
    bool decidePromotion(PlayMode& playMode, FastMoveValidator& validator,
                         const FastMoveValidator::Turn& turnMove,
                         int& fileFrom, int& rankFrom, int& fileTo, int& rankTo, QChar& piece,  ShogiMove& currentMove);

    /// 相手の手番をTurnで取得する
    Turn getNextPlayerSfen();

    /// MoveValidatorに渡す手番を取得する
    FastMoveValidator::Turn getCurrentTurnForValidator(FastMoveValidator& validator);
};

#endif // SHOGIGAMECONTROLLER_H
