# コード監査：抜本的な書き直しが見合う箇所（2026-10-16）

「本来はもっと綺麗に実装できるのにコードが汚く、抜本的に書き直した方が良い箇所はあるか」という依頼で、`src/` 全体（約8.5万行）と `mcp/` を監査した。直近の監査（5282965f・54ca37d7）で整理した後の状態が対象である。細かいスタイルや小さなバグではなく、データ構造や責務の切り方を変える書き直しが見合う箇所を探した。

- 対象のコミットは 54ca37d7。本文の行番号はこの時点のもの。
- 6つの範囲（core・kifu／game・engine・network・analysis／app・ui・navigation・board／widgets・views・models／dialogs／services・common・automation・cli・mcp）に分けてコードを読み、呼び出し元と重複箇所を grep で確かめた。不具合は `shogiboardq-cli` で再現できるものは再現した。
- 監査ではソースを変更していない。

> **修正の進み具合**: 6章の順に直し、各項目に「状態: 済（コミット）」を書く。途中で止まったら、状態のない項目から再開する。
> 直す前に今のコードで確かめ、メモと違っていた点は「（確認: …）」として書き足した。

## 全体の傾向

- 文字どおりの重複はほぼない。コメント・空行・括弧だけの行を除いて6行以上同じ並びが別ファイルにあるのは2組だけだった（`fmvlegalcore_internal.h` の `charToPieceType` と `fmvconverter.cpp` の `pieceToPieceType`、KIF と KI2 の Exporter のヘッダー部分）。
- 残っている問題は構造で、次の3つに分けられる。
  1. **値の型がない**: 局面・探索する局面・1手を、SFEN や position コマンドの文字列、QObject の盤（`ShogiBoard`）で受け渡している。
  2. **正本が複数ある**: 同じ情報を何か所もが持ち、手で揃えている（検討中か、評価値、対局者、持ち時間、局面編集中か、エンジン設定など）。
  3. **同じ流れを場所ごとに書いている**: ローカル対局と通信対局、go mate の5つの利用者、A-/A+ の9通りの実装、エンジン選択の7か所など。
- 見つかった不具合（1章）の多くは、この構造から直接生まれている。
- 複数の範囲の監査で別々に見つかった問題がある（文字の大きさの仕組み、エンジン設定の保存、駒セットの一覧、go mate の重複、評価値グラフの書き手）。これらは確度が高い。

---

## 1. 見つかった不具合

書き直しとは別に、個別に直せる。

### 1.1 再現・確認したもの

| # | 不具合 | 原因 | 確認 |
|---|---|---|---|
| B1 | KIF・KI2 に「パス」の手があると棋譜全体が読めない（`Cannot restore move 2: Invalid USI move: pass`） | `kiflexer.cpp:255`・`ki2tosfenconverter.cpp:113` が文字列 `"pass"` を積み、`parsecommon.cpp` の `resolve()` が `UsiMove` として読めずに全体を失敗にする。パスのテストはない | CLI で再現。<br>**状態: 済（455ec224）**。パスの手の手前までを読み、警告を出す。読めない手・指せない手も全形式で同じく打ち切る（`truncationNotice()`、`docs/dev/kifu-io-features.md`） |
| B2 | 同じ手順でも、読み込んだ形式で KIF の出力が変わる。KI2 から読むと「同銀(31)」、USI から読むと「同　銀(31)」。「５二金右(61)」と「５二金(61)」も同様 | KIF の書き出しだけが `RecordEntry::displayText` をそのまま書く（`kifexporter.cpp:88-91`）。`displayText` の作り方は入力形式ごとに違う（KIF は原文、KI2 は原文＋`(xy)`、ほかは `KifMoveText::format`） | CLI で再現。<br>**状態: 済（455ec224）**。KIF の書き出しも指し手と直前の手から表記を作る（成れたのに成らない手は「不成」。`KifMoveText::couldPromote`） |
| B3 | CSA で局面に合わない手を読み飛ばすと、以降の手番がずれる。`+7776FU -3334FU +1112HI -8384FU +2726FU` を読むと、`+1112HI` を飛ばした後の `-8384FU` が3手目（先手番）になる。CSA に書き出すと `+8384FU`・`-2726FU`、終局は「△投了」。エラーも警告も返らない | `csatosfenconverter.cpp:290-294` は読めない手を飛ばして読み続け、手番を指し手の `+`/`-` から確かめない | CLI で再現（確認: 警告 `Move does not match the position` は返っていた。読み込みは成功し、手番がずれる）。<br>**状態: 済（455ec224）**。読めない手と手番と違う符号の手の手前で打ち切る（KIF と同じ） |
| B4 | 英語・中国語の表示で手合割「左七枚落ち」「右七枚落ち」だけが日本語のまま出る | `kifupresentation.cpp:248-256` の手合割の訳語の表は `core/handicaps.h:22-37` の写しで、この2つが抜けている。`ShogiBoardQ_en.ts` にも訳がない | コードで確認。<br>**状態: 済（455ec224）**。写しをやめ、`handicaps.h` の名前を `QT_TRANSLATE_NOOP` で訳の対象にして表示はその表から引く（3章の「手合割の表示名」も兼ねる） |
| B5 | 通信対局の終局でライブ記録が確定されない。終局後も `LiveGameSession` が有効のまま残り、次の3つが起きる。「新規」か次の対局で破棄されるまで続く。<br>・盤で変化の手を指せない（`HumanTurnPolicy::isHumanTurnNow` が false）<br>・分岐の編集メニューが無効（`BranchTreeEditController::canEdit()` が false）<br>・評価値グラフの縦線が棋譜の移動に付いてこない（`EvaluationGraphPresenter::showCurrentPly` が早く抜ける） | `LiveGameSession::commit()` を呼ぶのは `gamesession.cpp:279`（`GameSession::onGameEnded`、ローカル対局）だけ。通信対局の終局は `CsaGameSession::onGameEnded` → フック `recordGameEnd` → `GameSession::recordGameEndTime` で、終了時刻を書くだけで commit しない。`tst_csagamesession`・`tst_csagamecoordinator` は終局後のライブ記録の状態を確かめていない | コードの経路で確認（実行はしていない）。<br>**状態: 済（455ec224）**。通信対局の終局でも `GameSession::endCsaGameRecord()` がライブ記録を確定する（`tst_app_game_session::csaGameEndCommitsLiveRecord`） |
| B6 | 時間のある対局の後に別の棋譜を読み込み、CSA で保存・コピーすると、前の対局の `$TIME` が書かれる。読み込んだ棋譜に開始日時がなければ、前の対局の `$START_TIME` も書かれる | `MatchTimeRecord` の持ち時間は `begin()` 以外で消えない。開始日時を消すのも `SessionResetCoordinator::prepareGameStart` の `clearStart()` だけ。棋譜の読み込み（`clearUiBeforeKifuLoad` ＝ `SessionResetCoordinator::clearBeforeKifuLoad`）は表示を消すだけ。CSA の書き出しは `ctx.timeControl` に持ち時間があると対局情報の持ち時間を捨て、`ctx` の値で `$TIME` を書く（`csaexporter.cpp:96-97, 122-134`） | コードの経路で確認（実行はしていない）。<br>**状態: 済（455ec224）**。棋譜を読み込むと `GameSession::forgetFinishedMatch()` が `MatchTimeRecord` を消す（`tst_app_kifu_load::loadedKifuForgetsPreviousMatch`） |
| B7 | 棋譜を読み込んでも前の対局の対局者が残り、次の3か所に出る。<br>・「名前を付けて保存」の既定のファイル名（`kifuexportcontroller.cpp:105`）<br>・CSA の `N+`/`N-` の補い。読み込んだ棋譜に対局者名がないとき（`csaexporter.cpp:69-73`）<br>・読み筋ダイアログの対局者名（`pvclickcontroller.cpp:55`） | `AppState::players` を書くのは対局開始（`playerinfopresenter.cpp:173`、`gamesession.cpp:228`）だけで、消すのは「新規」（`gamesession.cpp:262`）だけ。読み込みでは消さない | コードの経路で確認（実行はしていない）。<br>**状態: 済（455ec224）**。B6 と同じく読み込みで消す |
| B8 | エンジンの id name を、そのまま設定ファイルの最上位のグループ名に使っている。名前が `General`・`Engines`・`SizeRelated` などと同じなら他の設定と衝突し、エンジンを削除すると（`removeEngineNameGroup`）そのグループごと消える。`/` を含む名前は入れ子のグループになる | `engineregistrationhandler.cpp:347` の `settings.beginWriteArray(m_engineIdName)` と `:376` の `beginGroup(removeEngineName)` | コードで確認（実害は該当する名前のエンジンがあるときだけ）。<br>**状態: 済（455ec224）**。保存先を `EngineOptions/<エスケープした名前>`（`EngineSettingsConstants::engineOptionsGroup`）に移し、旧形式からは GUI と CLI の起動時に `EngineListSettings::migrateLegacyOptionGroups()` で移す（R8 で `EngineProfileStore` へ移した） |

### 1.2 疑い（コードから追った推定で、実行では確かめていない）

| # | 疑い | 根拠 |
|---|---|---|
| U1 | エンジン設定に保存した MultiPV が2以上だと、棋譜解析が最善でない行の評価値で確定し得る | `AnalysisJob::onBestMove` は `UsiBestMove`（multipv 1 の最終評価値を持つ）を引数ごと捨てている（`analysisjob.cpp:94`）。1局面の結果は、multipv で絞らない `progress(ply, info)` から組み立てている（`analysisresulthandler.cpp`）。<br>**確認: 起きる**（`tst_analysis_coordinator::commitPendingResult_usesBestLineWithMultiPv` で再現）。**状態: 済（455ec224）**。multipv 1 の行だけで確定する。結果の組み立ての書き直しは R5 |
| U2 | 先読み中の読み筋で「同」が付かない、または2手前の升に誤って付く | 先読みを始めると `lastUsiMove` は予想手に変わる（`usimatchhandler.cpp:130`）。一方 `previousTo` は Strategy が前に入れた升のままで、整形は毎回 `previousTo` を使う（`thinkinginfoformatter.cpp:66`）。先読み中の行は思考の表に出る（`usimodelbinding.cpp:51`）。<br>**確認: 起きる**（`tst_usiprotocolhandler::ponderPvUsesPredictedMoveForSameSquare` で再現）。**状態: 済（455ec224）**。`setLastUsiMove` がその手の移動先を「同」の基準にする。写しの削除は R1 |
| U3 | GUI の詰み探索には応答待ちの期限も stop もなく、応答しないエンジンを待ち続ける。CLI の `MateSearchRunner` は打ち切れる | `Usi::startMateSearch`（`usi.cpp:150-156`）は `go mate` を送るだけ。<br>**確認: 起きる**（`tst_analysisflow::unresponsiveMateSearchIsAbandoned` で再現）。**状態: 済（455ec224）**。CLI と同じ `UsiSearchRequest::mate()`（制限時間＋5秒で stop、その後3秒待つ）を使い、答えなければ打ち切って知らせる。打ち切りの共通化は R5 |
| U4 | 加算・秒読みのある CSA 対局で、相手の投了時の消費時間を誤って計算する | 「持ち時間 − 時計の残り − それまでの累計」で求めている（`csagamecoordinator.cpp:325-338`）。この式は、`CsaGameProgress` が手番の始めに足す加算時間を無視している。また、秒読みに入った後の `ShogiClock::remainingMs` は「秒読みの残り」を意味する。<br>**確認: 起きる**（加算10秒の対局で 0 秒になる。`tst_csagamecoordinator::opponentResignWithoutTimeUsesElapsedTurnTime`）。**状態: 済（455ec224）**。自分の投了と同じく、相手の手番が始まってからの考慮時間を使う。時間の接口は R4 |

---

## 2. 書き直す価値がある箇所（重要度順）

### R1. 局面を値で持つ型がない（core・kifu・engine）

**問題**

- **計算用の一時盤に QObject を使っている。** `ShogiBoard` はシグナル4つ（`dataChanged`・`boardReset`・`standChanged`・`turnChanged`）を持つ QObject で、盤は `QList<Piece>`、駒台は `QMap<Piece,int>`。これを `ShogiBoard board;` の形で src 全体で約25か所（core・kifu で12か所）作り、1手ごとにシグナルを出しながら局面を計算している。重さへの回避策がすでに入っている（空の駒台の共有 `shogiboard.cpp:31-35`、`copyPositionFrom` の「info 行ごとに使う」の注記 `shogiboard.h:93-95`）。
- **責務が混ざっている。** 局面・表示座標（`pieceCharacter` の筋10・11・12）・局面編集（`updateBoardAndPieceStand`・`promoteOrDemotePiece`・行き所のない駒の補正）が1クラスにある。
- **SFEN の解析・組み立てが散らばっている。**
  - 手書きの解析が5つ: `BodTextGenerator::generate`、`SfenCsaPositionConverter::toCsaPositionLines`、`SfenUtils::canonicalHands`、`KifuBranchTree` の `normalizeHands`/`positionKey`、USI の `looksLikeSfenBoard`/`isHandsToken`。
  - 手書きの組み立てが2つ: BOD の `rowTokensToSfen`＋`buildHandsSfen`、JKF の `buildSfenFromInitialData`。
  - `convertBoardToSfen()+convertStandToSfen()` をつないで手数を各自付ける箇所が9か所（`csalexer.cpp:366`、`jkfexporter.cpp:78`、`kifuexportcontroller.cpp:272`、`positioneditcontroller.cpp:107/145`、`candidatearrowpresenter.cpp:151`、`pvboardcontroller.cpp:118`、`tsumeshogigenerator.cpp:522`、`tsumeshogipositiongenerator.cpp:257`）。
  - 手番を `contains(" w ")` で判定し（`parsecommon.cpp:37`、`parsemoveformat.cpp:16`、`kiftosfenconverter.cpp:34`、`usimatchhandler.cpp:45`）、`replace(" b ", " w ")` で書き換えている（`kiftosfenconverter.cpp:157,161`）。
  - 同じ変換でも向きで経路が違う。CSA→SFEN は `ShogiBoard`、SFEN→CSA は手書きの解析。JKF はその逆。
- **駒の表現が2系統ある。**
  - `Piece` は成駒を SFEN にない文字 Q・M・O・T・C・U で表す。`charToPiece`・`promote`・`demote`・`toBlack`・`toWhite` は12〜28行の switch。
  - 合法手生成（fmv）は独自の `PieceType` を持つのに、盤を `std::array<char,81>` で持ち `Piece` の文字をそのまま入れている。手生成の内側のループで毎回 `toupper` と switch で `PieceType` に戻している。
  - 同じ対応・規則が何か所にも書かれている。文字から駒種への対応は fmv の中だけで5回（`mapPieceChar`、`pieceToPieceType`、`pieceToHandType`、`charToPieceType`、`charToPieceInfo`/`pieceInfoToChar`）。`PieceType` から `HandType` への対応は3回。行き所のない駒の規則は5か所（`shogiboard.cpp` に3、fmv に2）。持ち駒の書き出し順も5か所。`isMandatoryPromotion` と `isDropDeadSquare` は本体が同じ（`fmvlegalcore_internal.h:41,62`）。
