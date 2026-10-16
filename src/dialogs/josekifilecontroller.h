#ifndef JOSEKIFILECONTROLLER_H
#define JOSEKIFILECONTROLLER_H

/// @file josekifilecontroller.h
/// @brief 定跡ウィンドウのファイル操作（開く・新規・保存・最近使ったファイル）と、その確認ダイアログ

#include <QObject>
#include <QString>

class JosekiBookSession;
class QWidget;

/**
 * @brief 定跡ファイルの利用者操作
 *
 * 読み書きそのものと、ファイルパス・変更状態・履歴の記録は JosekiBookSession が行う。
 * ここはファイルの選択・未保存の確認・失敗の知らせと、読み書き中の待機カーソルを受け持つ。
 */
class JosekiFileController : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        JosekiBookSession *session = nullptr;  ///< 開いている定跡（必須）
        QWidget *parentWidget = nullptr;       ///< ダイアログの親（必須）
    };

    explicit JosekiFileController(const Deps &deps, QObject *parent = nullptr);
    ~JosekiFileController() override;

    /// 未保存の変更があれば保存・破棄・取りやめを聞く（保存は終わるまで待つ）。続けてよければ true
    [[nodiscard]] bool confirmDiscardChanges();
    /// 保存先がなければ選ばせて保存する（マージの前）。保存先があれば true
    [[nodiscard]] bool ensureFilePath();
    /// ファイルを読み込み始める
    void openFile(const QString &filePath);
    /// filePath に保存し始める
    void saveTo(const QString &filePath);

public slots:
    void open();
    void newBook();
    void save();
    void saveAs();
    /// 最近使ったファイルのメニューの項目（項目の data がパス）
    void openRecent(const QString &filePath);
    void clearRecentFiles();

signals:
    /// 開いている定跡を替える直前（マージのダイアログを閉じる）
    void bookSwitching();
    /// 状態表示に出す文（「読み込み中...」など）
    void statusMessage(const QString &text);

private slots:
    void onLoadFailed(const QString &errorMessage);
    void onSaveFailed(const QString &errorMessage);
    void onBusyChanged(bool busy);

private:
    bool isBusy() const;
    QString selectSaveFilePath();
    /// すぐ保存する（失敗は知らせる）
    bool saveNow(const QString &filePath);

    Deps m_deps;
};

#endif // JOSEKIFILECONTROLLER_H
