#ifndef JOSEKIEDITCONTROLLER_H
#define JOSEKIEDITCONTROLLER_H

/// @file josekieditcontroller.h
/// @brief 定跡ウィンドウの定跡手の追加・編集・削除（入力と確認のダイアログ）

#include <QObject>
#include <QString>

#include "josekitypes.h"

class JosekiBookSession;
class QWidget;

/**
 * @brief 定跡手の追加・編集・削除の利用者操作
 *
 * 入力・確認のダイアログを出し、決まったら JosekiBookSession に書く。ダイアログを開いている間に
 * 局面が進んでも（対局中にエンジンが指すなど）、開いたときの局面（引数の targetSfen）に書く
 * （引数は呼んだときに写すので、表の局面・行を渡してよい）。
 * 読み書き中は何もしない（ダイアログを閉じた時点で読み書き中なら書かない）。
 */
class JosekiEditController : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        JosekiBookSession *session = nullptr;  ///< 開いている定跡（必須）
        QWidget *parentWidget = nullptr;       ///< ダイアログの親（必須）
    };

    explicit JosekiEditController(const Deps &deps, QObject *parent = nullptr);

    /// targetSfen の局面に定跡手を足す（指し手をダイアログで選び、登録済みなら上書きを確認する）。足したら true
    bool addMove(const QString &targetSfen);
    /// targetSfen の局面の定跡手 target の予想応手・値・コメントをダイアログで書き換える。書き換えたら true
    bool editMove(const QString &targetSfen, const JosekiMove &target);
    /// targetSfen の局面の定跡手 targetMove を、確認してから消す。消したら true
    bool deleteMove(const QString &targetSfen, const QString &targetMove);

private:
    bool isBusy() const;

    Deps m_deps;
};

#endif // JOSEKIEDITCONTROLLER_H
