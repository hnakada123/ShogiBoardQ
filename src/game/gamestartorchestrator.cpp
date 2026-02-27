/// @file gamestartorchestrator.cpp
/// @brief 対局開始フローを管理するオーケストレータの実装

#include "gamestartorchestrator.h"
#include "shogigamecontroller.h"
#include "startgamedialog.h"
#include "settingscommon.h"
#include "sfenpositiontracer.h"
#include "logcategories.h"

#include <QDebug>
#include <QSettings>

// ============================================================
// configureAndStart 分解用ユーティリティ（static helper）
// ============================================================

/// 探索対象SFENを正規化する（"startpos"→初期SFEN、"position sfen ..."→素のSFEN）
static QString normalizeTargetSfen(const QString& sfenStart)
{
    QString target = sfenStart.trimmed();
    if (target == QLatin1String("startpos")) {
        return QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    }
    if (target.startsWith(QLatin1String("position sfen "))) {
        return target.mid(14).trimmed();
    }
    return target;
}

/// SFEN文字列から開始手番を決定する
static ShogiGameController::Player decideStartSideFromSfen(const QString& sfen)
{
    auto start = ShogiGameController::Player1;
    if (sfen.isEmpty()) return start;
    const QStringList tok = sfen.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (tok.size() < 2) return start;

    qsizetype sideIdx = -1;
    if (tok.size() >= 4
        && tok[0] == QLatin1String("position")
        && tok[1] == QLatin1String("sfen")) {
        sideIdx = 3; // "position sfen <board> <side> ..."
    } else {
        sideIdx = 1; // "<board> <side> ..."
    }

    if (sideIdx >= 0 && sideIdx < tok.size()) {
        if (tok[sideIdx].compare(QLatin1String("w"), Qt::CaseInsensitive) == 0) {
            start = ShogiGameController::Player2; // 後手番
        }
    }
    return start;
}

/// SFEN末尾の手数フィールドから、保持すべき手数を計算する（N → N-1）
static int parseKeepMovesFromSfen(const QString& sfenLike)
{
    QString s = sfenLike.trimmed();
    if (s.startsWith(QLatin1String("position sfen "))) {
        s = s.mid(QStringLiteral("position sfen ").size()).trimmed();
    }
    const QStringList tok = s.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (tok.size() >= 4) {
        bool ok = false;
        const int mv = tok.last().toInt(&ok);
        if (ok) return qMax(0, mv - 1);
    }
    return -1;
}

/// "position ... moves ..." の手順を先頭 keep 手だけ残してトリムする
static QString trimMovesPreserveHeader(const QString& full, int keep)
{
    const QString movesKey = QStringLiteral(" moves ");
    const qsizetype pos = full.indexOf(movesKey);
    QString head = full.trimmed();
    QString tail;
    if (pos >= 0) {
        head = full.left(pos).trimmed();
        tail = full.mid(pos + movesKey.size()).trimmed();
    }
    if (keep <= 0 || tail.isEmpty()) return head;
    QStringList mv = tail.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (keep < mv.size()) mv = mv.mid(0, keep);
    if (mv.isEmpty()) return head;
    return head + movesKey + mv.join(QLatin1Char(' '));
}

/// position文字列中のmoves数をカウントする
static int countMovesInPositionStr(const QString& posStr)
{
    const qsizetype idx = posStr.indexOf(QLatin1String(" moves "));
    if (idx < 0) return 0;
    const QString after = posStr.mid(idx + 7).trimmed();
    if (after.isEmpty()) return 0;
    return static_cast<int>(after.split(QLatin1Char(' '), Qt::SkipEmptyParts).size());
}

/// "position sfen" 接頭辞対応の平手初期局面判定（手数フィールドは無視）
static bool hasStandardStartposBoard(const QString& sfenLike)
{
    QString s = sfenLike.trimmed();
    if (s.startsWith(QLatin1String("position sfen "))) {
        s = s.mid(QStringLiteral("position sfen ").size()).trimmed();
    }
    const QStringList tok = s.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (tok.size() < 3) return false;
    static const QString kStartBoard =
        QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL");
    return (tok[0] == kStartBoard
            && tok[1] == QLatin1String("b")
            && tok[2] == QLatin1String("-"));
}

// ============================================================
// setRefs / setHooks
// ============================================================

