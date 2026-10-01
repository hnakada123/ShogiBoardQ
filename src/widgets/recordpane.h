#ifndef RECORDPANE_H
#define RECORDPANE_H

/// @file recordpane.h
/// @brief 棋譜欄ペインクラスの定義


#include <QWidget>
#include <QPersistentModelIndex>
#include "recordpaneappearancemanager.h"

class QTableView; class QPushButton; class QSplitter; class QToolButton;
class KifuRecordListModel; class KifuBranchListModel;

class RecordPane : public QWidget {
    Q_OBJECT
public:
    explicit RecordPane(QWidget* parent=nullptr);
    void setModels(KifuRecordListModel* recModel, KifuBranchListModel* brModel);

    QTableView* kifuView() const;
    QTableView* branchView() const;

    void setArrowButtonsEnabled(bool on);
    void setKifuViewEnabled(bool on);
    void setNavigationEnabled(bool on);
    bool isNavigationDisabled() const { return m_navigationDisabled; }

    QPushButton* firstButton()  const { return m_btn1; }
    QPushButton* back10Button() const { return m_btn2; }
    QPushButton* prevButton()   const { return m_btn3; }
    QPushButton* nextButton()   const { return m_btn4; }
    QPushButton* fwd10Button()  const { return m_btn5; }
    QPushButton* lastButton()   const { return m_btn6; }

    QPushButton* fontIncreaseButton() const { return m_btnFontUp; }
    QPushButton* fontDecreaseButton() const { return m_btnFontDown; }
    QPushButton* bookmarkEditButton() const { return m_btnBookmarkEdit; }

    void setupKifuSelectionAppearance();
    void setupBranchViewSelectionAppearance();

signals:
    void mainRowChanged(int row);
    void branchActivated(const QModelIndex&);
    void bookmarkEditRequested();

private:
    void buildUi();
    void buildKifuTable();
    void buildToolButtons();
    void buildNavigationPanel();
    void buildBranchPanel();
    void buildMainLayout();
    void wireSignals();
    void updateKifuTableWidth();
    void updateBranchAppearance();

    QTableView *m_kifu=nullptr, *m_branch=nullptr;
    QWidget *m_navButtons=nullptr;  // ナビゲーションボタン群（棋譜と分岐の間に縦配置）
    QWidget *m_branchContainer=nullptr;  // 分岐候補欄のコンテナ
    QToolButton* m_branchToggle = nullptr;
    QPushButton *m_btn1=nullptr,*m_btn2=nullptr,*m_btn3=nullptr,*m_btn4=nullptr,*m_btn5=nullptr,*m_btn6=nullptr;
    QPushButton *m_btnFontUp=nullptr, *m_btnFontDown=nullptr;  // 文字サイズ変更ボタン
    QPushButton *m_btnBookmarkEdit=nullptr;  // しおり編集ボタン
    QPushButton *m_btnToggleTime=nullptr;      // 消費時間列トグル
    QPushButton *m_btnToggleBookmark=nullptr;  // しおり列トグル
    QPushButton *m_btnToggleComment=nullptr;   // コメント列トグル
    QSplitter *m_lr=nullptr;
    QMetaObject::Connection m_connKifuCurrentRow;

    RecordPaneAppearanceManager m_appearanceManager{10};

private slots:
    void onBranchToggled(bool expanded);
    void onKifuRowsInserted(const QModelIndex& parent, int first, int last);
    void onKifuCurrentRowChanged(const QModelIndex& cur, const QModelIndex& prev);
    void onBranchCurrentRowChanged(const QModelIndex& current, const QModelIndex& previous);
    void connectKifuCurrentRowChanged();  ///< currentRowChanged 接続を再構築

    /// 分岐候補欄のマウスクリック（QAbstractItemView::clicked）
    void onBranchClicked(const QModelIndex& index);
    /// 分岐候補欄の活性化（QAbstractItemView::activated: Enter キー、または環境によってはシングルクリック）
    void onBranchActivated(const QModelIndex& index);
    /// clicked 直後の activated 二重発火ガードを解除する
    void clearBranchClickGuard();

private:
    QMetaObject::Connection m_connRowChanged;
    QMetaObject::Connection m_connRowsInserted;
    QMetaObject::Connection m_connBranchCurrentRow;

    // シングルクリックで activated も発火する環境で、同じ行に対する
    // clicked → activated の二重処理を防ぐ（同一イベントループ周回内のみ有効）
    QPersistentModelIndex m_lastClickedBranchIndex;
    bool m_branchClickGuard = false;

public slots:
    // 文字サイズ変更スロット
    void onFontIncrease(bool checked = false);
    void onFontDecrease(bool checked = false);

    // 列表示トグルスロット
    void onToggleTimeColumn(bool checked);
    void onToggleBookmarkColumn(bool checked);
    void onToggleCommentColumn(bool checked);

private:
    // 対局中フラグ（ナビゲーション無効化時にtrue）
    bool m_navigationDisabled = false;
};

#endif // RECORDPANE_H
