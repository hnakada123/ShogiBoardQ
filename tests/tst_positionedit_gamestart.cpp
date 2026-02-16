/// @file tst_positionedit_gamestart.cpp
/// @brief 局面編集→対局開始→開始局面復元のフロー検証テスト
///
/// バグ再現シナリオ:
///   1. 「編集」→「局面編集開始」で局面を編集
///   2. 「対局」→「対局開始」で対局
///   3. 対局終了後、棋譜欄の「=== 開始局面 ===」をクリック
///   4. 期待: 編集した局面が表示される
///   5. 実際: 平手の局面が表示される

#include <QtTest>
#include <QSignalSpy>

#include "prestartcleanuphandler.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"
#include "livegamesession.h"
#include "shogimove.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

// 局面編集で作る想定のカスタムSFEN（例: 王だけの局面）
static const QString kEditedSfen =
    QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b - 1");

class TestPositionEditGameStart : public QObject
{
    Q_OBJECT

private slots:
    // ============================================================
    // A) forceCurrentPositionSelection 条件テスト
    // ============================================================

    /// 局面編集後（非平手）: forceCurrentPositionSelection が呼ばれるべき
    void forceCondition_nonHirateSfen_shouldForce()
    {
        // GameStartCoordinator::initializeGame() の条件を再現
        QString startSfenStr = kEditedSfen;
        QString* startSfenPtr = &startSfenStr;

        bool shouldForce = (startSfenPtr && !startSfenPtr->isEmpty() && *startSfenPtr != kHirateSfen);
        qDebug() << "startSfenStr =" << startSfenStr;
        qDebug() << "shouldForce =" << shouldForce;
        QVERIFY2(shouldForce, "forceCurrentPositionSelection should be called for non-hirate SFEN");
    }

    /// 平手の場合: forceCurrentPositionSelection は呼ばれない
    void forceCondition_hirateSfen_shouldNotForce()
    {
        QString startSfenStr = kHirateSfen;
        QString* startSfenPtr = &startSfenStr;

        bool shouldForce = (startSfenPtr && !startSfenPtr->isEmpty() && *startSfenPtr != kHirateSfen);
        QVERIFY2(!shouldForce, "forceCurrentPositionSelection should NOT be called for hirate SFEN");
    }

    /// 空の場合: forceCurrentPositionSelection は呼ばれない
    void forceCondition_emptySfen_shouldNotForce()
    {
        QString startSfenStr;
        QString* startSfenPtr = &startSfenStr;

        bool shouldForce = (startSfenPtr && !startSfenPtr->isEmpty() && *startSfenPtr != kHirateSfen);
        QVERIFY2(!shouldForce, "forceCurrentPositionSelection should NOT be called for empty SFEN");
    }

    /// startSfenが平手でも、履歴先頭が編集局面なら強制される（フォールバック）
    void forceCondition_hirateStartButEditedHistory_shouldForce()
    {
        QString startSfenStr = kHirateSfen;
        QStringList sfenRecord;
        sfenRecord.append(kEditedSfen);

        const auto isEditedStart = [&](const QString& raw) -> bool {
            const QString s = raw.trimmed();
            return !s.isEmpty()
                   && s != kHirateSfen
                   && s != QLatin1String("startpos");
        };

        bool shouldForce = false;
        if (isEditedStart(startSfenStr)) {
            shouldForce = true;
        } else if (!sfenRecord.isEmpty() && isEditedStart(sfenRecord.first())) {
            shouldForce = true;
        }

        QVERIFY2(shouldForce, "forceCurrentPositionSelection should use sfenRecord[0] as fallback");
    }

    // ============================================================
    // B) shouldStartFromCurrentPosition テスト
    // ============================================================

    /// 局面編集直後: startSfen == currentSfen, selectedPly == 0 → false
    void shouldStartFromCurrentPosition_afterEdit()
    {
        const bool result = PreStartCleanupHandler::shouldStartFromCurrentPosition(
            kEditedSfen, kEditedSfen, 0);
        qDebug() << "shouldStartFromCurrentPosition(editedSfen, editedSfen, 0) =" << result;
        QCOMPARE(result, false);
    }

