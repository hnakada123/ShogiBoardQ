/// @file tsumeshogisearchdialog.cpp
/// @brief 詰将棋検索ダイアログクラスの実装

#include "tsumeshogisearchdialog.h"

// 詰み探索ダイアログを表示する。
TsumeShogiSearchDialog::TsumeShogiSearchDialog(QWidget *parent)
    : ConsiderationDialog(parent)
{
    // ウィンドウタイトルを「詰み探索」に設定する。
    setWindowTitle(tr("詰み探索"));
}

TsumeShogiSearchDialog::~TsumeShogiSearchDialog()
{
}
