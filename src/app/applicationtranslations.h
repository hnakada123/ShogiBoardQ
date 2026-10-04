#ifndef APPLICATIONTRANSLATIONS_H
#define APPLICATIONTRANSLATIONS_H

#include <QString>
#include <QTranslator>

/// 起動時の言語・翻訳・棋譜表記を設定し、翻訳オブジェクトを保持する。
/// QApplication の生成後、ウィジェットの生成前に作成し、終了処理まで生存させる。
class ApplicationTranslations final
{
public:
    /// 空のパスは実行ファイルと同じディレクトリを使用する。
    explicit ApplicationTranslations(const QString& translationsDirectory = {});

    Q_DISABLE_COPY_MOVE(ApplicationTranslations)

    const QString& language() const { return m_language; }

private:
    QString m_language;
    // QTranslator は破棄時にアプリケーションへの登録も解除する。
    QTranslator m_appTranslator;
    QTranslator m_qtTranslator;
};

#endif // APPLICATIONTRANSLATIONS_H
