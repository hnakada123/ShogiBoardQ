#ifndef SHOGIBOARD_H
#define SHOGIBOARD_H

/// @file shogiboard.h
/// @brief 将棋盤の状態管理クラスの定義

#include <QObject>
#include <QVector>
#include <QMap>

/**
 * @brief 将棋盤の盤面データと駒台を管理するクラス
 *
 * 81マスの駒配置データと両手番の駒台を保持し、
 * SFEN文字列との相互変換、駒の移動・成り・打ち等の操作を提供する。
 *
 * @todo remove コメントスタイルガイド適用済み
 */
class ShogiBoard : public QObject
{
    Q_OBJECT

public:
    explicit ShogiBoard(int ranks = 9, int files = 9, QObject *parent = nullptr);

    QMap<QChar, int> m_pieceStand;  ///< 駒台データ（駒文字 → 枚数）

    // --- 盤面アクセス ---

    /// 指定マスの駒文字を返す（盤上・駒台共通）
    QChar getPieceCharacter(const int file, const int rank);

    /// 盤面データ（81マスの駒文字配列）を返す
    const QVector<QChar>& boardData() const;

    /// 駒台データを返す
    const QMap<QChar, int>& getPieceStand() const;

    int ranks() const { return m_ranks; }
    int files() const { return m_files; }
    QString currentPlayer() const;

    // --- SFEN変換 ---

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
    void addSfenRecord(const QString& nextTurn, const int moveNumber, QStringList* m_sfenRecord);

    // --- 駒操作 ---

    /// 指定マスへ駒を移動する（配置データのみ更新）
    void movePieceToSquare(QChar selectedPiece, const int fileFrom, const int rankFrom,
                           const int fileTo, const int rankTo, const bool promote);

    /// 局面編集用: 盤面と駒台を同時に更新する
    void updateBoardAndPieceStand(const QChar source, const QChar dest, const int fileFrom, const int rankFrom, const int fileTo, const int rankTo, const bool promote);

    /// 局面編集用: 指定マスの駒を成/不成に切り替える
    void promoteOrDemotePiece(const int fileFrom, const int rankFrom);

    /// 駒台に打てる駒があるか判定する
    bool isPieceAvailableOnStand(const QChar source, const int fileFrom) const;

    // --- 駒台操作 ---

    /// 取った駒を駒台に追加する（成駒は元の駒に変換）
    void addPieceToStand(QChar dest);

    /// 駒台の駒を1枚増やす
    void incrementPieceOnStand(const QChar dest);

    /// 駒台の駒を1枚減らす（駒打ち時）
    void decrementPieceOnStand(QChar source);

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
    /// エラー発生通知（現在connect先なし）
    void errorOccurred(const QString& errorMessage);

    /// 盤面全体リセット通知（→ ShogiView::repaint）
    void boardReset();

    /// 盤面の1マス変更通知（→ ShogiView::repaint）
    void dataChanged(int c, int r);

private:
    int m_ranks;                    ///< 盤の段数（9）
    int m_files;                    ///< 盤の筋数（9）
    QVector<QChar> m_boardData;     ///< 81マスの駒文字データ
    int m_currentMoveNumber;        ///< SFEN文字列の手数
    QString m_currentPlayer;        ///< 現在の手番（"b" or "w"）

    // --- 内部操作 ---
    void setData(const int file, const int rank, const QChar value);
    void initBoard();
    bool setDataInternal(const int file, const int rank, const QChar value);
    void setPieceStandFromSfen(const QString& str);
    QString validateAndConvertSfenBoardStr(QString sfenStr);
    void setPiecePlacementFromSfen(QString& initialSfenStr);
    void validateSfenString(const QString& sfenStr, QString& sfenBoardStr, QString& sfenStandStr);
    void printPlayerPieces(const QString& player, const QString& pieceSet) const;
    QChar convertPieceChar(const QChar c) const;
    QChar convertPromotedPieceToOriginal(const QChar dest) const;
    void setInitialPieceStandValues();
    QString convertPieceToSfen(const QChar piece) const;
};

#endif // SHOGIBOARD_H
