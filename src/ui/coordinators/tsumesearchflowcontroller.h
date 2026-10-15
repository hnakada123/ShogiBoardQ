#ifndef TSUMESEARCHFLOWCONTROLLER_H
#define TSUMESEARCHFLOWCONTROLLER_H

/// @file tsumesearchflowcontroller.h
/// @brief 詰み探索フローコントローラクラスの定義


#include "recordline.h"
#include <QObject>
#include <functional>

class QWidget;
class TsumeShogiSearchDialog;
class AnalysisSession;

/**
 * @brief 詰将棋探索ダイアログと探索開始処理を仲介するコントローラ
 *
 * 探索開始局面を組み立て、ユーザーのダイアログ入力に基づいて
 * 詰み探索のセッション（AnalysisSession）へ探索開始を委譲する。
 *
 */
class TsumeSearchFlowController : public QObject
{
    Q_OBJECT
public:
    /// 依存オブジェクト
    struct Deps {
        AnalysisSession* session = nullptr;  ///< 詰み探索のセッション（必須、非所有）
        RecordLine line;                    ///< 表示中の手順（分岐ツリー）。line.currentPly の局面を探索する
        QString fallbackSfen;               ///< 手順が無いときに探索する局面（空可）
        std::function<void(const QString&)> onError;  ///< エラー表示コールバック（任意）
    };

    explicit TsumeSearchFlowController(QObject* parent=nullptr);

    /// ダイアログ表示後、詰み探索を開始する
    /// @return 探索が開始された場合は true、キャンセルやエラーで中止された場合は false
    bool runWithDialog(const Deps& d, QWidget* parent);

private:
    /// 詰み探索用の`position ...`文字列を組み立てる
    QString buildPositionForMate(const Deps& d) const;
};

#endif // TSUMESEARCHFLOWCONTROLLER_H
