#ifndef PVBOARDDIALOG_H
#define PVBOARDDIALOG_H

/// @file pvboarddialog.h
/// @brief 読み筋盤面ダイアログクラスの定義


#include <QDialog>
#include <QStringList>
#include <memory>
#include "shogiview.h"

class ShogiBoard;
class QPushButton;
class QLabel;
class PvBoardController;

/**
 * @brief 読み筋（PV: Principal Variation）を再生する別ウィンドウダイアログ
 *
 * 思考タブの読み筋をクリックすると、このダイアログが開き、
 * 1手進む/戻るボタンで読み筋の盤面を再現できる。
 *
 * ロジック（ナビゲーション状態、SFEN履歴、ハイライト座標計算）は
 * PvBoardController に委譲し、本クラスはUI表示のみを担当する。
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
     */
    void setLastMove(const QString& lastMove);
    void setPrevSfenForHighlight(const QString& prevSfen);

    /**
     * @brief 盤面の反転モードを設定（GUI本体の向きに合わせる）
     * @param flipped trueで反転表示
     */
    void setFlipMode(bool flipped);

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
    /// 盤面を回転
    void onFlipBoard();

private:
    /// UIを構築
    void buildUi();
    /// ボタンの有効/無効を更新
    void updateButtonStates();
    /// 盤面を現在の手数で更新
    void updateBoardDisplay();
    /// 時計ラベルを非表示にする
    void hideClockLabels();
    /// 現在の手のハイライトを更新
    void updateMoveHighlights();
    /// ハイライトをクリア
    void clearMoveHighlights();
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
    std::unique_ptr<PvBoardController> m_controller;

    ShogiView* m_shogiView = nullptr;
    ShogiBoard* m_board = nullptr;

    QString m_blackPlayerName;    ///< 先手の対局者名
    QString m_whitePlayerName;    ///< 後手の対局者名

    // UI要素
    QPushButton* m_btnFirst = nullptr;
    QPushButton* m_btnBack = nullptr;
    QPushButton* m_btnForward = nullptr;
    QPushButton* m_btnLast = nullptr;
    QPushButton* m_btnEnlarge = nullptr;  ///< 将棋盤拡大ボタン
    QPushButton* m_btnReduce = nullptr;   ///< 将棋盤縮小ボタン
    QPushButton* m_btnFlip = nullptr;     ///< 盤面回転ボタン
    QLabel* m_plyLabel = nullptr;
    QLabel* m_pvLabel = nullptr;

    // ハイライト用
    std::unique_ptr<ShogiView::FieldHighlight> m_fromHighlight;  ///< 移動元ハイライト
    std::unique_ptr<ShogiView::FieldHighlight> m_toHighlight;    ///< 移動先ハイライト
};

#endif // PVBOARDDIALOG_H
