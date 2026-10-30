#ifndef RECORDPANE_H
#define RECORDPANE_H

/// @file recordpane.h
/// @brief 棋譜欄ペインクラスの定義


#include <QString>
#include <QWidget>
#include <QPersistentModelIndex>
#include "fontsizehelper.h"

class QTableView; class QPushButton; class QSplitter; class QToolButton;
class KifuRecordListModel; class KifuBranchListModel; class StepButtons;

class RecordPane : public QWidget {
    Q_OBJECT
public:
    /// 棋譜欄の見た目（文字の大きさ・表示する列・行のクリック）。applyAppearance() の1か所で表に反映する
    struct Appearance {
        int fontSize = 10;
        bool timeVisible = true;
        bool bookmarkVisible = true;
        bool commentVisible = true;
        bool navigationEnabled = true;   ///< 行のクリックで局面を移れる（対局中は無効）
    };

    explicit RecordPane(QWidget* parent=nullptr);
    void setModels(KifuRecordListModel* recModel, KifuBranchListModel* brModel);

    QTableView* kifuView() const;
    QTableView* branchView() const;

    /// 棋譜欄の行のクリックを有効・無効にする（局面送りのボタンは KifuNavigationController の QAction に従う）
    void setKifuViewEnabled(bool on);
    bool isNavigationDisabled() const { return !m_appearance.navigationEnabled; }

    /// 棋譜欄・分岐候補欄の表のスタイルシート（見出しの文字の大きさと選択の色）
    static QString tableStyleSheet(int fontSize);

    /// 局面送りのボタン（KifuNavigationController::bindButtons で QAction に結び付ける）
    StepButtons* stepButtons() const { return m_stepButtons; }

    QPushButton* fontIncreaseButton() const { return m_btnFontUp; }
    QPushButton* fontDecreaseButton() const { return m_btnFontDown; }
    QPushButton* bookmarkEditButton() const { return m_btnBookmarkEdit; }

signals:
    void mainRowChanged(int row);
    /// 分岐候補欄の行を選んだ（クリック・Enter）
    void branchRowActivated(int row);
    /// 分岐候補欄の右クリック（row は候補の行、行が無ければ -1）
    void branchContextMenuRequested(int row, const QPoint& globalPos);
    void bookmarkEditRequested();

private:
    void buildUi();
    void buildKifuTable();
    void buildToolButtons();
    void buildNavigationPanel();
    void buildBranchPanel();
    void buildMainLayout();
    void wireSignals();
    /// m_appearance を表・列・分岐候補の開閉ボタンに反映する
    void applyAppearance();
    void updateKifuTableWidth();
    void updateBranchAppearance();

    QTableView *m_kifu=nullptr, *m_branch=nullptr;
    QWidget *m_navButtons=nullptr;  // ナビゲーションボタン群（棋譜と分岐の間に縦配置）
    QWidget *m_branchContainer=nullptr;  // 分岐候補欄のコンテナ
    QToolButton* m_branchToggle = nullptr;
    StepButtons* m_stepButtons = nullptr;  ///< 局面送りの6つのボタン（縦）
    QPushButton *m_btnFontUp=nullptr, *m_btnFontDown=nullptr;  // 文字サイズ変更ボタン
    QPushButton *m_btnBookmarkEdit=nullptr;  // しおり編集ボタン
    QPushButton *m_btnToggleTime=nullptr;      // 消費時間列トグル
    QPushButton *m_btnToggleBookmark=nullptr;  // しおり列トグル
    QPushButton *m_btnToggleComment=nullptr;   // コメント列トグル
    QSplitter *m_lr=nullptr;

    FontSizeHelper m_fontSize;  ///< 棋譜と分岐候補の文字の大きさ（変更のたびに保存する）
    Appearance m_appearance;

private slots:
    void onBranchToggled(bool expanded);
    /// 文字の大きさが変わった（A-/A+）
    void onFontSizeChanged(int size);
    void onKifuRowsInserted(const QModelIndex& parent, int first, int last);
    void onKifuCurrentRowChanged(const QModelIndex& cur, const QModelIndex& prev);

    /// 分岐候補欄のマウスクリック（QAbstractItemView::clicked）
    void onBranchClicked(const QModelIndex& index);
    /// 分岐候補欄の活性化（QAbstractItemView::activated: Enter キー、または環境によってはシングルクリック）
    void onBranchActivated(const QModelIndex& index);
    /// clicked 直後の activated 二重発火ガードを解除する
    void clearBranchClickGuard();
    /// 分岐候補欄の右クリック
    void onBranchContextMenuRequested(const QPoint& pos);

private:
    // シングルクリックで activated も発火する環境で、同じ行に対する
    // clicked → activated の二重処理を防ぐ（同一イベントループ周回内のみ有効）
    QPersistentModelIndex m_lastClickedBranchIndex;
    bool m_branchClickGuard = false;

public slots:
    // 列の表示（列のボタン。表示は保存する）
    void onToggleTimeColumn(bool checked);
    void onToggleBookmarkColumn(bool checked);
    void onToggleCommentColumn(bool checked);
};

#endif // RECORDPANE_H