- **エンジンへは探索する局面を position コマンドの文字列で渡している。**
  - エンジン層が `SfenReplay::sfenFromPositionCommand` で全手順を並べ直す。手番を知るため（`usimatchhandler.cpp:45`）、探索局面を知るため（`usiprotocolhandler.cpp:161`）、先読みを始めるたびにもう一度で、1手で最大3回になる。
  - 先読みが当たったかは `simplified()` した文字列の比較。次の局面は文字列の連結（`usimatchhandler.cpp:116,129`）、直前の手は `section(' ', -1)` で取り出す。
- **直前の手・先読みの局面が写しで、揃えるのは手作業（U2 の原因）。**
  - 読み筋の「同」の基準（直前の手）を2系統のセッターで入れている。`setPreviousMove` は Strategy 2つ・`AnalysisSession`・`AnalysisJob`、`setLastUsiMove` は `AnalysisSession`・`AnalysisJob`・`UsiMatchHandler::lastMoveChanged` から。
  - 先読みの局面の写しが3つ（`UsiMatchHandler::m_ponder`、`MatchSessionState::m_ponder[2]`、`CsaEngineController::m_ponderPosition`）。
  - 「同」の基準そのものにも写しが3つある。`ShogiGameController::m_previousTo`（待ったでは `RecordLine` から手で合わせ直す、`matchcoordinator.cpp:455`）、`InterruptedGameState::previousTo`、CSA の上書き（`csagamecoordinator_moves.cpp:125`）。

**書き直し案**

- core に値型 `Position` を作る（`std::array<Piece,81>`、持ち駒の配列、`Turn`、手数）。`fromSfen`/`toSfen`/`apply(UsiMove)`/`pieceAt`/`positionKey` をここだけに置く。
  - `ShogiBoard` は `Position` を1つ持って変更を知らせる表示用の包みにし、局面編集の操作はそちらに残す。
  - 局面を扱う側（`SfenReplay`、差分計算、変換器、Exporter、`Ki2MoveNotation`、`EngineMoveValidator::load`、`BodTextGenerator`、`SfenCsaPositionConverter`）は `Position` を受け取る。局面の比較キーは `Position::positionKey` に統一する。
- `Piece` は「色＋駒種」を詰めた値にし、成る・戻す・先後の反転を constexpr の算術にする。並びは fmv の `PieceType` と揃え、fmv の盤も同じ符号で持つ。行き所のない駒・成れる段・持ち駒の順は1つのヘッダーに置く。
- engine に値型 `UsiPosition{startSfen, QStringList moves, 結果の局面, std::optional<UsiMove> lastMove}` を作る。`command()`・`sideToMove()`・`withMove()`・`operator==` を持たせ、`RecordLine::usiPosition(ply)` で作る。
  - `UsiSearchRequest` と `requestMatchMove` はこれを受ける。
  - `ThinkingInfoFormatter` が「同」の基準を自分で導き、`setPreviousMove`・`setLastUsiMove` とその写しを削除する。
  - 先読みの局面は `UsiMatchHandler` だけが持ち、待ったでは `forgetPonder()` で消す。

**規模とテスト**

- `Position`: core の4〜5ファイルを新設・改修し、約30ファイルを置き換える（`shogiboard.h` を include するのは53ファイル）。
- 駒の表現: core の8ファイル前後で300〜400行減る。
- `UsiPosition`: 約15ファイル（engine 5、game 5、analysis 3、network 2、kifu の `RecordLine`）。
- テスト: `tst_shogiboard`（636行）・`tst_sfenreplay`・変換器のテスト6本・`tst_fmvlegalcore`・`tst_fmvconverter`・`tst_usimatchhandler`・`tst_usiprotocolhandler`・`tst_gamestrategy`・`tst_matchcoordinator`。

**状態: 済（6143b3b0）**。案から変えた点:
- `UsiPosition` は engine ではなく core に置いた（`src/core/usiposition.h`）。kifu の `RecordLine::usiPosition(ply)` が作るので、層の規則（kifu は engine を include しない）に合わせた。直前の手（`lastMove`）は手順と別に持ち、局面だけを送る検討（`position sfen ...`）でも「同」の基準を持てる。
- 「同」の基準の写し（`ShogiGameController::m_previousTo`・`InterruptedGameState::previousTo`・CSA の上書き）は、手順を持つ呼び出し側が指すたびに渡す形にした（`validateAndMove`・`applyMove`・`applyRecordedMove` の `previousTo`、`MatchCoordinator::lastMoveDestination()`）。
- 使う箇所のなかった `UsiMatchMove::position`・`ponderPosition` は削除した。エンジンの指した手が局面に指せないときは、先読みせずに対局の側の反則の処理へ渡す（従来どおり）。

### R2. 棋譜の読み込みの中間表現と変換器（kifu/formats）。B1・B2・B3 の原因

**問題**

- **中間表現 `KifLineDraft` が2本のリストになっている。** `QStringList usiMoves` と `QList<KifDisplayItem> disp`（先頭は開始局面の項目、末尾は終局のこともある）を別々に積み、`resolve()` が件数で突き合わせる（`parsecommon.cpp:29-31` の "Move and display counts differ"）。
- **型付きの手を文字列に戻し、局面を2回再生している。**
  - `UsiMove` を作っても `toString()` で文字列にして積み（`ki2tosfenconverter.cpp:113`、`csalexer.cpp:134`）、`resolve()` が `SfenReplay::replay` で読み直して指し直す。
  - KI2 と CSA は読み込み中にも `ShogiBoard` で指しているので、`resolve()` で2回目の再生になる。`replay` は1手ごとに `toSfen()` を2回呼ぶ（`sfenreplay.cpp:50,63`）。
  - 消費時間も、CSA はミリ秒、JKF は秒を文字列にし、`RecordTime::fromText` で読み直している。
- **1手・1手順を表す型が重複している。**
  - 1手の型が3つある。
    - `RecordEntry`: 本来の1手の型。
    - `KifDisplayItem`: 時間と USI を文字列で持つ。`operator==` は10項目のうち5項目しか比べない（`kifdisplayitem.h:37-40`）。製品での用途は、取り込みの下書きと CLI の出力（`kifuconversionservice.cpp:62`）だけ。
    - `KifuBranchNode`: `RecordEntry` を包み、約20の getter/setter で転送するだけ。
  - 1手順の型が4つある（`KifLineDraft`、`KifLine`、`RecordLine`、`BranchLine`）。`sfenAt`・`usiMoves`・`sfens`・`moveCount` が型ごとにあり、意味が少しずつ違う。
- **表記と指し手が二重の正本になっている（B2 の原因）。** KIF の書き出しだけが `displayText` をそのまま書き、KI2・JKF・CSA・USEN は指し手から作り直す。
- **変換器ごとに公開 API が違う。** 唯一の利用者 `KifuLoadParser` が形式ごとに別の呼び方をするため、同じテキストを何度も解析する。
  - JKF の JSON を3回（`jkftosfenconverter.cpp:63,74,103`）、`parseUsiText` を3回（`kifuloadparser.cpp:89`、`usitosfenconverter.cpp:171,185`）、USEN の初期局面の判定を2回、CSA の `splitStatements` を2回。
  - 手合割の名前は、CSA だけ `KifuLoadParser` が自前で決める（`kifuloadparser.cpp:71-72`）。
- **KIF と KI2 が同じ処理を二重に持つ。**
  - 「変化：N手」での区切りと分岐元の決め方（`kiftosfenconverter.cpp:168-200` と `ki2tosfenconverter.cpp:128-160`）。
  - ブロック解析の骨格（コメント・しおり・開始局面の項目・終局・finalize・resolve）。
  - 指し手の文法。KIF は正規表現の `KifLexer::convertMoveLine`/`findDestination`/`isPromotionMoveText`、KI2 は1文字ずつ読む `Ki2Lexer::parseMoveText` で、同じ日本語の表記を別々に実装している。KIF の手は「KI2 の手＋`(xy)`」にすぎない。
- **失敗時の方針が兄弟でばらばら（B3 の原因）。** KIF は読めない手で手順を打ち切って続ける（`kiftosfenconverter.cpp:84-91`）。KI2 は全体を失敗にする（`ki2tosfenconverter.cpp:101-105, 145-149`）。CSA は飛ばして読み続ける。エラー文も、KIF は `+=` で追記、KI2 は `=` で上書き。
- **転送するだけの API がある。** Ki2 の `detectInitialSfenFromText`・`buildInitialSfenFromBod`・`mapHandicapToSfen` は KIF へ、`Kif::mapHandicapToSfen`・`Jkf::mapPresetToSfen` は `Handicaps` へ転送するだけ（後の2つはテストだけが呼ぶ）。

**書き直し案**

- 形式に共通の `KifLineBuilder`（開始局面の `Position` を持つ）を作る。
  - 各パーサは `addMove(UsiMove, RecordTime, 表記の原文)`・`addTerminal(...)`・`appendComment()`・`appendBookmark()` を呼ぶだけにする。
  - 指したその場で `RecordEntry` を作り、手番・直前の移動先（「同」）・累計時間は builder が持つ。
  - `KifLineDraft`・`resolve`・`KifDisplayItem` を操作する補助関数群（`createMove`/`Terminal`/`OpeningDisplayItem`、`finalizeDisplayItems`、`flushCommentToLastItem` など）は廃止する。
- 形式ごとに公開するのは `KifuParseOutcome parse(QStringView)` 1つにする。棋譜・初期局面・手合割の名前・対局情報・警告を1回の解析で返す。`KifuLoadParser` は「`KifuFormat` → パーサ関数」の表で振り分ける。
- KIF と KI2 は、行の分類・変化ブロックの分割・指し手の文法を共有する。違いは「移動元が書いてあるか（なければ `Ki2MoveNotation::resolve` で決める）」だけにする。
- 失敗時の方針は `ParseOptions` の1か所で決める。パスは型で表すか、未対応として明示的にエラーにする。
- 1手は `RecordEntry` だけにする。
  - 表記は指し手と直前の手から導く。取り込んだ原文を残すなら任意の項目として持ち、表示にだけ使う。
  - 手番は `move.mover()` から導く。
  - `KifLine` と `RecordLine` は1つの型にし、`KifuBranchNode` の転送 setter は削る。CLI は `RecordLine` を JSON にする。

**規模とテスト**

- 取り込み側の約13ファイル＋`kifuloadparser`（約2,500行のうち1,000行前後を書き換え）。1手の型の整理は kifu のヘッダー6本、ツリー・ノード、Exporter 6本、CLI・自動化の2〜3ファイルに及ぶ。
- テスト: 変換器のテスト6本（約3,700行）が回帰を守る。`tst_gamerecordmodel`（1,224行）・`tst_jkfexporter`・`tst_kifu_comment_sync` は `KifDisplayItem` に依存しているので、書き換えが要る。
- R1 の `Position` と同時に進めるのが効率的。

**状態: 済（6143b3b0）**。案から変えた点:
- KIF・KI2 は `KifTextParser`（`src/kifu/formats/kiftextparser.h`）が両方を読む。手数で始まる KIF の行と手番の印で始まる KI2 の行をどちらの形式でも読み、形式で違うのは KI2 の「まで…」の行・対局情報の終わりの見分け方・警告の見出しだけ。指し手の文法は `KifMoveGrammar`（`Ki2Lexer::parseMoveText` に移動元 "(xy)" を足したもの）。
- 失敗の方針は `ParseOptions`（既定は手順を打ち切って続ける、`FailRecord` で全体を失敗）と `ParseReport` に置いた。パスは未対応の手として同じ方針で扱う。
- `RecordEntry::mover` は残した（終局の行は指し手を持たないので、記録した側をこの欄で持つ）。
- `KifuBranchNode` の転送 setter の代わりに、書き換えは `record()`（非 const）で行う。

### R3. 棋譜に付く情報が1つの文書にまとまっていない（kifu・widgets・ui・app）。B6・B7 の原因

**問題**

- **書き出しに使う付随情報が5か所に散らばっている。** 棋譜の正本は分岐ツリーだが、付随情報は別々の場所が持つ。
  - **対局情報**: ウィジェット `GameInfoPane` が持つ。読む前に `commitPendingEditor()` が要り、その呼び出しが6か所ある（`kifuexportcontroller.cpp:90,133,146`、`kifufilecontroller.cpp:30`、`gamesession.cpp:232`、`playerinfopresenter.cpp:169`）。
  - **対局者**: `AppState::players` と `MatchCoordinator::players()` の2つの写し。`AppState::players` は表示役の `PlayerInfoPresenter` も書く（`playerinfopresenter.cpp:173`）。
  - **持ち時間と開始・終了の日時**: `MatchTimeRecord`。
  - **上書き保存先**: `AppState::saveFileName`。`KifuFileController` が5か所で書き、`GameSession` が2か所で消す。
  - **未保存の印**: `GameRecordModel::markDirty` の呼び出しが6クラス9か所、`clearDirty` が3か所、それとは別に `GameInfoPane::isDirty` もある。分岐ツリーの `revision()` は使っていない。
- **同じ処理が3か所ずつある。** 対局情報から対局者名を取り出す処理（`playerinfopresenter.cpp:120-140`、`kifuanalysiscontroller.cpp:139-147`、`kifuconversionservice.cpp:68-71`）と、「下手・上手と呼ぶか」の計算（`gamesession.cpp:175`、`kifuexportcontroller.cpp:299`、`MatchCoordinator::m_handicapNames` の写し）。
- **依存の null チェックが多い。** `KifuFileController` は Q_ASSERT で依存を確かめた後で、同じ依存の null チェックを15か所書いている。`KifuExportController` にも13か所ある。
- **書き出し側の `GameRecordModel` は転送だけの層になっている。**
  - `toXxxLines` の6本は、どれも `Exporter::exportLines(*this, ctx)` を呼ぶ1行。Exporter 側は `model.branchTree()` しか使わない。`collectGameInfo`・`resolvePlayerNames` も転送。
  - `gamerecordmodel.cpp` が Exporter 6本を include し、各 Exporter の .h が `gamerecordmodel.h` を include している（循環）。
  - CLI は書き出しのためだけに QObject を作る（`kifuconversionservice.cpp:57-58`）。`KifuFormat` ごとの switch も2か所にある（`kifuexportcontroller.cpp:58-75`、`kifuconversionservice.cpp:52-84`）。

**書き直し案**

- kifu 層に `KifuDocument` を置き、次をまとめて持たせる。
  - 分岐ツリー
  - 対局情報（`QList<KifGameInfoItem>`）
  - 対局の記録（対局者・持ち時間・開始と終了の日時）
  - ファイルの紐づけ（パス・形式・保存した時点の revision）
- 未保存かどうかは「revision が保存時と違う、またはヘッダーが変わった」で求め、`markDirty` の呼び出しはなくす。
- 読み込み・新規・対局の開始は、文書を丸ごと置き換える操作にする。消し忘れが起きる構造そのものがなくなる。
- `GameInfoPane` は文書のヘッダーを編集する画面、`PlayerInfoPresenter` は表示だけにする。
- 書き出しは「`const KifuDocument&` から `QStringList` を返す関数」を `KifuFormat` ごとに並べた表にする。`GameRecordModel` にはコメントの編集だけを残す（または廃止する）。`KifuFileController` と `KifuExportController` は「文書とファイル」の1つの部品にまとめ、GUI（非同期）と自動化 API（同期）の後処理を共通にする。

