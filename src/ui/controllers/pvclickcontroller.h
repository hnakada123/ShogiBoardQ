#ifndef PVCLICKCONTROLLER_H
#define PVCLICKCONTROLLER_H

/// @file pvclickcontroller.h
/// @brief 読み筋の行をクリックしたとき、読み筋を盤で再生するダイアログを開く

#include <QHash>
#include <QObject>
#include <QString>

#include "shogienginethinkingmodel.h"

struct AppState;
class KifuNavigationState;
class ShogiView;

/**
 * @brief 思考タブ・検討タブの読み筋の行から、読み筋の局面と手順で読み筋ダイアログを開く
 *
 * 読み筋の局面は、エンジンが読み筋を出したときの局面（ShogiInfoRecord::baseSfen）を使う。
 */
class PvClickController : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        const KifuNavigationState* navState = nullptr;      ///< 表示中の手順（読み筋の局面に至った手を探す）
        const AppState* state = nullptr;                    ///< 対局者名
        const ShogiView* shogiView = nullptr;               ///< 盤の反転状態
    };

    explicit PvClickController(const Deps& deps, QObject* parent = nullptr);

signals:
    /// model の行から開いた読み筋ダイアログが閉じた（表の選択を外す）
    void pvDialogClosed(const ShogiEngineThinkingModel* model);

public slots:
    /// 思考の表（model）の row 行目の「盤面」ボタンが押された
    void onPvRowClicked(ShogiEngineThinkingModel* model, int row);

private slots:
    void onPvDialogFinished();

private:
    Deps m_deps;
    QHash<const QObject*, const ShogiEngineThinkingModel*> m_dialogSources;  ///< 開いているダイアログと元の表
};

#endif // PVCLICKCONTROLLER_H
