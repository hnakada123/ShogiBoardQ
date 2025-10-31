#ifndef SHOGIGAMECONTROLLER_H
#define SHOGIGAMECONTROLLER_H

#include <QObject>
#include "movevalidator.h"
#include "playmode.h"

// 将棋盤を表すクラス
class ShogiBoard;

// 将棋の対局全体を管理し、盤面の初期化、指し手の処理、合法手の検証、対局状態の管理を行うクラス
class ShogiGameController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(Player currentPlayer READ currentPlayer NOTIFY currentPlayerChanged)

public:
    // 対局結果を表す列挙型
    enum Result { NoResult, Player1Wins, Draw, Player2Wins };

    Q_ENUM(Result)

    // 対局者を表す列挙型
    enum Player { NoPlayer, Player1, Player2 };

    Q_ENUM(Player)

    // コンストラクタ
    explicit ShogiGameController(QObject* parent = nullptr);

    // 将棋盤のポインタを返す。
    ShogiBoard* board() const;

    // 対局結果を返す。
    inline Result result() const { return m_result; }

    // 現在の手番を返す。
    inline Player currentPlayer() const { return m_currentPlayer; }

    // 現在の対局者を設定する。
    void setCurrentPlayer(const Player);

    // 将棋の指し手を検証し、合法手であれば盤面を更新する。
    // 将棋の指し手を検証し、合法手であれば盤面を更新する。
    bool validateAndMove(QPoint& outFrom, QPoint& outTo, QString& record, PlayMode& playMode, int& moveNumber,
                         QStringList* m_sfenRecord, QVector<ShogiMove>& gameMoves);

    // 局面編集で駒移動を行い、局面を更新する。
    bool editPosition(const QPoint& outFrom, const QPoint& outTo);

    // 指し手を漢字の文字列に変換する。
    QString convertMoveToKanjiStr(const QString piece, const int fileFrom, const int rankFrom, const int fileTo, const int rankTo);

    // 対局結果を設定する。
    void gameResult();

    // 歩、桂馬、香車を指した場合に成りが必須になる時のフラグを設定する。
    void setMandatoryPromotionFlag(const int fileTo, const int rankTo, const QChar source);

    // 編集局面モードの際、右クリックで駒を成・不成に変換する。
    void switchPiecePromotionStatusOnRightClick(const int fileFrom, const int rankFrom) const;

    // 二歩になるかどうかをチェックする。
    bool checkTwoPawn(const QChar source, const int fileFrom, const int fileTo) const;

    // 指そうとした場所に味方の駒があるかどうかチェックする。
    bool checkWhetherAllyPiece(const QChar source, const QChar dest, const int fileFrom, const int fileTo) const;

    // 駒台から指そうとした場合、駒台の駒数が0以下の時は、駒台の駒は無いので指せない。
    bool checkNumberStandPiece(const QChar source, const int fileFrom) const;

    // 駒台から駒台に駒を移動することが可能かどうかをチェックする。
    // 先手の駒台の駒から後手の駒台の異種駒には、駒は移せない。
    // また、後手の駒台の駒から先手の駒台の異種駒には、駒は移せない。
    bool checkFromPieceStandToPieceStand(const QChar source, const QChar dest, const int fileFrom, const int fileTo) const;

    // 王、玉は、相手の駒で取ることはできないので、取れない場合はfalseを返す。
    bool checkGetKingOpponentPiece(const QChar source, const QChar dest) const;

    // 禁じ手をチェックする。
    bool checkMovePiece(const QChar source, const QChar dest, const int fileFrom, const int fileTo) const;

    // 局面編集後の局面をSFEN形式に変換し、リストに追加する。
    void updateSfenRecordAfterEdit(QStringList* m_sfenRecord);

    // 手番を変える。
    void changeCurrentPlayer();

    // 成り・不成のフラグを設定する。
    void setPromote(bool newPromote);

    // 成り・不成のフラグを返す。
    bool promote() const;

    QPoint lastMoveTo() const;  // 直前着手の移動先（筋, 段）を返す

    // 既存の public: メソッド群のどこか（validateAndMove などの近く）に追記
    void applyTimeoutLossFor(int clockPlayer);     // player==1 なら先手の時間切れ→後手勝ち／player==2 なら先手勝ち
    void applyResignationOfCurrentSide();          // 現在手番側が投了 → 相手の勝ち
    void finalizeGameResult();                     // 結果未確定なら最終決定（未決のときは currentPlayer に基づいて勝敗をセット）

public slots:
    // 新規対局の準備
    // 将棋盤、駒台を初期化（何も駒がない）し、入力のSFEN文字列の配置に将棋盤、駒台の駒を
    // 配置し、対局結果を結果なし、現在の手番がどちらでもない状態に設定する。
    void newGame(QString& startsfenstr);

signals:
    // 盤面が変更されたことを通知する。
    void boardChanged(ShogiBoard *);

    // 対局結果が変更されたことを通知する。
    void gameOver(ShogiGameController::Result);

    // 対局者に成るかどうかを選択させるダイアログを表示する。
    void showPromotionDialog();

    // 現在の手番が変更されたことを通知する。
    void currentPlayerChanged(ShogiGameController::Player);

    // 駒のドラッグを終了したことを通知する。
    void endDragSignal();

    void moveCommitted(ShogiGameController::Player mover, int ply);

    // エラーを報告するためのシグナル
    void errorOccurred(const QString& errorMessage);

private:
    // 将棋盤のポインタ
    ShogiBoard* m_board;

    // 対局結果
    Result m_result;

    // 現在の手番
    Player m_currentPlayer;

    // 成るかどうかのフラグ
    bool m_promote;

    // 1手前に指した手の移動先の筋
    int previousFileTo;

    // 1手前に指した手の移動先の段
    int previousRankTo;

    // 将棋盤のポインタを設定する。
    void setupBoard();

    // 将棋盤の81マスに空白を代入し、駒台の駒を0にする。
    // 将棋盤を示すポインタm_boardを作成する。
    void setBoard(ShogiBoard* board);

    // 対局結果を設定する。
    void setResult(Result);

    // アルファベットの駒文字を漢字表記にする。
    QString getPieceKanji(const QChar& piece);

    // 指定された駒が成ることが可能な駒であるかを判定する。
    bool isPromotablePiece(QChar& piece);

    // ダイアログを表示してユーザーに成るかどうかを選択させる。
    void decidePromotionByDialog(QChar &piece, int &rankFrom, int &rankTo);

    // 人間が対局者に含まれているかどうかを判定する。
    bool isCurrentPlayerHumanControlled(PlayMode& playMode);

    // 人間が指した場合に指し手で成る手と不成の手が合法手であるかを判定し、GUIのダイアログで対局者に
    // 成るか成らないかを選択させて、その結果をcurrentMove.isPromotionに保存する。
    // 指し手が合法であればtrue、不合法であればfalseを返す。
    bool decidePromotion(PlayMode& playMode, MoveValidator& validator, const MoveValidator::Turn& turnMove,
                         int& fileFrom, int& rankFrom, int& fileTo, int& rankTo, QChar& piece,  ShogiMove& currentMove);

    // 相手の手番をSFEN形式の手番bまたはwで取得する。
    QString getNextPlayerSfen();

    // 合法手生成に関するクラスで利用するための手番を設定する。
    MoveValidator::Turn getCurrentTurnForValidator(MoveValidator& validator);
};

#endif // SHOGIGAMECONTROLLER_H
