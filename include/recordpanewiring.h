#ifndef RECORDPANEWIRING_H
#define RECORDPANEWIRING_H

#include <QObject>

class QWidget;
class RecordPane;
class KifuRecordListModel;
class KifuBranchListModel;

// ★ 追加: 前方宣言（ヘッダ内でポインタ型として使うだけならこれでOK）
class NavigationController;
class INavigationContext;

class RecordPaneWiring : public QObject {
    Q_OBJECT
public:
    struct Deps {
        QWidget*             parent = nullptr;         // 親（通常は MainWindow の central）
        QObject*             ctx = nullptr;            // RecordPane::mainRowChanged などを受ける QObject（MainWindow*）
        INavigationContext*  navCtx = nullptr;         // NavigationController 用の INavigationContext*
        KifuRecordListModel* recordModel = nullptr;    // 棋譜モデル
        KifuBranchListModel* branchModel = nullptr;    // 分岐候補モデル
    };

    explicit RecordPaneWiring(const Deps& d, QObject* parent=nullptr);

    // RecordPane の生成と配線を行う
    void buildUiAndWire();

    // 生成物の取得
    RecordPane*           pane() const { return m_pane; }
    NavigationController* nav()  const { return m_nav;  }

private:
    Deps                  m_d;
    RecordPane*           m_pane = nullptr;
    NavigationController* m_nav  = nullptr;
};

#endif // RECORDPANEWIRING_H
