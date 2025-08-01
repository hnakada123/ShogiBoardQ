#ifndef TSUMESHOGISEARCHDIALOG_H
#define TSUMESHOGISEARCHDIALOG_H

#include "considarationdialog.h"

namespace Ui {
class TsumeShogiSearchDialog;
}

// 詰み探索ダイアログを表示する。
class TsumeShogiSearchDialog : public ConsidarationDialog
{
    Q_OBJECT

public:
    // コンストラクタ
    explicit TsumeShogiSearchDialog(QWidget *parent = nullptr);

    // デストラクタ
    ~TsumeShogiSearchDialog();
};

#endif // TSUMESHOGISEARCHDIALOG_H
