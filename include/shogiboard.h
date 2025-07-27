#ifndef SHOGIBOARD_H
#define SHOGIBOARD_H

#include <QObject>
#include <QVector>
#include <QMap>

// 将棋盤に関するクラス
class ShogiBoard : public QObject
{
    Q_OBJECT

public:
    // コンストラクタ
    explicit ShogiBoard(int ranks = 9, int files = 9, QObject *parent = 0);

    // 持ち駒の駒文字と枚数を格納するマップ
    QMap<QChar, int> m_pieceStand;

    // 将棋盤内の駒m_boardDataと駒台の駒m_standPieceの駒文字を返す。
    // 例．先手の歩 P、後手の銀 g char型の1文字を返す。
    QChar getPieceCharacter(const int file, const int rank);

    // 将棋盤と駒台を含むSFEN文字列で将棋盤全体を更新する場合、この関数を使う。
    // 将棋盤に入力で渡されるsfen形式の文字列に文法的に誤りが無いかチェックする。
    // 将棋盤と駒台のSFEN文字列を指定して将棋盤と駒台の駒の更新を行い、再描画する。
    // 入力は、将棋盤と駒台を含むSFEN文字列
    void setSfen(QString& sfenStr);

    // 駒を指定したマスへ移動する。配置データのみを更新する。
    void movePieceToSquare(QChar selectedPiece, const int fileFrom, const int rankFrom,
                           const int fileTo, const int rankTo, const bool promote);

    // 持ち駒を出力する。
    void printPieceStand();

    // 局面編集中に使用される。将棋盤と駒台の駒を更新する。
    void updateBoardAndPieceStand(const QChar source, const QChar dest, const int fileFrom, const int rankFrom, const int fileTo, const int rankTo, const bool promote);

    // 駒台の駒数が0以下の時は、駒台の駒は無いので指せない。
    bool isPieceAvailableOnStand(const QChar source, const int fileFrom) const;

    // 将棋盤内のデータ（81マスの駒文字情報）を返す。
    const QVector<QChar>& boardData() const;

    // 駒台のデータ（駒台の各駒の枚数情報）を返す。
    const QMap<QChar, int>& getPieceStand() const;

    // 局面編集中に右クリックで成駒、不成駒に変換する。
    void promoteOrDemotePiece(const int fileFrom, const int rankFrom);

    // 将棋盤の段の数すなわち9を返す。
    int ranks() const { return m_ranks; }

    // 将棋盤の筋の数すなわち9を返す。
    int files() const { return m_files; }

    // 「全ての駒を駒台へ」をクリックした時に実行される。
    // 先手と後手の駒を同じ枚数にして全ての駒を駒台に載せる。
    void resetGameBoard();

    // 駒台の各駒の数を全て0にする。
    void initStand();

    // 「先後反転」をクリックした時に実行される。
    // 先手の配置を後手の配置に変更し、後手の配置を先手の配置に変更する。
    void flipSides();

    // SFEN文字列への変換してリストに追加する。
    void addSfenRecord(const QString& nextTurn, const int moveNumber, QStringList* m_sfenRecord);

    // 持ち駒を出力する。
    void printPieceCount() const;

    // 自分の駒台に駒を置いた場合、自分の駒台の枚数に1加える。
    void incrementPieceOnStand(const QChar dest);

    QString currentPlayer() const;

signals:
    // エラーを報告するためのシグナル
    void errorReported(const QString& errorMessage);

    // ShogiView::setBoardのconnectで使用されているシグナル
    // スロットのrepaint関数が実行される。（将棋盤、駒台の再描画）
    void boardReset();

    // ShogiView::setBoardのconnectで使用されているシグナル
    // スロットのrepaint関数が実行される。（将棋盤、駒台の再描画）
    void dataChanged(int c, int r);

private:
    // 将棋盤の段の数すなわち9
    int m_ranks;

    // 将棋盤の筋の数すなわち9
    int m_files;

    // 将棋盤内81マスの駒文字データを格納
    QVector<QChar> m_boardData;

    // SFEN文字列の次の手が何手目になるかを示す数
    int m_currentMoveNumber;

    // 先手か後手かのどちらか（bあるいはw）
    QString m_currentPlayer;

    // 将棋盤のマスに駒を配置する。
    // file 筋、rank 段で指定されたマスに駒文字valueをセットする。
    void setData(const int file, const int rank, const QChar value);

    // 将棋盤の81マスに空白を代入する。
    void initBoard();

    // 将棋盤のマスに駒を配置する。
    bool setDataInternal(const int file, const int rank, const QChar value);

    // SFEN文字列の持ち駒部分の文字列を入力して、持ち駒を表す配列に各駒の枚数をセットする。
    void setPieceStandFromSfen(const QString& str);

    // 将棋盤内のみのSFEN文字列を入力し、エラーチェックを行い、成り駒を1文字に変換したSFEN文字列を返す。
    QString validateAndConvertSfenBoardStr(QString sfenStr);

    // SFEN文字列から将棋盤内に駒を配置する。
    void setPiecePlacementFromSfen(QString& initialSfenStr);

    // 指したマスが将棋盤内で相手の駒があった場合、自分の駒台の枚数に1加える。
    void addPieceToStand(QChar dest);

    // 駒台から指した場合、駒台の駒の枚数を1減らす。
    void decrementPieceOnStand(QChar source);

    // SFEN文字列を入力し、エラーチェックを行い、次の手番、次の手が何手目かを取得する。
    void validateSfenString(QString& sfenStr, QString& sfenBoardStr, QString& sfenStandStr);

    // 手番の持ち駒を出力する。
    void printPlayerPieces(const QString& player, const QString& pieceSet) const;

    // 駒の文字を変換する。成駒は相手の駒になるように、大文字を小文字に、小文字を大文字に変換する。
    QChar convertPieceChar(const QChar c) const;

    // 成駒を元の駒に変換する。
    QChar convertPromotedPieceToOriginal(const QChar dest) const;

    // 駒の初期値を設定する。
    void setInitialPieceStandValues();

    // 成り駒の文字をSFEN形式の駒文字列に変換する。
    QString convertPieceToSfen(const QChar piece) const;

    // 盤面をSFEN形式へ変換する。
    QString convertBoardToSfen();

    // 駒台のSFEN形式への変換
    QString convertStandToSfen() const;
};

#endif // SHOGIBOARD_H