**規模とテスト**

- 10〜12ファイル、約1,500行（kifu・widgets・ui・app）。書き出し側は約1,300行。
- テスト: `tst_game_info_pane`・`tst_gamerecordmodel`・`tst_app_kifu_load`・`tst_kifusavecoordinator`・`tst_kifu_format_compliance`・`tst_jkfexporter`。
- これを済ませると、R4 と R6 で必要な「作り直しで何を消すか」の多くがなくなる。

**状態: 済（3953517c）**。案から変えた点:
- `KifuDocument`（`src/kifu/kifudocument.h`）は分岐ツリーを持たずに指す（ツリーは合成ルートが作り、表示中の手・ライブ対局と共有する）。読み込み・新規・対局の開始は文書の中身を置き換える操作で、対局者・持ち時間・上書き保存先を消す。B6・B7 の `GameSession::forgetFinishedMatch()` はこの置き換えに吸収した。
- 未保存は「ツリーの revision・コメントとしおりの変更回数・対局情報が保存した時点と違う」か「ファイルにない内容を読み込んだ」で求める。指し手のない棋譜の対局情報だけの変更（対局の前に棋戦・場所を書いた）は数えない。
- 対局情報の欄（`GameInfoPane`）は文書の対局情報を編集する画面になり、入力中のセルの確定は文書の Hooks（`flushPendingHeaderEdits`）だけが呼ぶ。対局者名の取り出しは `KifuExportMetadataBuilder::playerName` の1か所、下手・上手と呼ぶかは `KifuDocument::usesHandicapNames` の1か所。
- `GameRecordModel` は廃止した（コメント・しおりの書き換えも文書へ）。書き出しは `KifuExport::lines`（`src/kifu/kifuexport.h`）の形式ごとの表で、GUI と CLI が同じ表を使う。`KifuExportController` は `KifuFileController` にまとめ、読み込みの後始末を `finishLoad` 1つにした。
- `AppState` は動作状態だけになった。対局者は文書と `MatchCoordinator::players()` に残る（司令塔は対局の進行に使う）。

### R4. ローカル対局と通信対局の流れが別々（app・ui・network）。B5・U4 の原因

**問題**

主な場所: `gamesession.cpp:136-165, 186-211, 222-245, 272-283`、`csagamesession.cpp:103-145, 160-166`、`mainwindowcomposer_create.cpp:495-497`、`dialoglauncher.cpp:76-110`、`nyugyokudeclarationhandler.cpp:27-91`。

- **開始の経路が別々。**
  - ローカル対局は `GameSession::startGame` → `PlayerInfoPresenter::onMatchPlayersResolved` → `showPlayers`。
  - 通信対局は `CsaGameSession::onGameStarted` → フック `prepareRecord` → `GameSession::beginCsaGameRecord`。`state->players` を直接書き、盤の名前も `chrome()->setBlackPlayerName("▲"+…)` で直接書く（`showPlayers` を通らない）。
  - 作り直しで対局情報が消えないように、「確定 → 退避 → `resetToNewRecord` → 書き戻し」の回避策を取っている（`gamesession.cpp:230-235`）。
- **終局の後始末が違う。** ローカル対局は commit してから終了時刻を書く。通信対局は終了時刻を書くだけで、これが B5 になっている。
- **自動保存が別々。** ローカル対局は `MatchCoordinator::autoSaveRequested` から KIF 固定で保存する。通信対局は `CsaGameSession` が `m_autoSave*` の3つのフィールドを持って独自に保存する。
- **評価値の符号の処理が2つある。** `EvaluationGraphPresenter` と `csagamesession.cpp:164-165`。
- **対局中の操作の分岐が場所ごとに違う。**
  - 投了・中断・即指しは、`GameSession` の3つの関数がそれぞれ `isCsaGameActive()` で分岐する。
  - 入玉宣言だけは `DialogLauncher` が通信対局を分岐する。確認の文言は両方にあり、DialogLauncher 側は `translate("NyugyokuDeclarationHandler", …)` で訳を流用している。
- **「下手・上手と呼ぶか」が2通り。** ローカル対局は `MatchCoordinator` の写し、通信対局はフックで `GameSession::usesHandicapNames` を呼ぶ。
- **通信対局の時計の表示は、ローカル対局の仕組みに暗黙に頼っている。** `MatchCoordinator` が `ShogiClock::timeUpdated` を中継するのを使っている（`matchcoordinator.cpp:44, 214-222`）。
- **中断・再開の状態が3か所にある。** `MatchCoordinator::m_interruptedGame`、`GameSession::m_interruptedNodeId`、`UiStatePolicyManager::m_resumeAvailable`。
- **通信対局の持ち時間を写しで持っている（U4 の原因）。**
  - 正本は `CsaGameProgress` で、表示用の `ShogiClock` に着手ごとに `setRemainingTimes` で写している。
  - `UsiMatchHandler` は時間を2通りで読む。ローカル対局は `ShogiClock*`、通信対局は `UsiTimingParams`。
- **通信対局のエンジンの状態をフラグで補っている。** `CsaEngineController::isInitialized()` はポインタが空でないかを見るだけで、readyok の前も、終局で quit した後も true のまま。これを `CsaGameCoordinator` の `m_initializingEngine`・`m_engineNeedsRestart` で補っている。
- **終局の行への対応付けが2か所にある。** 連続王手の千日手の規則を含む処理が、`MatchCoordinator::appendTerminalRecord` と `CsaGameProgress::endRecord`（`csagameprogress.cpp:95`）にある。

**書き直し案**

- 対局相手の窓口を1つの型にまとめる（例: `GameChannel`。実装はローカル用と CSA 用）。持たせる操作は resign・breakOff・moveNow・declareNyugyoku・isMyTurn・players・timeControl。
- `GameSession` は「開始（棋譜の準備・対局者・時刻の記録・表示）→ 指し手の記録 → 終局（終了時刻・確定・自動保存）」の1本だけを持つ。CSA の `prepareRecord`/`recordGameEnd` フックと、DialogLauncher の CSA 分岐はなくす。
- 中断した対局は `InterruptedGameState` に node ID も持たせて1か所にする。UiStatePolicy は `MatchCoordinator` に問い合わせる。
- 時間の出どころを1つの接口（`snapshot(side, pondering)`・`elapsedThisTurn(side)`）にする。ローカル対局は `ShogiClock`、通信対局は `CsaGameProgress` が実装し、表示用の時計と `UsiMatchHandler` はこの接口を読む。
- 通信対局のエンジンの状態は Usi の実際の状態から読み、フラグ2つを削除する。

**規模とテスト**

- 対局の流れは6〜7ファイル約700行、時間の接口は約8ファイル。
- テスト: `tst_app_game_session`・`tst_csagamesession`・`tst_csagamecoordinator`・`tst_game_start_flow`・`tst_match_end`・`tst_usimatchhandler`。通信対局の終局後の状態を確かめるテストは新しく要る。

**状態: 済（d8dbb2bd）**。案から変えた点:
- 対局相手の窓口 `GameChannel`（`src/game/gamechannel.h`）の実装は、司令塔に直接持たせず、`LocalGameChannel`（`MatchCoordinator`）と `CsaGameChannel`（`src/network/csagamechannel.h`）の小さな包みにした。司令塔の `players()` などと名前がぶつからず、司令塔の公開の操作も増えない。窓口を選ぶのは `GameSession::activeChannel()` の1か所（動作状態で決まる）。
- 入玉宣言は `NyugyokuDeclarationHandler::handleDeclaration(GameChannel*)` の1本にした。宣言できるのは手番の人間だけ（通信対局でエンジンが指しているときも、人間はエンジンの代わりに宣言しない）。成否をこちらで判定するかは窓口（`judgesDeclaration`）が答える。
- 通信対局の棋譜は `GameSession` が司令塔の `gameStarted`・`recordEntryReady`・`gameEnded` を直接受けて作る（フック `prepareRecord`/`recordGameEnd` を廃止）。`CsaGameSession` は画面への反映だけにした。自動保存の指定は `MatchAutoSave`（`src/game/matchautosave.h`）で、ローカル対局は対局の開始条件、通信対局は `CsaGameCoordinator::StartOptions` に持たせ、終局の後始末（`finishMatchRecord`）で保存する。
- 時間の出どころは `MatchTimeSource`（`src/engine/matchtimesource.h`）で、`snapshot`・`elapsedThisTurn` に加えて `finishTurn`（エンジンの応答で手番を精算し、時間切れかを返す）を持たせた。ローカル対局は `ClockTimeSource`、通信対局は `CsaGameCoordinator` 自身が実装する（投了の消費時間も表示用の時計ではなく自分の手番の経過時間から読む）。表示用の時計は `TimeDisplayPresenter` が `ShogiClock::timeUpdated` を直接受ける（司令塔の中継を廃止）。
- 通信対局のエンジンは、終局で gameover を送ったら枠を空ける（`CsaEngineController::sendGameOver`）。次局は `isReady()` が false なので起動し直し、初期化中（`isStarting()`）は readyok を待って同意する。

### R5. エンジンへの問い合わせの骨組み（詰み探索・詰将棋生成・棋譜解析）。U1・U3 の原因

**問題**

- **`TsumeshogiGenerator`（682行）が、1クラスで4つの処理を回している。**
  - 回している処理: 候補局面の一括生成（ワーカースレッド）、候補のエンジンでの確認、唯一解の検査、要らない駒を1枚ずつ除くトリミング。
  - 状態は34個のメンバ変数に散らばっている。`Phase`、検査後に戻るフェーズ `m_verificationOrigin`、bool 6個（`m_starting`、`m_stopRequestedDuringStart`、`m_waitingForPositions`、`m_verificationAwaiting`、`m_trimComplete`、`m_trimProvenInTarget`）。`m_phase` の読み書きは37か所ある。
  - 採択の正しさは状態からは保証されず、最後に `registerFoundPosition` が `m_trimComplete && sfen == m_verifiedSfen && pv == m_verifiedPv` を文字列で比べて確かめ直している。
  - 修正の履歴が多い（155e0b7b、53375d25、6498f1fc、f8cba488、9c9bb25e）。`m_starting` のコメントは、今はない待機ループを前提にしている。
- **go mate の扱いを5クラスが手書きしている。**
  - 5クラス（`EngineAnalysisRunner`、`MateSearchRunner`、`TsumeVerificationRunner`、`TsumeMateEngine`、`TsumeshogiGenerator`）が、それぞれ `UsiEngineSession` を持って同じ骨組み（終了済みフラグ、準備完了で探索、エラーで終了、応答待ちの期限切れ）を書いている。
  - `UsiCheckmate::Result` を変換する switch が5つあり、変換先の型が4種類（`MateSearchRunner::Status`、`TsumeshogiVerifier::Reply`、`TsumeEvaluation::Status`、`AnalysisSession` の文言）。stop の後の結果を「不明」扱いにするのは2か所だけ。
  - 「制限時間+5000ms で stop、その後3000ms 待つ」の定数が3組ある。
  - 余詰の検査ループが2つある。`TsumeVerificationRunner::step`/`submit`（`tsumeverificationrunner.cpp:77-136`）と `TsumeshogiGenerator::continueVerification`/`submitVerification`（`tsumeshogigenerator.cpp:599-646`）はほぼ同じ文で、ヘッダー（`tsumeverificationrunner.h:22`）に「TsumeshogiGenerator の検査フェーズと同じ手順で動く」と書いてある。1回の問い合わせの上限 1000ms は、片方が定数 `kQueryMaxMs`、もう片方がリテラル。
- **棋譜解析は、結果の組み立てを UI 層に頼っている。**
  - `AnalysisCoordinator` は局面の番号を進めるだけで、探索の依頼は3つの要求シグナル（`requestSetOption`・`requestSearch`・`requestStop`）で `AnalysisJob` に返す。
  - 製品で使われない汎用部分がある。`SinglePosition`/`startAnalyzeSingle` はテストだけが使う。`startpos`・`position ` で始まる入力の分岐もテストの入力専用。MultiPV>1 の setoption は呼び出し側が設定しない。
  - 状態機械が2つある（`AnalysisJob::State` と `AnalysisCoordinator` の `m_running`/`m_mode`/`m_currentPly`）。stop は終了処理を2回呼んでしのいでいる（`analysisjob.cpp:67-72`）。
  - 1局面の結果は、`AnalysisResultHandler` が3本の流れ（`progress`・`thinkingInfoUpdated`・`resultReady`）から組み立てる。正しく組み上がるかは、Usi のコンストラクタの接続（`usi.cpp:27`）が `AnalysisJob` の接続より先に張られていることに依存している。
  - UI が解析の内部を読んでいる（`coord->currentPly()` と `AnalysisCoordinator::Mode`）。
  - `AnalysisJob::onBestMove` は `UsiBestMove`（multipv 1 の最終評価値と探索局面を持つ）を捨てている。

**書き直し案**

- 詰みの問い合わせの共通部品 `MateOracle` を作る。
  - `UsiEngineSession` を所有し、`ask(sfen, limitMs)` に `answered(MateAnswer{Mate|NoMate|Unknown|Unsupported, pv})` で答える。打ち切りの方針はここ1か所に置く。
  - 引き出し型の問い合わせ元（`TsumeshogiVerifier` や下のジョブ）を総予算つきで回す関数を用意する。
  - `MateSearchRunner`・`TsumeVerificationRunner`（30行程度になる）・Generator・`AnalysisSession` の詰み探索・`TsumeMateEngine` をこの上に載せる。`EngineAnalysisRunner` も「1回だけ使う接続」の土台に揃える。
- 候補1つ分の処理を、Qt に依存しない引き出し型の `TsumeCandidateJob`（`nextQuery()` → `submit(MateAnswer)` → `outcome()`）にする。
  - 内部は「手数の確認 → 元局面の検査 → i枚目を除く → 除いた局面の検査 → … → 採択／棄却」の明示的な enum にする。
  - 出力は採択の状態からしか作れないので、文字列での確かめ直しは要らなくなる。
  - QObject 側は、候補の待ち行列・問い合わせの中継・結果の集計だけを受け持つ。
- `AnalysisCoordinator` を `AnalysisJob` に吸収する。
  - bestmove の時点で `PlyAnalysis{ply, UsiScore score, QStringList usiPv, QString pvKanji, bool book}` を1回出す。評価値は `UsiBestMove.score`、読み筋は multipv 1 の最後の行から取る。
  - ほかの出力は `plyStarted(ply)` と `finished(bool completed)` だけにする。`AnalysisResultHandler` は「`PlyAnalysis` を表の1行にする」だけになる。

**規模とテスト**