    /// 途中局面から: selectedPly > 0 → true
    void shouldStartFromCurrentPosition_midGame()
    {
        const QString midGameSfen = QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 1");
        const bool result = PreStartCleanupHandler::shouldStartFromCurrentPosition(
            kHirateSfen, midGameSfen, 3);
        QCOMPARE(result, true);
    }

    // ============================================================
    // C) ensureBranchTreeRoot テスト（修正後）
    // ============================================================

    /// 既にルートがある場合、SFENが異なれば更新する
    void ensureBranchTreeRoot_updatesWhenDifferent()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        QCOMPARE(tree.root()->sfen(), kHirateSfen);

        // 修正後の ensureBranchTreeRoot ロジック
        QString startSfenStr = kEditedSfen;
        QString rootSfen = kHirateSfen;
        if (!startSfenStr.trimmed().isEmpty()) {
            rootSfen = startSfenStr;
        }

        if (tree.root() != nullptr && tree.root()->sfen() != rootSfen) {
            tree.clear();
            tree.setRootSfen(rootSfen);
        }

        qDebug() << "After update: root sfen =" << tree.root()->sfen();
        QCOMPARE(tree.root()->sfen(), kEditedSfen);
    }

    /// 既にルートがあり同じSFENの場合、更新しない
    void ensureBranchTreeRoot_noUpdateWhenSame()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kEditedSfen);

        QString startSfenStr = kEditedSfen;
        QString rootSfen = kHirateSfen;
        if (!startSfenStr.trimmed().isEmpty()) {
            rootSfen = startSfenStr;
        }

        bool updated = false;
        if (tree.root() != nullptr && tree.root()->sfen() != rootSfen) {
            tree.clear();
            tree.setRootSfen(rootSfen);
            updated = true;
        }

        QVERIFY(!updated);
        QCOMPARE(tree.root()->sfen(), kEditedSfen);
    }

    // ============================================================
    // D) sfenRecord シードテスト
    // ============================================================

    /// startingPosNumber==0: sfenRecord[0] に editedSfen が保持される
    void sfenRecordSeed_currentPosition_preservesEditedSfen()
    {
        QStringList sfenRecord;
        sfenRecord.append(kEditedSfen);
        int selectedPly = 0;
        int startingPosNumber = 0;
        QString startSfen = kEditedSfen;

        auto canonicalizeStart = [](const QString& sfen) -> QString {
            const QString t = sfen.trimmed();
            if (t.isEmpty() || t == QLatin1String("startpos")) {
                return kHirateSfen;
            }
            return t;
        };
        const QString seedSfen = canonicalizeStart(startSfen);

        if (startingPosNumber == 0 && !sfenRecord.isEmpty() && selectedPly >= 0) {
            int keepIdx = qBound(0, selectedPly, static_cast<int>(sfenRecord.size()) - 1);
            const int takeLen = keepIdx + 1;
            QStringList preserved;
            preserved.reserve(takeLen);
            for (int i = 0; i < takeLen; ++i) {
                preserved.append(sfenRecord.at(i));
            }
            if (!preserved.isEmpty()) {
                preserved[preserved.size() - 1] = seedSfen;
            }
            sfenRecord.clear();
            sfenRecord.append(preserved);
        } else {
            sfenRecord.clear();
            sfenRecord.append(seedSfen);
        }

        qDebug() << "sfenRecord[0] =" << sfenRecord.at(0);
        QCOMPARE(sfenRecord.at(0), kEditedSfen);
    }

    /// startingPosNumber==1 (平手): sfenRecord[0] は hirate になる
    void sfenRecordSeed_hiratePreset_becomesHirate()
    {
        QStringList sfenRecord;
        sfenRecord.append(kEditedSfen);
        int selectedPly = 0;
        int startingPosNumber = 1;
        QString startSfen = kHirateSfen;

        auto canonicalizeStart = [](const QString& sfen) -> QString {
            const QString t = sfen.trimmed();
            if (t.isEmpty() || t == QLatin1String("startpos")) {
                return kHirateSfen;
            }
            return t;
        };
        const QString seedSfen = canonicalizeStart(startSfen);

        if (startingPosNumber == 0 && !sfenRecord.isEmpty() && selectedPly >= 0) {
            // NOT taken for 平手
        } else {
            sfenRecord.clear();
            sfenRecord.append(seedSfen);
        }

        QCOMPARE(sfenRecord.at(0), kHirateSfen);
    }

    // ============================================================
    // E) resetBranchTreeForNewGame テスト
    // ============================================================

    void resetBranchTree_alwaysHirate()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kEditedSfen);

        // resetBranchTreeForNewGame のロジック
        tree.clear();
        tree.setRootSfen(kHirateSfen);

        QCOMPARE(tree.root()->sfen(), kHirateSfen);
        qDebug() << "★ resetBranchTreeForNewGame always sets root to hirate";
    }

    // ============================================================
    // F) RecordNavigation テスト
    // ============================================================

    void recordNavigation_row0_usesSfenRecord()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kEditedSfen);

        KifuNavigationState navState;
        navState.setTree(&tree);

        QStringList sfenRecord;
        sfenRecord.append(kEditedSfen);
        sfenRecord.append(QStringLiteral("sfen_after_move1"));

        int row = 0;
        bool handledByBranchTree = false;

        if (!navState.isOnMainLine() && tree.root() != nullptr) {
            handledByBranchTree = true;
        }

        QString displayedSfen;
        if (!handledByBranchTree && row >= 0 && row < sfenRecord.size()) {
            displayedSfen = sfenRecord.at(row);
        }

        qDebug() << "isOnMainLine =" << navState.isOnMainLine();
        qDebug() << "displayedSfen =" << displayedSfen;
        QCOMPARE(displayedSfen, kEditedSfen);
    }

    // ============================================================
    // G) LiveGameSession テスト
    // ============================================================

    void liveGameSession_rootSfenPreserved()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kEditedSfen);

        LiveGameSession session;
        session.setTree(&tree);
        session.startFromRoot();

        ShogiMove move;
        session.addMove(move, QStringLiteral("▲５八玉"),
                        QStringLiteral("sfen_after_move1"), QString());
        session.addMove(move, QStringLiteral("△５二玉"),
                        QStringLiteral("sfen_after_move2"), QString());

        auto* lastNode = session.commit();
        QVERIFY(lastNode != nullptr);
        QCOMPARE(tree.root()->sfen(), kEditedSfen);
    }

    // ============================================================
    // H) ★★ 完全フローテスト: 「現在局面」パス ★★
    // ============================================================

    void fullFlow_currentPosition_path()
    {
        qDebug() << "========================================";
        qDebug() << "FULL FLOW TEST: 「現在局面」パス";
        qDebug() << "(forceCurrentPositionSelection → index 0)";
        qDebug() << "========================================";

        // === Step 1: 局面編集後の状態 ===
        QString m_startSfenStr = kEditedSfen;
        QString m_currentSfenStr = kEditedSfen;
        QStringList m_sfenRecord;
        m_sfenRecord.append(kEditedSfen);
        int m_currentSelectedPly = 0;

        QCOMPARE(m_startSfenStr, kEditedSfen);
        QCOMPARE(m_sfenRecord.at(0), kEditedSfen);

        // === Step 2: forceCurrentPositionSelection 判定 ===
        bool shouldForce = (!m_startSfenStr.isEmpty() && m_startSfenStr != kHirateSfen);
        QVERIFY2(shouldForce, "forceCurrentPositionSelection should be triggered");
        int startingPosNumber = 0;  // forced to "現在の局面"

        // === Step 3: 分岐ツリー（アプリ起動時に初期化済み） ===
        KifuBranchTree branchTree;
        branchTree.setRootSfen(kHirateSfen);

        // === Step 4: performCleanup → ensureBranchTreeRoot (修正後) ===
        {
            QString rootSfen = kHirateSfen;
            if (!m_startSfenStr.trimmed().isEmpty()) {
                rootSfen = m_startSfenStr;
            }
            if (branchTree.root() != nullptr && branchTree.root()->sfen() != rootSfen) {
                branchTree.clear();
                branchTree.setRootSfen(rootSfen);
            }
        }
        QCOMPARE(branchTree.root()->sfen(), kEditedSfen);

        // === Step 5: prepareDataCurrentPosition → baseSfen ===
        QString baseSfen;
        if (!m_currentSfenStr.isEmpty()) {
            baseSfen = m_currentSfenStr;
        } else if (!m_startSfenStr.isEmpty()) {
            baseSfen = m_startSfenStr;
        } else {
            baseSfen = QStringLiteral("startpos");
        }
        QCOMPARE(baseSfen, kEditedSfen);

        // === Step 6: sfenRecord シード ===
        auto canonicalizeStart = [](const QString& sfen) -> QString {
            const QString t = sfen.trimmed();
            if (t.isEmpty() || t == QLatin1String("startpos")) {
                return kHirateSfen;
            }
            return t;
        };
        QString startSfen = baseSfen;
        const QString seedSfen = canonicalizeStart(startSfen);

        if (startingPosNumber == 0 && !m_sfenRecord.isEmpty() && m_currentSelectedPly >= 0) {
            int keepIdx = qBound(0, m_currentSelectedPly,
                                 static_cast<int>(m_sfenRecord.size()) - 1);
            QStringList preserved;
            for (int i = 0; i <= keepIdx; ++i) {
                preserved.append(m_sfenRecord.at(i));
            }
            if (!preserved.isEmpty()) {
                preserved[preserved.size() - 1] = seedSfen;
            }
            m_sfenRecord.clear();
            m_sfenRecord.append(preserved);
        }
        QCOMPARE(m_sfenRecord.at(0), kEditedSfen);

        // === Step 7: LiveGameSession + ゲーム ===
        LiveGameSession liveSession;
        liveSession.setTree(&branchTree);
        liveSession.startFromRoot();

        ShogiMove move;
        liveSession.addMove(move, QStringLiteral("▲５八玉"),
                            QStringLiteral("4k4/9/9/9/9/9/9/9/3K5 w - 2"), QString());
        m_sfenRecord.append(QStringLiteral("4k4/9/9/9/9/9/9/9/3K5 w - 2"));

        liveSession.addMove(move, QStringLiteral("△５二玉"),
                            QStringLiteral("3k5/9/9/9/9/9/9/9/3K5 b - 3"), QString());
        m_sfenRecord.append(QStringLiteral("3k5/9/9/9/9/9/9/9/3K5 b - 3"));

        auto* lastNode = liveSession.commit();
        QVERIFY(lastNode != nullptr);

        // === Step 8: ゲーム終了後、row 0 クリック ===
        KifuNavigationState navState;
        navState.setTree(&branchTree);

        QString displayedSfen;
        bool handledByBranch = false;

        if (!navState.isOnMainLine()) {
            const int lineIndex = navState.currentLineIndex();
            QVector<BranchLine> lines = branchTree.allLines();
            if (lineIndex >= 0 && lineIndex < lines.size()) {
                const BranchLine& line = lines.at(lineIndex);
                if (0 < line.nodes.size()) {
                    displayedSfen = line.nodes.at(0)->sfen();
                    handledByBranch = true;
                }
            }
        }
        if (!handledByBranch && !m_sfenRecord.isEmpty()) {
            displayedSfen = m_sfenRecord.at(0);
        }

        qDebug() << "RESULT: displayedSfen =" << displayedSfen;
        qDebug() << "EXPECTED: " << kEditedSfen;
        QCOMPARE(displayedSfen, kEditedSfen);

        qDebug() << "✓ 「現在局面」パスは正常に動作。";
        qDebug() << "  sfenRecord[0] = editedSfen ✓";
        qDebug() << "  branchTree root = editedSfen ✓";
    }

    // ============================================================
    // I) ★★ 完全フローテスト: 「平手」プリセットパス（バグ再現） ★★
    // ============================================================

    void fullFlow_hiratePreset_path_bugReproduction()
    {
        qDebug() << "========================================";
        qDebug() << "FULL FLOW TEST: 「平手」プリセットパス";
        qDebug() << "(forceCurrentPositionSelection 失敗時のバグ再現)";
        qDebug() << "========================================";

        // === Step 1: 局面編集後の状態 ===
        QString m_startSfenStr = kEditedSfen;
        QString m_currentSfenStr = kEditedSfen;
        QStringList m_sfenRecord;
        m_sfenRecord.append(kEditedSfen);

        // === Step 2: ダイアログで「平手」が選択された ===
        // (forceCurrentPositionSelection が効かなかった、
        //  またはユーザーが手動で「平手」に変更した場合)
        const int startingPosNumber = 1;
        Q_UNUSED(startingPosNumber);

        // === Step 3: prepareInitialPosition ===
        // line 328: *c.startSfenStr = sfen;
        // line 329: *c.currentSfenStr = sfen;
        m_startSfenStr = kHirateSfen;   // ← OVERWRITTEN!
        m_currentSfenStr = kHirateSfen;  // ← OVERWRITTEN!

        qDebug() << "★ prepareInitialPosition OVERWRITES m_startSfenStr to hirate!";
        qDebug() << "  m_startSfenStr is now:" << m_startSfenStr;

        // === Step 4: sfenRecord in prepareInitialPosition ===
        // line 359-360: c.sfenRecord->clear(); c.sfenRecord->append(sfen);
        m_sfenRecord.clear();
        m_sfenRecord.append(kHirateSfen);

        qDebug() << "★ prepareInitialPosition CLEARS sfenRecord and sets to hirate!";
        qDebug() << "  sfenRecord[0] is now:" << m_sfenRecord.at(0);

        // === Step 5: resetBranchTreeForNewGame ===
        KifuBranchTree branchTree;
        branchTree.setRootSfen(kHirateSfen);

        // === Step 6: sfenRecord seed (else パス) ===
        QString startSfen = kHirateSfen;
        auto canonicalizeStart = [](const QString& sfen) -> QString {
            const QString t = sfen.trimmed();
            if (t.isEmpty() || t == QLatin1String("startpos")) {
                return kHirateSfen;
            }
            return t;
        };
        const QString seedSfen = canonicalizeStart(startSfen);

        // startingPosNumber != 0 → else パス
        m_sfenRecord.clear();
        m_sfenRecord.append(seedSfen);

        // === Step 7: ゲーム終了後、row 0 クリック ===
        const QString displayedSfen = m_sfenRecord.at(0);

        qDebug() << "RESULT: displayedSfen =" << displayedSfen;
        qDebug() << "★★★ THIS IS HIRATE - the edited position is completely lost! ★★★";
        QCOMPARE(displayedSfen, kHirateSfen);

        qDebug() << "";
        qDebug() << "==========================================";
        qDebug() << "CONCLUSION:";
        qDebug() << "==========================================";
        qDebug() << "ダイアログで「平手」が選択されると、";
        qDebug() << "prepareInitialPosition() が:";
        qDebug() << "  1. m_startSfenStr を平手に上書き";
        qDebug() << "  2. m_currentSfenStr を平手に上書き";
        qDebug() << "  3. sfenRecord を [平手] にリセット";
        qDebug() << "  4. resetBranchTreeForNewGame で分岐ツリーも平手にリセット";
        qDebug() << "";
        qDebug() << "→ 編集した局面は完全に消失する！";
        qDebug() << "";
        qDebug() << "修正の鍵: forceCurrentPositionSelection() が確実に";
        qDebug() << "startingPosNumber=0 を返すようにする必要がある。";
    }

    // ============================================================
    // J) ★★ 統合テスト ★★
    // ============================================================

    void integration_fullFlow_withLiveGameSession()
    {
        qDebug() << "========================================";
        qDebug() << "INTEGRATION TEST: Full flow with LiveGameSession";
        qDebug() << "========================================";

        // --- Setup: 局面編集後の状態 ---
        QString m_startSfenStr = kEditedSfen;
        QString m_currentSfenStr = kEditedSfen;
        QStringList m_sfenRecord;
        m_sfenRecord.append(kEditedSfen);
        int m_currentSelectedPly = 0;

        // --- Dialog: startingPosNumber = 0 ---
        const int startingPosNumber = 0;

        // --- Branch tree: アプリ起動時のデフォルト ---
        KifuBranchTree branchTree;
        branchTree.setRootSfen(kHirateSfen);

        // --- performCleanup → ensureBranchTreeRoot (修正後) ---
        {
            QString rootSfen = kHirateSfen;
            if (!m_startSfenStr.trimmed().isEmpty()) {
                rootSfen = m_startSfenStr;
            }
            if (branchTree.root() != nullptr && branchTree.root()->sfen() != rootSfen) {
                branchTree.clear();
                branchTree.setRootSfen(rootSfen);
            }
        }
        QCOMPARE(branchTree.root()->sfen(), kEditedSfen);

        // --- startLiveGameSession ---
        LiveGameSession liveSession;
        liveSession.setTree(&branchTree);
        liveSession.startFromRoot();

        // --- sfenRecord seed ---
        auto canonicalizeStart = [](const QString& sfen) -> QString {
            const QString t = sfen.trimmed();
            if (t.isEmpty() || t == QLatin1String("startpos")) {
                return kHirateSfen;
            }
            return t;
        };
        const QString seedSfen = canonicalizeStart(m_currentSfenStr);

        if (startingPosNumber == 0 && !m_sfenRecord.isEmpty() && m_currentSelectedPly >= 0) {
            int keepIdx = qBound(0, m_currentSelectedPly,
                                 static_cast<int>(m_sfenRecord.size()) - 1);
            QStringList preserved;
            for (int i = 0; i <= keepIdx; ++i) {
                preserved.append(m_sfenRecord.at(i));
            }
            if (!preserved.isEmpty()) {
                preserved[preserved.size() - 1] = seedSfen;
            }
            m_sfenRecord.clear();
            m_sfenRecord.append(preserved);
        }
        QCOMPARE(m_sfenRecord.at(0), kEditedSfen);

        // --- ゲーム中の手 ---
        ShogiMove move;
        liveSession.addMove(move, QStringLiteral("▲５八玉"),
                            QStringLiteral("4k4/9/9/9/9/9/9/9/3K5 w - 2"), QString());
        m_sfenRecord.append(QStringLiteral("4k4/9/9/9/9/9/9/9/3K5 w - 2"));

        liveSession.addMove(move, QStringLiteral("△５二玉"),
                            QStringLiteral("3k5/9/9/9/9/9/9/9/3K5 b - 3"), QString());
        m_sfenRecord.append(QStringLiteral("3k5/9/9/9/9/9/9/9/3K5 b - 3"));

        auto* lastNode = liveSession.commit();
        QVERIFY(lastNode != nullptr);

        // --- ゲーム終了後 ---
        qDebug() << "After game:";
        qDebug() << "  sfenRecord size =" << m_sfenRecord.size();
        for (int i = 0; i < m_sfenRecord.size(); ++i) {
            qDebug() << "  sfenRecord[" << i << "] =" << m_sfenRecord.at(i).left(50);
        }
        qDebug() << "  branchTree root =" << branchTree.root()->sfen();

        // --- row 0 クリック ---
        KifuNavigationState navState;
        navState.setTree(&branchTree);

        QString displayedSfen;
        bool handledByBranch = false;

        if (!navState.isOnMainLine()) {
            const int lineIndex = navState.currentLineIndex();
            QVector<BranchLine> lines = branchTree.allLines();
            if (lineIndex >= 0 && lineIndex < lines.size()) {
                const BranchLine& line = lines.at(lineIndex);
                if (!line.nodes.isEmpty()) {
                    displayedSfen = line.nodes.at(0)->sfen();
                    handledByBranch = true;
                }
            }
        }
        if (!handledByBranch && !m_sfenRecord.isEmpty()) {
            displayedSfen = m_sfenRecord.at(0);
        }

        qDebug() << "";
        qDebug() << "=== FINAL RESULT ===";
        qDebug() << "displayedSfen =" << displayedSfen;
        qDebug() << "expected      =" << kEditedSfen;
        qDebug() << "match         =" << (displayedSfen == kEditedSfen);

        QCOMPARE(displayedSfen, kEditedSfen);
        QCOMPARE(branchTree.root()->sfen(), kEditedSfen);

        qDebug() << "✓ ALL CHECKS PASSED";
        qDebug() << "";
        qDebug() << "If the real app STILL shows hirate after this test passes,";
        qDebug() << "the issue is that startingPosNumber is NOT 0 in the real flow.";
        qDebug() << "This means forceCurrentPositionSelection() is not having the";
        qDebug() << "expected effect on startingPositionNumber().";
        qDebug() << "Possible causes:";
        qDebug() << "  1. The dialog's combobox change doesn't update m_startingPositionNumber";
        qDebug() << "     until updateGameSettingsFromDialog() is called (on accept)";
        qDebug() << "  2. Something else resets the combobox after forceCurrentPositionSelection()";
        qDebug() << "  3. m_startSfenStr is empty or hirate at the time of the check";
    }
};

QTEST_GUILESS_MAIN(TestPositionEditGameStart)
#include "tst_positionedit_gamestart.moc"
