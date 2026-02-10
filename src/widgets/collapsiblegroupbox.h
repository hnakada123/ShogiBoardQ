#ifndef COLLAPSIBLEGROUPBOX_H
#define COLLAPSIBLEGROUPBOX_H

/// @file collapsiblegroupbox.h
/// @brief 折りたたみ可能グループボックスクラスの定義


#include <QWidget>
#include <QVBoxLayout>
#include <QFrame>
#include <QPushButton>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>

// 折りたたみ可能なグループボックスウィジェット
// クリックで展開/折りたたみを切り替えられる
class CollapsibleGroupBox : public QWidget
{
    Q_OBJECT

public:
    explicit CollapsibleGroupBox(const QString& title, QWidget* parent = nullptr);

    // コンテンツエリアのレイアウトを取得
    QVBoxLayout* contentLayout() const { return m_contentLayout; }

    // 展開/折りたたみ状態を設定
    void setExpanded(bool expanded);

    // 展開状態かどうかを取得
    bool isExpanded() const { return m_expanded; }

signals:
    // 展開状態が変更された時に発行
    void expandedChanged(bool expanded);

private slots:
    // トグルボタンがクリックされた時の処理
    void onToggleClicked();

private:
    // ヘッダーボタン（タイトルと矢印を表示）
    QPushButton* m_toggleButton;

    // コンテンツを格納するフレーム
    QFrame* m_contentFrame;

    // コンテンツエリアのレイアウト
    QVBoxLayout* m_contentLayout;

    // 展開状態
    bool m_expanded = true;

    // アニメーショングループ
    QParallelAnimationGroup* m_animationGroup = nullptr;

    // コンテンツの高さアニメーション
    QPropertyAnimation* m_contentAnimation = nullptr;

    // ボタンテキストを更新
    void updateToggleButtonText(const QString& title);

    // タイトル（矢印なし）
    QString m_title;
};

#endif // COLLAPSIBLEGROUPBOX_H