- Generator: 約850行を2〜3ファイルに分ける。ジョブはエンジンなしで単体テストできるようになる。
- `MateOracle`: 6〜7ファイル、正味200〜300行減る。Generator の書き直しの前提になる。
- `AnalysisCoordinator`（276行）は削除する。
- テスト: `tst_tsumeshogi_generator`（51件）、`tst_tsumeshogi_verification`（friend で内部を触っているので書き直し）、`tst_tsume_search`、`mock_mate_engine`、`tst_analysis_coordinator`（530行。AnalysisJob のテストへ移す）、`tst_analysisflow`（410行）、`mcp/tests`。

**状態: 済（b625d5eb）**。案から変えた点:
- `MateOracle`（`src/engine/mateoracle.h`）は go mate の問い合わせだけを受け持ち、打ち切りの方針は既存の `UsiSearchRequest::mate`（制限時間＋5秒で stop、その後3秒待つ）の1か所に残した。stop の後の答えを不明にするのは `MateAnswer::fromCheckmate` の1か所。KomoringHeights の「go mate infinite を期限まで待ち、stop は送らない」は `askWithin` にした。
- GUI の詰み探索（`AnalysisSession`）は、思考情報の表示を出すために `Usi` のまま探索し、答えの変換だけ `MateAnswer::fromCheckmate` を使う。`EngineAnalysisRunner` は go mate ではないので `MateOracle` には載せず、初期化前の停止の扱いだけ他の実行器と揃えた。
- 引き出し型の問い合わせ元は `MateQuerySource`（`nextQuery`・`submit(answer, elapsedMs)`・`isDone`）で、回すのは `MateQueryLoop`（`src/engine/matequeryloop.h`）。余詰検査の総予算と1回の上限（`kQueryMaxMs`）は `TsumeVerificationJob`（`src/analysis/tsumeverificationjob.h`）が持ち、総予算は答えにかかった時間の合計で数える（エンジンなしで試せる）。
- 候補1つ分は `TsumeCandidateJob`（`src/analysis/tsumecandidatejob.h`。段階は `Stage`）。1枚除いた局面の作り方（`onePieceRemovedPositions`）もここへ移した。
- `AnalysisCoordinator` は削除した。1局面の結果は `PlyAnalysisCollector` が info と bestmove から作り、`AnalysisJob` が `plyStarted`・`plyAnalyzed(PlyAnalysis)`・`finished(bool completed)` を出す。読み筋の漢字表記はこの集め役が探索した局面の上で作る（思考情報の整形の接続順に頼らない）。テストだけが使っていた1局面の解析と MultiPV の指定は削除した。評価値グラフの点は従来どおり次の局面に移るときに足す。

### R6. エンジンの表示（思考・検討・通信ログ・評価値グラフ・矢印）に持ち主がない（ui・widgets・models）

**問題**

- **表示を消す処理が2か所にある。** 思考情報と通信ログのモデルを消す処理が、`SessionResetCoordinator::clearEngineDisplays` と `EngineDisplayPresenter::clearDisplay` にある。`SessionResetCoordinator` の Deps 17個のうち12個は、この消去のためだけにある。
- **検討タブの表示を3つの部品が書く。**
  - 書くのは `ConsiderationUiController`（モデルをつなぐ・実行中の表示・タイマー）、`SessionResetCoordinator`（`setModel(nullptr)`・実行中の解除・タイマーのリセット・エンジン名）、`PlayerInfoPresenter`（エンジン名）。
  - 「検討中か」の値は4か所にある（`AnalysisSession::isConsidering`、`ConsiderationUiController::m_considerationActive`、`CandidateArrowPresenter::m_considerationActive`、`ConsiderationTabManager::m_considerationRunning`）。`CandidateArrowPresenter` は自分の写しを信用できず、`m_considerationActive && analysis->isConsidering()` と両方を見ている（`candidatearrowpresenter.cpp:83`）。
  - 「矢印を出すか」も3か所（タブのチェック、`m_showArrows`、`m_showConsideration`）にある。
- **評価値の履歴がウィジェットにしかない。**
  - 履歴は `EvaluationChartWidget::m_scores` だけが持ち、書き手が4つある（`EvaluationGraphPresenter`、`CsaGameSession`、`KifuAnalysisController`、`SessionResetCoordinator`）。
  - 先手から見た値に符号を直す処理が3通りある（`evaluationgraphpresenter.cpp:63`、`csagamesession.cpp:165`、`analysisresulthandler.cpp:130`）。消去と切り詰めを呼ぶ箇所は5か所。
  - 系列名の書き手も4つある。`KifuAnalysisController` はウィジェットを直接書くので、`EvaluationGraphPresenter` が持つ名前の写し（`m_engine1Name`）とずれる。
- **探索状況（info 1行）を、項目ごとに4段の中継で表に届けている。**
  1. engine 層が書式化済みの文字列を5つのシグナルで出す
  2. `UsiModelBinding` の5つのスロット
  3. `UsiCommLogModel` の7つの Q_PROPERTY
  4. `EngineInfoWidget` の7つのスロット
  5. 1行だけの `QTableWidget` のセル

  - 1行の info で約25回の呼び出しが起きる。
  - 書式化も2系統ある。ノード数などは engine 層で3桁区切りの文字列にされ、思考の表は数値を持ってデリゲートで区切る。
  - `usiCommLog` プロパティは最後の1行しか持たず、実際には「行が届いた」というイベントとして使われている（`UsiLogPanel` が変更通知を受けて中身を読む）。
- **検討タブ `ConsiderationTabManager` が設定と配線まで抱えている。**
  - 設定値の正本がウィジェット自身（`isUnlimitedTime()`・`byoyomiSec()` がラジオボタンとスピンボックスを直接読む）。
  - 部品を並べる `layoutToolbar()` が、エンジン一覧の読み込み・設定の復元・シグナルの接続まで行う。
  - 開始/中止ボタンは、状態が変わるたびに `QMetaObject::Connection` を外して付け直している。

**書き直し案**

- エンジン表示の状態を1つの presenter に持たせる。動作状態・エンジンの枠・`AnalysisSession` の mode・対局者から、表示（名前・つなぐモデル・実行中の表示・2つ目のエンジン欄・矢印の元）を毎回求める。
- 評価値は `EvaluationSeries`（node ID か手数ごとの、先手から見た値の列）としてモデルに置き、グラフは表示するだけにする。符号の処理は書き込み口の1か所にする。
- 探索状況は、数値で持つ `EngineSearchStatus` 構造体を1回だけ送る。`UsiCommLogModel` は「状況（構造体）＋`statusChanged`」と「`logLineAppended(QString)`」に分ける。
- 検討タブの設定は値の構造体 `ConsiderationSettings` で持ち、`settingsChanged` で知らせる。実行状態は1つの enum にし、ボタンのスロットは1つにする。
- こうすると `SessionResetCoordinator` は棋譜の作り直しだけになり、Deps は5個前後で済む。

**規模とテスト**

- 表示の持ち主の整理は8〜9ファイル約800行、探索状況の中継は5ファイル約900行。
- テスト: `tst_candidate_arrows`・`tst_wiring_analysistab`・`tst_thinking_display_updates`・`tst_evaluationchart`・`tst_analysisflow`・`tst_preset_gamestart_cleanup`。

**状態: 済（0dac64de・f0b1d80c）**。案から変えた点:
- 評価値は手数ごとの値の列 `EvaluationSeries`（`src/models/evaluationseries.h`）にした（node ID ではなく手数。グラフの横軸と棋譜解析の結果が手数で引くため）。書き手は `EvaluationGraphPresenter` の1つで、対局のエンジンの値は `plotEngineScore`、棋譜解析の値は `plotAnalysisScore` で書く。先手から見た値への符号の処理は `EvaluationSeries::blackView` の1か所。グラフ（`EvaluationChartWidget`）は系列の変更の知らせを受けて描くだけで、点は100msまとめてから描く。
- 探索状況は `EngineSearchStatus`（`src/engine/enginesearchstatus.h`）を info 1行につき1回送る（`ThinkingInfoFormatter::searchStatusUpdated`）。3桁区切りなどの書式は表示の側（`EngineInfoWidget`）で決める。通信ログの1行は `UsiCommLogModel::logLineAppended` で送る。
- エンジンの表示の持ち主は既存の `EngineDisplayPresenter` に集めた。検討中かは `AnalysisSession::isConsidering()` だけを正本にし、検討タブの実行の状態（`ConsiderationTabManager::RunState`）はセッションの知らせから決める。2つ目のエンジン欄と評価値グラフの系列名は文書の対局者（`KifuDocument::playersChanged`）から決める。
- 矢印を出すかは検討タブの設定（`ConsiderationSettings::showArrows`）の1つで、`CandidateArrowPresenter` はフック `showConsiderationArrows` で読む。
- `SessionResetCoordinator` の Deps は5個（文書・ライブ記録・読み込み・評価値グラフ・エンジンの表示）になった。

### R7. 盤の操作の判定・局面編集・ハイライト（board・views・ui）

**問題**

- **1回のクリックが4段の判定を通る。**
  1. UiStatePolicy の表で `setMoveInputEnabled` を切り替える。
  2. `BoardInteractionController` の `Mode::Edit` なら 1 を無視して通す。
  3. `std::bind` で渡された `isHumanTurnNow` で、動作状態ごとに分岐する。
  4. `BoardMoveController::playMove` が動作状態ごとに分岐し直し、`canAcceptMove()` と `isHumanSide` で手番をもう一度確かめる（`boardmovecontroller.cpp:62`）。
- **合法手を表示する条件が複雑。** 8項目を並べた式で（`boardinteractioncontroller.cpp:245-248`）、その中で `m_mode == Mode::Edit` と `m_view->positionEditMode()` の両方を見ている。
- **`Mode` は実質 bool になっている。** `Mode::HumanVsEngine` はどこからも設定されず（`setMode` を呼ぶのは `positioneditcontroller.cpp:126,159,161` だけ）、`Mode` は「局面編集中か」の bool と変わらない。
- **定跡手は遠回りして盤に渡している。** USI の手をいったん盤の `QPoint` に戻してから `playMove` に渡す。成り・不成は `setForcedPromotion(true/false)` で前後を挟み、`ShogiGameController` の可変状態を経由して伝える。
- **「局面編集中」を3通りで表している。**
  - 3通りは `ShogiView::positionEditMode`、`BoardInteractionController::Mode::Edit`、`AppActivityState::DuringPositionEdit`。読む側もばらばらで、移動前の確認と終了処理はビューを、`BoardMoveController` は動作状態を、`BoardInteractionController` は Mode とビューの両方を見ている。
  - 開始と終了の処理は2クラスにまたがる。`PositionEditCoordinator` は毎回 `BeginEditContext`/`FinishEditContext` を組み立てて渡す。`PositionEditController` はそれを受け取りながら、メニューのスロットのために `m_view`・`m_gc`・`m_bic` を `QPointer` で控えている。
  - 編集前の Mode を int で控え、未保存を -1 で表している（`m_prevBoardMode`）。Mode は HumanVsHuman にしかならないので、戻す意味がない。
  - 「編集終了」ボタンは、オブジェクト名の文字列 `findChild("editExitButton")` で3回探す。
  - 終了処理が返す SFEN は、受け手がログに出すだけ（`positioneditcoordinator.cpp:65`）。`MainWindowActions::resetRecordForEditedPosition` が盤から SFEN を作り直している。
- **ハイライトの寿命と色の扱いが脆い。**
  - `ShogiViewHighlighting` は、呼び出し側が `unique_ptr` で持つハイライトを非所有ポインタの一覧として受け取る。一括消去のときは `highlightsCleared` を出し、呼び出し側が自分の `unique_ptr` を捨てて合わせる（同期は呼び出し側4か所）。
  - 描く色は、渡された色の成分から推測している。`red() > 200 && green() < 100 && blue() < 150` なら移動元の色、それ以外は移動先の色（`shogiviewhighlighting.cpp:109-113`）。呼び出し側は5種類の色を渡すが、表示色はこの推測で決まる。色を少し変えると、表示の種類が黙って入れ替わる。

**書き直し案**

- 動作状態ごとに入力の行き先を表す型 `BoardInputRoute`（Edit・Variation・LocalGame・Csa・None）を1つ作り、`canPick(Turn)`・`submit(UsiMove 候補)` を持たせる。
  - `BoardInteractionController` は、クリックを from/to の候補に変えることとハイライトだけを受け持つ。成りの選択は行き先の側で行う。
  - 定跡手は `UsiMove` のまま渡す。
  - `Mode`・`setMoveInputEnabled`・`HumanTurnPolicy` のコールバックはなくす。
- 局面編集は1つの `PositionEditSession` が受け持つ。コンストラクタで依存を受け取り、開始・終了・メニューの操作・編集の着手を持つ。
  - 「編集中」は動作状態の1つだけにし、ビューの描き方は動作状態から決める。
  - 編集終了ボタンは `BoardChromeWidget` のシグナルにする。
  - 終了処理が返す SFEN で棋譜を作り直す。
- ハイライトは値で渡す `setHighlights(Layer, QList<QPoint>)` にする（Layer は Selection・LastMoveFrom・LastMoveTo・LegalDestinations）。色は Layer ごとに `BoardColors` から決める。`FieldHighlight`・`highlightsCleared`・呼び出し側の `unique_ptr` 群はなくなる。

**規模とテスト**

- 入力の判定は6〜7ファイル約600行、局面編集は5ファイル約500行で、`Mode` の廃止と同時に行う。ハイライトは views の2ファイルと呼び出し側5ファイル。
- テスト: `tst_legal_move_highlights`・`tst_board_drag_repaint`・`tst_lifecycle_runtime`・`tst_positionedit_gamestart`・`tst_service_initialization`・`tests/gui`。

**状態: 済（f6f3fe36）**。案から変えた点:
- 行き先は型を動作状態ごとに分けず、インターフェイス `BoardInputRoute`（`acceptsInput`・`isEditing`・`submit`。`src/board/boardinputroute.h`）にした。主画面では `BoardMoveController` が動作状態で処理を振り分け、詰将棋を解くダイアログは自分が行き先になる。`submit` は駒台・駒箱を含む局面編集の移動も渡すので、盤の座標（移動元・移動先）のままにした。クリックは行き先が1回だけ `UsiMove` にし（成りの選択もここ）、局面編集以外は `playUsiMove` で処理する。定跡手と通信対局へ渡す手も `UsiMove` のまま。
- 受け付けるかは `HumanTurnPolicy::isHumanTurnNow`（局面編集中は常に）だけで決め、`UiStatePolicyManager` の盤面クリックの項目は廃止した。動作状態が変わったら、つまんでいた駒を戻す。`HumanTurnPolicy` は層の規則（ui は app を include しない）に合わせて `src/ui/controllers` へ移した。
- 局面編集は `PositionEditSession`（`src/board/positioneditsession.h`）。開始・終了は表示中の局面・編集した局面を `editingStarted`/`editingFinished` で渡し、受け手（`MainWindowActions::resetRecordForEditedPosition`）はその局面で棋譜を作り直す。ビューの描き方（`setPositionEditMode`）は、このクラスが動作状態に合わせて切り替える。
- ハイライトの色は、ユーザーが選ぶ配色（`BoardColors`）に加えず、層ごとの固定の色を `ShogiViewHighlighting` の表に置いた（今の表示色と同じ）。

