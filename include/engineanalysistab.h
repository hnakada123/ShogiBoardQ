#ifndef ENGINEANALYSISTAB_H
#define ENGINEANALYSISTAB_H

#include <QWidget>
class QTabWidget; class QTableView; class ShogiEngineThinkingModel; class QTextBrowser; class QPlainTextEdit;
class EngineInfoWidget; class QGraphicsView; class UsiCommLogModel; class QSplitter; class QAbstractItemModel;

class EngineAnalysisTab : public QWidget {
    Q_OBJECT
public:
    explicit EngineAnalysisTab(QWidget* parent=nullptr);

    void setModels(ShogiEngineThinkingModel* m1, ShogiEngineThinkingModel* m2,
                   UsiCommLogModel* log1, UsiCommLogModel* log2);

    QTabWidget* tab() const;

    void setEngine1ThinkingModel(QAbstractItemModel* m);
    void setEngine2ThinkingModel(QAbstractItemModel* m);
    void setDualEngineVisible(bool on);

public slots:
    void setAnalysisVisible(bool on);
    void setSecondEngineVisible(bool on);
    void setCommentText(const QString& text);

private:
    void buildUi();

    QPlainTextEdit* m_usiLog=nullptr;
    QTextBrowser* m_comment=nullptr;
    QGraphicsView* m_branchTree=nullptr;

    // 既存フィールドに加えて以下を追加
    QTabWidget*    m_tab = nullptr;

    // エンジン1/2の UI パーツ（既存）
    EngineInfoWidget* m_info1 = nullptr;
    EngineInfoWidget* m_info2 = nullptr;
    QTableView*       m_view1 = nullptr;
    QTableView*       m_view2 = nullptr;

    // ★ 追加: 同一タブ内の上下分割管理
    QSplitter* m_engineSplit = nullptr;
    QWidget*   m_engine1Pane = nullptr;
    QWidget*   m_engine2Pane = nullptr;
};

#endif // ENGINEANALYSISTAB_H
