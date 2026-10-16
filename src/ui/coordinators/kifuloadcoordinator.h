#ifndef KIFULOADCOORDINATOR_H
#define KIFULOADCOORDINATOR_H

/// @file kifuloadcoordinator.h
/// @brief 棋譜ファイル・貼り付けた棋譜・局面を読み、分岐ツリーを作り直す

#include <QFutureWatcher>
#include <QList>
#include <QObject>
#include <QString>

#include "kifuloadparser.h"

class KifuDocument;

/**
 * @brief 棋譜を読み、棋譜の正本（分岐ツリー）を作り直す
 *
 * 読む処理（KifuLoadParser）はワーカースレッドで行い、結果をメインスレッドで文書（KifuDocument）に載せる。
 * 文書は丸ごと置き換わる（前の棋譜の対局情報・対局者・持ち時間・上書き保存先は残らない）。
 * 表示（棋譜欄・分岐・盤）は、ツリーが変わると表示中の手（KifuNavigationState）が開始局面へ戻り、
 * それを受けた部品が合わせる。
 */
class KifuLoadCoordinator : public QObject
{
    Q_OBJECT

public:
    explicit KifuLoadCoordinator(KifuDocument* document, QObject* parent = nullptr);
    ~KifuLoadCoordinator() override;

    // --- 非同期（画面からの読み込み。終わったら loadFinished） ---
    /// ファイルを読む（形式は拡張子と内容から判定する）
    void loadFileAsync(const QString& filePath);
    /// 貼り付けたテキストを読む（形式は内容から判定する。SFEN・BOD は局面だけ）
    void loadTextAsync(const QString& content);
    /// 読み込み中の処理を捨てる
    void cancelLoad();

    // --- 同期（自動化 API・テスト。失敗時は errorOccurred を出してから false） ---
    [[nodiscard]] bool loadFile(const QString& filePath);
    [[nodiscard]] bool loadText(const QString& content);
    /// SFEN の局面を開始局面にする
    [[nodiscard]] bool loadPositionFromSfen(const QString& sfen);

signals:
    void loadFinished(bool success);
    void errorOccurred(const QString& errorMessage);

private slots:
    void onLoadFinished();

private:
    void startLoad(const QString& input, bool text);
    bool applyLoadResult(const KifuLoadResult& result);
    bool applyRecord(const KifuLoadResult& result);

    KifuDocument* m_document = nullptr;                    ///< 開いている棋譜（非所有）
    QFutureWatcher<KifuLoadResult>* m_loadWatcher = nullptr;
    CancelFlag m_loadCancel;
};

#endif // KIFULOADCOORDINATOR_H