### R8. エンジン設定の保存形式を5か所が直接読み書きしている（services・engine・dialogs）。B8 の原因

2つの範囲の監査で別々に見つかった。

**問題**

- **保存形式を知っているコードが5か所あり、それぞれ `QSettings` を直接開いている。** 形式はエンジンごとのオプション配列 `{name,type,default,min,max,value,valueList}`。
  - `engineregistrationhandler.cpp:314-325, 336-359, 373-379`: 全項目とエンジン一覧を書く。
  - `usiprotocolhandler.cpp:37-59`: `"name"`・`"value"`・`"type"` を生の文字列キーで読み、button 型は値が on のとき送ると自分で解釈する。
  - `enginesettingsoptionhandler.cpp:97-131, 330-343`: `valueList` の JSON 形式と旧い空白区切りの形式の両方を読むのはここだけ。値だけを添字の順に書き戻すので、並びが一致していることが前提になっている。
  - `enginepondersettings.cpp:14-57`: `USI_Ponder` を配列と GUI 側の別のキーの2か所に持ち、同期するコードがある。
  - `enginelistsettings.cpp:13-32`。
- **`"USI_Ponder"` の特別扱いが4か所にある。**
- **エンジン名を最上位のグループ名にしている（B8）。**
- **未使用の定数がある。** `enginesettingsconstants.h:16` の `SettingsFileName` は使われておらず、`settingscommon` の `kIniName` と二重になっている。
- **`UsiProtocolHandler` を設定ファイルなしでテストできない。**

**書き直し案**

- 値型 `EngineProfile{name, path, author, QList<EngineOption>, 先読みの設定}` と、services の `EngineProfileStore`（名前で読み・書き・削除する）を作る。保存形式を知るのはここ1か所だけにし、ほかの層は `QSettings` を開かない。
- 保存先は専用の親グループ（例: `EngineOptions/<エスケープした名前>`）に移し、旧形式からの移行はこの1か所で行う。
- `UsiProtocolHandler` には、送るオプションの一覧を外から渡す。`EngineOption` 型は services の側へ下ろす。

**規模とテスト**: 5〜7ファイル約250行。テストは `tst_engineregistrationhandler`・`tst_enginepondersettings`・`tst_settings_roundtrip`。

**状態: 済（9d05747b）**。案から変えた点:
- `EngineProfileStore`（`src/services/engineprofilestore.h`）は値型の `EngineProfile` を作らず、エンジン名で読み書きする関数の名前空間にした（一覧の `loadEngines`/`saveEngines`、オプションの `loadOptions`/`saveOptions`、先読みの `loadPonder`/`savePonder`、削除の `removeEngine`）。エンジン一覧の検索・選択（`EngineListSettings`）は残し、読み書きは店に任せる。
- エンジンを起動するときに送るオプションは値 `EngineProfileStore::LaunchOptions` にまとめ、`UsiEngineSession` が店から作って `UsiProtocolHandler::setLaunchOptions` に渡す。プロトコルの処理は設定ファイルを開かない（`tst_usiprotocolhandler::launchOptionsAreSentAfterUsiOk`）。
- `USI_Ponder` の名前と判定（`kPonderOptionName`・`isPonderOption`）は店に置き、ほかはそれを使う。`EngineOption` は `src/services/engineoption.h` へ下ろした。
- `valueList` は JSON で書き、旧い空白区切りは読むときだけ解釈する。値は名前で突き合わせて書き戻す（`tst_engineprofilestore`）。旧形式の移行（B8）も `EngineProfileStore::migrateLegacyOptionGroups()` に移した。

### R9. 画面部品の共通化（dialogs・widgets）

#### R9-1. 文字の大きさ（A-/A+）の仕組みが3つある

2つの範囲の監査で別々に見つかった。

**問題**

- **ダイアログの仕組みが3つある。**
  - (a) `FontSizeHelper` と、自前で作った A-/A+、`applyFontSize` スロットの組（15ダイアログ。ボタンは .ui に置くものもコードで作るものもある）。
  - (b) `DialogFontScale::install(dialog, "id")` で自動で付けるもの（7ダイアログ）。`"DialogAppearance/%1/fontSize"` の文字列キーで、`Setting<T>` の表の外に保存している。
  - (c) `MenuWindow` 独自の、範囲チェック付きのカウンタ。
- **パネル側の A-/A+ は widgets の中だけで9通りある。**
  - ボタンの型（QToolButton/QPushButton）、大きさ（5通り）、見た目（`panelToolButton`/`fontButton`）、ツールチップ（3種）がばらばら。
  - 上限・下限で無効にするのは3か所だけで、そこも 8/24 を直書きしている。
  - `RecordPaneAppearanceManager`・`CommentEditorPanel`・`AnalysisResultsPanel` は `FontSizeHelper` を使わない。
- **`DialogUtils::updateFontButtons`（`dialogutils.cpp:62-83`）は表示文字でボタンを探す。**
  - ボタンの文字が `"A-"`/`"A+"` かどうかで探し、ツールチップとスタイルを上書きする。
  - そのため、各ダイアログが付けている tr 文字列（「フォントサイズを縮小」など16個）は表示されないまま、訳だけが4言語分残っている。
  - ボタンの文字を変えると、黙って機能しなくなる。
- **(a) は相対的な大きさを各ダイアログが付け直している。**
  - (a) は全部品に同じ大きさを付けるので、相対的な大きさを各ダイアログが後から付け直している（`csawaitingdialog.cpp:284-290`、`kifuanalysisdialog.cpp:333-340`、`josekiwindow.cpp:372-376`、`josekimoveinputwidget.cpp:441-455`、`jishogiscoredialog.cpp:201-215`）。(b) は元の大きさから相対的に拡大するので、付け直しは要らない。
  - 文字を大きくしたときに欠けないようにする処理も、2か所（`kifuanalysisdialog.cpp:127-132`、`tsumeshogisearchdialog.cpp:149-157`）が (b) の `ensureContentFits` を別に書いている。

**書き直し案**

- ダイアログは、(b) を `Setting<int>`/`Setting<QSize>` を受け取れるように広げて、すべて (b) に揃える。
- パネルは `FontSizeControl`（2つのボタンを `FontSizeHelper` に結び付け、範囲・無効化・見た目を受け持つ部品）を共通で使う。
- 文字一致での探索をやめ、ボタンは objectName かメンバで持つ。

**規模とテスト**: dialogs 約18ファイル（各15〜30行減）、widgets 約10ファイル、services 1。テストは `tst_dialog_appearance`（文字一致に依存している。131行目）と `tst_menu_window`。テストが objectName で参照しているので、objectName は残す。

**状態: 済（47c5ea2b・a6fd2089）**。案から変えた点:
- `DialogFontScale::install(dialog, Setting<int>, Options)` の `Options` に、範囲・保存するウィンドウの大きさ・ダイアログが置いた A-/A+（.ui のボタンなど）・大きさを出すラベルを渡せるようにした。ボタンを渡さなければ、これまでどおり決定ボタンの列に作る。`AppSettings::dialogFontSize(id)` などの文字列 id の関数は、同じキーの `Setting<T>` の宣言（例: `AppSettings::promotionDialogFontSize`）に置き換えたので、保存値はそのまま読める。
- 相対的な大きさ（太字の見出し・プレビューなど）は作るときに1度だけ付け、文字の大きさに合わせて変える寸法（表の行の高さなど）は `fontSize()` の `fontSizeChanged` につないだ。盤（`TsumePlayDialog`）は `dialogFontScaleExcluded` で対象外にする。
- `FontSizeControl`（`src/widgets/fontsizecontrol.h`）はボタンに役割のプロパティ（`kRoleProperty`）を付け、テストは表示の文字ではなくこれで探す。ボタンの大きさはパネルでは並ぶボタンに合わせて呼び出し側が決める（`Sizing::Panel`）。
- `MenuWindow` の独自の範囲チェック、`RecordPaneAppearanceManager`・`CommentEditorPanel`・`AnalysisResultsPanel` の増減の処理も `FontSizeHelper` にした。

#### R9-2. エンジンを選ぶ部品がない

**問題**

- **同じ処理を7か所以上で別々に書いている。** エンジン一覧の読み込み・選択の復元・設定ダイアログ・エラー文を、`startgamedialog`・`csagamedialog`・`kifuanalysisdialog`・`tsumeshogisearchdialog`・`tsumeshogigeneratordialog`・`tsumecollectiondialog`・`considerationtabmanager`/`considerationuicontroller` がそれぞれ書いている。
- **前回選んだエンジンの戻し方が4通りある。**
  - 共通の `EngineListSettings::restoreSelection`/`saveSelection`
  - StartGame だけの `savedPlayerIndex`（番号・名前・パスで照合）
  - CSA の `findText(name)` から番号への後戻り（専用のキー `csaEngineName`/`csaEngineNumber`）
  - 詰将棋問題一覧の `"@hayanagi"` という印の文字列
- **作者名の扱いに漏れがある。**
  - 同じ型 `EngineEntry` を1項目ずつ写しており、StartGame と CSA は作者名を落としている。
  - `ChangeEngineSettingsDialog` の初期化は段階ごとで、作者名を渡すのは7か所のうち2か所だけ。開く場所によって、見出しに作者名が出たり出なかったりする。
- **同じエラー文と範囲確認が重複している。**
  - 「将棋エンジンが選択されていません。」が7か所にある。
  - KifuAnalysis と TsumeSearch は `engineList()` と `engineNumber()` を外に出し、呼び出し側3か所（`kifuanalysiscontroller.cpp:57-71`、`tsumesearchcontroller.cpp:43-52`、`gamestartoptionsbuilder.cpp:108-110`）がそれぞれ添字の範囲を確かめ直している。

**書き直し案**

- widgets/ に `EngineSelector` を置く。持たせるのはコンボ・「設定」ボタン・未登録の案内・絞り込み・内蔵エンジン項目の有無。
  - `SelectionKeys` を受け取り、`std::optional<EngineEntry> selected()` を返し、`settingsRequested(EngineEntry)` を出す。
  - 層の規則上、widgets から dialogs は include できないので、設定ダイアログはシグナルで頼む。
- `ChangeEngineSettingsDialog(const EngineEntry&, QWidget*)` の1段で作れるようにする。

**規模とテスト**: dialogs 7、widgets 1、ui 3、game 1。テストは `tests/gui/tst_tsume_search_dialog.cpp`（`engineList().at(engineNumber())` を使っている）と `tst_startgamedialog`。

**状態: 済（77436034）**。案から変えた点:
- `EngineSelector`（`src/widgets/engineselector.h`）は QWidget ではなく、ダイアログが置いたコンボ・「設定」ボタン・案内を結ぶ QObject にした（.ui の部品と objectName をそのまま使える）。内蔵エンジンの項目（詰将棋問題一覧の Hayanagi）を選んだことは空の名前で保存し、`"@hayanagi"` の印は `TsumeshogiSettings::migrateCollectionEngineSelection()` で移す。通信対局も名前で保存し、以前の番号は読むだけにした（`NetworkSettings::csaEngineName`/`csaEngineNumber` は廃止）。
- 対局開始ダイアログは R9-3 で `PlayerSetupWidget` に組み替えるときに使う（コンボの先頭の「人間」を内蔵の項目にする）。
- 棋譜解析・詰み探索のダイアログは選んだエンジンを `engine()`（`std::optional<EngineEntry>`）で返す。テストは `tst_engineselector`。

#### R9-3. 対局開始ダイアログが、先手・後手を「1」「2」で二重に書いている

**問題**

- **.ui の約半分が二重。** `startgamedialog.ui` の1117行のうち約600行が、先手・後手の同じ部品の組（`comboBoxPlayer1/2`・`stackedWidgetPlayer1/2` など22組）。
- **.cpp の処理もすべて 1/2 で分かれている。** `data()`、`swapSides()`（4つの値を1つずつ入れ替え）、`saveGameSettings`/`loadGameSettings`/`resetSettingsToDefault`（同じ約25項目を3回列挙）、`updatePlayerUI(int)`、`showEngineSettingsDialog(int)`。
- **受け渡す構造体も平ら。** `StartGameDialogData` は `isHuman1/isEngine1/engineNumber1/…` の形。`isHuman` と `isEngine` が別の bool なので、ありえない組み合わせも表せてしまい、builder がそれを防いでいる（`gamestartoptionsbuilder.cpp:104` の `data.isEngine1 && !data.isHuman1`）。設定側は、すでに `GameSettings::StartGame::Player` 構造体にまとまっている。
- **開始局面の一覧が3か所にある。** .ui のコンボの15項目、`GameStartOptionsBuilder::kPositions`（14項目、添字で対応）、`Handicaps::kAll`（16項目）。保存も番号と日本語名の2つを持ち、`setCurrentText` で戻している（`startgamedialog.cpp:532-537`）。
- **未使用の公開関数がある。** `StartGameDialog::engineList()` はどこからも呼ばれていない。

**書き直し案**

- `PlayerSetupWidget`（人間/エンジン・名前・R9-2 の `EngineSelector`）と、`TimeControl::Side` を読み書きする `TimeControlEditor` を作り、2つずつ置く。
- 構造体は `std::array<PlayerChoice,2>`（`Kind kind; QString humanName; EngineEntry engine; TimeControl::Side time;`）にする。
- 開始局面は `Handicaps::kAll` から作る。
- 設定と入力欄の対応は表にして、読み込み・保存・既定値に戻すを1か所にする。同じ手法は `csagamedialog.cpp:482-559` と `tsumeshogigeneratordialog.cpp:134-176, 329-339` にも使える（後者は既定値を直書きしていて、設定の宣言と二重管理）。

**規模とテスト**: dialogs 3ファイル（.ui 約600行減）、game 2、app 1。`tst_startgamedialog`・`tst_game_start_flow`・`tst_game_start_options`・`tst_wiring_contracts`・`tests/gui/tst_start_game_flow`・`scripts/docs/shoot_tables.py` が部品名を参照しているので、objectName は残す。

**状態: 済（a90f603c）**。案から変えた点:
- `PlayerSetupWidget`・`TimeControlEditor`・`SettingsFormBinding`（設定と入力欄の対応の表）は `src/widgets` に置いた。対局者のコンボは以前と同じく「人間」とエンジンを1つのコンボに並べ、R9-2 の `EngineSelector` の内蔵の項目を「人間」に使う。保存したエンジンが削除されていれば人間に戻す（以前と同じ）。
- `PlayerChoice` の時間は `TimeControlSide`（ミリ秒）にし、時間設定の検証（`TimeControlValidator`）も同じ型を受け取る。開始局面は `Handicaps::Handicap*`（nullptr は現在の局面）で渡し、手合割の名前で保存する。以前の版の番号は以前の並びとして読むだけにした。
- `SettingsFormBinding` は詰将棋生成ダイアログにも使い、「既定値に戻す」は設定の宣言の既定値を使う（直書きの既定値を消した）。CSA の対局ダイアログは項目ごとの変換（パスワード・履歴）が多いので使わなかった。

#### R9-4. ドックのパネルの作り方が2系統ある

