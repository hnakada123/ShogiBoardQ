/// @file tst_app_kifu_load.cpp
/// @brief KifuFileController / KifuLoadCoordinator の構造的契約テスト
///
/// 棋譜ファイルI/O操作のオーケストレーション（KifuFileController）と
/// 棋譜読み込みパイプライン（KifuLoadCoordinator）の構造的契約を
/// ソース解析テストで検証する。
///
/// - dispatchKifuLoad のファイル拡張子ルーティングが正しいこと
/// - 各スロットが必要なコールバックを呼んでいること
/// - ロードフローの順序が正しいこと（UI クリア → 初期化 → ロード）
/// - エラーパスが存在すること
///
/// 既存の tst_app_lifecycle_pipeline / tst_structural_kpi と同パターン。

#include <QtTest>

#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

#ifndef SOURCE_DIR
#error "SOURCE_DIR must be defined by CMake"
#endif

class TestAppKifuLoad : public QObject
{
    Q_OBJECT

private:
    static QString readSourceFile(const QString& relPath)
    {
        QFile file(QStringLiteral(SOURCE_DIR "/") + relPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return {};
        return QTextStream(&file).readAll();
    }

    static QStringList readSourceLines(const QString& relPath)
    {
        QFile file(QStringLiteral(SOURCE_DIR "/") + relPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return {};
        QStringList lines;
        QTextStream in(&file);
        while (!in.atEnd())
            lines << in.readLine();
        return lines;
    }

    static QPair<int, int> findFunctionBody(const QStringList& lines, const QString& signature)
    {
        const int sz = int(lines.size());

        int sigLine = -1;
        for (int i = 0; i < sz; ++i) {
            if (lines[i].contains(signature)) {
                sigLine = i;
                break;
            }
        }
        if (sigLine < 0)
            return {-1, -1};

        int braceStart = -1;
        for (int i = sigLine; i < sz; ++i) {
            if (lines[i].contains(QLatin1Char('{'))) {
                braceStart = i;
                break;
            }
        }
        if (braceStart < 0)
            return {-1, -1};

        int depth = 0;
        for (int i = braceStart; i < sz; ++i) {
            for (const QChar& ch : lines[i]) {
                if (ch == QLatin1Char('{'))
                    ++depth;
                else if (ch == QLatin1Char('}'))
                    --depth;
            }
            if (depth == 0)
                return {braceStart, i};
        }
        return {-1, -1};
    }

    static QString bodyText(const QStringList& lines, const QPair<int, int>& range)
    {
        QStringList body;
        for (int i = range.first; i <= range.second && i < int(lines.size()); ++i)
            body << lines[i];
        return body.join(QLatin1Char('\n'));
    }

    const QStringList& kfcLines()
    {
        if (m_kfcLines.isEmpty())
            m_kfcLines = readSourceLines(QStringLiteral("src/kifu/kifufilecontroller.cpp"));
        return m_kfcLines;
    }

    const QString& kfcHeader()
    {
        if (m_kfcHeader.isEmpty())
            m_kfcHeader = readSourceFile(QStringLiteral("src/kifu/kifufilecontroller.h"));
        return m_kfcHeader;
    }

    const QStringList& klcLines()
    {
        if (m_klcLines.isEmpty())
            m_klcLines = readSourceLines(QStringLiteral("src/kifu/kifuloadcoordinator.cpp"));
        return m_klcLines;
    }

    const QString& klcHeader()
    {
        if (m_klcHeader.isEmpty())
            m_klcHeader = readSourceFile(QStringLiteral("src/kifu/kifuloadcoordinator.h"));
        return m_klcHeader;
    }

    QStringList m_kfcLines;
    QString m_kfcHeader;
    QStringList m_klcLines;
    QString m_klcHeader;

private slots:
    // ================================================================
    // A) KifuFileController ヘッダー構造
    // ================================================================

    /// Deps 構造体が必要なコールバックを持つこと
    void kfc_depsHasRequiredCallbacks()
    {
        const QString& hdr = kfcHeader();
        QVERIFY2(!hdr.isEmpty(), "Failed to read KifuFileController header");

        const QStringList required = {
            QStringLiteral("clearUiBeforeKifuLoad"),
            QStringLiteral("setReplayMode"),
            QStringLiteral("ensurePlayerInfoAndGameInfo"),
            QStringLiteral("ensureGameRecordModel"),
            QStringLiteral("ensureKifuExportController"),
            QStringLiteral("createAndWireKifuLoadCoordinator"),
            QStringLiteral("prepareKifuLoadCoordinatorForLive"),
            QStringLiteral("getKifuExportController"),
            QStringLiteral("getKifuLoadCoordinator"),
        };

        for (const QString& cb : required) {
            QVERIFY2(hdr.contains(cb),
                      qPrintable(QStringLiteral("KFC Deps missing: %1").arg(cb)));
        }
    }

    /// 全パブリックスロットが宣言されていること
    void kfc_allSlotsDeclared()
    {
        const QString& hdr = kfcHeader();

        const QStringList expectedSlots = {
            QStringLiteral("chooseAndLoadKifuFile()"),
            QStringLiteral("saveKifuToFile()"),
            QStringLiteral("overwriteKifuFile()"),
            QStringLiteral("pasteKifuFromClipboard()"),
            QStringLiteral("onKifuPasteImportRequested"),
            QStringLiteral("onSfenCollectionPositionSelected"),
        };

        for (const QString& slot : expectedSlots) {
            QVERIFY2(hdr.contains(slot),
                      qPrintable(QStringLiteral("Missing slot: %1").arg(slot)));
        }
    }

    /// autoSaveKifuToFile が public メソッドとして宣言されていること
    void kfc_autoSaveMethodDeclared()
    {
        const QString& hdr = kfcHeader();
        QVERIFY2(hdr.contains(QStringLiteral("autoSaveKifuToFile")),
                  "autoSaveKifuToFile must be declared");
    }

    // ================================================================
    // B) dispatchKifuLoad 拡張子ルーティング
    // ================================================================

    /// dispatchKifuLoad が .csa を loadCsaFromFile にルーティングすること
    void dispatchKifuLoad_routesCsa()
    {
        const QStringList& lines = kfcLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("KifuFileController::dispatchKifuLoad"));
        QVERIFY2(range.first >= 0, "dispatchKifuLoad not found");

        const QString body = bodyText(lines, range);
        QVERIFY2(body.contains(QStringLiteral(".csa")),
                  "Must check for .csa extension");
        QVERIFY2(body.contains(QStringLiteral("loadCsaFromFile")),
                  "Must route .csa to loadCsaFromFile");
    }

    /// dispatchKifuLoad が .ki2/.ki2u を loadKi2FromFile にルーティングすること
    void dispatchKifuLoad_routesKi2()
    {
        const QStringList& lines = kfcLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("KifuFileController::dispatchKifuLoad"));
        QVERIFY2(range.first >= 0, "dispatchKifuLoad not found");

        const QString body = bodyText(lines, range);
        QVERIFY2(body.contains(QStringLiteral(".ki2")),
                  "Must check for .ki2 extension");
        QVERIFY2(body.contains(QStringLiteral(".ki2u")),
                  "Must check for .ki2u extension");
        QVERIFY2(body.contains(QStringLiteral("loadKi2FromFile")),
                  "Must route .ki2/.ki2u to loadKi2FromFile");
    }

