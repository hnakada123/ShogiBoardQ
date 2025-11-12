#ifndef GAMELAYOUTBUILDER_H
#define GAMELAYOUTBUILDER_H

#include <QObject>

class QWidget;
class QVBoxLayout;
class QSplitter;
class QTabWidget;

class GameLayoutBuilder : public QObject {
    Q_OBJECT
public:
    struct Deps {
        QWidget*     shogiView = nullptr;          // 左ペイン
        QWidget*     recordPaneOrWidget = nullptr; // 右ペイン（RecordPane）
        QTabWidget*  analysisTabWidget = nullptr;  // 下段タブ（EngineAnalysisTab 等）
    };

    explicit GameLayoutBuilder(const Deps& d, QObject* parent=nullptr);

    QSplitter* buildHorizontalSplit();                   // 左右分割を作る
    void       attachToCentralLayout(QVBoxLayout* vbox); // 中央レイアウトへ貼る

    QSplitter* splitter() const { return m_splitter; }

private:
    Deps       m_d;
    QSplitter* m_splitter = nullptr;
};

#endif // GAMELAYOUTBUILDER_H
