#include <QtTest>
#include <QDir>
#include <QGuiApplication>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>

#include "applicationrendering.h"

/// 描画を GUI のスレッドだけで行わせる設定が、使っている Qt で効くこと。
/// Qt がこの環境変数を見なくなると、大きな塗りで作業用のスレッドが増えて失敗する。
class TestApplicationRendering : public QObject
{
    Q_OBJECT

    static constexpr char kNoGuiThreadPool[] = "QT_NO_GUI_THREADPOOL";

    static qsizetype threadCount()
    {
        return QDir(QStringLiteral("/proc/self/task")).entryList(QDir::Dirs | QDir::NoDotAndDotDot).size();
    }

private slots:
    // Qt が別のスレッドに分けて頼む大きさの塗り・縮小・形式の変換をしても、スレッドが増えない
    void largePaintingStaysOnGuiThread()
    {
#ifndef Q_OS_LINUX
        QSKIP("スレッドの数は Linux の /proc で数える");
#endif
        // 小さな塗り（分けない大きさ）で描画の準備を済ませてから数える
        QImage small(16, 16, QImage::Format_ARGB32_Premultiplied);
        small.fill(Qt::white);
        {
            QPainter painter(&small);
            painter.fillRect(small.rect(), QColor(0x52, 0x79, 0x9c));
        }
        const qsizetype before = threadCount();
        QVERIFY(before > 0);

        // 表のスクロールバーのような縦長の段階的な塗りと、ハイライトのような枠
        QImage image(400, 600, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::white);
        {
            QPainter painter(&image);
            QLinearGradient gradient(0, 0, image.width(), 0);
            gradient.setColorAt(0, QColor(0xf1, 0xf3, 0xf5));
            gradient.setColorAt(1, QColor(0xd8, 0xde, 0xe5));
            painter.fillRect(image.rect(), gradient);
            painter.setPen(QPen(QColor(0x24, 0x3b, 0x53), 2));
            painter.drawRect(QRect(20, 20, 300, 500));
        }
        const QImage scaled = image.scaled(200, 300, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        const QImage converted = image.convertToFormat(QImage::Format_RGB888);
        QCOMPARE(scaled.size(), QSize(200, 300));
        QCOMPARE(converted.size(), image.size());
        QCOMPARE(threadCount(), before);
    }

    // 利用者が指定した値は変えず、無ければ設定する
    void keepsUserValue()
    {
        const QByteArray original = qgetenv(kNoGuiThreadPool);
        qputenv(kNoGuiThreadPool, "custom");
        ApplicationRendering::paintOnGuiThreadOnly();
        QCOMPARE(qgetenv(kNoGuiThreadPool), QByteArray("custom"));
        qunsetenv(kNoGuiThreadPool);
        ApplicationRendering::paintOnGuiThreadOnly();
        QVERIFY(qEnvironmentVariableIsSet(kNoGuiThreadPool));
        qputenv(kNoGuiThreadPool, original);
    }
};

int main(int argc, char** argv)
{
    // アプリと同じく、QGuiApplication を作る前に設定する
    ApplicationRendering::paintOnGuiThreadOnly();
    QGuiApplication app(argc, argv);
    TestApplicationRendering test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_application_rendering.moc"
