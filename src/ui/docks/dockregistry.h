#ifndef DOCKREGISTRY_H
#define DOCKREGISTRY_H

/// @file dockregistry.h
/// @brief メインウィンドウのドックの表と、作成・規定の配置・状態の保存と復元

#include <QList>
#include <QObject>
#include <array>

class QDockWidget;
class QMainWindow;
class QMenu;
class QWidget;

/**
 * @brief メインウィンドウのドックを1つの表（specs()）から作り、置き、状態を保存する
 *
 * ドックを足すときは DockId と表に1行足し、中身の部品を createAll() に渡す。
 * 規定の配置は表の並びで決まる（同じ場所のドックは、表で前にあるドックのタブに後ろから重ねる）。
 * 起動時の配置と「レイアウトを元に戻す」（resetToDefault()）はどちらもこの表から作る。
 * 浮動・表示・位置はすべてのドックで同じ方式で保存する（saveStates()）。
 */
class DockRegistry : public QObject
{
    Q_OBJECT

public:
    /// ドック（並びは表の並びと同じにする）
    enum class DockId {
        Record, GameInfo, Thinking, Consideration, UsiLog, CsaLog, Comment, BranchTree, EvalChart,
        Menu, Joseki, AnalysisResults, Count
    };
    static constexpr std::size_t kDockCount = static_cast<std::size_t>(DockId::Count);

    struct DockSpec {
        DockId id;
        const char* objectName;     ///< ドックの objectName（保存したレイアウト・自動化 API が使う）
        const char* title;          ///< 題名と「表示」メニューの項目（翻訳前）
        Qt::DockWidgetArea area;    ///< 規定の場所
        bool defaultVisible;        ///< 規定で表示するか
        bool raised;                ///< 規定の配置で、重ねたタブの前面に出すか
        bool layoutBoundary;        ///< 中身の大きさが頻繁に変わる（LayoutBoundaryWidget で包む）
        bool opaqueContent;         ///< 中身の描き直しをドックとウィンドウまで広げない
        int minimumHeight;          ///< ドックの最小の高さ（0 は指定しない）
        int defaultWidth;           ///< 規定の配置での幅（0 は指定しない）
        const char* settingsGroup;  ///< 状態を保存する設定のグループ
    };

    /// ドックごとの中身（DockId の順。nullptr のドックは作らない）
    using Contents = std::array<QWidget*, kDockCount>;

    DockRegistry(QMainWindow* window, QMenu* displayMenu, QObject* parent = nullptr);

    /// ドックの表（並びは規定の配置の順）
    static const std::array<DockSpec, kDockCount>& specs();
    static const DockSpec& spec(DockId id);
    static QString title(DockId id);

    /// 表の順にドックを作って規定の場所に置き、保存した状態（浮動・表示・位置）を戻す
    void createAll(const Contents& contents);

    /// 作ったドック（作っていなければ nullptr）
    QDockWidget* dock(DockId id) const;
    /// 作ったドックすべて（表の順）
    QList<QDockWidget*> docks() const;

    /// 起動時と同じ規定の配置に戻す
    void resetToDefault();
    /// すべてのドックの浮動・表示・位置を保存する
    void saveStates() const;

private:
    /// 表の順に、規定の場所へ置く（同じ場所の2つ目からは前のドックのタブに重ねる）
    void placeAll();
    /// 規定の配置で前面に出すドックを前に出す
    void raiseDefaults();

    QMainWindow* m_window = nullptr;
    QMenu* m_displayMenu = nullptr;
    std::array<QDockWidget*, kDockCount> m_docks{};
};

#endif // DOCKREGISTRY_H