**問題**

- **作り方が2系統ある。**
  - QWidget を継承し、コンストラクタで部品をすべて作る: `ThinkingPanel`・`GameInfoPane`・`RecordPane`。
  - QObject を継承し、後から部品を作る: `ConsiderationTabManager`・`UsiLogPanel`・`CsaLogPanel`・`CommentEditorPanel`・`AnalysisResultsPanel`・`BranchTreeManager`。
- **入口が5種類ある。** `DockCreationService` から見て、QWidget をそのまま渡す、`buildUi()`、`buildCommentUi()`、`buildView()`、`containerWidget()`（`m_uiBuilt` フラグで初回だけ作り、`new QWidget` を親なしで作る）。
- **後から作る側に null チェックが積み上がっている。** メンバの `if (m_x)` は consideration 52・comment 20・analysis 20・csa 10 に対し、コンストラクタで作る側は0。
- **`AnalysisResultsPanel` が余計な仕事をしている。**
  - 自分の入ったドックを表示・前面化する（`m_dock->raise()`）。
  - モデルは1つしか作られないのに、`showWithModel()` のたびに接続を付け直す。
  - 解析中は選択モードを切り替え、`m_autoSelecting` フラグで自動選択を見分ける。

**書き直し案**: パネルをすべて QWidget の派生にし、コンストラクタで部品を作りきる（null チェックは消える）。パネルからドックの操作とモデルの差し替えを取り除く。

**規模とテスト**: widgets の6クラス（約2,800行）と `DockCreationService`。テストは `tst_usilogpanel`・`tst_csalogpanel`・`tst_wiring_analysistab` ほか4本。

**状態: 済（5cd05642）**。案から変えた点:
- クラス名（`ConsiderationTabManager`・`BranchTreeManager`）は変えていない。分岐ツリーは描く `QGraphicsView` を子に持つ（`view()`）。
- 棋譜解析の結果のドックは、解析を始めたときに `KifuAnalysisController`（Deps の `resultsDock`）が出す。結果の表のモデルは `setModel` で1度だけ結ぶ。解析中は最後に確定した行を選んで見せ、その間の選択・盤面の表示の通知は無視する（選択モードは切り替えない）。

#### R9-5. 思考の表と解析結果の表のモデル

**問題**

- **モデルの作り方が揃っていない。** 棋譜欄・分岐候補・対局情報のモデルは、値の行・列の enum・差分更新の形。思考の表と解析結果だけは `AbstractListModel<T*>` で、`new`・`qDeleteAll`・`delete` で行の寿命を管理している（所有権のガイドラインに反する）。
- **解析結果の1行 `KifuAnalysisResultsDisplay` の作りが曖昧。** シグナルを持たないのに QObject で、public フィールドと getter/setter が混在している。表記ごとの表示キャッシュを2クラス（`ShogiInfoRecord` と `KifuAnalysisResultsDisplay`）で別々に手書きしている。
- **列番号を直書きしている。** 解析モデルの列は 0〜7 の直書きで、パネル側も 3・5・6・7 列のデリゲート設定・列幅の配列・`minWidths[7]` で同じ番号を直書きしている。
- **同じ部品を2つの表が別々に組み立てている。** 数値の右寄せデリゲート、盤面ボタン、読み筋のデリゲート、「表示」ボタン列とそのクリック判定、列幅の保存と文字の大きさ。
- **指し手の書体の選び方が2経路ある。** 4つのモデルの FontRole と `KifuMoveDelegate`。

**書き直し案**

- 行は値で持つ（`QList<ThinkingInfoRow>`、変更しない構造体 `KifuAnalysisRow`）。`AbstractListModel` は廃止する。
- 解析モデルに列の enum を作る。
- `ThinkingTableView` を一般化した「読み筋の表」を作り、解析結果の表もそれを使う。
- 表示キャッシュは、表記の設定が変わるたびに増える番号で無効にする。

**規模とテスト**: 約8ファイル（約1,200行）。テストは `tst_abstractlistmodel` ほか5本以上。

**状態: 済（2764bf92）**。案から変えた点:
- 思考の表の行は `ThinkingInfoRow` を持つ `ShogiInfoRecord` の値（表示の文字列を覚えるため）、棋譜解析の結果の表の行は `KifuAnalysisRow`（`src/models/kifuanalysisrow.h`）の値にした。`AbstractListModel` と `tst_abstractlistmodel` は削除した。
- 共通の表は `ThinkingTableView` を一般化せず、基底の `PvTableView`（`src/widgets/pvtableview.h`）を作り、`ThinkingTableView` と棋譜解析の結果の表がそれを使う。列幅の決め方と保存（思考の表は表の組ごと、解析の結果は内容に合わせて広げる）はそれぞれに残した。
- 指し手の書体は FontRole と `KifuMoveDelegate` の2経路のまま、決め方を `MoveFont`（`src/common/movefont.h`）の1か所にした。

#### R9-6. 局面送り・編集ボタンが2組ある

- **局面送り。** 6つのボタンが棋譜欄（`recordpane.cpp:128-183`）と評価値グラフ（`evaluationchartwidget.cpp:153-205`）に1組ずつある。12個のアクセサで外に出し、有効/無効は `arrowButtonsEnabledChanged` で中継して合わせている。
  - 案: `KifuNavigationController` が持つ6つの QAction にし、両方の欄の `QToolButton::setDefaultAction` で共有する。
- **編集ボタン。** 元に戻す・やり直す・切り取り・コピー・貼り付けと「更新」「未反映」の表示、破棄の確認ダイアログを、コメント欄（`commenteditorpanel.cpp:400-433`）と対局情報（`gameinfopane.cpp:393-404`）で二重に実装している。
  - 案: 標準ショートカット付きの QAction 群を共通の部品にする。

**状態: 済（8876a775）**。案から変えた点:
- 局面送りは `KifuNavigationController` が6つの QAction を持つ。ボタンは `QToolButton::setDefaultAction` にせず、今の QPushButton を `bindButtons()` で QAction に結び付けた（押すと実行し、有効・無効は QAction に従う）。`setDefaultAction` ではボタンの文字が QAction の文字になり、棋譜欄（縦に並ぶので ▲▼）と評価値グラフ（横軸が手数なので ◀▶）で記号を変えられないため。アクセサはテストが使うので残し、`arrowButtonsEnabledChanged` の中継と `EvaluationChartWidget::setNavigationEnabled` はなくした。
- 編集ボタンは `EditActions`（`src/widgets/editactions.h`）にした。ツールチップには実際のショートカットを出す（以前は「Ctrl+Y」と固定の文字で、環境によっては違っていた）。対局情報の `QShortcut` 5つはこの QAction のショートカットに置き換えた。
- 「更新」ボタン・「修正中」「未反映」の表示は共通にしなかった。破棄の確認ダイアログはコメント欄（別の手へ移る前の確認）にしかなく、二重の実装ではなかった。
- 合わせて、詰将棋局面生成ダイアログの設定欄が開いた直後に6px足りずスクロールしていたのを直した（6319c5ae。R9-1 以降、内容の大きさが変わっても設定欄の古い希望サイズが使われていた）。

### R10. 文字列の表を手で同期している

#### R10-1. 駒セットの一覧と外観ダイアログ

2つの範囲の監査で別々に見つかった。

**問題**

- **駒セットの情報が5か所以上に分かれている。**
  - ID の一覧: `appsettings.cpp:36-64`（40個）。
  - ID から表示名を引く処理: `boardcolorpresets.cpp:45-126`（40分岐の if 連鎖。配色プリセットのクラスに入っている）。
  - 分類: `boardcolordialog.cpp:567-568` の分類名8つと、`694-695`（メモの初版では 693-695）の同じ順の接頭辞（`"torafu_"` など。分類は接頭辞の文字列一致で決める）。分類番号は `appsettings.cpp:217-225` で `qBound(0,…,7)` と手書き。
  - おすすめの組み合わせ: `boardcolordialog.cpp:389-414`。盤などの見本は日本語名（`"白榧"`）をキーにして指す。
  - 資源のパス: `pieceimageprovider.cpp:56-59`（メモの初版では 84-90。standard だけ特別扱い）と qrc。
- **足し忘れてもビルドが通る。** 駒セットを1つ足すときにどこかを忘れると、表示名が「標準の駒」になる・分類から漏れる・保存値が standard に戻される（`appsettings.cpp:67-73`）といった目立たない不具合になる。
- **外観ダイアログ（`boardcolordialog.cpp` 785行）の作りも複雑。**
  - 選び方が4系統あり（配色プリセット・テーマ・おすすめの組み合わせ・部品ごとの見本）、それぞれが選択状態を合わせる関数（`syncPresetSelection`・`syncThemeSelection`・`syncCatalog`）を持つ。
  - 部品の一覧は `m_componentLists[i]` と `static_cast<Component>(i)` で添字で結びついている。背景だけは `insertTab(2, …)` で途中に入れるのでタブの並びが別になり、その並びを `appearanceSectionNames()` に4つ目の表として持つ。
  - 色・見た目・駒セットの3つのシングルトンを、操作ごとに個別に読み、書き換えて戻している。
  - `palettes()` は駒セットによらないのに、駒セットが変わるたびに作り直している（以前の名残）。

**書き直し案**

- `PieceStyleCatalog` に `{id, Family, displayName(QT_TRANSLATE_NOOP)}` の表を1つ置き、ID の一覧・表示名・分類・資源のパス・分類の上限をすべてそこから引く。見本も ID で参照する。
- 外観は `AppearanceState{style, colors, visuals}` の値1つで扱い、適用と比較を1経路にする。
- タブは `struct Section { QString id; Component; QListWidget* }` の配列で持ち、保存は id で行う。

**規模とテスト**: services 3〜4ファイル、dialogs 1。テストは `tst_piece_images` と `tst_dialog_appearance` の一部。

**状態: 済（80aa1e03）**。案から変えた点:
- `PieceStyleCatalog`（`src/services/piecestylecatalog.h`）の表は `{id, Family, 表示名}`。画像のパスは表の分類（標準の駒だけ `:/pieces/`）から決め、絞り込みの項目と上限も `Family` から作る。表の整合（ID と表示名が重ならない・どの分類にも1つ以上ある・画像がある）は `tst_piece_images::catalogIsConsistent` で確かめる。
- 盤などの見本は新しい ID を足さず、今の「変わらない名前」（翻訳前の名前。`BoardAppearanceSample::key`）で指すままにした。代わりに、おすすめの組み合わせを `BoardAppearanceCatalog::combinations()` に移し、すべての組み合わせが存在する駒のセットと見本を指し、当てはめるとその組み合わせとして選ばれることを `tst_board_image_renderer::combinationsReferToCatalog` で確かめる（足し忘れ・書き間違いはテストで分かる）。
- 外観は `AppearanceState`（`src/services/appearancestate.h`）の値で読み、変えて `apply()` で戻す。テーマ・組み合わせ・部品の見本の選択状態は、この値から `BoardAppearanceCatalog::selectedIndex`・`combinationIndex` で求める（選択状態を合わせる関数は残したが、それぞれの比べ方は1つにした）。
- タブは id の表から作り、`m_sectionIds` で id とタブの位置を対応させる。保存は id（`AppSettings::appearanceSection()` は id を返し、旧版のタブ番号は id に読み替える）。部品の一覧は `Component` を添字にした配列にした。

#### R10-2. ドックの定義

**問題**

- **ドック1つの名前を5か所以上に書いている。** 12個のドックそれぞれが、次のすべてに名前を持つ。
  - `DockWidgets` のフィールド
  - `DockCreationService` のメンバと getter
  - 作成コード（共通設定の書き方が `setupDockFeatures` とラムダ `setupDock` の2通りあり、同じ設定を両方に書いている）
  - `DockLayoutManager::DockType` と `registerDock` の呼び出し
- **規定の配置を2か所に書いている。** 作成時の tabify の順と、`resetToDefault` の `bottomDocks`（`docklayoutmanager.cpp:97`）。後者には「起動時（DockCreationService）と同じ配置に戻す」というコメント（`:84`）がある。
- **保存と復元の対象が一部だけ。** 保存（`saveDockStates`）と復元（`DockCreationService::restoreDockState`）は別々のクラスにあり、どちらも同じ5つのドックだけを扱う。下段の7つ（思考・検討など）の表示・浮動の状態は保存しない。
- **名残の層と null チェックが残っている。**
  - `DockUiCoordinator` は、引数1つを nullptr にして渡すだけの層（39行）。
  - `ensureMenuWindow`/`ensureJosekiWindow` は遅延生成だった頃の名残で、起動時に必ず呼ばれる。それでも「未作成なら」の null チェックが8か所残っている（`JosekiWindowController` に5、自動化 API に3）。

**書き直し案**: `DockSpec{id, objectName, title, area, tabifyWith, defaultVisible, layoutBoundary, persist}` の表を1つ作り、`DockRegistry` がこの表だけを見て作成・登録・規定の配置・保存・復元を行う。保存は全ドックで同じ方式にする。

**規模とテスト**: 6ファイル、約1,050行を約450行に減らせる見込み。テストは `tst_dock_layout`・`tst_layout_boundary_widget`・`tst_settings_roundtrip`。

**状態: 済（7d829071）**。案から変えた点:
- 表は `DockRegistry::DockSpec`（`src/ui/coordinators/dockregistry.h`）。`tabifyWith` の代わりに、同じ場所のドックは表で前にあるドックのタブに後ろから重ねる（表の並びがタブの並び）。中身の扱い（`LayoutBoundaryWidget` で包む・描き直しを広げない・最小の高さ）と、前面に出すか・リセット時の幅も表に持つ。起動時の配置とリセットは同じ `placeAll()` で作る。
- `DockLayoutManager` は名前付きレイアウト・起動時のレイアウト・固定だけを受け持ち、ドックの一覧・リセット・保存は `DockRegistry` に任せる。`DockCreationService`・`DockUiCoordinator`・`DockWidgets`・`DockType` は削除した。
- 保存は全12個のドックで `DockSettings::dockState`/`setDockState`（グループごとの floating・visible・geometry）。既にあった5つのドックのキーは変えていない。下段の7つも、閉じたまま・浮動のまま次回に戻るようになった。
- メニューウィンドウと定跡ウィンドウは各コントローラのコンストラクタで作る。メニューウィンドウの分類は、ドックを作った後に集め直す（「表示」メニューのドックの切り替えも入る）。
- 行数は8ファイル 1,128行から6ファイル 785行（表と、表から作る処理を `DockRegistry` に置いたため、見込みほどは減っていない）。

#### R10-3. MCP・CLI・自動化 API の引数仕様

**問題**

- **同じ引数の仕様を最大5か所に書いている。** 例えば generate-tsume の `max_positions` は次の5か所にある。
  - `tooldefs.py:329`（既定5、1〜100）
  - `handlers_cli.py:263` と `:274`（既定5を2回）
  - `clicommands_tsume.cpp:143`（addOption の既定 `"10"`）
  - `clicommands_tsume.cpp:160`（intOption の既定10、1〜100）
