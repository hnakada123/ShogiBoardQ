#ifndef SHOGIBOARD_H
#define SHOGIBOARD_H

/// @file shogiboard.h
/// @brief 将棋盤の状態管理クラスの定義

#include <QObject>
#include <QList>
#include <QMap>
#include <optional>
#include "shogitypes.h"
#include "usimove.h"

/**
 * @brief 将棋盤の盤面データと駒台を管理するクラス
 *
 * 81マスの駒配置データと両手番の駒台を保持し、
 * SFEN文字列との相互変換、駒の移動・成り・打ち等の操作を提供する。
 *
 */
class ShogiBoard : public QObject
{
    Q_OBJECT

public:
    explicit ShogiBoard(int ranks = 9, int files = 9, QObject *parent = nullptr);

    // --- 盤面アクセス ---

    /// 指定マスの駒を返す（盤上・駒台共通）
    Piece pieceCharacter(const int file, const int rank) const;

    /// 盤上のマスの駒（盤外のマスは None）
    Piece pieceAt(BoardSquare square) const;

    /// 盤上のマスの駒を置き換える（局面の組み立て用。成りの補正も駒台の増減もしない。盤外のマスは無視する）
    void setPieceAt(BoardSquare square, Piece piece);

    /// 盤面データ（81マスの駒配列）を返す
    const QList<Piece>& boardData() const { return m_boardData; }

    /// 駒台データを返す
    const QMap<Piece, int>& pieceStand() const { return m_pieceStand; }

    /// 指定駒の駒台枚数を返す（存在しない場合は0）
    int pieceStandCount(Piece piece) const;

    /// 盤上・両駒台にない未使用駒を、生駒・先手向きで数える（標準40枚との差分）。
    QMap<Piece, int> pieceBox() const;
    int pieceBoxCount(Piece piece) const;

    /// 指定駒を駒台に加算する（delta>0 のときのみ）
    void addStandPiece(Piece piece, int delta = 1);

    /// 指定駒を駒台から1枚消費する（存在しない/0枚時は false）
    bool consumeStandPiece(Piece piece);

    int ranks() const { return m_ranks; }
    int files() const { return m_files; }
    Turn currentPlayer() const;

    /// 手番のみを更新する（盤面・持ち駒は変更しない。局面編集の手番変更などに使う）
    void setCurrentPlayer(Turn turn);

    /// 次に指す手の手数（SFEN の4つ目の欄）
    int moveNumber() const { return m_moveNumber; }
    void setMoveNumber(int moveNumber) { m_moveNumber = qMax(1, moveNumber); }

    // --- SFEN変換 ---

    /**
     * @brief SFEN文字列をパースし、各要素に分解する（副作用なし）
     * @param sfenStr 盤面＋手番＋駒台＋手数を含むSFEN文字列
     * @return パース成功時は SfenComponents、失敗時は std::nullopt
     */
    [[nodiscard]] static std::optional<SfenComponents> parseSfen(const QString& sfenStr);

    /**
     * @brief SFEN文字列で盤面全体を更新する
     * @param sfenStr 盤面＋手番＋駒台＋手数を含むSFEN文字列
     *
     * 文法チェックを行い、不正な場合は盤面を変更せず、エラーを通知する（ErrorBus）。
     */
    void setSfen(const QString& sfenStr);

    /**
     * @brief SFEN文字列で盤面全体を更新する（不正でも通知しない）
     * @param error 不正だったときの理由（任意）
     * @return 盤面を更新したら true。不正な場合は盤面を変更せず false
     */
    bool trySetSfen(const QString& sfenStr, QString* error = nullptr);

    /// 別の盤の局面（盤・駒台・手番）を写す。
    /// 盤の大きさと駒台の駒の種類が同じなら、確保し直さずに上書きする（読み筋の変換で info 行ごとに使う）
    void copyPositionFrom(const ShogiBoard& other);

