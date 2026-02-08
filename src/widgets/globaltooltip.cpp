/// @file globaltooltip.cpp
/// @brief グローバルツールチップクラスの実装
/// @todo remove コメントスタイルガイド適用済み

#include "globaltooltip.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPoint>
#include <QString>

// 軽量なツールチップ風ポップアップ（QFrame 派生）。
// 役割：指定したグローバル座標付近にプレーンテキストを表示する小さなフローティング UI を提供。
// 特徴：
//  - Qt::ToolTip | Qt::FramelessWindowHint により枠なし・フォーカス奪取なしで表示
//  - HTML を無効化し、テキストは HTML エスケープして表示（XSS 的な混入を防ぐ）
//  - setCompact(true) で余白・角丸・フォントを小さめに一括変更
GlobalToolTip::GlobalToolTip(QWidget* parent)
    : QFrame(parent, Qt::ToolTip | Qt::FramelessWindowHint)
{
    // 役割：表示してもアクティブ化しない（フォーカスを奪わないようにする）
    setAttribute(Qt::WA_ShowWithoutActivating);

    // 役割：見た目の枠はスタイルシート側で制御するためフレームは未使用
    setFrameShape(QFrame::NoFrame);
    setFrameShadow(QFrame::Plain);

    // 役割：背景・文字色・パディング・角丸の基本スタイルを指定
    setStyleSheet(
        "background-color:#FFF9C4;"
        "color:#333;"
        "padding:10px 12px;"
        "border-radius:6px;"
        );

    // 役割：単純な横並びレイアウトを作成（中身は QLabel のみ）
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(10, 10, 10, 10);

    // 役割：テキスト表示用のラベルを準備（HTML は使わない想定）
    m_label = new QLabel(this);
    m_label->setFrameStyle(QFrame::NoFrame);
    lay->addWidget(m_label);

    // 役割：初期フォントサイズを設定（後で setCompact で上書き可能）
    setPointSizeF(14.0);
}

GlobalToolTip::~GlobalToolTip() = default;

// フォントサイズ（pt）を設定するセッター。
// 役割：ラベルのフォントサイズを更新し、ウィジェットのサイズも内容に合わせて調整する。
// 注意：一部スタイル環境でフォントが上書きされる可能性があるため、StyleSheet でもサイズを明示。
void GlobalToolTip::setPointSizeF(qreal pt)
{
    QFont f = m_label->font();
    f.setPointSizeF(pt);
    m_label->setFont(f);

    // 念のため CSS でも指定（テーマによる上書き対策）
    m_label->setStyleSheet(QStringLiteral("font-size:%1pt;").arg(pt));

    // 内容に合わせて自身のサイズを更新
    adjustSize();
}

// 指定位置にプレーンテキストのツールチップを表示する。
// 役割：文字列を HTML エスケープして安全に表示し、カーソルの右下へ少しオフセットして配置。
// 注意：引数 globalPos はスクリーン座標（グローバル座標）を想定。
void GlobalToolTip::showText(const QPoint& globalPos, const QString& plainText)
{
    // HTML 無効化（素の文字列として表示）
    m_label->setText(plainText.toHtmlEscaped());
    adjustSize();

    // カーソルを覆わないよう、少し右下にずらして表示
    move(globalPos + QPoint(12, 16));
    show();
}

// ツールチップを非表示にする。
// 役割：単純なラッパ（将来的にアニメーション等を入れる場合のフックにもなる）。
void GlobalToolTip::hideTip()
{
    hide();
}

// コンパクト表示の切り替え。
// 役割：フォント・余白・角丸を小さめに調整し、全体の見た目を詰める。
// 注意：レイアウトの ContentsMargins とフレームの padding を両方更新しないと充分に縮まらない。
void GlobalToolTip::setCompact(bool on)
{
    // 小さめ設定（フォント/余白/角丸）
    const int   padV   = on ? 4 : 6;       // 上下 padding
    const int   padH   = on ? 4 : 6;       // 左右 padding
    const int   radius = on ? 4 : 6;       // 角丸半径
    const qreal pt     = on ? 12.0 : 14.0; // フォントサイズ

    // フォント更新（内部で adjustSize() も実行）
    setPointSizeF(pt);

    // レイアウト余白の調整（これが無いと見た目が詰まらない）
    if (auto* lay = qobject_cast<QHBoxLayout*>(layout()))
        lay->setContentsMargins(padH, padV, padH, padV);

    // フレームの padding/角丸を更新（背景・文字色は据え置き）
    setStyleSheet(QStringLiteral(
                      "background-color:#FFF9C4;"
                      "color:#333;"
                      "padding:%1px %2px;"
                      "border-radius:%3px;"
                      ).arg(padV).arg(padH).arg(radius));

    adjustSize();
}
