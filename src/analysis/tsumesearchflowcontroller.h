#ifndef TSUMESEARCHFLOWCONTROLLER_H
#define TSUMESEARCHFLOWCONTROLLER_H

/// @file tsumesearchflowcontroller.h
/// @brief 詰み探索フローコントローラクラスの定義
/// @todo remove コメントスタイルガイド適用済み


#include <QObject>
#include <functional>

class QWidget;
class TsumeShogiSearchDialog;
class MatchCoordinator;

/**
 * @brief 詰将棋探索ダイアログと探索開始処理を仲介するコントローラ
 *
 * 探索開始局面を組み立て、ユーザーのダイアログ入力に基づいて
 * MatchCoordinatorへ探索開始を委譲する。
 *
 * @todo remove コメントスタイルガイド適用済み
 */
class TsumeSearchFlowController : public QObject
{
    Q_OBJECT
public:
    /// 依存オブジェクト
    /// @todo remove コメントスタイルガイド適用済み
    struct Deps {
        MatchCoordinator* match = nullptr;  ///< 司令塔（必須、非所有）
        const QStringList* sfenRecord = nullptr;  ///< 棋譜のSFEN列（任意、非所有）
        QString            startSfenStr;          ///< 開始SFEN（空可）
        QStringList        positionStrList;       ///< 既存`position ...`列
        int                currentMoveIndex = 0;  ///< 現在の行
        const QStringList* usiMoves = nullptr;    ///< USI形式指し手列（任意、非所有）
        QString            startPositionCmd;      ///< 開始局面コマンド（`startpos`または`sfen ...`）
        std::function<void(const QString&)> onError;  ///< エラー表示コールバック（任意）
    };

    /// @todo remove コメントスタイルガイド適用済み
    explicit TsumeSearchFlowController(QObject* parent=nullptr);

    /// ダイアログ表示後、詰み探索を開始する
    /// @todo remove コメントスタイルガイド適用済み
    void runWithDialog(const Deps& d, QWidget* parent);

private:
    /// 詰み探索用の`position ...`文字列を組み立てる
    QString buildPositionForMate(const Deps& d) const;

    /// MatchCoordinatorへ詰み探索開始を委譲する
    void startAnalysis(MatchCoordinator* match,
                        const QString& enginePath,
                        const QString& engineName,
                        const QString& positionStr,
                        int byoyomiMs);
};

#endif // TSUMESEARCHFLOWCONTROLLER_H
