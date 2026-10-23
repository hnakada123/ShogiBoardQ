#ifndef PLAYERINFOPRESENTER_H
#define PLAYERINFOPRESENTER_H

/// @file playerinfopresenter.h
/// @brief 盤の対局者名の表示

#include <QObject>
#include <QString>

#include "matchplayers.h"

class AppActivityState;
class KifuDocument;
class ShogiView;

/**
 * @brief 盤の対局者名を出す（表示だけ）
 *
 * 対局中は対局者、対局していなければ対局情報の名前（初期値は「先手」「後手」）を出す。
 * 対局者と対局情報は棋譜の文書（KifuDocument）が持ち、このクラスは書き換えない。
 * 文書の知らせ（対局者・対局情報の作り直しと編集）と動作状態の変化を自分で受け、そのたびに求め直す
 * （ほかの部品は名前を書き込まない）。
 * エンジンの表示（通信ログの見出し・2つ目のエンジン欄・評価値グラフの系列名）は EngineDisplayPresenter が受け持つ。
 */
class PlayerInfoPresenter : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        const KifuDocument* document = nullptr;           ///< 対局者と対局情報（必須、非所有）
        const AppActivityState* activity = nullptr;       ///< 対局中か（必須、非所有）
        ShogiView* shogiView = nullptr;
    };

    explicit PlayerInfoPresenter(const Deps& deps, QObject* parent = nullptr);
    ~PlayerInfoPresenter() override;

private slots:
    /// 対局中なら対局者の名前を、対局していなければ対局情報の名前を盤に出す
    void refresh();

private:
    void setBoardNames(const QString& black, const QString& white);

    Deps m_deps;
};

#endif // PLAYERINFOPRESENTER_H
