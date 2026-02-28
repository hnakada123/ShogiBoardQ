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
            QStringLiteral("ensureKifuLoadCoordinatorForLive"),
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
        const QStringList& lines = kfcLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("KifuFileController::chooseAndLoadKifuFile()"));
        QVERIFY2(range.first >= 0, "chooseAndLoadKifuFile not found");

        const QString body = bodyText(lines, range);

        // 1) ファイル選択ダイアログ
        const auto fileDialogIdx = body.indexOf(QStringLiteral("getOpenFileName"));
        QVERIFY2(fileDialogIdx >= 0, "Must use QFileDialog::getOpenFileName");

        // 2) リプレイモード設定
        const auto replayIdx = body.indexOf(QStringLiteral("setReplayMode"));
        QVERIFY2(replayIdx >= 0, "Must call setReplayMode");
        QVERIFY2(replayIdx > fileDialogIdx,
                  "setReplayMode must come after file dialog");

        // 3) UI クリア
        const auto clearIdx = body.indexOf(QStringLiteral("clearUiBeforeKifuLoad"));
        QVERIFY2(clearIdx >= 0, "Must call clearUiBeforeKifuLoad");
        QVERIFY2(clearIdx > fileDialogIdx,
                  "clearUi must come after file dialog");

        // 4) KifuLoadCoordinator 作成
        const auto createIdx = body.indexOf(QStringLiteral("createAndWireKifuLoadCoordinator"));
        QVERIFY2(createIdx >= 0, "Must call createAndWireKifuLoadCoordinator");
        QVERIFY2(createIdx > clearIdx,
                  "createAndWire must come after clearUi");

        // 5) dispatchKifuLoad
        const auto dispatchIdx = body.indexOf(QStringLiteral("dispatchKifuLoad"));
        QVERIFY2(dispatchIdx >= 0, "Must call dispatchKifuLoad");
        QVERIFY2(dispatchIdx > createIdx,
                  "dispatchKifuLoad must come after createAndWire");
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
        const QStringList& lines = kfcLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("KifuFileController::onKifuPasteImportRequested"));
        QVERIFY2(range.first >= 0, "onKifuPasteImportRequested not found");

        const QString body = bodyText(lines, range);

        // 1) UI クリア
        const auto clearIdx = body.indexOf(QStringLiteral("clearUiBeforeKifuLoad"));
        QVERIFY2(clearIdx >= 0, "Must call clearUiBeforeKifuLoad");

        // 2) KLC 確保
        const auto ensureIdx = body.indexOf(QStringLiteral("ensureKifuLoadCoordinatorForLive"));
        QVERIFY2(ensureIdx >= 0, "Must call ensureKifuLoadCoordinatorForLive");
        QVERIFY2(ensureIdx > clearIdx, "ensure must come after clearUi");

        // 3) ロード実行
        const auto loadIdx = body.indexOf(QStringLiteral("loadKifuFromString"));
        QVERIFY2(loadIdx >= 0, "Must call loadKifuFromString");
        QVERIFY2(loadIdx > ensureIdx, "load must come after ensure");

        // 4) ステータスバー更新
        QVERIFY2(body.contains(QStringLiteral("statusBar")),
                  "Must update status bar on result");
    }

    /// onKifuPasteImportRequested が KLC null 時にエラーハンドリングすること
    void onKifuPasteImport_handlesNullKLC()
    {
        const QStringList& lines = kfcLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("KifuFileController::onKifuPasteImportRequested"));
        QVERIFY2(range.first >= 0, "onKifuPasteImportRequested not found");

        const QString body = bodyText(lines, range);

        // null チェック後のエラーメッセージ
        QVERIFY2(body.contains(QStringLiteral("KifuLoadCoordinator is null")),
                  "Must log warning when KLC is null");
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

        const auto clearIdx = body.indexOf(QStringLiteral("clearUiBeforeKifuLoad"));
        const auto ensureIdx = body.indexOf(QStringLiteral("ensureKifuLoadCoordinatorForLive"));
        const auto loadIdx = body.indexOf(QStringLiteral("loadPositionFromSfen"));

        QVERIFY2(clearIdx >= 0, "Must call clearUiBeforeKifuLoad");
        QVERIFY2(ensureIdx >= 0, "Must call ensureKifuLoadCoordinatorForLive");
        QVERIFY2(loadIdx >= 0, "Must call loadPositionFromSfen");

        QVERIFY2(clearIdx < ensureIdx, "clearUi must come before ensure");
        QVERIFY2(ensureIdx < loadIdx, "ensure must come before load");
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

    /// loadKifuCommon が共通パイプラインを実装していること
    void klc_loadKifuCommonPipeline()
    {
        const QStringList& lines = klcLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("KifuLoadCoordinator::loadKifuCommon("));
        QVERIFY2(range.first >= 0, "loadKifuCommon not found");

        const QString body = bodyText(lines, range);

        // 1) loadingKifu フラグ設定
        QVERIFY2(body.contains(QStringLiteral("m_loadingKifu = true")),
                  "Must set m_loadingKifu flag");

        // 2) 初期局面の検出
        QVERIFY2(body.contains(QStringLiteral("initialSfen")),
                  "Must determine initial SFEN");

        // 3) パース実行
        QVERIFY2(body.contains(QStringLiteral("parseFunc")),
                  "Must call parseFunc");

        // 4) エラー時の errorOccurred シグナル
        QVERIFY2(body.contains(QStringLiteral("errorOccurred")),
                  "Must emit errorOccurred on parse failure");

        // 5) ゲーム情報抽出
        QVERIFY2(body.contains(QStringLiteral("extractGameInfoFunc")),
                  "Must call extractGameInfoFunc");

        // 6) 結果適用
        QVERIFY2(body.contains(QStringLiteral("applyParsedResult")),
                  "Must call applyParsedResult");
    }

    /// loadKifuCommon がパース失敗時にフラグをリセットすること
    void klc_loadKifuCommon_resetsOnFailure()
    {
        const QStringList& lines = klcLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("KifuLoadCoordinator::loadKifuCommon("));
        QVERIFY2(range.first >= 0, "loadKifuCommon not found");

        const QString body = bodyText(lines, range);

        // パース失敗時に m_loadingKifu = false が設定されること
        QVERIFY2(body.contains(QStringLiteral("m_loadingKifu = false")),
                  "Must reset m_loadingKifu on parse failure");
    }

    /// loadKifuFromString がフォーマット自動判定を行うこと
    void klc_loadFromString_autoDetectsFormat()
    {
        const QStringList& lines = klcLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("KifuLoadCoordinator::loadKifuFromString"));
        QVERIFY2(range.first >= 0, "loadKifuFromString not found");

        const QString body = bodyText(lines, range);

        // フォーマット判定
        QVERIFY2(body.contains(QStringLiteral("detectFormat")),
                  "Must call detectFormat for auto-detection");

        // 空テキストチェック
        QVERIFY2(body.contains(QStringLiteral("isEmpty")),
                  "Must check for empty content");
        QVERIFY2(body.contains(QStringLiteral("errorOccurred")),
                  "Must emit error for empty content");
    }

    /// loadKifuFromString が SFEN/BOD を直接処理すること
    void klc_loadFromString_handlesSfenAndBodDirectly()
    {
        const QStringList& lines = klcLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("KifuLoadCoordinator::loadKifuFromString"));
        QVERIFY2(range.first >= 0, "loadKifuFromString not found");

        const QString body = bodyText(lines, range);

        QVERIFY2(body.contains(QStringLiteral("KifuFormat::SFEN")),
                  "Must handle SFEN format directly");
        QVERIFY2(body.contains(QStringLiteral("KifuFormat::BOD")),
                  "Must handle BOD format directly");
        QVERIFY2(body.contains(QStringLiteral("loadPositionFromSfen")),
                  "Must call loadPositionFromSfen for SFEN");
        QVERIFY2(body.contains(QStringLiteral("loadPositionFromBod")),
                  "Must call loadPositionFromBod for BOD");
    }

    /// loadKifuFromString が一時ファイルを作成・削除すること
    void klc_loadFromString_cleanupTempFile()
    {
        const QStringList& lines = klcLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("KifuLoadCoordinator::loadKifuFromString"));
        QVERIFY2(range.first >= 0, "loadKifuFromString not found");

        const QString body = bodyText(lines, range);

        QVERIFY2(body.contains(QStringLiteral("tempFilePath")),
                  "Must create temp file path");
        QVERIFY2(body.contains(QStringLiteral("writeTempFile")),
                  "Must write temp file");
        QVERIFY2(body.contains(QStringLiteral("QFile::remove")),
                  "Must remove temp file after loading");
    }

    /// loadKifuFromString が全フォーマットに対応した switch を持つこと
    void klc_loadFromString_switchCoversAllFormats()
    {
        const QStringList& lines = klcLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("KifuLoadCoordinator::loadKifuFromString"));
        QVERIFY2(range.first >= 0, "loadKifuFromString not found");

        const QString body = bodyText(lines, range);

        const QStringList formats = {
            QStringLiteral("KifuFormat::KIF"),
            QStringLiteral("KifuFormat::KI2"),
            QStringLiteral("KifuFormat::CSA"),
            QStringLiteral("KifuFormat::USI"),
            QStringLiteral("KifuFormat::JKF"),
            QStringLiteral("KifuFormat::USEN"),
        };

        for (const QString& fmt : formats) {
            QVERIFY2(body.contains(fmt),
                      qPrintable(QStringLiteral("loadKifuFromString missing format case: %1").arg(fmt)));
        }
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

        QVERIFY2(body.contains(QStringLiteral("m_branchPlyContext = -1")),
                  "Must reset branchPlyContext");
        QVERIFY2(body.contains(QStringLiteral("m_branchablePlySet.clear()")),
                  "Must clear branchablePlySet");
        QVERIFY2(body.contains(QStringLiteral("setRootSfen")),
                  "Must set root SFEN");
    }

    /// resetBranchContext がコンテキストをリセットすること
    void klc_resetBranchContext_resetsContext()
    {
        const QStringList& lines = klcLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("KifuLoadCoordinator::resetBranchContext()"));
        QVERIFY2(range.first >= 0, "resetBranchContext not found");

        const QString body = bodyText(lines, range);

        QVERIFY2(body.contains(QStringLiteral("m_branchPlyContext = -1")),
                  "Must reset branchPlyContext to -1");
    }

    // ================================================================
    // I) 各フォーマットのロードメソッド実装
    // ================================================================

    /// 各フォーマットのロードメソッドが loadKifuCommon に委譲すること
    void klc_formatMethods_delegateToCommon()
    {
        const QStringList& lines = klcLines();

        struct FormatCheck {
            const char* signature;
            const char* converter;
        };

        const FormatCheck checks[] = {
            {"KifuLoadCoordinator::loadKi2FromFile", "Ki2ToSfenConverter"},
            {"KifuLoadCoordinator::loadCsaFromFile", "CsaToSfenConverter"},
            {"KifuLoadCoordinator::loadJkfFromFile", "JkfToSfenConverter"},
            {"KifuLoadCoordinator::loadKifuFromFile", "KifToSfenConverter"},
            {"KifuLoadCoordinator::loadUsenFromFile", "UsenToSfenConverter"},
            {"KifuLoadCoordinator::loadUsiFromFile", "UsiToSfenConverter"},
        };

        for (const auto& chk : checks) {
            const auto range = findFunctionBody(lines, QString::fromLatin1(chk.signature));
            QVERIFY2(range.first >= 0,
                      qPrintable(QStringLiteral("Method not found: %1")
                                     .arg(QString::fromLatin1(chk.signature))));

            const QString body = bodyText(lines, range);

            QVERIFY2(body.contains(QStringLiteral("loadKifuCommon")),
                      qPrintable(QStringLiteral("%1 must delegate to loadKifuCommon")
                                     .arg(QString::fromLatin1(chk.signature))));

            QVERIFY2(body.contains(QString::fromLatin1(chk.converter)),
                      qPrintable(QStringLiteral("%1 must use %2")
                                     .arg(QString::fromLatin1(chk.signature),
                                          QString::fromLatin1(chk.converter))));
        }
    }
};

QTEST_MAIN(TestAppKifuLoad)

#include "tst_app_kifu_load.moc"
