#ifndef PVBOARDDIALOG_H
#define PVBOARDDIALOG_H

/// @file pvboarddialog.h
/// @brief 読み筋盤面ダイアログクラスの定義


#include <QDialog>
#include <QStringList>
#include <memory>
#include "shogiview.h"

class BoardViewerWidget;
class QLabel;
class PvLine;

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
    /// 起動時の局面に至る前の局面（直前の手が分からないとき、盤の差分からハイライトを決める）
    void setPrevSfenForHighlight(const QString& prevSfen);

    /**
     * @brief 盤面の反転モードを設定（GUI本体の向きに合わせる）
     * @param flipped trueで反転表示
     */
    void setFlipMode(bool flipped);

private slots:
    /// 局面送りで表示する手数が変わった
    void onPlyChanged(int ply);

private:
    /// UIを構築
    void buildUi();
    /// 手数ラベルと直前の手のハイライトを更新
    void updatePlyDisplay();

private:
    std::unique_ptr<PvLine> m_line;
    QString m_kanjiPv;     ///< エンジンの読み筋の漢字表記（日本語表記のときの読み筋の欄）
    QString m_lastMove;    ///< 起動時の局面に至った手
    QString m_prevSfen;    ///< 起動時の局面に至る前の局面

    BoardViewerWidget* m_viewer = nullptr;  ///< 盤・拡大縮小・回転・局面送り
    QLabel* m_pvLabel = nullptr;
};

#endif // PVBOARDDIALOG_H