    /// dispatchKifuLoad が .jkf を loadJkfFromFile にルーティングすること
    void dispatchKifuLoad_routesJkf()
    {
        const QStringList& lines = kfcLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("KifuFileController::dispatchKifuLoad"));
        QVERIFY2(range.first >= 0, "dispatchKifuLoad not found");

        const QString body = bodyText(lines, range);
        QVERIFY2(body.contains(QStringLiteral(".jkf")),
                  "Must check for .jkf extension");
        QVERIFY2(body.contains(QStringLiteral("loadJkfFromFile")),
                  "Must route .jkf to loadJkfFromFile");
    }

    /// dispatchKifuLoad が .usen を loadUsenFromFile にルーティングすること
    void dispatchKifuLoad_routesUsen()
    {
        const QStringList& lines = kfcLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("KifuFileController::dispatchKifuLoad"));
        QVERIFY2(range.first >= 0, "dispatchKifuLoad not found");

        const QString body = bodyText(lines, range);
        QVERIFY2(body.contains(QStringLiteral(".usen")),
                  "Must check for .usen extension");
        QVERIFY2(body.contains(QStringLiteral("loadUsenFromFile")),
                  "Must route .usen to loadUsenFromFile");
    }

    /// dispatchKifuLoad が .usi を loadUsiFromFile にルーティングすること
    void dispatchKifuLoad_routesUsi()
    {
        const QStringList& lines = kfcLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("KifuFileController::dispatchKifuLoad"));
        QVERIFY2(range.first >= 0, "dispatchKifuLoad not found");

        const QString body = bodyText(lines, range);
        QVERIFY2(body.contains(QStringLiteral(".usi")),
                  "Must check for .usi extension");
        QVERIFY2(body.contains(QStringLiteral("loadUsiFromFile")),
                  "Must route .usi to loadUsiFromFile");
    }

    /// dispatchKifuLoad が .sfen を .usi と同じ USI/SFEN ローダーにルーティングすること
    void dispatchKifuLoad_routesSfen()
    {
        const QStringList& lines = kfcLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("KifuFileController::dispatchKifuLoad"));
        QVERIFY2(range.first >= 0, "dispatchKifuLoad not found");

        const QString body = bodyText(lines, range);
        const auto sfenIdx = body.indexOf(QStringLiteral(".sfen"));
        const auto kifIdx = body.indexOf(QStringLiteral("loadKifuFromFile"));
        QVERIFY2(sfenIdx >= 0, "Must check for .sfen extension");
        QVERIFY2(sfenIdx < kifIdx, ".sfen must be routed before the KIF default");
        QVERIFY2(body.mid(sfenIdx).contains(QStringLiteral("loadUsiFromFile")),
                  "Must route .sfen to loadUsiFromFile");
    }

    /// dispatchKifuLoad がデフォルトで loadKifuFromFile を呼ぶこと
    void dispatchKifuLoad_defaultToKif()
    {
        const QStringList& lines = kfcLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("KifuFileController::dispatchKifuLoad"));
        QVERIFY2(range.first >= 0, "dispatchKifuLoad not found");

        const QString body = bodyText(lines, range);
        // else ブロックで loadKifuFromFile が呼ばれる
        QVERIFY2(body.contains(QStringLiteral("loadKifuFromFile")),
                  "Must have loadKifuFromFile as default route");
    }

    /// dispatchKifuLoad が全6形式をカバーしていること
    void dispatchKifuLoad_coversAllFormats()
    {
        const QStringList& lines = kfcLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("KifuFileController::dispatchKifuLoad"));
        QVERIFY2(range.first >= 0, "dispatchKifuLoad not found");

        const QString body = bodyText(lines, range);

        const QStringList formats = {
            QStringLiteral("loadCsaFromFile"),
            QStringLiteral("loadKi2FromFile"),
            QStringLiteral("loadJkfFromFile"),
            QStringLiteral("loadUsenFromFile"),
            QStringLiteral("loadUsiFromFile"),
            QStringLiteral("loadKifuFromFile"),
        };

        for (const QString& fmt : formats) {
            QVERIFY2(body.contains(fmt),
                      qPrintable(QStringLiteral("dispatchKifuLoad missing format: %1").arg(fmt)));
        }
    }

    // ================================================================
    // C) chooseAndLoadKifuFile フロー順序
    // ================================================================

    /// chooseAndLoadKifuFile が正しい順序で処理すること
    void chooseAndLoad_flowOrder()
    {
        const auto lines = kfcLines();
        const auto range = findFunctionBody(lines, QStringLiteral("KifuFileController::chooseAndLoadKifuFile("));
        QVERIFY(range.first >= 0);
        const QString body = bodyText(lines, range);
        const auto guard = body.indexOf(QStringLiteral("confirmDiscardUnsaved"));
        QVERIFY(guard >= 0);
        QVERIFY(body.indexOf(QStringLiteral("startAsyncLoad")) > guard);
    }

    /// ファイル・貼り付け・局面集の共通準備で、リプレイと対局情報を初期化すること
    void prepareForKifuLoad_initializesReplayAndGameInfo()
    {
        const QStringList& lines = kfcLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("KifuFileController::prepareForKifuLoad()"));
        QVERIFY(range.first >= 0);
        const QString body = bodyText(lines, range);
        const auto replayIdx = body.indexOf(QStringLiteral("setReplayMode(true)"));
        const auto clearIdx = body.indexOf(QStringLiteral("clearUiBeforeKifuLoad"));
        const auto infoIdx = body.indexOf(QStringLiteral("ensurePlayerInfoAndGameInfo"));
        QVERIFY(replayIdx >= 0);
        QVERIFY(clearIdx > replayIdx);
        QVERIFY(infoIdx > clearIdx);
    }

    /// chooseAndLoadKifuFile が読み込み成功時に上書き保存先を記録すること
    void chooseAndLoad_recordsOverwriteTarget()
    {
        const auto lines = kfcLines();
        const auto range = findFunctionBody(lines, QStringLiteral("KifuFileController::onAsyncLoadFinished("));
        QVERIFY(range.first >= 0);
        const QString body = bodyText(lines, range);
        const auto guard = body.indexOf(QStringLiteral("if (!success) return"));
        QVERIFY(guard >= 0);
        QVERIFY(body.indexOf(QStringLiteral("setOverwriteTarget(m_pendingLoadPath)")) > guard);
    }

    /// setOverwriteTarget が保存形式を決められる拡張子だけを対象にすること
    void setOverwriteTarget_requiresKnownExtension()
    {
        const QStringList& lines = kfcLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("KifuFileController::setOverwriteTarget"));
        QVERIFY2(range.first >= 0, "setOverwriteTarget not found");

        const QString body = bodyText(lines, range);
        QVERIFY2(body.contains(QStringLiteral("hasKnownSaveExtension")),
                  "Must check the extension via KifuSaveCoordinator::hasKnownSaveExtension");
        QVERIFY2(body.contains(QStringLiteral("saveFileName")),
                  "Must write to deps.saveFileName");
        QVERIFY2(body.contains(QStringLiteral("clear()")),
                  "Must clear the target for unknown extensions");
    }

    /// dispatchKifuLoad が読み込み結果を返すこと
    void dispatchKifuLoad_returnsResult()
    {
        QVERIFY2(kfcHeader().contains(QStringLiteral("bool dispatchKifuLoad(")),
                  "dispatchKifuLoad must return bool");

        const QStringList& lines = kfcLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("KifuFileController::dispatchKifuLoad"));
        QVERIFY2(range.first >= 0, "dispatchKifuLoad not found");

        const QString body = bodyText(lines, range);
        QVERIFY2(body.contains(QStringLiteral("return klc->load")),
                  "Must propagate the loader result");
    }

    /// chooseAndLoadKifuFile がサポートするファイルフィルタを持つこと
    void chooseAndLoad_hasFileFilters()
    {
        const QStringList& lines = kfcLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("KifuFileController::chooseAndLoadKifuFile()"));
        QVERIFY2(range.first >= 0, "chooseAndLoadKifuFile not found");

        const QString body = bodyText(lines, range);

        // 主要フォーマットがフィルタに含まれること
        QVERIFY2(body.contains(QStringLiteral("*.kif")), "Must support .kif filter");
        QVERIFY2(body.contains(QStringLiteral("*.ki2")), "Must support .ki2 filter");
        QVERIFY2(body.contains(QStringLiteral("*.csa")), "Must support .csa filter");
        QVERIFY2(body.contains(QStringLiteral("*.jkf")), "Must support .jkf filter");
        QVERIFY2(body.contains(QStringLiteral("*.usi")), "Must support .usi filter");
        QVERIFY2(body.contains(QStringLiteral("*.usen")), "Must support .usen filter");
    }

    /// chooseAndLoadKifuFile が最後に選択したディレクトリを保存すること
    void chooseAndLoad_savesLastDirectory()
    {
        const QStringList& lines = kfcLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("KifuFileController::chooseAndLoadKifuFile()"));
        QVERIFY2(range.first >= 0, "chooseAndLoadKifuFile not found");

        const QString body = bodyText(lines, range);

        QVERIFY2(body.contains(QStringLiteral("lastKifuDirectory")),
                  "Must read last kifu directory");
        QVERIFY2(body.contains(QStringLiteral("setLastKifuDirectory")),
                  "Must save last kifu directory");
    }

    // ================================================================
    // D) 保存フロー
    // ================================================================

    /// saveKifuToFile が必要な ensure を呼ぶこと
    void saveKifuToFile_ensuresRequired()
    {
        const QStringList& lines = kfcLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("KifuFileController::saveKifuToFile()"));
        QVERIFY2(range.first >= 0, "saveKifuToFile not found");

        const QString body = bodyText(lines, range);

        QVERIFY2(body.contains(QStringLiteral("ensureGameRecordModel")),
                  "Must ensure GameRecordModel");
        QVERIFY2(body.contains(QStringLiteral("ensureKifuExportController")),
                  "Must ensure KifuExportController");
        QVERIFY2(body.contains(QStringLiteral("updateKifuExportDependencies")),
                  "Must update export dependencies");
        QVERIFY2(body.contains(QStringLiteral("saveToFile")),
                  "Must call kec->saveToFile()");
    }

    /// overwriteKifuFile が空ファイル名で saveKifuToFile にフォールバックすること
    void overwriteKifuFile_fallsBackToSave()
    {
        const QStringList& lines = kfcLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("KifuFileController::overwriteKifuFile()"));
        QVERIFY2(range.first >= 0, "overwriteKifuFile not found");

        const QString body = bodyText(lines, range);

        // 空ファイル名チェック
        QVERIFY2(body.contains(QStringLiteral("saveFileName"))
                     && body.contains(QStringLiteral("isEmpty")),
                  "Must check if saveFileName is empty");

        // フォールバック
        QVERIFY2(body.contains(QStringLiteral("saveKifuToFile")),
                  "Must fall back to saveKifuToFile when no filename");

        // 上書き処理
        QVERIFY2(body.contains(QStringLiteral("overwriteFile")),
                  "Must call kec->overwriteFile()");
    }

    /// autoSaveKifuToFile が KifuExportController に委譲すること
    void autoSaveKifuToFile_delegatesToExport()
    {
        const QStringList& lines = kfcLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("KifuFileController::autoSaveKifuToFile"));
        QVERIFY2(range.first >= 0, "autoSaveKifuToFile not found");

        const QString body = bodyText(lines, range);

        QVERIFY2(body.contains(QStringLiteral("ensureKifuExportController")),
                  "Must ensure KifuExportController");
        QVERIFY2(body.contains(QStringLiteral("autoSaveToDir")),
                  "Must call kec->autoSaveToDir()");
    }

    // ================================================================
    // E) 棋譜貼り付けフロー
    // ================================================================

    /// pasteKifuFromClipboard が既存ダイアログをチェックすること
    void pasteKifu_checksExistingDialog()
    {
        const QStringList& lines = kfcLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("KifuFileController::pasteKifuFromClipboard()"));
        QVERIFY2(range.first >= 0, "pasteKifuFromClipboard not found");

        const QString body = bodyText(lines, range);

        QVERIFY2(body.contains(QStringLiteral("m_kifuPasteDialog")),
                  "Must check existing dialog");
        QVERIFY2(body.contains(QStringLiteral("KifuPasteDialog")),
                  "Must create KifuPasteDialog");
        QVERIFY2(body.contains(QStringLiteral("WA_DeleteOnClose")),
                  "Must set WA_DeleteOnClose");
    }

    /// onKifuPasteImportRequested が正しいフローで処理すること
    void onKifuPasteImport_flowOrder()
    {
        const auto lines = kfcLines();
        const auto range = findFunctionBody(lines, QStringLiteral("KifuFileController::onKifuPasteImportRequested("));
        QVERIFY(range.first >= 0);
        const QString body = bodyText(lines, range);
        const auto guard = body.indexOf(QStringLiteral("confirmDiscardUnsaved"));
        QVERIFY(guard >= 0);
        QVERIFY(body.indexOf(QStringLiteral("startAsyncLoad")) > guard);
    }

    /// onKifuPasteImportRequested が成功時に上書き保存先をクリアすること
    void onKifuPasteImport_clearsOverwriteTarget()
    {
        const auto lines = kfcLines();
        const auto range = findFunctionBody(lines, QStringLiteral("KifuFileController::onAsyncLoadFinished("));
        QVERIFY(range.first >= 0);
        const QString body = bodyText(lines, range);
        const auto guard = body.indexOf(QStringLiteral("if (!success) return"));
        QVERIFY(guard >= 0);
        QVERIFY(body.indexOf(QStringLiteral("clearOverwriteTarget")) > guard);
        QVERIFY(body.contains(QStringLiteral("m_loadingText")));
        QVERIFY(body.contains(QStringLiteral("markDirty()")));
    }

    /// onKifuPasteImportRequested が KLC null 時にエラーハンドリングすること
    void onKifuPasteImport_handlesNullKLC()
    {
        const auto lines = kfcLines();
        const auto range = findFunctionBody(lines, QStringLiteral("KifuFileController::startAsyncLoad("));
        QVERIFY(range.first >= 0);
        const QString body = bodyText(lines, range);
        QVERIFY(body.contains(QStringLiteral("KifuLoadCoordinator is null")));
        QVERIFY(body.contains(QStringLiteral("if (!coordinator)")));
    }

    // ================================================================
    // F) SFEN 局面選択フロー
    // ================================================================

    /// onSfenCollectionPositionSelected が正しいフローで処理すること
    void onSfenPositionSelected_flowOrder()
    {
        const QStringList& lines = kfcLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("KifuFileController::onSfenCollectionPositionSelected"));
        QVERIFY2(range.first >= 0, "onSfenCollectionPositionSelected not found");

        const QString body = bodyText(lines, range);

        const auto guardIdx = body.indexOf(QStringLiteral("confirmDiscardUnsaved"));
        const auto clearIdx = body.indexOf(QStringLiteral("prepareForKifuLoad"));
        const auto ensureIdx = body.indexOf(QStringLiteral("prepareKifuLoadCoordinatorForLive"));
        const auto loadIdx = body.indexOf(QStringLiteral("loadPositionFromSfen"));

        QVERIFY2(guardIdx >= 0, "Must guard unsaved changes");
        QVERIFY2(clearIdx > guardIdx, "Load preparation must follow the unsaved guard");
        QVERIFY2(ensureIdx >= 0, "Must call prepareKifuLoadCoordinatorForLive");
        QVERIFY2(loadIdx >= 0, "Must call loadPositionFromSfen");

        QVERIFY2(clearIdx < ensureIdx, "clearUi must come before ensure");
        QVERIFY2(ensureIdx < loadIdx, "ensure must come before load");
    }

    /// onSfenCollectionPositionSelected が成功時に上書き保存先をクリアすること
    void onSfenPositionSelected_clearsOverwriteTarget()
    {
        const QStringList& lines = kfcLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("KifuFileController::onSfenCollectionPositionSelected"));
        QVERIFY2(range.first >= 0, "onSfenCollectionPositionSelected not found");

        const QString body = bodyText(lines, range);
        const auto loadIdx = body.indexOf(QStringLiteral("loadPositionFromSfen"));
        const auto clearIdx = body.indexOf(QStringLiteral("clearOverwriteTarget"));
        QVERIFY2(clearIdx >= 0,
                  "Position from the SFEN collection must not overwrite the previously loaded file");
        QVERIFY2(clearIdx > loadIdx, "Target must be cleared after a successful load");
    }

    /// onSfenCollectionPositionSelected は反映した局面を未保存扱いにしないこと
    /// （局面集から続けて別の局面を選ぶたびに保存の確認が出ないように）
    void onSfenPositionSelected_doesNotMarkDirty()
    {
        const QStringList& lines = kfcLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("KifuFileController::onSfenCollectionPositionSelected"));
        QVERIFY2(range.first >= 0, "onSfenCollectionPositionSelected not found");

        const QString body = bodyText(lines, range);
        QVERIFY2(!body.contains(QStringLiteral("markDirty")),
                  "A position picked from the SFEN collection is still in the collection file");
    }

    /// onSfenCollectionPositionSelected がステータスバーを更新すること
    void onSfenPositionSelected_updatesStatusBar()
    {
        const QStringList& lines = kfcLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("KifuFileController::onSfenCollectionPositionSelected"));
        QVERIFY2(range.first >= 0, "onSfenCollectionPositionSelected not found");

        const QString body = bodyText(lines, range);

        QVERIFY2(body.contains(QStringLiteral("showMessage")),
                  "Must show status bar message");
    }

    // ================================================================
    // G) KifuLoadCoordinator 棋譜読み込みパイプライン
    // ================================================================

    /// KifuLoadCoordinator が全フォーマットのロードメソッドを持つこと
    void klc_hasAllFormatLoadMethods()
    {
        const QString& hdr = klcHeader();
        QVERIFY2(!hdr.isEmpty(), "Failed to read KifuLoadCoordinator header");

        const QStringList methods = {
            QStringLiteral("loadKifuFromFile"),
            QStringLiteral("loadJkfFromFile"),
            QStringLiteral("loadCsaFromFile"),
            QStringLiteral("loadKi2FromFile"),
            QStringLiteral("loadUsenFromFile"),
            QStringLiteral("loadUsiFromFile"),
            QStringLiteral("loadKifuFromString"),
            QStringLiteral("loadPositionFromSfen"),
            QStringLiteral("loadPositionFromBod"),
        };

        for (const QString& m : methods) {
            QVERIFY2(hdr.contains(m),
                      qPrintable(QStringLiteral("KLC missing method: %1").arg(m)));
        }
    }

    /// KifuLoadCoordinator が必要なシグナルを持つこと
    void klc_hasRequiredSignals()
    {
        const QString& hdr = klcHeader();

        const QStringList expectedSignals = {
            QStringLiteral("errorOccurred"),
            QStringLiteral("displayGameRecord"),
            QStringLiteral("syncBoardAndHighlightsAtRow"),
            QStringLiteral("enableArrowButtons"),
            QStringLiteral("gameInfoPopulated"),
            QStringLiteral("branchTreeBuilt"),
        };

        for (const QString& sig : std::as_const(expectedSignals)) {
            QVERIFY2(hdr.contains(sig),
                      qPrintable(QStringLiteral("KLC missing signal: %1").arg(sig)));
        }
    }

    /// applyLoadResult が共通パイプラインを実装していること
    void klc_applyLoadResultPipeline()
    {
        const auto lines = klcLines();
        const auto range = findFunctionBody(lines, QStringLiteral("KifuLoadCoordinator::applyLoadResult("));
        QVERIFY(range.first >= 0);
        const QString body = bodyText(lines, range);
        QVERIFY(body.contains(QStringLiteral("m_loadingKifu = true")));
        QVERIFY(body.contains(QStringLiteral("initialSfen")));
        QVERIFY(body.contains(QStringLiteral("errorOccurred")));
        QVERIFY(body.contains(QStringLiteral("populateGameInfo")));
        QVERIFY(body.contains(QStringLiteral("applyParsedResult")));
    }

    /// applyLoadResult がパース失敗時にフラグをリセットすること
    void klc_applyLoadResult_resetsOnFailure()
    {
        const auto lines = klcLines();
        const auto range = findFunctionBody(lines, QStringLiteral("KifuLoadCoordinator::applyLoadResult("));
        QVERIFY(range.first >= 0);
        const QString body = bodyText(lines, range);
        QVERIFY(body.contains(QStringLiteral("!result.success")));
        QVERIFY(body.contains(QStringLiteral("m_loadingKifu = false")));
        QVERIFY(body.contains(QStringLiteral("return false")));
    }

    /// loadUsiFromFile が指し手のないファイルを局面として反映すること
    void klc_loadUsi_handlesPositionOnlyFile()
    {
        const auto lines = readSourceLines(QStringLiteral("src/kifu/kifuloadparser.cpp"));
        const auto range = findFunctionBody(lines, QStringLiteral("KifuLoadParser::parseFile("));
        QVERIFY(range.first >= 0);
        const QString body = bodyText(lines, range);
        QVERIFY(body.contains(QStringLiteral("parseUsiFile")));
        QVERIFY(body.contains(QStringLiteral("moves.isEmpty() && terminal.isEmpty()")));
        QVERIFY(body.contains(QStringLiteral("result.positionOnly = base")));
        QVERIFY(body.contains(QStringLiteral("UsiToSfenConverter::parseWithVariations")));
    }

    /// 各フォーマットのロードメソッドが成否を返すこと
    void klc_loadMethods_returnResult()
    {
        const QString& header = klcHeader();
        const QStringList methods = {
            QStringLiteral("loadKifuFromFile"), QStringLiteral("loadJkfFromFile"),
            QStringLiteral("loadCsaFromFile"),  QStringLiteral("loadKi2FromFile"),
            QStringLiteral("loadUsenFromFile"), QStringLiteral("loadUsiFromFile"),
        };
        for (const QString& m : methods) {
            QVERIFY2(header.contains(QStringLiteral("bool %1(").arg(m)),
                      qPrintable(QStringLiteral("%1 must return bool").arg(m)));
        }
        QVERIFY2(header.contains(QStringLiteral("bool applyLoadResult(")),
                  "applyLoadResult must return bool");
    }

    /// loadKifuFromString が読み込み結果をそのまま返すこと
    void klc_loadFromString_propagatesResult()
    {
        const auto lines = klcLines();
        const auto range = findFunctionBody(lines, QStringLiteral("KifuLoadCoordinator::loadKifuFromString("));
        QVERIFY(range.first >= 0);
        const QString body = bodyText(lines, range);
        QVERIFY(body.contains(QStringLiteral("return applyLoadResult(KifuLoadParser::parseText(content))")));
    }

    /// loadKifuFromString がフォーマット自動判定を行うこと
    void klc_loadFromString_autoDetectsFormat()
    {
        const auto lines = readSourceLines(QStringLiteral("src/kifu/kifuloadparser.cpp"));
        const auto range = findFunctionBody(lines, QStringLiteral("KifuLoadParser::parseText("));
        QVERIFY(range.first >= 0);
        const QString body = bodyText(lines, range);
        QVERIFY(body.contains(QStringLiteral("detectFormat")));
        QVERIFY(body.contains(QStringLiteral("isEmpty")));
        QVERIFY(body.contains(QStringLiteral("result.error")));
    }

    /// loadKifuFromString が SFEN/BOD を直接処理すること
    void klc_loadFromString_handlesSfenAndBodDirectly()
    {
        const auto lines = readSourceLines(QStringLiteral("src/kifu/kifuloadparser.cpp"));
        const auto range = findFunctionBody(lines, QStringLiteral("KifuLoadParser::parseText("));
        QVERIFY(range.first >= 0);
        const QString body = bodyText(lines, range);
        QVERIFY(body.contains(QStringLiteral("Format::SFEN")));
        QVERIFY(body.contains(QStringLiteral("Format::BOD")));
        QVERIFY(body.contains(QStringLiteral("result.positionOnly")));
        QVERIFY(body.contains(QStringLiteral("buildInitialSfenFromBod")));
    }

    /// loadKifuFromString が一時ファイルを作成・削除すること
    void klc_loadFromString_cleanupTempFile()
    {
        const auto lines = readSourceLines(QStringLiteral("src/kifu/kifuloadparser.cpp"));
        const auto range = findFunctionBody(lines, QStringLiteral("KifuLoadParser::parseText("));
        QVERIFY(range.first >= 0);
        const QString body = bodyText(lines, range);
        QVERIFY(body.contains(QStringLiteral("const auto file = KifuFileReader::createTempFile")));
        QVERIFY(body.contains(QStringLiteral("return parseFile(file->fileName()")));
    }

    /// KifuLoadParser が全フォーマットに対応すること
    void klc_loadFromString_switchCoversAllFormats()
    {
        const auto lines = readSourceLines(QStringLiteral("src/kifu/kifuloadparser.cpp"));
        const auto range = findFunctionBody(lines, QStringLiteral("KifuLoadParser::parseFile("));
        QVERIFY(range.first >= 0);
        const QString body = bodyText(lines, range);
        QVERIFY(body.contains(QStringLiteral("Format::KI2")));
        QVERIFY(body.contains(QStringLiteral("Format::CSA")));
        QVERIFY(body.contains(QStringLiteral("Format::USI")));
        QVERIFY(body.contains(QStringLiteral("Format::JKF")));
        QVERIFY(body.contains(QStringLiteral("Format::USEN")));
        QVERIFY(body.contains(QStringLiteral("KifToSfenConverter::parseWithVariations")));
    }

    // ================================================================
    // H) 分岐管理
    // ================================================================

    /// resetBranchTreeForNewGame が分岐データをクリアすること
    void klc_resetBranchTree_clearsAllData()
    {
        const QStringList& lines = klcLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("KifuLoadCoordinator::resetBranchTreeForNewGame()"));
        QVERIFY2(range.first >= 0, "resetBranchTreeForNewGame not found");

        const QString body = bodyText(lines, range);

        QVERIFY2(body.contains(QStringLiteral("setRootSfen")),
                  "Must set root SFEN");
    }

    // ================================================================
    // I) 各フォーマットのロードメソッド実装
    // ================================================================

    void klc_ki2UsesKi2GameInfo()
    {
        const auto lines = readSourceLines(QStringLiteral("src/kifu/kifuloadparser.cpp"));
        const auto range = findFunctionBody(lines, QStringLiteral("KifuLoadParser::parseFile("));
        QVERIFY(range.first >= 0);
        const QString body = bodyText(lines, range);
        QVERIFY(body.contains(QStringLiteral("Ki2ToSfenConverter::extractGameInfo")));
    }

    /// 各フォーマットのロードメソッドが KifuLoadParser に委譲すること
    void klc_formatMethods_delegateToCommon()
    {
        const auto lines = klcLines();
        const QStringList methods = {"loadKifuFromFile", "loadKi2FromFile", "loadCsaFromFile",
                                     "loadJkfFromFile", "loadUsenFromFile", "loadUsiFromFile"};
        for (const auto& method : methods) {
            const auto range = findFunctionBody(lines, QStringLiteral("KifuLoadCoordinator::") + method + '(');
            QVERIFY(range.first >= 0);
            const auto body = bodyText(lines, range);
            QVERIFY(body.contains(QStringLiteral("cancelLoad()")));
            QVERIFY(body.contains(QStringLiteral("return applyLoadResult(KifuLoadParser::parseFile(")));
        }
    }
};

QTEST_MAIN(TestAppKifuLoad)

#include "tst_app_kifu_load.moc"
