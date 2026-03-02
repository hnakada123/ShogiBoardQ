/// @file collapsiblegroupbox.cpp
/// @brief 折りたたみ可能グループボックスクラスの実装

#include "collapsiblegroupbox.h"
#include <QStyle>

CollapsibleGroupBox::CollapsibleGroupBox(const QString& title, QWidget* parent)
    : QWidget(parent)
    , m_toggleButton(new QPushButton(this))
    , m_contentFrame(new QFrame(this))
    , m_contentLayout(new QVBoxLayout(m_contentFrame))
    , m_animationGroup(new QParallelAnimationGroup(this))
    , m_title(title)
{
    // サイズポリシーを設定して水平方向に広がるようにする
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // トグルボタンの設定
    m_toggleButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_toggleButton->setCheckable(true);
    m_toggleButton->setChecked(true);
    m_toggleButton->setStyleSheet(
        "QPushButton {"
        "  text-align: left;"
        "  padding: 10px 14px;"
        "  font-weight: bold;"
        "  border: none;"
        "  border-radius: 6px;"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #4a9eff, stop:1 #2d7dd2);"
        "  color: white;"
        "}"
        "QPushButton:hover {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #5aafff, stop:1 #3d8de2);"
        "}"
        "QPushButton:checked {"
        "  border-bottom-left-radius: 0px;"
        "  border-bottom-right-radius: 0px;"
        "}"
    );
    updateToggleButtonText(title);

    // コンテンツフレームの設定
    m_contentFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_contentFrame->setStyleSheet(
        "QFrame {"
        "  border: 1px solid #d4c9a8;"
        "  border-top: none;"
        "  border-bottom-left-radius: 6px;"
        "  border-bottom-right-radius: 6px;"
        "  background-color: #fefcf6;"
        "}"
    );

    // コンテンツレイアウトの設定
    m_contentLayout->setContentsMargins(12, 8, 12, 12);
    m_contentLayout->setSpacing(8);

    // メインレイアウトに追加（ストレッチファクター0で幅を親に合わせる）
    mainLayout->addWidget(m_toggleButton, 0);
    mainLayout->addWidget(m_contentFrame, 0);

    // アニメーションの設定
    m_contentAnimation = new QPropertyAnimation(m_contentFrame, "maximumHeight", this);
    m_contentAnimation->setDuration(200);
    m_contentAnimation->setEasingCurve(QEasingCurve::InOutQuad);
    m_animationGroup->addAnimation(m_contentAnimation);

    // ボタンクリック時の接続
    connect(m_toggleButton, &QPushButton::clicked, this, &CollapsibleGroupBox::onToggleClicked);
}

void CollapsibleGroupBox::updateToggleButtonText(const QString& title)
{
    QString arrow = m_expanded ? QString::fromUtf8("\u25BC ") : QString::fromUtf8("\u25B6 ");
    m_toggleButton->setText(arrow + title);
}

void CollapsibleGroupBox::setExpanded(bool expanded)
{
    if (m_expanded == expanded) {
        return;
    }

    m_expanded = expanded;
    m_toggleButton->setChecked(expanded);
    updateToggleButtonText(m_title);

    // アニメーションを停止
    m_animationGroup->stop();

    if (expanded) {
        // 展開アニメーション
        m_contentFrame->setMaximumHeight(0);
        m_contentFrame->setVisible(true);

        // コンテンツの実際の高さを計算
        m_contentFrame->setMaximumHeight(QWIDGETSIZE_MAX);
        int contentHeight = m_contentFrame->sizeHint().height();
        m_contentFrame->setMaximumHeight(0);

        m_contentAnimation->setStartValue(0);
        m_contentAnimation->setEndValue(contentHeight);
        m_animationGroup->start();
    } else {
        // 折りたたみアニメーション
        int contentHeight = m_contentFrame->height();
        m_contentAnimation->setStartValue(contentHeight);
        m_contentAnimation->setEndValue(0);

        // アニメーション終了後にコンテンツを非表示
        connect(m_animationGroup, &QParallelAnimationGroup::finished,
                this, &CollapsibleGroupBox::onCollapseAnimationFinished,
                Qt::SingleShotConnection);

        m_animationGroup->start();
    }

    emit expandedChanged(expanded);
}

void CollapsibleGroupBox::onToggleClicked()
{
    setExpanded(!m_expanded);
}

void CollapsibleGroupBox::onCollapseAnimationFinished()
{
    if (!m_expanded) {
        m_contentFrame->setVisible(false);
        m_contentFrame->setMaximumHeight(QWIDGETSIZE_MAX);
    }
}
