#ifndef RECORDPANE_H
#define RECORDPANE_H

#include <QWidget>
#include <QTextBrowser>

class QTableView; class QPushButton; class QTextBrowser; class QSplitter; class QScrollArea;
class KifuRecordListModel; class KifuBranchListModel;
class EvaluationChartWidget;

// ★ 追加: コメント表示用のラッパ（setText を提供）
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
    EvaluationChartWidget* evalChart() const;

    void setArrowButtonsEnabled(bool on);

    QPushButton* firstButton()  const { return m_btn1; }
    QPushButton* back10Button() const { return m_btn2; }
    QPushButton* prevButton()   const { return m_btn3; }
    QPushButton* nextButton()   const { return m_btn4; }
    QPushButton* fwd10Button()  const { return m_btn5; }
    QPushButton* lastButton()   const { return m_btn6; }

    CommentTextAdapter* commentLabel();

    QPushButton* backToMainButton();

    void setupKifuSelectionAppearance();

signals:
    void mainRowChanged(int row);
    void branchActivated(const QModelIndex&);

private:
    void buildUi();
    void wireSignals();

    QTableView *m_kifu=nullptr, *m_branch=nullptr;
    QWidget *m_arrows=nullptr;
    QPushButton *m_btn1=nullptr,*m_btn2=nullptr,*m_btn3=nullptr,*m_btn4=nullptr,*m_btn5=nullptr,*m_btn6=nullptr;
    QTextBrowser* m_branchText=nullptr;
    QSplitter *m_lr=nullptr, *m_right=nullptr;
    QScrollArea* m_scroll=nullptr;
    EvaluationChartWidget* m_eval=nullptr;
    QMetaObject::Connection m_connKifuCurrentRow;

private slots:
    void onKifuRowsInserted(const QModelIndex& parent, int first, int last);
    void onKifuCurrentRowChanged(const QModelIndex& cur, const QModelIndex& prev);

private:
    QMetaObject::Connection m_connRowChanged;
    QMetaObject::Connection m_connRowsInserted;

    // ★ 追加: コメント用アダプタの実体
    CommentTextAdapter m_commentAdapter{nullptr};

public slots:
    // プレーンテキストで分岐コメント欄に反映
    void setBranchCommentText(const QString& text);

    // HTMLで分岐コメント欄に反映（将来HTML対応したくなった時用）
    void setBranchCommentHtml(const QString& html);
};

#endif // RECORDPANE_H
