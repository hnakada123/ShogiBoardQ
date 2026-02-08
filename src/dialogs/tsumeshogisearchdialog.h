#ifndef TSUMESHOGISEARCHDIALOG_H
#define TSUMESHOGISEARCHDIALOG_H

/// @file tsumeshogisearchdialog.h
/// @brief 詰将棋検索ダイアログクラスの定義
/// @todo remove コメントスタイルガイド適用済み


#include "considerationdialog.h"

namespace Ui {
class TsumeShogiSearchDialog;
}

// 詰み探索ダイアログを表示する。
class TsumeShogiSearchDialog : public ConsiderationDialog
{
    Q_OBJECT

public:
    // コンストラクタ
    explicit TsumeShogiSearchDialog(QWidget *parent = nullptr);

    // デストラクタ
    ~TsumeShogiSearchDialog() override;
};

#endif // TSUMESHOGISEARCHDIALOG_H
