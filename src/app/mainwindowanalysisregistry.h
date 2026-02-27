#ifndef MAINWINDOWANALYSISREGISTRY_H
#define MAINWINDOWANALYSISREGISTRY_H

/// @file mainwindowanalysisregistry.h
/// @brief Analysis系（解析・検討モード・エンジン連携）の ensure* を集約するサブレジストリ

#include <QObject>

class MainWindow;
class MainWindowServiceRegistry;

/**
 * @brief Analysis系の ensure* メソッドを集約するサブレジストリ
 *
 * 責務:
 * - 解析・検討モード・エンジン連携に関する遅延初期化を一箇所に集約する
 * - カテゴリ外の ensure* 呼び出しは MainWindowServiceRegistry（ファサード）経由で解決する
 *
 * MainWindowServiceRegistry が所有し、同名メソッドへの転送で呼び出される。
 */
class MainWindowAnalysisRegistry : public QObject
{
    Q_OBJECT

public:
    explicit MainWindowAnalysisRegistry(MainWindow& mw,
                                        MainWindowServiceRegistry& registry,
                                        QObject* parent = nullptr);

    /// 読み筋クリックコントローラを生成し状態参照を設定する
    void ensurePvClickController();
    /// 検討局面サービスを生成し依存を更新する
    void ensureConsiderationPositionService();
    /// 解析結果プレゼンターを生成する
    void ensureAnalysisPresenter();
    /// 検討モード配線を生成する
    void ensureConsiderationWiring();
    /// USIコマンドコントローラを生成し依存を設定する
    void ensureUsiCommandController();

private:
    MainWindow& m_mw;                       ///< MainWindow への参照（生涯有効）
    MainWindowServiceRegistry& m_registry;  ///< ファサードへの参照（カテゴリ外呼び出し用）
};

#endif // MAINWINDOWANALYSISREGISTRY_H
