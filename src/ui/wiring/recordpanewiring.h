#ifndef RECORDPANEWIRING_H
#define RECORDPANEWIRING_H

/// @file recordpanewiring.h
/// @brief 棋譜欄配線クラスの定義
/// @todo remove コメントスタイルガイド適用済み


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
