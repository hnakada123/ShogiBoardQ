#ifndef RECORDPANE_H
#define RECORDPANE_H

/// @file recordpane.h
/// @brief 棋譜欄ペインクラスの定義


#include <QWidget>
#include <QTextBrowser>
#include "recordpaneappearancemanager.h"

class QTableView; class QPushButton; class QTextBrowser; class QSplitter;
class KifuRecordListModel; class KifuBranchListModel;

// コメント表示用のラッパ（setText を提供）
class CommentTextAdapter {
public:
    explicit CommentTextAdapter(QTextBrowser* tb = nullptr) : m_tb(tb) {}
    void setText(const QString& text) { if (m_tb) m_tb->setPlainText(text); }
    void setHtml(const QString& html) { if (m_tb) m_tb->setHtml(html); }
    QTextBrowser* widget() const { return m_tb; }
    void reset(QTextBrowser* tb) { m_tb = tb; }
private:
    QTextBrowser* m_tb = nullptr;
};

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

    CommentTextAdapter* commentLabel();

    QPushButton* backToMainButton();

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

    QTableView *m_kifu=nullptr, *m_branch=nullptr;
    QWidget *m_navButtons=nullptr;  // ナビゲーションボタン群（棋譜と分岐の間に縦配置）
    QWidget *m_branchContainer=nullptr;  // 分岐候補欄のコンテナ（本譜に戻るボタン用）
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
    void onKifuRowsInserted(const QModelIndex& parent, int first, int last);
    void onKifuCurrentRowChanged(const QModelIndex& cur, const QModelIndex& prev);
    void onBranchCurrentRowChanged(const QModelIndex& current, const QModelIndex& previous);
    void connectKifuCurrentRowChanged();  ///< 遅延接続用スロット

private:
    QMetaObject::Connection m_connRowChanged;
    QMetaObject::Connection m_connRowsInserted;
    QMetaObject::Connection m_connBranchCurrentRow;

    // コメント用アダプタの実体
    CommentTextAdapter m_commentAdapter{nullptr};

public slots:
    // プレーンテキストで分岐コメント欄に反映
    void setBranchCommentText(const QString& text);

    // HTMLで分岐コメント欄に反映（将来HTML対応したくなった時用）
    void setBranchCommentHtml(const QString& html);

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
