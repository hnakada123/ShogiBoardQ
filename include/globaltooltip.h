#ifndef GLOBALTOOLTIP_H
#define GLOBALTOOLTIP_H
#pragma once

#include <QFrame>

class QLabel;
class QPoint;
class QString;

/*
 * GlobalToolTip
 * 軽量なツールチップ風ポップアップ（QFrame 派生）。
 * 役割：任意のグローバル座標付近にプレーンテキストを表示する小さなフローティング UI を提供。
 * 特徴：
 *  - Qt::ToolTip | Qt::FramelessWindowHint（実装側）により枠なし・フォーカス奪取なしで表示
 *  - HTML は無効化し、文字列は HTML エスケープして安全に表示（XSS 対策）
 *  - setCompact(true) で余白・角丸・フォントを小さめに一括変更可能
 * 注意：シグナル/スロットは持たない。必要に応じて Q_OBJECT 化やアニメーションの拡張を検討。
 */
class GlobalToolTip : public QFrame {
public:
    // コンストラクタ
    // 役割：基本スタイルと内部レイアウト（QLabel）を構築。
    explicit GlobalToolTip(QWidget* parent = nullptr);

    // デストラクタ
    ~GlobalToolTip() override;

    // ツールチップを表示
    // 役割：スクリーン座標 globalPos の少し右下に、plainText をプレーン表示する。
    // 注意：plainText は実装側で toHtmlEscaped() され、HTML は無効化される。
    void showText(const QPoint& globalPos, const QString& plainText);

    // ツールチップを非表示
    // 役割：単純なラッパ。将来的なフェード等のフックとしても利用可。
    void hideTip();

    // コンパクト表示の切り替え（既定：true）
    // 役割：フォントサイズ・余白・角丸を小さめに調整して、全体の見た目を詰める。
    void setCompact(bool on = true);

    // 文字サイズ（pt）を直接指定
    // 役割：任意のポイントサイズに変更し、内容に合わせてウィジェットのサイズも更新する。
    // 注意：一部スタイル環境でフォントが上書きされるため、実装側で StyleSheet も併用している。
    void setPointSizeF(qreal pt);

private:
    // テキスト表示用の内部ラベル（所有：this）
    QLabel* m_label = nullptr;
};

#endif // GLOBALTOOLTIP_H