void GameStartOrchestrator::setRefs(const Refs& refs)
{
    m_refs = refs;
}

void GameStartOrchestrator::setHooks(const Hooks& hooks)
{
    m_hooks = hooks;
}

// ============================================================
// 対局開始フロー
// ============================================================

void GameStartOrchestrator::configureAndStart(const StartOptions& opt)
{
    // 1. 既存履歴同期と探索
    const QString targetSfen = normalizeTargetSfen(opt.sfenStart);
    const HistorySearchResult searchResult = syncAndSearchGameHistory(targetSfen);

    // 2. フック呼び出し / UI初期化
    applyStartOptionsAndHooks(opt);

    // 3. 開始SFEN正規化とposition文字列構築
    buildPositionStringsFromHistory(opt, targetSfen, searchResult);

    // 4. PlayMode別起動
    if (m_hooks.createAndStartModeStrategy) m_hooks.createAndStartModeStrategy(opt);

    qCDebug(lcGame).noquote()
        << "configureAndStart: LEAVE m_positionStr1(final)=" << *m_refs.positionStr1
        << "hist.size=" << m_refs.positionStrHistory->size()
        << "hist.last=" << (m_refs.positionStrHistory->isEmpty()
                                ? QString("<empty>")
                                : m_refs.positionStrHistory->constLast());
}

// ------------------------------------------------------------------
// configureAndStart 分解: 1. 既存履歴同期と探索
// ------------------------------------------------------------------
GameStartOrchestrator::HistorySearchResult
GameStartOrchestrator::syncAndSearchGameHistory(const QString& targetSfen)
{
    // 直前の対局で更新された positionStr1 を履歴に反映してから保存
    if (!m_refs.positionStr1->isEmpty()
        && m_refs.positionStr1->startsWith(QLatin1String("position "))) {
        if (m_refs.positionStrHistory->isEmpty()) {
            m_refs.positionStrHistory->append(*m_refs.positionStr1);
        } else if (m_refs.positionStrHistory->constLast() != *m_refs.positionStr1) {
            (*m_refs.positionStrHistory)[m_refs.positionStrHistory->size() - 1] = *m_refs.positionStr1;
        }
        qCDebug(lcGame).noquote()
            << "configureAndStart: synced m_positionStrHistory with m_positionStr1="
            << *m_refs.positionStr1;
    }

    // 直前の対局履歴が残っていれば、確定した過去の対局として蓄積
    if (!m_refs.positionStrHistory->isEmpty()) {
        m_refs.allGameHistories->append(*m_refs.positionStrHistory);
        while (m_refs.allGameHistories->size() > kMaxGameHistories) {
            m_refs.allGameHistories->removeFirst();
        }
    }

    // --- 過去対局のどれをベースに切り詰めるかを探索 ---
    HistorySearchResult result;

    if (m_refs.allGameHistories->isEmpty()) return result;

    qCDebug(lcGame) << "=== Accumulated Game Histories (Count:"
                     << m_refs.allGameHistories->size() << ") ===";
    for (qsizetype i = 0; i < m_refs.allGameHistories->size(); ++i) {
        qCDebug(lcGame) << " [Game" << (i + 1) << "]";
        const QStringList& rec = m_refs.allGameHistories->at(i);
        for (const QString& pos : rec) {
            qCDebug(lcGame).noquote() << "  " << pos;
        }
    }
    qCDebug(lcGame) << "=======================================================";
    qCDebug(lcGame) << "--- Searching for start position in previous games ---";

    for (qsizetype i = 0; i < m_refs.allGameHistories->size(); ++i) {
        const QStringList& hist = m_refs.allGameHistories->at(i);
        if (hist.isEmpty()) continue;

        const QString fullCmd = hist.last();
        SfenPositionTracer tracer;
        QStringList moves;

        if (fullCmd.startsWith(QLatin1String("position startpos"))) {
            tracer.resetToStartpos();
            if (fullCmd.contains(QLatin1String(" moves "))) {
                moves = fullCmd.section(QLatin1String(" moves "), 1)
                    .split(QLatin1Char(' '), Qt::SkipEmptyParts);
            }
        } else if (fullCmd.startsWith(QLatin1String("position sfen "))) {
            const qsizetype mIdx = fullCmd.indexOf(QLatin1String(" moves "));
            const QString sfenPart = (mIdx == -1) ? fullCmd.mid(14)
                                                  : fullCmd.mid(14, mIdx - 14);
            tracer.setFromSfen(sfenPart);
            if (mIdx != -1) {
                moves = fullCmd.mid(mIdx + 7).split(QLatin1Char(' '), Qt::SkipEmptyParts);
            }
        }

        // 0手目（開始局面）の比較
        if (tracer.toSfenString() == targetSfen) {
            qCDebug(lcGame).noquote()
                << QString(" -> MATCH FOUND: [Game %1] Start Position (Move 0)").arg(i + 1);
            if (result.bestGameIdx == -1) {
                result.bestGameIdx  = static_cast<int>(i);
                result.bestBaseFull = fullCmd;
                result.bestMatchPly = 0;
            }
        }

        // 1手ずつ進めて比較
        for (qsizetype m = 0; m < moves.size(); ++m) {
            tracer.applyUsiMove(moves[m]);
            if (tracer.toSfenString() == targetSfen) {
                qCDebug(lcGame).noquote()
                    << QString(" -> MATCH FOUND: [Game %1] Move %2").arg(i + 1).arg(m + 1);
                if (result.bestGameIdx == -1) {
                    result.bestGameIdx  = static_cast<int>(i);
                    result.bestBaseFull = fullCmd;
                    result.bestMatchPly = static_cast<int>(m + 1);
                }
            }
        }
    }
    qCDebug(lcGame) << "----------------------------------------------------";

    return result;
}

