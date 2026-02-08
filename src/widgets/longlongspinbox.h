#ifndef LONGLONGSPINBOX_H
#define LONGLONGSPINBOX_H

/// @file longlongspinbox.h
/// @brief 64bit整数対応スピンボックスクラスの定義
/// @todo remove コメントスタイルガイド適用済み


#include <QtWidgets/QAbstractSpinBox>

// Qtには、long long型の値を直接扱うためのQSpinBoxは存在しない。
// このため、QAbstractSpinBoxを継承して、long long型の値を扱うクラス
class LongLongSpinBox : public QAbstractSpinBox
{
    Q_OBJECT

public:
    // コンストラクタ
    explicit LongLongSpinBox(QWidget* parent = nullptr);

    // 現在の値を取得する。
    qlonglong value() const;

    // スピンボックスの範囲を設定する。
    void setSpinBoxRange(qlonglong min, qlonglong max);

public slots:
    // スピンボックスに値を設定する。
    void setSpinBoxValue(qlonglong value);

signals:
    // long long型の値が変更されたときにシグナルを送信する。
    void longLongValueChanged(qlonglong i);

    // 文字列型の値が変更されたときにシグナルを送信する。
    void textValueChanged(const QString& text);

protected:
    // キーボードイベントを処理する。
    void keyPressEvent(QKeyEvent* event) override;

    // フォーカスが外れたときのイベントを処理する。
    void focusOutEvent(QFocusEvent* event) override;

    // スピンボックスのステップが有効かどうかを確認する。
    StepEnabled stepEnabled() const override;

    // スピンボックスの値をステップに応じて増減する。
    void stepBy(int steps) override;

    // 入力された値を検証する。
    QValidator::State validate(QString& input, int& pos) const override;

private:
    // スピンボックスに入力した値を確定させ、その値が有効であれば内部の値に反映させる。
    // 入力した値が無効な場合、現在の値を再設定して入力内容をリセットする。
    void finalizeEditing();

    // 必要に応じて編集を終了する。
    void finalizeEditingIfNeeded();

    // スピンボックスの最小値
    qlonglong m_minValue;

    // スピンボックスの最大値
    qlonglong m_maxValue;

    // スピンボックスの現在の値
    qlonglong m_currentValue;

    // 1ステップの増減幅
    const qlonglong m_singleStepValue;

    // クラスのコピーコンストラクタとコピー代入演算子を明示的に削除し、コピー操作を禁止する。
    Q_DISABLE_COPY(LongLongSpinBox)
};

#endif // LONGLONGSPINBOX_H
