#include "longlongspinbox.h"
#include <QtWidgets/QLineEdit>
#include <QEvent>
#include <QKeyEvent>

// Qtには、long long型の値を直接扱うためのQSpinBoxは存在しない。
// このため、QAbstractSpinBoxを継承して、long long型の値を扱うクラス
// コンストラクタ
LongLongSpinBox::LongLongSpinBox(QWidget *parent) :
    QAbstractSpinBox(parent),

    // スピンボックスの最小値
    m_minValue(std::numeric_limits<qlonglong>::min()),

    // スピンボックスの最大値
    m_maxValue(std::numeric_limits<qlonglong>::max()),

    // 現在の値を初期化リストで0に設定する。
    m_currentValue(0),

    // 1ステップの増減幅を初期化リストで設定する。
    m_singleStepValue(1)
{
    // 初期値をスピンボックスに設定する。
    setSpinBoxValue(m_currentValue);
}

// 現在の値を返す。
qlonglong LongLongSpinBox::value() const
{
    return m_currentValue;
}

// スピンボックスに値を設定する
void LongLongSpinBox::setSpinBoxValue(qlonglong newValue)
{
    // 値が範囲内に収まるように制限する。
    const qlonglong boundedValue = qBound(m_minValue, newValue, m_maxValue);
    const QString boundedValueString = QString::number(boundedValue);

    // テキストを設定する。
    lineEdit()->setText(boundedValueString);

    // 値が変更された場合
    if (m_currentValue != boundedValue) {
        // スピンボックスの値を現在の値に設定する。
        m_currentValue = boundedValue;

        // long long型のシグナルを送信する。
        emit longLongValueChanged(boundedValue);

        // 文字列型のシグナルを送信する。
        emit textValueChanged(boundedValueString);
    }
}

// スピンボックスの範囲を設定する。
void LongLongSpinBox::setSpinBoxRange(qlonglong min, qlonglong max)
{
    // minとmaxを比較し、正しい範囲に設定する。
    // 範囲設定を行う際に、minとmaxの順序が間違って渡されても、適切に範囲が設定されるようにする。
    m_minValue = std::min(min, max);
    m_maxValue = std::max(min, max);

    // 範囲設定後に値を更新する。
    setSpinBoxValue(m_currentValue);
}

// キーボードイベントを処理する。
void LongLongSpinBox::keyPressEvent(QKeyEvent* event)
{
    // EnterキーやReturnキーが押された場合、入力を確定し、テキスト選択を更新する。
    switch (event->key()) {
    case Qt::Key_Enter:
    case Qt::Key_Return:
        // スピンボックスに入力した値を確定させ、その値が有効であれば内部の値に反映させる。
        // 入力した値が無効な場合、現在の値を再設定して入力内容をリセットする。
        finalizeEditing();
        break;
    default:
        break;
    }

    // 親クラスのイベントハンドラを呼び出す。
    QAbstractSpinBox::keyPressEvent(event);
}

// フォーカスが外れたときのイベントを処理する。
void LongLongSpinBox::focusOutEvent(QFocusEvent* event)
{
    // スピンボックスに入力した値を確定させ、その値が有効であれば内部の値に反映させる。
    // 入力した値が無効な場合、現在の値を再設定して入力内容をリセットする。
    finalizeEditing();

    // 親クラスのイベントハンドラを呼び出す。
    QAbstractSpinBox::focusOutEvent(event);
}

// スピンボックスのステップが有効かどうかを確認する。
// スピンボックスのインターフェースには、通常、値を増減するための上下ボタンが付いている。
// これらのボタンが有効（クリック可能）か無効（グレーアウトされてクリック不可）かを制御する。
QAbstractSpinBox::StepEnabled LongLongSpinBox::stepEnabled() const
{
     // 読み取り専用の場合
    if (isReadOnly()) {
        // StepEnabledのデフォルト状態（すべてのステップ操作が無効）を返す。
        return StepEnabled();
    }

    // StepEnabledはQFlags<QAbstractSpinBox::StepEnabledFlag>型のビットフィールドで、ステップアップ(StepUpEnabled)
    // およびステップダウン(StepDownEnabled)の操作を個別に有効または無効にするために使用する。
    StepEnabled enabledSteps;

    // 現在の値が最大値より小さい場合、ステップアップを有効にする。
    if (m_currentValue < m_maxValue) {
        enabledSteps |= StepUpEnabled;
    }

    // 現在の値が最小値より大きい場合、ステップダウンを有効にする。
    if (m_currentValue > m_minValue) {
        enabledSteps |= StepDownEnabled;
    }

    // 有効なステップを返す。
    return enabledSteps;
}

// スピンボックスの値をステップに応じて増減する。
void LongLongSpinBox::stepBy(int steps)
{
    // 読み取り専用の場合
    if (isReadOnly()) {
        // ステップ操作を無効化する。
        return;
    }

    // 必要に応じてエディットを終了する。
    finalizeEditingIfNeeded();

    qlonglong newValue = m_currentValue + (steps * m_singleStepValue);

    // ラッピングは行わず、範囲内に収める
    newValue = qBound(m_minValue, newValue, m_maxValue);

    setSpinBoxValue(newValue);
}

// 入力された値を検証する。
QValidator::State LongLongSpinBox::validate(QString &input, int &/*pos*/) const
{
    // まず数値として解釈を試みる
    bool ok;
    const qlonglong value = input.toLongLong(&ok);

    // 入力が空、または範囲内の数値なら受け入れる
    if (input.isEmpty() || (ok && value <= m_maxValue)) {
        return QValidator::Acceptable;
    }

    // それ以外は無効とする。
    return QValidator::Invalid;
}

// スピンボックスに入力した値を確定させ、その値が有効であれば内部の値に反映させる。
// 入力した値が無効な場合、現在の値を再設定して入力内容をリセットする。
void LongLongSpinBox::finalizeEditing()
{
    // スピンボックスの文字列を取得する。
    const QString text = lineEdit()->text();

    // 変換が成功したかどうかを示すフラグ
    bool ok;

    // 文字列をlong long型の数値に変換しようと試みる。
    // 変換が成功すればokがtrueになり、失敗すればfalseになる。
    qlonglong value = text.toLongLong(&ok);

    // 数値に変換できた場合
    if (ok) {
        // 変換された値を設定する。
        setSpinBoxValue(value);
    }
    // 数値に変換できなかった場合
    else {
        // 現在の値を再設定して入力内容を再設定する。
        setSpinBoxValue(m_currentValue);
    }
}

// 必要に応じて編集を終了する
void LongLongSpinBox::finalizeEditingIfNeeded()
{
    // 現在のテキストが期待通りでない場合、エディットを終了する
    if (QString::number(m_currentValue) != lineEdit()->text()) {
        finalizeEditing();
    }
}
