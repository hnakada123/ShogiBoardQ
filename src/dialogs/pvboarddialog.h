#ifndef PVBOARDDIALOG_H
#define PVBOARDDIALOG_H

/// @file pvboarddialog.h
/// @brief 読み筋盤面ダイアログクラスの定義


#include <QDialog>
#include <QStringList>
#include <memory>
#include "shogiview.h"

class BoardViewerWidget;
class PvLine;
class PvMoveStrip;
class PvText;

/**
 * @brief 読み筋（PV: Principal Variation）を再生する別ウィンドウダイアログ
 *
 * 思考タブの読み筋をクリックすると、このダイアログが開き、
 * 1手進む/戻るボタンで読み筋の盤面を再現できる。
 *
 * 読み筋の値（各手数の局面・手の表記・ハイライト）は PvLine に、
 * 盤と拡大縮小・回転・局面送りの操作は BoardViewerWidget に委譲し、本クラスは組み立てのみを担当する。
 */
class PvBoardDialog : public QDialog
{
    Q_OBJECT

public:
    /// pv は読み筋（開始局面の上で指した手。読み筋の欄の表記は表記の設定に合わせて作る）
    explicit PvBoardDialog(const PvText& pv, QWidget* parent = nullptr);
    ~PvBoardDialog() override;

    /// 盤に出す対局者名（呼び出し側が決めた名前をそのまま出す。設定しなければ「先手」「後手」）
    void setPlayerNames(const QString& blackName, const QString& whiteName);

    /**
     * @brief 起動時の局面に至った最後の手を設定（USI形式）
     * @param lastMove USI形式の手（例: "7g7f"）
     */
    void setLastMove(const QString& lastMove);

    /**
     * @brief 盤面の反転モードを設定（GUI本体の向きに合わせる）
     * @param flipped trueで反転表示
     */
    void setFlipMode(bool flipped);

    /// 盤のマスの大きさ（px。GUI本体の盤に合わせる。範囲外は端に寄せる）
    void setSquareSize(int squareSize);

private slots:
    /// 局面送りで表示する手数が変わった
    void onPlyChanged(int ply);
    /// A-/A+ で文字の大きさが変わった（帯とボタンの高さが変わるので、ウィンドウを盤に合わせ直す）
    void onFontSizeChanged();

private:
    /// UIを構築
    void buildUi();
    /// 読み筋の帯の今の手・手数ラベル・直前の手のハイライトを更新
    void updatePlyDisplay();

private:
    std::unique_ptr<PvLine> m_line;

    BoardViewerWidget* m_viewer = nullptr;  ///< 盤・拡大縮小・回転・局面送り
    PvMoveStrip* m_moveStrip = nullptr;     ///< 読み筋の手の帯（表示中の局面の手を示し、押すとその局面へ移る）
};

#endif // PVBOARDDIALOG_H
