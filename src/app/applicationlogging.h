#ifndef APPLICATIONLOGGING_H
#define APPLICATIONLOGGING_H

#include <QString>
#include <QtLogging>

/// アプリケーション全体のログ出力をスコープで管理する。
/// QApplication より先に生成し、その破棄後まで生存させる。
/// Debug はファイルへ追記し、それ以外はメッセージを破棄する。
class ApplicationLogging final
{
public:
    explicit ApplicationLogging(const QString& filePath = QStringLiteral("debug.log"));
    ~ApplicationLogging();

    Q_DISABLE_COPY_MOVE(ApplicationLogging)

private:
    QtMessageHandler m_previousHandler = nullptr;
    bool m_installed = false;
};

#endif // APPLICATIONLOGGING_H
