#ifndef RECORDPANEWIRING_H
#define RECORDPANEWIRING_H

#include <QObject>

class QWidget;
class RecordPane;
class KifuRecordListModel;
class KifuBranchListModel;

class RecordPaneWiring : public QObject {
    Q_OBJECT
public:
    struct Deps {
        QWidget*             parent = nullptr;         // 親（通常は MainWindow の central）
        QObject*             ctx = nullptr;            // RecordPane::mainRowChanged などを受ける QObject（MainWindow*）
        KifuRecordListModel* recordModel = nullptr;    // 棋譜モデル
        KifuBranchListModel* branchModel = nullptr;    // 分岐候補モデル
    };

    explicit RecordPaneWiring(const Deps& d, QObject* parent=nullptr);

    // RecordPane の生成と配線を行う
    void buildUiAndWire();

    // 生成物の取得
    RecordPane* pane() const { return m_pane; }

private:
    Deps        m_d;
    RecordPane* m_pane = nullptr;
};

#endif // RECORDPANEWIRING_H
