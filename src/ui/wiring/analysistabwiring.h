#ifndef ANALYSISTABWIRING_H
#define ANALYSISTABWIRING_H

/// @file analysistabwiring.h
/// @brief 解析タブ配線クラスの定義


#include <QObject>

class QWidget;
class QTabWidget;
class EngineAnalysisTab;
class ShogiEngineThinkingModel;
class UsiCommLogModel;

// EngineAnalysisTab 周辺の生成と配線だけを担当する小さなワイヤリングクラス
class AnalysisTabWiring : public QObject
{
    Q_OBJECT
public:
    struct Deps {
        QWidget*        centralParent = nullptr;  // EngineAnalysisTab の親にする QWidget（通常は MainWindow の central）
        UsiCommLogModel* log1 = nullptr;          // USIログ(先手)
        UsiCommLogModel* log2 = nullptr;          // USIログ(後手)
    };

    explicit AnalysisTabWiring(const Deps& d, QObject* parent = nullptr);

    // 一度だけ UI を構築し、モデルを配線する（再入しても多重生成しない）
    EngineAnalysisTab* buildUiAndWire();

    // アクセサ（MainWindow 側へ返す）
    EngineAnalysisTab*           analysisTab() const { return m_analysisTab; }
    QTabWidget*                  tab()         const { return m_tab; }
    ShogiEngineThinkingModel*    thinking1()   const { return m_think1; }
    ShogiEngineThinkingModel*    thinking2()   const { return m_think2; }

signals:
    // 必要なら MainWindow はこの signal を受けて自身のスロットに繋げられます
    void branchNodeActivated(int row, int ply);

private:
    Deps                         m_d;
    EngineAnalysisTab*           m_analysisTab = nullptr;
    QTabWidget*                  m_tab         = nullptr;
    ShogiEngineThinkingModel*    m_think1      = nullptr;
    ShogiEngineThinkingModel*    m_think2      = nullptr;
};

#endif // ANALYSISTABWIRING_H
