#ifndef SHOGIBOARD_H
#define SHOGIBOARD_H

/// @file shogiboard.h
/// @brief 将棋盤の状態管理クラスの定義

#include <QObject>
#include <QVector>
#include <QMap>
#include "shogitypes.h"

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

    QMap<Piece, int> m_pieceStand;  ///< 駒台データ（駒 → 枚数）

    // --- 盤面アクセス ---

    /// 指定マスの駒を返す（盤上・駒台共通）
    Piece getPieceCharacter(const int file, const int rank);

    /// 盤面データ（81マスの駒配列）を返す
    const QVector<Piece>& boardData() const;

    /// 駒台データを返す
    const QMap<Piece, int>& getPieceStand() const;

    int ranks() const { return m_ranks; }
    int files() const { return m_files; }
    Turn currentPlayer() const;

    // --- SFEN変換 ---

    /**
     * @brief SFEN文字列をパースし、各要素に分解する（副作用なし）
     * @param sfenStr 盤面＋手番＋駒台＋手数を含むSFEN文字列
     * @return パース成功時は SfenComponents、失敗時は std::nullopt
     */
    static std::optional<SfenComponents> parseSfen(const QString& sfenStr);

    /**
     * @brief SFEN文字列で盤面全体を更新する
     * @param sfenStr 盤面＋手番＋駒台＋手数を含むSFEN文字列
     *
     * 文法チェックを行い、不正な場合は盤面を変更しない。
     */
    void setSfen(const QString& sfenStr);

    /// 盤面をSFEN形式の文字列に変換する
    QString convertBoardToSfen();

    /// 駒台をSFEN形式の文字列に変換する
    QString convertStandToSfen() const;

    /// 現局面のSFENレコードをリストに追加する
    void addSfenRecord(Turn nextTurn, const int moveNumber, QStringList* m_sfenHistory);

    // --- 駒操作 ---

    /// 指定マスへ駒を移動する（配置データのみ更新）
    void movePieceToSquare(Piece selectedPiece, const int fileFrom, const int rankFrom,
                           const int fileTo, const int rankTo, const bool promote);

    /// 局面編集用: 盤面と駒台を同時に更新する
    void updateBoardAndPieceStand(const Piece source, const Piece dest, const int fileFrom, const int rankFrom, const int fileTo, const int rankTo, const bool promote);

    /// 局面編集用: 指定マスの駒を成/不成に切り替える
    void promoteOrDemotePiece(const int fileFrom, const int rankFrom);

    /// 駒台に打てる駒があるか判定する
    bool isPieceAvailableOnStand(const Piece source, const int fileFrom) const;

    // --- 駒台操作 ---

    /// 取った駒を駒台に追加する（成駒は元の駒に変換）
    void addPieceToStand(Piece dest);

    /// 駒台の駒を1枚増やす
    void incrementPieceOnStand(const Piece dest);

    /// 駒台の駒を1枚減らす（駒打ち時）
    void decrementPieceOnStand(Piece source);

    // --- 盤面リセット ---

    /// 全駒を駒台に配置する（局面編集用）
    void resetGameBoard();

    /// 駒台を全て0枚にリセットする
    void initStand();

    /// 先後の配置を反転する
    void flipSides();

    // --- デバッグ ---

    void printPieceStand();
    void printPieceCount() const;

signals:
    /// 盤面全体リセット通知（→ ShogiView::repaint）
    void boardReset();

    /// 盤面の1マス変更通知（→ ShogiView::repaint）
    void dataChanged(int c, int r);

private:
    int m_ranks;                    ///< 盤の段数（9）
    int m_files;                    ///< 盤の筋数（9）
    QVector<Piece> m_boardData;     ///< 81マスの駒データ
    int m_currentMoveNumber;        ///< SFEN文字列の手数
    Turn m_currentPlayer = Turn::Black; ///< 現在の手番

    // --- 内部操作 ---
    void setData(const int file, const int rank, const Piece value);
    void initBoard();
    bool setDataInternal(const int file, const int rank, const Piece value);
    bool setPieceStandFromSfen(const QString& str);
    std::optional<QString> validateAndConvertSfenBoardStr(QString sfenStr);
    bool setPiecePlacementFromSfen(QString& initialSfenStr);
    void printPlayerPieces(const QString& player, const QString& pieceSet) const;
    Piece convertPieceChar(const Piece c) const;
    Piece convertPromotedPieceToOriginal(const Piece dest) const;
    void setInitialPieceStandValues();
    QString convertPieceToSfen(const Piece piece) const;
};

#endif // SHOGIBOARD_H
