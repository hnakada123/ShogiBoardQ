#ifndef SHOGIGAMECONTROLLER_H
#define SHOGIGAMECONTROLLER_H

/// @file shogigamecontroller.h
/// @brief 将棋の対局進行・盤面更新・合法手検証を管理するコントローラ


#include <QObject>
#include <functional>
#include <memory>
#include "enginemovevalidator.h"
#include "playmode.h"
#include "shogitypes.h"
#include "usimove.h"

class ShogiBoard;

/**
 * @brief 将棋の対局全体を管理し、盤面の初期化・指し手処理・合法手検証・対局状態管理を行う
 *
 */
class ShogiGameController : public QObject
{
    Q_OBJECT

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

    // --- 生成・取得 ---

    explicit ShogiGameController(QObject* parent = nullptr);
    ~ShogiGameController() override;

    ShogiBoard* board() const;
    Result result() const { return m_result; }
    /// 手番（新しい対局の前は先手）
    Turn currentPlayer() const { return m_currentPlayer; }

    /// 「同」表記の基準を復元する（1始まりの盤座標、QPoint() は開始局面）。
    void setPreviousMoveDestination(const QPoint& square)
    {
        previousFileTo = square.x();
        previousRankTo = square.y();
    }
    QPoint previousMoveDestination() const { return QPoint(previousFileTo, previousRankTo); }
    void setCurrentPlayer(Turn player);

    // --- 指し手処理 ---

    /**
     * @brief 盤のクリック操作（表示の座標）による着手を検証し、合法手であれば指す
     *
     * 成り・不成の両方が合法なら、成りの選び方（setPromotionChooser）で対局者に選ばせる。
     * @return 合法手で盤面更新できた場合 true
     */
    bool validateAndMove(QPoint& outFrom, QPoint& outTo, QString& record, int moveNumber,
                         QStringList* sfenHistory, QList<ShogiMove>& gameMoves);

    /**
     * @brief 成り・不成まで決まったエンジンの指し手を、合法手か確かめてから指す
     *
     * 通信対局の手はここを通らない（相手の手は CsaMoveConverter::applyMoveToBoard、
     * 自分のエンジンの手は CsaMoveProgressHandler が盤に直接指す）。
     * @return 合法手で盤面更新できた場合 true。反則手は盤を変えずに false
     */
    bool applyMove(const UsiMove& move, QString& record, int moveNumber,
                   QStringList* sfenHistory, QList<ShogiMove>& gameMoves);

    /// 直前に指した手（validateAndMove・applyMove が成功したときに更新する）
    const UsiMove& lastMove() const { return m_lastMove; }

    /// 局面編集モードで駒を移動し、盤面を更新する
    bool editPosition(const QPoint& outFrom, const QPoint& outTo, Turn pieceBoxSide = Turn::Black);

    /// 局面編集モードで右クリックによる成/不成を切り替える
    void switchPiecePromotionStatusOnRightClick(const int fileFrom, const int rankFrom) const;

    // --- SFEN・手番 ---

    void changeCurrentPlayer();

    // --- 成り制御 ---

    /// 成り・不成を対局者に選ばせる関数（成るなら true）。未設定なら成らない
    using PromotionChooser = std::function<bool()>;
    void setPromotionChooser(PromotionChooser chooser);

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
     * 対局結果はNoResult、手番は SFEN の手番になる。
     */
    void newGame(QString& startsfenstr);

signals:
    /// 手番が変更された（→ TurnManager、BoardInteractionController）
    void currentPlayerChanged(Turn);

    /// 駒のドラッグ終了を通知する（→ ShogiView）
    void endDragSignal();

    /// 着手が確定した（→ MainWindow::onMoveCommitted、PieceSoundPlayer）
    void moveCommitted(Turn mover, int ply);

private:
    // --- メンバー変数 ---

    std::unique_ptr<ShogiBoard> m_board; ///< 将棋盤（所有）
    Result m_result = NoResult;    ///< 対局結果
    Turn m_currentPlayer = Turn::Black; ///< 現在の手番
    bool m_sideToMoveSet = false;       ///< 手番を一度でも設定したか

    PromotionChooser m_promotionChooser; ///< 成り・不成の選び方
    UsiMove m_lastMove;                  ///< 直前に指した手
    bool m_forcedPromotionMode = false;  ///< 強制成りモード有効フラグ（定跡手用）
    bool m_forcedPromotionValue = false; ///< 強制成りモード時の成り/不成の値

    int previousFileTo = 0;  ///< 1手前の移動先の筋（「同」表記判定用）
    int previousRankTo = 0;  ///< 1手前の移動先の段（「同」表記判定用）

    // --- 内部処理 ---

    void setupBoard();

    /// 指し手を漢字の棋譜文字列に変換する（例: "▲７六歩(77)"）
    QString convertMoveToKanjiStr(Piece moving, const UsiMove& move);

    /// 投了時などの対局結果を設定する（現手番の相手を勝ちにする）
    void gameResult();

    void setResult(Result);

    /// 成り・不成それぞれで合法手か
    LegalMoveStatus legalStatus(const UsiMove& move, Turn mover, Piece moving, Piece captured) const;

    /// 合法と確かめた指し手を盤・棋譜・SFEN 履歴に記録して手番を渡す
    bool commitMove(const UsiMove& move, Turn mover, Piece moving, Piece captured, QString& record,
                    int moveNumber, QStringList* sfenHistory, QList<ShogiMove>& gameMoves);
};

#endif // SHOGIGAMECONTROLLER_H