// ------------------------------------------------------------------
// configureAndStart 分解: 2. フック呼び出し / UI初期化
// ------------------------------------------------------------------
void GameStartOrchestrator::applyStartOptionsAndHooks(const StartOptions& opt)
{
    // 前回の対局終了状態をクリア（再対局時に棋譜追加がブロックされる問題を修正）
    if (m_hooks.clearGameOverState) m_hooks.clearGameOverState();

    *m_refs.playMode = opt.mode;
    *m_refs.maxMoves = opt.maxMoves;

    // 棋譜自動保存設定を保存
    *m_refs.autoSaveKifu = opt.autoSaveKifu;
    *m_refs.kifuSaveDir = opt.kifuSaveDir;
    *m_refs.humanName1 = opt.humanName1;
    *m_refs.humanName2 = opt.humanName2;
    *m_refs.engineNameForSave1 = opt.engineName1;
    *m_refs.engineNameForSave2 = opt.engineName2;

    // 盤・名前などの初期化（GUI側へ委譲）
    qCDebug(lcGame).noquote() << "configureAndStart: calling hooks";
    qCDebug(lcGame).noquote()
        << "configureAndStart: opt.engineName1=" << opt.engineName1
        << " opt.engineName2=" << opt.engineName2;
    if (m_hooks.initializeNewGame) m_hooks.initializeNewGame(opt.sfenStart);
    if (m_hooks.setPlayersNames) {
        qCDebug(lcGame).noquote()
            << "configureAndStart: calling setPlayersNames(\"\", \"\")";
        m_hooks.setPlayersNames(QString(), QString());
    }
    if (m_hooks.setEngineNames) {
        qCDebug(lcGame).noquote()
            << "configureAndStart: calling setEngineNames("
            << opt.engineName1 << "," << opt.engineName2 << ")";
        m_hooks.setEngineNames(opt.engineName1, opt.engineName2);
    }
    if (m_hooks.setGameActions) m_hooks.setGameActions(true);
    qCDebug(lcGame).noquote() << "configureAndStart: hooks done";

    // 開始手番の決定
    const ShogiGameController::Player startSide = decideStartSideFromSfen(opt.sfenStart);
    if (m_refs.gc) m_refs.gc->setCurrentPlayer(startSide);
    if (m_hooks.renderBoardFromGc) m_hooks.renderBoardFromGc();

    // HvE/HvH 共通の安全策
    *m_refs.currentMoveIndex = 0;

    // 司令塔の内部手番も GC に同期（既存互換）
    *m_refs.currentTurn =
        (startSide == ShogiGameController::Player2) ? MatchCoordinator::P2
                                                    : MatchCoordinator::P1;
    if (m_hooks.updateTurnDisplay) m_hooks.updateTurnDisplay(*m_refs.currentTurn);
}

