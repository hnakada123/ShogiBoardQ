#ifndef UIACTIONSWIRING_H
#define UIACTIONSWIRING_H

/// @file uiactionswiring.h
/// @brief メニュー/ツールバーアクションのシグナル/スロット接続クラスの定義

#include <QObject>

class ShogiView;
namespace Ui { class MainWindow; }

/**
 * @brief メニューやツールバーのQActionをMainWindowスロットに接続するクラス
 *
 * UIアクション（ファイル操作、対局、盤操作、解析など）の
 * シグナル/スロット接続を一括で行う。
 *
 * @todo remove コメントスタイルガイド適用済み
 */
class UiActionsWiring : public QObject {
    Q_OBJECT
public:
    /// 依存オブジェクト
    /// @todo remove コメントスタイルガイド適用済み
    struct Deps {
        Ui::MainWindow* ui = nullptr;       ///< UIオブジェクト
        ShogiView*      shogiView = nullptr; ///< 盤面ビュー（拡大/縮小を直結）
        QObject*        ctx = nullptr;       ///< 受け側オブジェクト（MainWindow*）
    };

    explicit UiActionsWiring(const Deps& d, QObject* parent=nullptr)
        : QObject(parent), m_d(d) {}

    /// 全アクションのシグナル/スロット接続を実行する（多重接続を避けるため1回だけ呼ぶ）
    /// @todo remove コメントスタイルガイド適用済み
    void wire();

private:
    Deps m_d;
};

#endif // UIACTIONSWIRING_H
