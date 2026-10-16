#ifndef SHOGIGAMECONTROLLER_H
#define SHOGIGAMECONTROLLER_H

/// @file shogigamecontroller.h
/// @brief 将棋の対局進行・盤面更新・合法手検証を管理するコントローラ


#include <QObject>
#include <functional>
#include <memory>
#include <QPoint>
#include "legalmovestatus.h"
#include "recordentry.h"
#include "shogimove.h"
#include "shogitypes.h"
#include "usimove.h"

class ShogiBoard;

/**
 * @brief 対局で使う盤（ShogiBoard）を持ち、指し手を確かめて盤に指す
 *
 * 合法手の判定、成り・不成の選択、棋譜の表記を受け持つ。「同」表記の基準（局面に至った手の移動先）は
 * 手順を持つ呼び出し側が指すたびに渡す（盤の局面に写しを持たない）。
 * 指した手は棋譜の1行（RecordEntry）として返し、対局の手順への追加は呼び出し側（司令塔・通信対局）が行う。
 */
class ShogiGameController : public QObject
{
    Q_OBJECT

public:

    explicit ShogiGameController(QObject* parent = nullptr);
    ~ShogiGameController() override;

    /// 対局の盤（起動から終了まで同じ1つ。局面は newGame・setSfen で入れ替える）
    ShogiBoard* board() const;
    /// 手番（盤の手番）
    Turn currentPlayer() const;
    /// 盤の手番を変える（局面編集の手番変更・検討する局面の手番）
    void setCurrentPlayer(Turn player);

    // --- 指し手処理 ---

    /**
     * @brief 盤のクリック操作（表示の座標）による着手を検証し、合法手であれば指す
     *
     * 成り・不成の両方が合法なら、成りの選び方（setPromotionChooser）で対局者に選ばせる。
     * @param previousTo 局面に至った手の移動先（「同」表記の基準。開始局面は無効なマス）
     * @return 指した手（盤に指せなければ nullopt）
     */
    std::optional<RecordEntry> validateAndMove(QPoint& outFrom, QPoint& outTo, BoardSquare previousTo);

    /// 盤面を変更せず、クリック操作の合法性と成り・不成を確定する。
    std::optional<UsiMove> prepareMove(const QPoint& from, const QPoint& to);

    /// 手番の側の合法手か（成り・不成も指し手どおりに判定する）
    bool isLegal(const UsiMove& move) const;

    /// エンジン・通信対局・定跡の指し手を合法手か確かめて指す。反則手は盤を変えずに nullopt
    std::optional<RecordEntry> applyMove(const UsiMove& move, BoardSquare previousTo);

    /// 確かめ済みの手、またはサーバーが裁定する手を指す。盤面に適用できない手は変更せず nullopt
    std::optional<RecordEntry> applyRecordedMove(const UsiMove& move, BoardSquare previousTo);

    /// 直前に指した手（指せたときに更新する）
    const UsiMove& lastMove() const { return m_lastMove; }

    /**
     * @brief 着手の確定を知らせる（盤・手順・棋譜・時計を更新し終えた後に、対局の司令塔が呼ぶ）
     * @param ply 対局の手順での手数

     */
    void notifyMoveCommitted(Turn mover, int ply);

    /// 局面編集モードで駒を移動し、盤面を更新する
    bool editPosition(const QPoint& outFrom, const QPoint& outTo, Turn pieceBoxSide = Turn::Black);

    /// 局面編集モードで右クリックによる成/不成を切り替える
    void switchPiecePromotionStatusOnRightClick(const int fileFrom, const int rankFrom) const;

    // --- 成り制御 ---

    /// 成り・不成を対局者に選ばせる関数（成るなら true）。未設定なら成らない
    using PromotionChooser = std::function<bool()>;
    void setPromotionChooser(PromotionChooser chooser);

public slots:
    /**
     * @brief SFEN文字列から新規対局を準備する
     *
     * 盤を指定SFENの局面にし、直前の手を消す。手番は SFEN の手番になる。
     */
    void newGame(const QString& startSfen);

signals:
    /// 駒のドラッグ終了を通知する（→ ShogiView）
    void endDragSignal();

    /// 着手が確定した（→ PieceSoundPlayer）
    void moveCommitted(Turn mover, int ply);

private:
    std::unique_ptr<ShogiBoard> m_board; ///< 将棋盤（所有。起動から終了まで同じ1つ）

    PromotionChooser m_promotionChooser; ///< 成り・不成の選び方
    UsiMove m_lastMove;                  ///< 直前に指した手

    /// 成り・不成それぞれで合法手か
    LegalMoveStatus legalStatus(const UsiMove& move, Turn mover) const;

    /// 盤に指して手番を渡し、指した手を棋譜の1行にする
    std::optional<RecordEntry> commitMove(const UsiMove& move, BoardSquare previousTo);
};

#endif // SHOGIGAMECONTROLLER_H