// ------------------------------------------------------------------
// configureAndStart 分解: 3. 開始SFEN正規化とposition文字列構築
// ------------------------------------------------------------------
void GameStartOrchestrator::buildPositionStringsFromHistory(
    const StartOptions& opt,
    const QString& /*targetSfen*/,
    const HistorySearchResult& searchResult)
{
    const int keepMoves = parseKeepMovesFromSfen(opt.sfenStart);
    qCDebug(lcGame).noquote()
        << "configureAndStart: keepMoves(from sfenStart)=" << keepMoves;

    // --- ベース候補：探索一致ゲームのみ採用
    QString candidateBase;
    if (!searchResult.bestBaseFull.isEmpty()) {
        candidateBase = searchResult.bestBaseFull;
        qCDebug(lcGame).noquote()
            << "configureAndStart: base=matched game"
            << "gameIdx=" << (searchResult.bestGameIdx + 1)
            << "matchPly=" << searchResult.bestMatchPly
            << "full=" << candidateBase;
    }

    bool applied = false;
    if (!candidateBase.isEmpty() && keepMoves >= 0) {
        if (candidateBase.startsWith(QLatin1String("position "))) {
            const QString trimmed = trimMovesPreserveHeader(candidateBase, keepMoves);
            qCDebug(lcGame).noquote()
                << "configureAndStart: trimmed(base, keep=" << keepMoves
                << ")=" << trimmed;

            const int actualMoves = countMovesInPositionStr(trimmed);

            if (actualMoves >= keepMoves) {
                *m_refs.positionStr1     = trimmed;
                *m_refs.positionPonder1  = trimmed;

                // 履歴はベースを差し替えて 1 件から再スタート（UNDO用）
                m_refs.positionStrHistory->clear();
                m_refs.positionStrHistory->append(trimmed);

                applied = true;
                qCDebug(lcGame).noquote()
                    << "configureAndStart: applied=true (actualMoves="
                    << actualMoves << " >= keepMoves=" << keepMoves << ")";
            } else {
                qCDebug(lcGame).noquote()
                    << "configureAndStart: candidateBase has insufficient moves (actualMoves="
                    << actualMoves << " < keepMoves=" << keepMoves
                    << "), falling back to SFEN";
            }
        } else {
            qCWarning(lcGame).noquote()
                << "configureAndStart: candidateBase is not 'position ...' :"
                << candidateBase;
        }
    }

    // ---- 履歴／一致ベースが使えないときのフォールバック
    if (!applied) {
        // 従来の初期化（position sfen <sfen> ... をそのまま使う）
        if (m_hooks.initializePositionStringsForStart)
            m_hooks.initializePositionStringsForStart(opt.sfenStart);

        // さらに、平手startposかつ明示のkeepMoves>0 なら、startpos形式で再構築
        if (hasStandardStartposBoard(opt.sfenStart) && keepMoves > 0) {
            *m_refs.positionStr1     = QStringLiteral("position startpos");
            *m_refs.positionPonder1  = *m_refs.positionStr1;
            m_refs.positionStrHistory->clear();
            m_refs.positionStrHistory->append(*m_refs.positionStr1);
        }
    }
}

// ============================================================
// 対局開始オプション構築（static — MC 内部状態に依存しない）
// ============================================================

