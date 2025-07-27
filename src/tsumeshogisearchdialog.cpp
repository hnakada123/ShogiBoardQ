#include "tsumeshogisearchdialog.h"

// 詰み探索ダイアログを表示する。
// コンストラクタ
TsumeShogiSearchDialog::TsumeShogiSearchDialog(QWidget *parent)
    : ConsidarationDialog(parent)
{
    // ウィンドウタイトルを「詰み探索」に設定する。
    setWindowTitle(tr("詰み探索"));
}

// デストラクタ
TsumeShogiSearchDialog::~TsumeShogiSearchDialog()
{
}
