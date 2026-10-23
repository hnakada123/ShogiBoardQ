#ifndef TSUMESEARCHCONTROLLER_H
#define TSUMESEARCHCONTROLLER_H

/// @file tsumesearchcontroller.h
/// @brief 詰み探索（メニューの「詰み探索」: 条件ダイアログを出して表示中の局面を探索する）

#include <QObject>
#include <optional>

#include "analysisoptions.h"

class AnalysisSession;
class KifuNavigationState;
class QWidget;

/**
 * @brief 表示中の局面で詰み探索の条件を選ばせ、検討・詰み探索のセッションで探索を始める
 *
 * 探索中の表示とメニューの状態は、セッションが動作状態を「詰み探索中」にすることで決まる。
 * エラーは ErrorBus で知らせる。
 */
class TsumeSearchController : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        QWidget* parentWidget = nullptr;                ///< 条件ダイアログの親
        const KifuNavigationState* navState = nullptr;  ///< 探索する局面（表示中の手。必須）
        AnalysisSession* session = nullptr;             ///< 探索するセッション（必須）
    };

    explicit TsumeSearchController(const Deps& deps, QObject* parent = nullptr);

    /// 表示中の局面を詰み探索する position コマンド（局面が無ければ空）
    std::optional<UsiPosition> positionForMate() const;

public slots:
    /// 条件ダイアログを出し、選んだ条件で探索を始める（ダイアログを閉じてから始める）
    void showDialogAndStart();

private slots:
    void startPendingSearch();

private:
    Deps m_deps;
    AnalysisOptions m_pending;   ///< ダイアログで選んだ条件（ダイアログを閉じた後に始める）
};

#endif // TSUMESEARCHCONTROLLER_H