- **既定値がすでに食い違っている。** analyze の `seconds` は、MCP が10、CLI が5、Request が5000ms。CLI の addOption に書いた既定値は使われず、`--help` に出るだけ。
- **範囲もずれている。** kifu.get の `max_moves` は MCP が1〜2000、アプリが1〜100000。goto の `ply` は MCP が0〜10000、アプリが0〜100000。アプリは受け付けるのに MCP は拒否する範囲がある。
- **既定値のリテラルが35回ある。** mcp SDK はスキーマの default を引数に補わないので、ハンドラが既定値を書き直している。
- **引数の検査と名前が揃っていない。**
  - 自動化 API は知らない引数を黙って無視する（手で検査しているのは dock.configure の `automationcommands_dock.cpp:164-168` だけ）。
  - 対象を指す引数名が `target` と `dialog` に分かれている。
- **CLI と Python の側に同じ形の繰り返しがある。** CLI は `failEvent(kInvalidArgument,…)` が27回、`kInvalidArgument` の定義が3ファイル、Reporter クラス4つが同じ形。Python は `_require_unrestricted_ui()` を7回呼ぶ。

**書き直し案**

- C++ に操作ごとの引数表 `ParamSpec{name,type,required,min,max,default,help}` を1つ置き、次の3つを同じ表から行う。
  - CLI の QCommandLineParser と usage の生成
  - フラグから JSON への変換
  - JSON-RPC の引数検査（知らないキーの拒否を含む）
- `shogiboardq-cli describe` で表を JSON Schema として出し、MCP はそこからスキーマを組み立てる（少なくとも pytest で一致を検査する）。
- Python 側は、ツール1つを「名前・スキーマ・既定値・権限・ハンドラ」の1か所で宣言するデコレータにする。

**規模**: C++ 約12ファイル、Python 5ファイル（`tooldefs.py` は854行）。Python のデコレータ化だけなら小さく始められる。

**状態: 済（828b0407）**。案から変えた点:
- 表は `ParamSpec`/`OperationSpec`（`src/automation/paramspec.h`）。CLI の表は `CliCommandSpecs`（`src/cli/clicommandspecs.h`）、自動化 API の表は `AutomationMethodSpecs`（`src/automation/automationmethodspecs.h`）。CLI のオプション名は引数名の `_` を `-` にしたもの。使い方の文も表から作る。
- 自動化 API は `AutomationDispatcher` が呼ぶ前に表で検査し（知らない引数・型・範囲・選択肢・必須）、既定値を補う。表に登録したメソッドと実際のメソッドが一致することは `tst_param_specs` で確かめる。表の行の数など、開いている画面で範囲が決まる引数だけはメソッドで検査する（`AutomationParams::requireInt` が残る）。
- MCP のスキーマは describe から組み立てず、`mcp/tests/test_param_specs.py` が describe の出力と比べる（そのまま渡すツールは引数名・必須・型・選択肢・既定値・範囲がすべて同じ。CLI に渡すツールは対応表で比べ、真偽の反転と秒→ミリ秒の換算も扱う）。この検査で見つかった食い違いはそろえた（analyze の秒数・generate-tsume の上限の既定値、max_moves と ply の上限、mate の sfen、行と列の上限など）。
- Python はデコレータにせず、スキーマの既定値をサーバーが呼び出し前に補い（ハンドラの既定値のリテラルは削除）、制限付きのツールは `_tool(..., unrestricted=True)` の宣言で検査する（`_require_unrestricted_ui()` の7回の呼び出しは削除）。
- 対象を指す引数名は `target` にそろえた（自動化 API の `dialog` と、MCP の click_dialog_button・close_dialog・get_widget_text の `dialog`）。MCP のクライアントは引数名を変える必要がある。
- CLI の `kInvalidArgument` の定義は `CliOutput` の1つにした。同じ形の Reporter クラス4つは、出すイベントがそれぞれ違うので残した。

#### R10-4. エラーの分類

**問題**

- **同じ失敗が経路で別の語になる。** 既存ファイルへの書き込みは、CLI で `io_error`、アプリで `file_error`、Python の検査で `file_exists`。引数の誤りは `invalid_argument` と `invalid_params` に分かれている。
- **一部のエラーコードがまとめられたり、意味が違ったりする。** -32603・-32600・-32700 は Python の表になく、`app_error` にまとめられる。kifu.load の解析失敗は `not_found`（-32005）になる（`automationcommands_kifu.cpp:92`）。
- **Python が C++ の値を手で写している。**
  - ソケットの既定の場所を、Python が QStandardPaths を OS ごとに真似て求めている。macOS では Qt の RuntimeLocation と食い違うとみられる。通常はエンドポイントファイルがあるので表に出ない。
  - 上限とファイル名も手で写している（`app_client.py:27` ↔ `automationserver.cpp:21`、`paths.py:21` ↔ `automationserver.cpp:22`）。

**書き直し案**

- C++ にエラー分類の enum と語（slug）を1つ置き、CLI の fail と `AutomationError` の両方で使う（JSON-RPC の `error.data` に slug を入れる）。
- 場所と上限は `shogiboardq-cli environment` が JSON で出し、Python は起動時に1回読む。

**規模とテスト**: C++ 約10ファイル、Python 3ファイル。pytest の期待値の更新が要る。

**状態: 済（26121096）**。案から変えた点:
- 分類は `ErrorKind`（`src/automation/errorkinds.h`）。語と JSON-RPC のコードの表を1つ持ち、`CliOutput::fail`/`failEvent` と `AutomationError` の両方が分類を受け取る。JSON-RPC のエラーには `error.data.slug` を付け、MCP サーバーはその語を返す（`data.slug` が無いときだけ表でコードから引く）。
- 語は CLI・アプリ・MCP でそろえた。既存ファイルへの書き込みは `file_exists`、引数の誤りは `invalid_argument`、知らないメソッド・コマンドは `unknown_operation`、棋譜の中身が読めないのは `parse_error`（新しいコード -32006。kifu.load の解析失敗もこれ）、エンジンの失敗は `engine_error`（-32007）。MCP サーバーだけの失敗（`app_unavailable`・`cli_failed`・`timeout` など）は `mcp/tests/test_environment.py` に一覧を持ち、それ以外の語は共有の表にあることを検査する。
- `shogiboardq-cli environment` はソケット・エンドポイントファイル・設定ファイル・メッセージの上限・エラーの分類を出す。MCP サーバーは関係する環境変数の組ごとに1回読む（`environment.py`）。そのため、既定のソケットを求めるには shogiboardq-cli が要る（見つからなければ `SHOGIBOARDQ_AUTOMATION_SOCKET` だけを使う）。

---

## 3. 中くらいの候補

| 候補 | 問題 | 書き直し案 |
|---|---|---|
| エンジンの起動失敗の知らせ方と枠の持ち主（engine・game・analysis） | **起動失敗**: 知らせ方が層ごとに入れ替わる。`UsiEngineSession` は起動中のエラーを戻り値に変え（`m_insideStart`）、`Usi::startEngine` は同期の `errorOccurred` に戻し、`EngineLifecycleManager`・`CsaEngineController` は戻り値を `(void)` で捨てる。そのため利用者がそれぞれ再入対策を持つ（`MatchCoordinator` の QueuedConnection、`AnalysisSession` の Starting/CancelStarting と `m_startupError`、Generator の `m_starting`/`m_stopRequestedDuringStart`、`KifuAnalysisController` の二重表示よけ）。<br>**枠の持ち主**: 4つの利用者が `createEngine(1, …)` で枠のエンジンを知らせずに破棄する。`AnalysisSession::stop()` は検討していないときも全部の枠を破棄し、`GameSession` は新規のときこれを「全エンジン停止」として使う（`gamesession.cpp:253`） | 起動の失敗は同期では決して知らせないと決め、`startAsync` を void にする。`EngineLifecycleManager::acquire(owner)` が move-only の `EngineLease` を返すようにし、取り上げは前の持ち主に知らせる |
| 分岐ツリー（kifu・widgets） | **変更通知**: `treeChanged` に中身がなく、`BranchTreeManager` は「nodeCount と revision が1増えた」から1手の追記だと推測している（`branchtreemanager.cpp:107-117`）。通知を抑える仕組みが3つ重なる（*Quiet 版のメソッド、beginBatchUpdate、revision）。<br>**所有権**: ノードは裸の new で作り、QHash が所有する。<br>**表示**: `BranchTreeManager` は QObject の eventFilter でビューを操り、素の `QGraphicsPathItem` に `setData` で役割を7つ持たせる。同じノードの見分け方が (行,手数) と nodeId の2通りある。開始局面のノードは `addNode` とほぼ同じ約40行で別に描く | 変更を型で知らせる（`nodeAppended(parentId, nodeId)`・`childrenReordered`・`subtreeRemoved`・`reset`）。children は `std::vector<std::unique_ptr<KifuBranchNode>>` にする。表示は `BranchTreeView : QGraphicsView` と `BranchNodeItem : QGraphicsItem` にし、外とのやり取りは nodeId だけにする |
| 盤の座標と対局者カード（views） | **座標計算**: マスの大きさを `m_squareSize` と `m_fieldSize` の2つで持ち、補い方が5か所に散らばる。`cachedFieldRect` は盤基準の座標を返すので、呼ぶ側の5か所以上でオフセットを足す。駒台は「盤の筋1・2のマスをずらす」という間接的な作り方で、駒の並びの表も2つある。段番号の文字の大きさの計算が2通りある。<br>**対局者カード**: `BoardChromeWidget` は先手・後手の部品を `std::array<…,2>` の並行配列6系統で持ち、10引数の `placeCard` で座標を足し上げる。外部の `setupNameAndClockFonts` が設定する書体は直後に上書きされ、効いていない | 変更しない値の型 `BoardGeometry` を一度だけ計算し、盤・各マス・駒台のセル・座標ラベル・カード・駒箱をすべてウィジェット座標で持つ。カードは `PlayerCard`（QFrame＋レイアウト）を1側1つにし、`card(Turn)` で触らせる |
| 定跡（kifu・dialogs） | **データ**: `JosekiRepository` が同じキーの2つの map（`m_josekiData` と `m_sfenWithPlyMap`）を手で同期しているが、揃っていない（`removeMoveByUsi` は片方だけ消す、`Repository::addMove` は手数付きの SFEN を登録しない）。変更できる参照を外に公開している。マージ登録済みの印は `sfen + ":" + usi` の文字列の集合。<br>**定跡手の入力**: `JosekiMoveInputWidget` は筋・段のコンボから USI を文字列で組み立て、core の `UsiMove`・`ShogiNotation` と重複する変換を自前で持つ（455行の大半）。局面は必ず渡されているのに、合法手から選ばせずに後から合法かを確かめる | 値型 `JosekiBook`（正規化キーから `JosekiPosition{sfenWithPly, moves}` を引く map 1つ）にし、Repository クラスは廃止する。定跡手は局面の合法手から選ぶ方式にし、受け渡しは `setEntry(const JosekiMove&)`/`entry()` にする |
| 書き出し側の重複（kifu/formats） | KIF と KI2 のヘッダー部分がほぼ同じ文。KI2 は本譜と変化で、手を1行に並べるループを二重に持つ。盤を再生して合わなければ `node->sfen()` から取り直す処理を、KI2 と JKF が別々に持つ（`ki2exporter.cpp:30-67`、`jkfexporter.cpp:92-141`）。USEN は符号化と復号が別ファイルで、駒打ちの符号表と base36 の文字表を別々に持つ。USI の position 文字列も2つの実装がある（`UsiExporter` と `RecordLine::positionCommand`） | 「Position 付きでノード列を歩く」共有の反復子を作る。`UsenCodec`（encode/decode）を1ファイルにまとめ、position 文字列は `RecordLine::positionCommand` に一本化する（R3 と同時に）<br>**状態: 済（32106804）**。反復子は `KifuNodeReplay`（`kifu/formats/kifunodereplay.h`。指す前の局面・手番・直前の移動先を読み、`advance()` で進め、合わない手はノードの局面から取り直す。変化は `branchFrom()` で写す）にし、KI2 と JKF が使う。KIF と KI2 の先頭は `KifExporter::appendHeaderLines`（KI2 は消費時間を書かない）、KI2 の本譜と変化の指し手の行は `appendKi2Moves` の1つのループにした。USEN は `UsenCodec`（`usencodec.h`。指し手・開始局面・手順の符号化と復号、駒打ちの符号表と base36 の文字表は1つ）。position 文字列は、R1 で手順を `UsiPosition` が持つようになったので `RecordLine::positionCommand` ではなく `UsiPosition::commandFor`（`command()` と `UsiExporter` が共有）に一本化した。src で "position sfen " を組み立てる箇所はここだけになった |
| 読み筋盤（dialogs） | `PvBoardController::buildSfenHistory` が自前の盤で局面列を作り、手数を常に `" 1"` にする。手の表記は、呼び出し側が渡した表示用の文字列を正規表現 `([▲△][^▲△]+)` で分割して使う。手数の状態がダイアログとビューアに二重にある。開始局面のハイライトは3段の後戻りで決め、結果は bool と int を7個並べた構造体 | `SfenReplay::Result` の各手から、表記を `KifuPresentation::move`、ハイライトを `BoardCoordinates` で作る。手数はビューアの番号だけにする<br>**状態: 済（d639d761）**。`PvBoardController` を廃し、値 `PvLine`（`dialogs/pvline.h`）にした。`SfenReplay` に手ごとの結果の型はないので、開始局面から `Position` で指して各手数の局面（手数を数える）・表記・ハイライトを作る（指せない手の手前で打ち切る）。日本語の表記は「同」を付けるため `KifuPresentation::move` ではなく `KifMoveText`（エンジンの読み筋と同じ）、英語は `KifuPresentation::move`。ハイライトは `PvLine::Highlight{optional<QPoint> from, to}`。開始局面のハイライトは「至った手、なければ盤の差分」の2段にし、平手で読み筋の初手を出す3段目は廃止。直したときに不具合を見つけた: 至った手が駒打ちのとき、開始局面の手番側の駒台を移動元にしていた（打ったのは手番の相手。先にテストで再現してから直した） |
| 局面集ファイル（dialogs・analysis） | 同じ「1行1局面」のファイルを、`SfenCollectionDialog::sfenFromLine` と `TsumeCollection::parse` が別々の規則で読む。`startpos` を受け付けるか、`position sfen … moves …` を指した後の局面にするか参考手順にするか、検証の方法が違う。「最近使ったファイル」の一覧も、局面集と定跡で同じ構造を2回書いている | `PositionCollectionFile::parseLine` を共有し、`RecentFileList`（データ）と `RecentFilesMenu`（メニューを作る補助）を作る<br>**状態: 済（23c05ea6）**。1行の規則は `PositionCollectionFile`（`analysis/positioncollectionfile.h`）に1つにした。position コマンドの指し手は指した後の局面、SFEN に添えた指し手は参考手順（詰将棋の解答）、局面は `SfenValidationService` で確かめ、指し手の形でない語・余計な語のある行は読めない行。詰将棋問題一覧は、これに加えて受方の玉があり王手がかかっていないことを確かめる。一覧は `RecentFileList`（common）、メニューは `RecentFilesMenu`（widgets）で、定跡ウィンドウと局面集ビューアが使う |
| 詰将棋対局（dialogs） | `TsumePlayDialog` が対局と正解手順の再生を `m_reviewing` などのフラグで切り替え、全部品の有効・無効を三項演算で決める。挑戦と正答の記録をダイアログが付ける。成りの確認は `QMessageBox` を自前で組み立てた3択で、メイン盤の `PromotionFlow` と別 | `enum class Mode { Playing, Reviewing }` と、部品ごとの有効・無効の表を1か所に置く。記録は `TsumeGameSession` へ移す<br>**状態: 済（0d72db56）**。`Mode` と `controlStates()`（部品ごとの有効・無効の表）・`applyControlStates()` にした。挑戦は開始してから初めて攻め方の手番になったとき、正答は詰ませたときに `TsumeGameSession` が記録する（どちらも `start()` ごとに1回）。成りの確認は主画面と同じ `PromoteDialog`（`PromoteDialog::ask`。1関数だけの `PromotionFlow` は削除）。閉じたときは主画面と同じく不成になる |
| KifuPresentation と common/（common） | `kifupresentation.cpp` が、`core/shoginotation.h` の駒名・全角の筋・漢数字の段、`core/handicaps.h` の手合割の表（B4 の原因）、`kifu/gameinfokeys.h` の見出しを写している。英語表記の移動元の判定は駒の利きを手書きしていて、KI2 の同じ判定（`ki2movenotation.cpp:149-160` は `EngineMoveValidator::pseudoLegalMoves` を使う）と2系統。`Options current` というグローバル状態がある。common には QWidget に依存するスタイル類、models 専用の `abstractlistmodel.h`、持将棋の計算、position コマンドの組み立てが同居している | KifuPresentation を kifu/ へ移し、表は core のものを使う。移動元の判定は KI2 と共有する。手合割の表示名は `handicaps.h` の表に `QT_TRANSLATE_NOOP` 付きで持たせる。スタイル類は UI 用の共通ディレクトリへ移す |
| 自動化 API の UI 操作（automation） | 12のメソッドがそれぞれ「受付時の検査 → 予約 → 実行時の再検査 → 失敗の握りつぶし」を手書きしている（`AutomationDeferredCall::schedule` が12回、`catch (const AutomationError&) { }` が12回）。マウスイベントの合成が4回書かれている。実行時の失敗は捨てられ、応答は常に `queued:true`。ウィジェットの型による分岐が、読み（18分岐）と書き（13分岐）で非対称 | `QueuedInteraction{QPointer target; check; perform}` と `queue()` を1つ作り、失敗の理由を記録して返す。マウス入力は `sendClick`/`sendDrag` に1本化し、ウィジェットの読み書きは型ごとのアダプタ表にする<br>**状態: 済（148c9da3）**。`AutomationInteractions::queue(QueuedInteraction{target, check, perform}, 応答)`（`automation/automationinteractions.h`）に13のメソッドを寄せた。応答に操作の番号 `interaction` を足し、`ui.interaction` で状態（pending・running・done・failed と分類つきの error・cancelled）を引ける。実行時に黙って何もしなかった分岐も失敗として記録する。MCP は `AppClient.call_queued` が実行を待ち、失敗をアプリの分類の語のエラーとして返す（開いたモーダルダイアログが閉じていなければ state "running"）。応答そのものを実行の後まで遅らせると、モーダルダイアログを開く操作で応答が返らなくなるので、番号で引く形にした。マウス入力は `AutomationInput::sendClick`/`sendDrag`、読み書きは `AutomationWidgetAdapters` の表（型・読み・書きの行） |
| `Setting<T>` で表せない設定（services） | 列幅の配列の読み書きが `analysissettings.cpp:14-37` と `josekisettings.cpp:11-38` で同じコード（`:77-91` は別の形式）。範囲付きの整数は、読みと書きの両方に qBound を書く（4か所）。構造体と複数キーの対応は BoardColors だけが表を使い、`PieceSoundTone`・`BoardVisuals` は手書き。`TsumeshogiSettings` は既定値を2か所に書く。`csagamedialog.cpp:380-457` は接続履歴をダイアログ内のキーで直接読み書きする | `BoundedSetting<int>`・`ListSetting<int>`・`StructSetting<T>{{key,&T::member}…}` を setting.h に足して置き換える。CSA の接続履歴は networksettings.h へ移す<br>**状態: 済（41580259）**。setting.h に `BoundedSetting<T>`（読み書きの両方で範囲に収める）・`StructSetting<T>`（`SettingField<T>::of(key, &T::member)` の表。normalize を渡せる）・`ListSetting<T>`（配列の形式。キーは実行時に決めてよい）・`StructListSetting<T>` を足した。駒音の音量・音色、外観の駒の絞り込み、USI ログの送信先、詰将棋の対局・問題一覧の設定（既定値は構造体だけに書く）、思考の表・定跡ウィンドウの列幅を宣言1つにし、`josekisettings.cpp` は削除。`BoardVisuals` も `StructSetting` で読み、駒台の木目だけ「未保存なら盤の木目に合わせる」補いを残す。CSA の接続履歴は `NetworkSettings::csaServerHistory`（保存形式は同じ）。棋譜解析の結果の表の列幅（`:77-91`）は QVariantList の形式のまま残した |

