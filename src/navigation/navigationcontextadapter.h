#ifndef NAVIGATIONCONTEXTADAPTER_H
#define NAVIGATIONCONTEXTADAPTER_H

#include "navigationcontext.h"
#include <QObject>
#include <QVector>
#include <functional>

class KifuRecordListModel;
class KifuLoadCoordinator;
class RecordNavigationController;
class RecordPane;
class ReplayController;
struct ResolvedRow;

/**
 * @brief INavigationContextの実装を担当するアダプタークラス
 *
 * MainWindowからINavigationContextの実装責務を分離する。
 * 必要なデータへのアクセスはコールバック経由で行う。
 */
class NavigationContextAdapter : public QObject, public INavigationContext
{
    Q_OBJECT

public:
    /**
     * @brief 依存オブジェクトとデータへの参照
     */
    struct Deps {
        const QVector<ResolvedRow>* resolvedRows = nullptr;
        const int* activeResolvedRow = nullptr;
        QStringList* const* sfenRecord = nullptr;       // pointer to pointer (tracks changes)
        KifuRecordListModel* const* kifuRecordModel = nullptr;  // pointer to pointer
        KifuLoadCoordinator* const* kifuLoadCoordinator = nullptr;  // pointer to pointer (ライブ対局状態参照用)
        RecordNavigationController* const* recordNavController = nullptr;  // pointer to pointer
        RecordPane* const* recordPane = nullptr;        // pointer to pointer
        ReplayController* const* replayController = nullptr;  // pointer to pointer
        const int* activePly = nullptr;
        const int* currentSelectedPly = nullptr;
        QString* currentSfenStr = nullptr;
    };

    /**
     * @brief コールバック関数群
     */
    struct Callbacks {
        std::function<void()> updateJosekiWindow;
        std::function<void()> ensureRecordNavController;
    };

    explicit NavigationContextAdapter(QObject* parent = nullptr);

    /**
     * @brief 依存オブジェクトを設定する
     */
    void setDeps(const Deps& deps);

    /**
     * @brief コールバックを設定する
     */
    void setCallbacks(const Callbacks& callbacks);

    // INavigationContext implementation
    bool hasResolvedRows() const override;
    int resolvedRowCount() const override;
    int activeResolvedRow() const override;
    int maxPlyAtRow(int row) const override;
    int currentPly() const override;
    void applySelect(int row, int ply) override;

signals:
    /**
     * @brief 位置が変更された時に発火するシグナル
     * @param row 行番号（分岐インデックス）
     * @param ply 手数
     * @param sfen 現在局面のSFEN（空の場合あり）
     */
    void positionChanged(int row, int ply, const QString& sfen);

private:
    Deps m_deps;
    Callbacks m_callbacks;
};

#endif // NAVIGATIONCONTEXTADAPTER_H