MatchCoordinator::StartOptions GameStartOrchestrator::buildStartOptions(
    PlayMode mode,
    const QString& startSfenStr,
    const QStringList* sfenRecord,
    const StartGameDialog* dlg)
{
    MatchCoordinator::StartOptions opt;
    opt.mode = mode;

    // --- 開始SFEN（空なら既定=司令塔側で startpos 扱い）
    if (!startSfenStr.isEmpty()) {
        opt.sfenStart = startSfenStr;
    } else if (sfenRecord && !sfenRecord->isEmpty()) {
        opt.sfenStart = sfenRecord->first();
    } else {
        opt.sfenStart.clear();
    }

    // --- エンジン座席（PlayMode から）
    const bool engineIsP1 =
        (mode == PlayMode::EvenEngineVsHuman) ||
        (mode == PlayMode::HandicapEngineVsHuman);
    opt.engineIsP1 = engineIsP1;
    opt.engineIsP2 = !engineIsP1;

    // --- 対局ダイアログあり：そのまま採用
    if (dlg) {
        const auto engines = dlg->getEngineList();

        const int idx1 = dlg->engineNumber1();
        if (idx1 >= 0 && idx1 < engines.size()) {
            opt.engineName1 = dlg->engineName1();
            opt.enginePath1 = engines.at(idx1).path;
        }

        const int idx2 = dlg->engineNumber2();
        if (idx2 >= 0 && idx2 < engines.size()) {
            opt.engineName2 = dlg->engineName2();
            opt.enginePath2 = engines.at(idx2).path;
        }

        // 最大手数を取得
        opt.maxMoves = dlg->maxMoves();

        // 棋譜自動保存設定を取得
        opt.autoSaveKifu = dlg->isAutoSaveKifu();
        opt.kifuSaveDir = dlg->kifuSaveDir();

        // 対局者名を取得
        opt.humanName1 = dlg->humanName1();
        opt.humanName2 = dlg->humanName2();

        return opt;
    }

    // --- 対局ダイアログなし：INI から直近選択を復元（StartGameDialog と同じ仕様）
    {
        QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);

        // 1) エンジン一覧（name/path）の読み出し
        struct Eng { QString name; QString path; };
        QVector<Eng> list;
        int count = settings.beginReadArray("Engines");
        for (int i = 0; i < count; ++i) {
            settings.setArrayIndex(i);
            Eng e;
            e.name = settings.value("name").toString();
            e.path = settings.value("path").toString();
            list.push_back(e);
        }
        settings.endArray();

        auto pathForName = [&](const QString& nm) -> QString {
            if (nm.isEmpty()) return {};
            for (const auto& e : std::as_const(list)) {
                if (e.name == nm) return e.path;
            }
            return {};
        };

        // 2) 直近の対局設定（GameSettings）からエンジン名を取得
        settings.beginGroup("GameSettings");
        const QString name1 = settings.value("engineName1").toString();
        const QString name2 = settings.value("engineName2").toString();
        settings.endGroup();

        if (!name1.isEmpty()) {
            opt.engineName1 = name1;
            opt.enginePath1 = pathForName(name1);
        }
        if (!name2.isEmpty()) {
            opt.engineName2 = name2;
            opt.enginePath2 = pathForName(name2);
        }
        // パスが空でもここでは許容（initializeAndStartEngineCommunication 内で失敗は通知）
        return opt;
    }
}

// ============================================================
// 盤面反転・対局準備
// ============================================================

void GameStartOrchestrator::ensureHumanAtBottomIfApplicable(
    const StartGameDialog* dlg, bool bottomIsP1)
{
    if (!dlg) return;

    // 「人を手前に表示する」がチェックされていない場合は何もしない
    if (!dlg->isShowHumanInFront()) {
        return;
    }

    const bool humanP1  = dlg->isHuman1();
    const bool humanP2  = dlg->isHuman2();
    const bool oneHuman = (humanP1 ^ humanP2); // HvE / EvH のときだけ true

    if (!oneHuman) {
        // HvH / EvE は対象外（仕様どおり）
        return;
    }

    // 「現在の向き（bottomIsP1）」と「人間が先手か？」が食い違っていたら1回だけ反転
    const bool needFlip = (humanP1 != bottomIsP1);
    if (needFlip) {
        if (m_hooks.flipBoard) m_hooks.flipBoard();
    }
}

// ============================================================
// 対局開始メインエントリ
// ============================================================

void GameStartOrchestrator::prepareAndStartGame(PlayMode mode,
                                                 const QString& startSfenStr,
                                                 const QStringList* sfenRecord,
                                                 const StartGameDialog* dlg,
                                                 bool bottomIsP1)
{
    // 1) オプション構築
    StartOptions opt = buildStartOptions(mode, startSfenStr, sfenRecord, dlg);

    // 2) 人間を手前へ（必要なら反転）
    ensureHumanAtBottomIfApplicable(dlg, bottomIsP1);

    // 3) 対局の構成と開始
    configureAndStart(opt);

    // 4) 初手がエンジン手番なら go→bestmove
    if (m_hooks.startInitialEngineMoveIfNeeded) m_hooks.startInitialEngineMoveIfNeeded();
}
