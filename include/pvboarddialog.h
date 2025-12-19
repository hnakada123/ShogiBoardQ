#ifndef PVBOARDDIALOG_H
#define PVBOARDDIALOG_H

#include <QDialog>
#include <QStringList>
#include <QVector>

class ShogiView;
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
    ~PvBoardDialog();

    /**
     * @brief 漢字表記の読み筋を設定
     * @param kanjiPv 漢字形式の読み筋（例: "▲７六歩△３四歩..."）
     */
    void setKanjiPv(const QString& kanjiPv);

private slots:
    /// 最初に戻る
    void onGoFirst();
    /// 1手戻る
    void onGoBack();
    /// 1手進む
    void onGoForward();
    /// 最後まで進む
    void onGoLast();

private:
    /// UIを構築
    void buildUi();
    /// ボタンの有効/無効を更新
    void updateButtonStates();
    /// 盤面を現在の手数で更新
    void updateBoardDisplay();
    /// SFEN形式の手を盤面に適用
    void applyMove(const QString& usiMove);
    /// 手番を取得（"b" or "w"）
    QString currentTurn() const;

private:
    ShogiView* m_shogiView = nullptr;
    ShogiBoard* m_board = nullptr;

    QString m_baseSfen;           ///< 開始局面のSFEN
    QStringList m_pvMoves;        ///< USI形式の読み筋手
    QString m_kanjiPv;            ///< 漢字表記の読み筋

    QVector<QString> m_sfenHistory; ///< 各手数での局面SFEN履歴

    int m_currentPly = 0;         ///< 現在の手数（0=開始局面）

    // UI要素
    QPushButton* m_btnFirst = nullptr;
    QPushButton* m_btnBack = nullptr;
    QPushButton* m_btnForward = nullptr;
    QPushButton* m_btnLast = nullptr;
    QLabel* m_plyLabel = nullptr;
    QLabel* m_pvLabel = nullptr;
};

#endif // PVBOARDDIALOG_H