---

## 4. 小さな修正・使われていないコード

監査の報告に基づく一覧。消す前に grep で確かめること。

**状態**: 下の各項目の末尾に書く。「済（de96637a）」は4章をまとめて直したコミット、「既に削除」はそれまでの段階で消えていたもの、「3章へ」は3章の書き直しで扱うもの。

- **使われていない API**
  - `StartGameDialog::engineList()` — **既に削除（R9-2、77436034）**
  - `UsiToSfenConverter::readUsiFile` — **既に削除（R2、6143b3b0）**
  - `KifuBranchTree::branchablePlysOnLine` — **済（de96637a）**
  - `BranchTreeManager::removeMoveNumberLabel` — **済（de96637a）**
  - `ShogiViewLayout::calculateRectangleForRankOrFileLabel`、中身が空の `fitBoardToWidget`、`standInnerEdgePx`、`labelBandPx`/`labelFontPt` の getter — **済（de96637a）**
  - `ElideLabel` の自動スクロール・手動パン・下線（設定する関数の呼び出しが0件、約100行） — **済（de96637a）。省略表示だけを残して書き直した**
  - `enginesettingsconstants.h:16` の `SettingsFileName` — **既に削除（R8、9d05747b）**
  - `kifu/josekitypes.h` の `JosekiEntry` — **済（de96637a）**
- **テストからしか呼ばれない API**
  - `KifuBranchTree::findByPlyOnMainLine`・`findBySfen`・`addMove`/`addMoveQuiet`/`addTerminalMove` — **3章へ（分岐ツリーの書き直しでテスト用の作り方へ移す）**
  - `AnalysisCoordinator` の `SinglePosition`/`startAnalyzeSingle` — **既に削除（R5、b625d5eb。`AnalysisCoordinator` ごと `AnalysisJob` へ吸収）**
  - `PvBoardController::goBack`/`goLast` — **3章へ（読み筋盤の書き直しで扱う）**
  - `KifToSfenConverter::mapHandicapToSfen`・`JkfToSfenConverter::mapPresetToSfen` — **`mapPresetToSfen` は既に削除（R2、6143b3b0）。`mapHandicapToSfen` は R2 以後 src から使うので残す**
- **書くだけで読まれない値・到達しない分岐**
  - `BranchTreeManager` の `BR_ROLE_PLY`・`BR_ROLE_STARTPLY`・`BR_ROLE_BUCKET`（`branchtreemanager_draw.cpp:137` で書くだけ） — **済（de96637a）**
  - `ShogiClock::m_resumingTurn` — **済（de96637a）**
  - `ShogiViewLayout` の `param1`/`param2` で引いている `tweak = 0` — **済（de96637a）**
  - `matchsearchsnapshot.h` の、時計がないときの分岐（到達しない） — **既に削除（R4、d8dbb2bd でファイルごと削除）**
- **効いていない処理**
  - `MainWindowAppearanceController::setupNameAndClockFonts`（`mainwindowappearancecontroller.cpp:146-170`）が設定する書体は、`BoardChromeWidget::relayoutCards`・`fitLabelFontToRect` がすぐ上書きする。 — **済（de96637a）。関数を消し、時計の太さ（DemiBold）は `BoardChromeWidget` で決める**
  - 各ダイアログの A-/A+ のツールチップの tr 文字列（16個）は、`DialogUtils::updateFontButtons` が上書きするので表示されない（R9-1）。 — **済（de96637a。R9-1 で残っていた `tsumeshogisearchdialog.ui` の分）**
- **文書と実装の食い違い**
  - `AutomationActionPolicy` の許可リストに `actionLanguageChineseSimplified`/`actionLanguageChineseTraditional` がない（文書の「言語切替は許可」と食い違う）。 — **済（de96637a）**
  - 同じヘッダーのコメント「言語切替・ドックレイアウト保存は含めない」が実態と逆になっている。 — **済（de96637a）**
- **転送だけ・名残の層**
  - `DockUiCoordinator`（39行。R10-2） — **既に削除（R10-2、7d829071）**
  - `JishogiScoreDialogController`（1関数だけの QObject。関数にしてもよい） — **済（de96637a）。`JishogiScoreDialog::showFor(position, parent)` にした**

---

## 5. 書き直し不要と判断した部分

- **対局の進行**
  - `MatchCoordinator` と Strategy 3つ（`MatchStrategyHost` 経由）: 一手の確定・終局・待った・中断が1か所にまとまっている。
  - `ConsecutiveGameSeries`・`GameStartOptionsBuilder`・`SennichiteDetector`・`NyugyokuJudgement`。
- **エンジン**
  - `UsiProtocolHandler`: 状態は1つの enum、遅れた結果は通番で捨てる、受信行は `UsiLineParser` で型付き。
  - `UsiEngineSession`・`EngineProcessManager`: プロセスの終了は `RetiringEngineProcess` の1か所。直すのは3章の起動失敗の知らせ方だけ。
  - `UsiGoTimeCalculator`。
- **通信対局**: `CsaClient`（接続状態と受信行の解釈）と値型の `CsaGameProgress`。
- **解析**
  - `AnalysisSession` の検討のやり直し（状態遷移が明示的な enum）。
  - `TsumeshogiVerifier`（引き出し型）・`TsumeCollectionSession`・`TsumePositionAnalyzer`・`TsumeProgressStore`・`TsumeGameSession`。
- **棋譜**
  - `UsiMove`/`BoardSquare`・`ShogiMove`・`RecordTime`・`Ki2MoveNotation`。
  - 表を1か所に持つ対応表のファイル（`terminaltype`・`terminalcodes`・`handicaps.h`・`shoginotation.h`・`kifmovetext.h`）。
  - `kifuformat`・`kifusavefile`・`kifuioservice`・`kifreader`。
- **合法手生成**: fmv のアルゴリズム（do/undo・打ち歩詰め・回避手生成）。問題は R1 の駒の符号化だけ。
- **ナビゲーションと表示**: `KifuNavigationState`・`KifuNavigationController`・`KifuDisplayPresenter`・`BoardSyncPresenter`（正本から毎回作り直し、写しを持たない）、`KifuLoadCoordinator`。
- **合成ルート・接続**
  - 合成ルート（`mainwindowcomposer*.cpp`）: Deps の組み立ては長いが1部品1ブロックで読める。ロジックを持つラムダは2つ（各1行）だけ。
  - `MenuActionsWiring`・`UiStatePolicyManager`（表と static_assert）。
- **表と描画の部品**
  - 手本になる形のモデル: `KifuRecordListModel`・`KifuBranchListModel`・`GameInfoEditModel`（値の行・列の enum・差分更新）。
  - 測定に基づく高速化で中に閉じている部品: `IncrementalTableView`・`TableHeaderView`・`ItemTextPainter`/`ItemTextCutter`/`ItemTextDelegate`・`LayoutBoundaryWidget`・`OpaqueBackground`。
  - `BranchTreeLayout`・`BoardStaticLayer`・`BoardSurfacePainter`・`PiecePainter`・`ThinkingPanel`・`ThinkingTableView`。
- **ダイアログ**
  - `JosekiWindow`: 定跡手の入力を除く。
  - `CsaGameDialog`: エンジン選択を除く。
  - `ChangeEngineSettingsDialog`/`EngineSettingsOptionHandler`: オプションの型ごとの `OptionEditor` で整理されている。
  - `TsumeCollectionDialog`・`TsumeshogiGeneratorDialog`・`MenuWindow`・`EngineRegistrationDialog`・`CsaWaitingDialog`・`JishogiScoreDialog` ほか小さなダイアログ。
- **自動化 API とサーバー**
  - `AutomationDispatcher`（JSON-RPC の解析）・`AutomationServer`（行バッファ）。
  - `KifuConversionService`・`BoardImageRenderer`・`EngineCatalog`。
  - `mcp/` の `jobs.py`・`app_client.py`（複雑だが必要な複雑さ）。
- **設定・共通部品**
  - `Setting<T>`・`SettingsCommon` と、大半の単純な設定項目の1行宣言。
  - `ErrorBus`・`AppState`/`AppActivityState`。

---

## 6. 進め方

1. **不具合 B1〜B8 を先に個別に直し、それぞれテストを足す。** どれも小さな修正で済み、書き直しを待たずに直せる。
   - B1: パスを型で表すか、読み込みを断る理由を明示する（仕様の判断が要る）。
   - B2: KIF の書き出しも指し手から表記を作る。
   - B3: 読めない手の方針を KIF と揃える（打ち切る）か、手番を指し手の符号で確かめる。
   - B4: 訳語の表に2つを足す。KifuPresentation の表を1つにするまでのつなぎ。
   - B5: 通信対局の終局でもライブ記録を確定する。
   - B6・B7: 棋譜の読み込みで、`MatchTimeRecord` と `AppState::players` を消す。
   - B8: 保存先のグループ名をエスケープし、専用の親グループの下に移す。旧形式からの移行が要る。
2. **R1（`Position`）と R2（棋譜の読み込み）を一緒に進める。** 効果が最も大きく、変換器のテストが揃っている。
3. **R3（`KifuDocument`）→ R4（対局の流れを1本に）→ R6（エンジンの表示）の順に進める。** R3 を済ませると、R4 と R6 の「作り直しで何を消すか」の多くがなくなる。R4 の時間の接口は、R1 の `UsiPosition` の後だと作りやすい。
4. **R5・R7・R8・R9・R10 は、ほかと独立していつでも進められる。**
   - R5 では、`MateOracle` を Generator の書き直しより先に作る。
   - R7 の `Mode` の廃止と局面編集の整理は同時に行う。

| 不具合 | 根本の書き直し |
|---|---|
| B1・B2・B3 | R2（R1） |
| B4 | 3章の KifuPresentation |
| B5 | R4 |
| B6・B7 | R3 |
| B8 | R8 |
| U1・U3 | R5 |
| U2 | R1（`UsiPosition`） |
| U4 | R4（時間の接口） |
