#ifndef JOSEKIREGISTRATIONCONTROLLER_H
#define JOSEKIREGISTRATIONCONTROLLER_H

/// @file josekiregistrationcontroller.h
/// @brief 思考の表・棋譜解析の結果の表の右クリックから、エンジンの読み筋を定跡に登録する

#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

#include "usiscore.h"

class JosekiBookSession;
class KifuAnalysisListModel;
class QMenu;
class QStatusBar;
class QWidget;
class ShogiEngineThinkingModel;
struct KifuAnalysisRow;

/**
 * @brief エンジンの読み筋の最初の手を、定跡ウィンドウで開いている定跡に登録する利用者操作
 *
 * 思考の表（対局・検討タブ）と棋譜解析の結果の表の右クリックのメニューに項目を足す。
 * 登録は JosekiBookSession::registerEnginePv（読み筋の2手目を予想応手にし、登録済みの手は評価値・深さ・
 * 予想応手だけを書き換える）。定跡ウィンドウで前回のファイルをまだ読み込んでいないときや読み書き中は、
 * 項目を押せなくする（canRegister）。
 * メニューを開いた時点の行の値を覚えて登録する（思考の表は探索中に行が入れ替わるため）。
 */
class JosekiRegistrationController : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        JosekiBookSession* book = nullptr;                    ///< 登録先（必須）
        std::function<bool()> canRegister;                    ///< 定跡に登録できるか（必須）
        const KifuAnalysisListModel* analysisModel = nullptr; ///< 棋譜解析の結果（まとめて登録する）
        QWidget* parentWidget = nullptr;                      ///< ダイアログとメッセージの親
        QStatusBar* statusBar = nullptr;                      ///< 登録した結果を出す（無くてもよい）
    };

    /// 登録する読み筋（局面と、その局面の手番側から見た評価値）
    struct Target {
        QString sfen;
        QStringList pv;
        UsiScore score;
        int depth = -1;
        bool isValid() const { return !sfen.isEmpty() && !pv.isEmpty(); }
    };

    /// まとめて登録した結果
    struct BatchResult {
        int added = 0;
        int updated = 0;
    };

    explicit JosekiRegistrationController(const Deps& deps, QObject* parent = nullptr);

    /// 棋譜解析の結果の1行を登録する読み筋にする（評価値は先手から見た値なので、局面の手番から見た値に直す）
    static Target targetOf(const KifuAnalysisRow& row);
    /// 棋譜解析の結果のうち、手数の範囲と手番に合う局面の読み筋をまとめて登録する
    BatchResult registerAnalysisRows(int fromPly, int toPly, bool black, bool white);

public slots:
    /// 思考の表の行の右クリック（ThinkingPanel・ConsiderationTabManager::rowContextMenuAboutToShow）
    void onThinkingRowContextMenu(ShogiEngineThinkingModel* model, int row, QMenu* menu);
    /// 棋譜解析の結果の表の行の右クリック（AnalysisResultsPanel::rowContextMenuAboutToShow）
    void onAnalysisRowContextMenu(int row, QMenu* menu);

private slots:
    /// メニューを開いた行の読み筋を登録する
    void registerMenuTarget();
    /// 「結果をまとめて定跡に登録...」
    void registerAnalysisResults();

private:
    /// menu に「読み筋の最初の手を定跡に登録」を足す（target を覚える）
    void addRegisterAction(QMenu* menu, const Target& target);
    bool canRegister() const;
    void showStatus(const QString& text) const;

    Deps m_deps;
    Target m_menuTarget;  ///< 開いているメニューの行の読み筋
};

#endif // JOSEKIREGISTRATIONCONTROLLER_H
