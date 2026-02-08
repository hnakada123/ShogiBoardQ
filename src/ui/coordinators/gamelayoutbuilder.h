#ifndef GAMELAYOUTBUILDER_H
#define GAMELAYOUTBUILDER_H

/// @file gamelayoutbuilder.h
/// @brief 対局画面レイアウトビルダークラスの定義


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

    QSplitter* buildHorizontalSplit();  // 左右分割を作る（将棋盤 + 棋譜欄）

private:
    Deps       m_d;
    QSplitter* m_splitter = nullptr;    // 水平Splitter
};

#endif // GAMELAYOUTBUILDER_H
