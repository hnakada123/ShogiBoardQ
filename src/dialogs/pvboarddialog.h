#ifndef PVBOARDDIALOG_H
#define PVBOARDDIALOG_H

#include <QDialog>
#include <QStringList>
#include <QVector>
#include "shogiview.h"

class ShogiBoard;
class QPushButton;
class QLabel;

/**
 * @brief 読み筋（PV: Principal Variation）を再生する別ウィンドウダイアログ
 *
 * 思考タブの読み筋をクリックすると、このダイアログが開き、
 * 1手進む/戻るボタンで読み筋の盤面を再現できる。
 */
class PvBoardDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief コンストラクタ
     * @param baseSfen 読み筋開始時点の局面（SFEN形式）
     * @param pvMoves 読み筋の手（USI形式の手のリスト、例: {"7g7f", "3c3d", ...}）
     * @param parent 親ウィジェット
     */
    explicit PvBoardDialog(const QString& baseSfen,
                           const QStringList& pvMoves,
                           QWidget* parent = nullptr);
    ~PvBoardDialog() override;

    /**
     * @brief 漢字表記の読み筋を設定
     * @param kanjiPv 漢字形式の読み筋（例: "▲７六歩△３四歩..."）
     */
    void setKanjiPv(const QString& kanjiPv);

    /**
     * @brief 対局者名を設定
     * @param blackName 先手の名前
     * @param whiteName 後手の名前
     */
    void setPlayerNames(const QString& blackName, const QString& whiteName);

    /**
     * @brief 起動時の局面に至った最後の手を設定（USI形式）
     * @param lastMove USI形式の手（例: "7g7f"）
     * 
     * この手が設定されている場合、開始局面（手数0）でもハイライトが表示される。
     */
    void setLastMove(const QString& lastMove);
    void setPrevSfenForHighlight(const QString& prevSfen);

private slots:
    /// 最初に戻る
    void onGoFirst();
    /// 1手戻る
    void onGoBack();
    /// 1手進む
    void onGoForward();
    /// 最後まで進む
    void onGoLast();
    /// 将棋盤を拡大
    void onEnlargeBoard();
    /// 将棋盤を縮小
    void onReduceBoard();

private:
    /// UIを構築
    void buildUi();
    /// ボタンの有効/無効を更新
    void updateButtonStates();
    /// 盤面を現在の手数で更新
    void updateBoardDisplay();
    /// 時計ラベルを非表示にする
    void hideClockLabels();
    /// SFEN形式の手を盤面に適用
    void applyMove(const QString& usiMove);
    /// 手番を取得（"b" or "w"）
    QString currentTurn() const;
    /// 現在の手のハイライトを更新
    void updateMoveHighlights();
    /// ハイライトをクリア
    void clearMoveHighlights();
    /// 漢字表記の読み筋を個々の手に分割
    void parseKanjiMoves();
    /// ウィンドウサイズを保存
    void saveWindowSize();
    /// レイアウト確定後にウィンドウサイズを調整
    void adjustWindowToContents();

protected:
    /// ウィンドウを閉じる際にサイズを保存
    void closeEvent(QCloseEvent* event) override;
    /// ShogiViewのCtrl+ホイールイベントを横取りして処理
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    ShogiView* m_shogiView = nullptr;
    ShogiBoard* m_board = nullptr;

    QString m_baseSfen;           ///< 開始局面のSFEN
    QStringList m_pvMoves;        ///< USI形式の読み筋手
    QStringList m_kanjiMoves;     ///< 漢字表記の各手のリスト
    QString m_kanjiPv;            ///< 漢字表記の読み筋
    QString m_blackPlayerName;    ///< 先手の対局者名
    QString m_whitePlayerName;    ///< 後手の対局者名
    QString m_lastMove;           ///< 起動時の局面に至った最後の手（USI形式）
    QString m_prevSfen;           ///< 起動時局面の1手前SFEN（差分ハイライト用）

    QVector<QString> m_sfenHistory; ///< 各手数での局面SFEN履歴

    int m_currentPly = 0;         ///< 現在の手数（0=開始局面）

    // UI要素
    QPushButton* m_btnFirst = nullptr;
    QPushButton* m_btnBack = nullptr;
    QPushButton* m_btnForward = nullptr;
    QPushButton* m_btnLast = nullptr;
    QPushButton* m_btnEnlarge = nullptr;  ///< 将棋盤拡大ボタン
    QPushButton* m_btnReduce = nullptr;   ///< 将棋盤縮小ボタン
    QLabel* m_plyLabel = nullptr;
    QLabel* m_pvLabel = nullptr;

    // ハイライト用
    ShogiView::FieldHighlight* m_fromHighlight = nullptr;  ///< 移動元ハイライト
    ShogiView::FieldHighlight* m_toHighlight = nullptr;    ///< 移動先ハイライト
};

#endif // PVBOARDDIALOG_H