    /// 盤面をSFEN形式の文字列に変換する
    QString convertBoardToSfen() const;

    /// 駒台をSFEN形式の文字列に変換する
    QString convertStandToSfen() const;

    /// 局面全体（盤・手番・持駒・手数）の SFEN
    QString toSfen() const;

    // --- 駒操作 ---

    /// 指定マスへ駒を移動する（配置データのみ更新）
    void movePieceToSquare(Piece selectedPiece, const int fileFrom, const int rankFrom,
                           const int fileTo, const int rankTo, const bool promote);

    /// 局面編集用: 盤面と駒台を同時に更新する
    void updateBoardAndPieceStand(const Piece source, const Piece dest, const int fileFrom, const int rankFrom, const int fileTo, const int rankTo, const bool promote);

    /**
     * @brief USI の指し手を盤と駒台に適用し、手番を相手に渡して手数を1つ進める
     *
     * 合法手かどうかは確かめない（対局で確かめてから指す経路は ShogiGameController::applyMove）。
     * 移動元が空、駒台に打つ駒がない、打つ先が埋まっている指し手は適用せず false を返す。
     * @param mover 指す側（駒打ちの駒の色を決める）
     */
    bool applyUsiMove(const UsiMove& move, Turn mover);
    /// 手番側の手として適用する
    bool applyUsiMove(const UsiMove& move) { return applyUsiMove(move, m_currentPlayer); }

    /// 局面編集用: 指定マスの駒を成/不成に切り替える
    void promoteOrDemotePiece(const int fileFrom, const int rankFrom);

    /// 駒台に打てる駒があるか判定する
    bool isPieceAvailableOnStand(const Piece source, const int fileFrom) const;

    // --- 駒台操作 ---

    /// 取った駒を駒台に追加する（成駒は元の駒に変換）
    void addPieceToStand(Piece dest);

    /// 駒台の駒を1枚増やす
    void incrementPieceOnStand(const Piece dest);

    /// 駒台の駒を1枚減らす（駒打ち時）。不足時は false。
    bool decrementPieceOnStand(Piece source);

    // --- 盤面リセット ---

    /// 盤上・両駒台を空にして、全駒を駒箱に戻す（局面編集用）
    void resetGameBoard();

    /// 駒台を全て0枚にリセットする
    void initStand();

    /// 先後の配置を反転する
    void flipSides();

signals:
    /// 盤面全体リセット通知（→ ShogiView::repaint）
    void boardReset();

    /// 盤面の1マス変更通知（→ ShogiView::repaint）
    void dataChanged(int c, int r);

    /// 駒台の枚数の変更通知（→ ShogiView の駒台のセルの描き直し）。
    /// 局面を丸ごと替える setSfen は、この通知ではなく boardReset で知らせる
    void standChanged(Piece piece);

    /// 手番が変わった（指した・手番だけを変えた。局面を丸ごと替えたときは boardReset で知らせる）
    void turnChanged(Turn turn);

private:
    int m_ranks;                    ///< 盤の段数（9）
    int m_files;                    ///< 盤の筋数（9）
    QList<Piece> m_boardData;     ///< 81マスの駒データ
    QMap<Piece, int> m_pieceStand;  ///< 駒台データ（駒 → 枚数）。玉を含む先後16種を常に持つ
    Turn m_currentPlayer = Turn::Black; ///< 現在の手番
    int m_moveNumber = 1;               ///< 次に指す手の手数

    // --- 内部操作 ---
    void setData(const int file, const int rank, const Piece value);
    void advanceTurn(Turn mover);
    void initBoard();
    bool setDataInternal(const int file, const int rank, const Piece value);
    Piece convertPieceChar(const Piece c) const;
    Piece convertPromotedPieceToOriginal(const Piece dest) const;
    QString convertPieceToSfen(const Piece piece) const;
};

#endif // SHOGIBOARD_H
