# 詰将棋局面自動生成機能

## 概要

USIエンジン（`go mate`）と内蔵探索（Hayanagi）を用いて、詰将棋の慣例に沿った局面を自動生成する機能。用途と手数に応じて次の2つの生成方式を備え、どちらもGUI（`TsumeshogiGeneratorDialog`）とCLI（`shogiboardq-cli`）から共通利用できる（目標手数は `TsumeshogiSettings::GeneratorLimits` で **1〜301の奇数** に統一）。

1. **ランダム配置モード（`TsumeshogiGenerator` / CLI `generate-tsume`）**: 短手数〜中手数向け。ランダムに候補局面を生成し、内蔵の有限深さ探索で事前選別してからUSIエンジンで詰み探索・主手順余詰検査・不要駒除去を行う。
2. **逆算・編集モード（`TsumeRetrogradeGenerator` / CLI `grow-tsume`）**: 長手数向け（15〜301手）。0手詰み局面または既存の種局面から逆算・盤面変異・標的型余詰リペア・多層ビームサーチで手数を伸ばし、目標手数に到達した候補を `TsumeStrictCertifier` で厳密証明（最短手数確定・主手順一意・駒余り禁止・全1枚除去の確定判定）する。チェックポイント保存・再開にも対応する。

どちらの方式でも主手順（玉方の最長抵抗）の各攻手が一意と確認できた局面だけを採択し、不要駒を除去した後も同じ検査を行う。主手順の余詰や判定不能の局面は出力しない。玉方が早く詰む変化での別の詰め方（変化別詰）は許容し、最終手の複数解はオプションで切替できる（ランダム配置の既定は許容、逆算・編集の既定は禁止）。

3〜31手詰を各1000題（計15,000題）作成した際の試行錯誤、Pythonからの並列実行、不要駒・類似作・駒余りの整理および長手数オーケストレータの運用手順は [詰将棋15,000題の生成・検証・再編の記録](tsumeshogi-collection-generation.md) を参照。

## アーキテクチャ

### クラス構成

| クラス | ファイル | 責務 |
|---|---|---|
| `TsumeshogiGeneratorDialog` | `src/dialogs/tsumeshogigeneratordialog.h/.cpp` | UI（生成の開始・停止、進捗表示、結果テーブル、保存/コピー） |
| `TsumeshogiGeneratorSettingsPanel` | `src/dialogs/tsumeshogigeneratorsettingspanel.h/.cpp` | ダイアログ上段の欄（エンジンの選択・生成方式ごとの欄の出し分け・設定の保存と復元・逆算用ファイルの欄）。欄の値を生成器の `Settings` にして返す |
| `TsumeshogiGenerator` | `src/analysis/tsumeshogigenerator.h/.cpp` | ランダム配置モードの制御（候補の待ち行列・問い合わせ中継・重複除外・上限・進捗） |
| `TsumeCandidateJob` | `src/analysis/tsumecandidatejob.{h,cpp}` | ランダム配置モードで候補1つを採択か棄却まで進める（手数確認→唯一解検査→駒除去→再検査）。Qt のイベントループを知らない引き出し型 |
| `TsumeRetrogradeGenerator` | `src/analysis/tsumeretrogradegenerator.{h,cpp}` | 逆算・編集モードの進行（エンジンへの問い合わせと、答えで何をするかの順番: 候補の評価 → 初手余詰チェック → 厳密証明。開始と終了・進捗・チェックポイントの保存の契機） |
| `TsumeRetrogradeBeam` | `src/analysis/tsumeretrogradebeam.{h,cpp}` | 探索の状態（手数層ごとのビーム・評価待ち・リペアキュー・証明待ち・既出と類似作の記録・統計・乱数）と、候補の供給（種局面・0手詰みからの逆算・ビームの展開・1枚除去）、チェックポイントへの書き出しと読み戻し。エンジンを使わない |
| `TsumeMateQueryLink` | `src/analysis/tsumematequerylink.{h,cpp}` | 詰将棋エンジンとの接続（`MateOracle` の起動・`ask()` / `askWithin()`・期限切れ後の自動再起動・応答しないエンジンの見分け・置換表のリセット） |
| `TsumeLocalScreen` | `src/analysis/tsumelocalscreen.{h,cpp}` | 内蔵 Hayanagi による事前ふるい分け（候補のプレスクリーニング・初手と主手順の明白な余詰の検出） |
| `TsumeFirstMoveCheck` | `src/analysis/tsumefirstmovecheck.{h,cpp}` | 成長中の候補の軽量初手余詰チェック（引き出し型: `nextQuery()` / `submit()` / `verdict()`） |
| `TsumeRetrogradeCheckpoint` | `src/analysis/tsumeretrogradecheckpoint.{h,cpp}` | チェックポイントファイルの形式（版・生成条件の記録と再開時の照合）と原子的な保存 |
| `TsumeRetrogradeOps` | `src/analysis/tsumeretrogradeops.{h,cpp}` | 逆算による候補局面の生成（詰み上がり局面生成・1手逆算・2手逆算・接尾辞切り出し・開始局面の正規化。純粋局面演算） |
| `TsumePositionEdits` | `src/analysis/tsumepositionedits.{h,cpp}` | いまある局面を変える候補の生成（複合盤面変異・標的型余詰リペア。純粋局面演算） |
| `TsumeLineMetrics` | `src/analysis/tsumelinemetrics.{h,cpp}` | 局面と詰み手順の評価（手順の特徴 `Metrics`・長手数ヒューリスティック評価・系統キー・類似作キー算出） |
| `TsumeEditBoard`・`TsumeEditRandom` | `src/analysis/tsumeeditboard.{h,cpp}`・`src/analysis/tsumeedittypes.h`・`src/analysis/tsumeeditrandom.h` | 候補局面を組み立てる盤（片玉の盤と攻方持駒）と盤面の規則・利きの計算（`buildSfen()` など）、候補の選び方に使う乱数の補助。盤は Hayanagi の型を使うので局面演算の実装からだけ include し、使う側に見せる値（駒数の上限 `Limits`・作った候補 `Candidate`）は `tsumeedittypes.h` に置く |
| `TsumeStrictCertifier` | `src/analysis/tsumestrictcertifier.{h,cpp}` | 逆算・編集モード用の厳密証明・不要駒トリミングジョブ（引き出し型 `MateQuerySource`、途中状態と確定キャッシュの JSON 保存/復元対応） |
| `TsumeVerificationJob` | `src/analysis/tsumeverificationjob.{h,cpp}` | `TsumeshogiVerifier` の問い合わせを総予算と1回の上限で区切って出す。ランダム配置・逆算証明・CLI `verify-tsume` で共通 |
| `TsumeshogiVerifier` | `src/analysis/tsumeshogiverifier.{h,cpp}` | 合法手・全応手の列挙、唯一性・駒余り（`requireNoSurplus`）の検査状態管理、確定キャッシュの注入/取り出し（`seedCache`/`exportCache`）、余詰詳細（`mainMove`/`cookMove`/`cookPly`）の記録 |
| `MateOracle`・`MateQueryLoop` | `src/engine/mateoracle.{h,cpp}`・`src/engine/matequeryloop.{h,cpp}` | エンジンへの `go mate` の問い合わせ（`ask()` / `askWithin()`、`UsiEngineSession` を持ち答えは `MateAnswer`）と、引き出し型問い合わせ元（`MateQuerySource`）の駆動ループ |
| `TsumeshogiCandidateScreener` | `src/analysis/tsumeshogicandidatescreener.{h,cpp}` | 内蔵探索（Hayanagi）によるランダム配置候補の事前選別とバッチ生成 |
| `TsumeshogiPositionGenerator` | `src/analysis/tsumeshogipositiongenerator.h/.cpp` | ランダム局面生成（SFEN文字列出力） |
| `TsumeshogiExportHeaderBuilder` | `src/analysis/tsumeshogiexportheaderbuilder.h/.cpp` | ファイル保存のコメントヘッダ生成（ランダム配置・逆算編集の両方に対応） |

```
TsumeshogiGeneratorDialog
  │
  ├─ [ランダム配置モード] TsumeshogiGenerator
  │    ├─ TsumeshogiCandidateScreener  ← 事前選別（ワーカースレッド。内部で TsumeshogiPositionGenerator を使用）
  │    │    └─ TsumeshogiPositionGenerator  ← ランダム局面生成
  │    ├─ TsumeCandidateJob            ← 候補1つの処理（引き出し型）
  │    │    └─ TsumeVerificationJob → TsumeshogiVerifier
  │    └─ MateQueryLoop → MateOracle   ← 問い合わせを1つずつエンジンへ
  │
  └─ [逆算・編集モード] TsumeRetrogradeGenerator
       ├─ TsumeRetrogradeBeam          ← 探索の状態（ビーム・待ち行列・既出の記録・統計）と候補の供給・途中保存
       │    ├─ TsumeRetrogradeOps      ← 0手詰み生成・1手/2手逆算・接尾辞切り出し
       │    ├─ TsumePositionEdits      ← 盤面変異・標的型余詰リペア
       │    └─ TsumeLineMetrics        ← 手順の評価・系統キー・類似キー
       │         （Ops・Edits は TsumeEditBoard の盤と規則で局面を組み立てる）
       ├─ TsumeLocalScreen             ← Hayanagi TsumeSearch による短時間枝刈り・早詰除外・明白な余詰の検出
       ├─ TsumeFirstMoveCheck          ← 軽量初手余詰チェック（引き出し型）
       ├─ TsumeStrictCertifier         ← 厳密証明・全1枚除去（引き出し型、チェックポイント保存/復元対応）
       │    └─ TsumeVerificationJob → TsumeshogiVerifier（最終手一意・駒余り禁止・キャッシュ共有）
       ├─ TsumeRetrogradeCheckpoint    ← チェックポイントファイルの形式と保存・読み込み
       └─ TsumeMateQueryLink → MateOracle ← 成長時は ask()、厳密証明時は askWithin()（自然終了のみ確定）＋タイムアウト自動再起動
```

## 処理フロー

1. ワーカースレッドでランダム局面を生成し、内蔵探索で「目標手数ちょうどで詰み、詰む初手が1つ」の候補だけを待ち行列に残す。生成器は候補を1つずつ `TsumeCandidateJob` にして `MateQueryLoop` で回す。
2. `CheckLength`: 候補を選択エンジンの `go mate` で調べる。目標手数のPVでなければ棄却する。
3. `Verify`: `TsumeVerificationJob`（`TsumeshogiVerifier`）で全変化の攻手を検査する。合格した局面だけを除去の元局面にする。
4. `Trim`・`VerifyTrimmed`: 駒の除去後に目標手数のPVが返っても、除いた局面の検査に合格するまで除去を確定しない。
5. 除去候補を試し終えたら `Accepted` になり、生成器が検証済みの最終SFEN/PV（`outcome()`）を出力して次の候補へ進む。

| Stage（`TsumeCandidateJob::Stage`） | 内容 |
|---|---|
| `CheckLength` | 候補局面の詰みの手数を確かめる |
| `Verify` | 候補局面の唯一解を検査する |
| `Trim` | 駒を1枚除いた局面の詰みを確かめる |
| `VerifyTrimmed` | 駒を1枚除いた局面の唯一解を検査する |
| `Accepted`・`Rejected` | 採択・棄却（`outcome()` は `Accepted` のときだけ値がある） |

`positionFound` は全検査に合格した局面だけを通知する。`progressUpdated` の探索済み数は、事前選別で落とした局面を含む生成局面数である。
`verificationProgress` は問い合わせ数、`verificationStatsUpdated` は検査で除外した局面数（事前選別で複数初手を検出した局面と駒除去の試行を含む）と、
そのうち判定不能の件数を通知する。

`start()` はエンジンの起動を頼むだけで（`MateOracle::start()`。値を返さない）、初期化を終えた `ready` で探索を始める。起動できないときは、`start()` から戻った後に `MateOracle::errorOccurred` で届き、生成を終える（`start()` の中に同期の失敗の分岐はない）。
問い合わせは `MateQueryLoop` が `MateOracle::answered` と間隔0の単発タイマーで進め、UIスレッドを探索待ちにしない。
`TsumeCandidateJob`・`TsumeVerificationJob` は引き出し型の問い合わせ元（`MateQuerySource`。`nextQuery()`・`submit(answer, elapsedMs)`・`isDone()`）で、Qt のイベントループもエンジンも知らない。
キャッシュだけで進められる場合も、64ステップごとにイベントループへ制御を戻す（`nextQuery()` が空を返し、ループが呼び直す）。

## ランダム局面生成 (TsumeshogiPositionGenerator)

### 生成パラメータ

| パラメータ | デフォルト | 説明 |
|---|---|---|
| `maxAttackPieces` | 4 | 攻め駒上限（盤上+持駒合計） |
| `maxDefendPieces` | 1 | 守り駒上限（玉以外） |
| `attackRange` | 3 | 玉中心の攻め駒配置範囲（マス） |
| `addRemainingToDefenderHand` | true | 未使用駒を受方持駒に入れる |

### 生成アルゴリズム

1. **後手の玉をランダム配置** — 9x9盤面の任意のマス
2. **攻め駒を配置** — 1〜maxAttackPieces枚
   - 70%の確率で盤上（玉周辺attackRangeマス以内）、30%の確率で持駒
   - 30%の確率で成駒として配置
   - 段制約に違反する場合は自動で成駒に変換（金は不可なので再試行）
   - 同じ筋に同じ側の不成歩がある場合は二歩になるため別のマスを再試行
3. **守り駒を配置** — 0〜maxDefendPieces枚（玉以外の後手駒、玉周辺2マス以内、二歩回避は攻め駒と同様）
4. **王手検証** — 初期局面で後手玉に王手がかかっていないことを `EngineMoveValidator::isInCheck` で検証
5. **リトライ** — 王手がかかっている場合は最大100回再生成

### 詰将棋の慣例

- 攻方（先手）の玉は配置しない
- 未使用駒は全て受方（後手）持駒に追加（40駒保存則）
- 持駒に成駒は置けない

### 盤面内部表現

局面は値の型 `Position`（`m_board`）で持つ。盤の駒は `Position::setPieceAt` で置き、攻方の持駒は `Position::addToHand` で足す。
未使用駒は SFEN を作る直前に守方の持駒に足し、SFEN は `Position::toSfen(Turn::Black, 1)`（攻方の手番・1手目）で書く。
王手の検証には局面（`m_board`）をそのまま `EngineMoveValidator::isInCheck()` に渡す。

## 事前選別 (TsumeshogiCandidateScreener)

ランダム局面のうち目標手数の詰みがあるものは3手詰で約1%しかなく、残りをすべて外部エンジンに送ると
1局面あたり数十msから、詰みの証明が難しい局面では5秒（時間切れ）かかる。
`TsumeshogiCandidateScreener::screen()` は Hayanagi の `TsumeSearch`（有限深さの詰み探索）で候補を先に選別する。

| 判定 | 条件 | 扱い |
|---|---|---|
| `Candidate` | 目標手数ちょうどの詰みがあり、目標手数＋上乗せ以内で詰む初手が1つ | エンジンに送る |
| `NotMateInTarget` | 目標手数以内に詰みがない（`Limit`/`NoMate`）、または目標より短く詰む | 捨てる |
| `MultipleFirstMoves` | 目標手数＋上乗せ以内で詰む初手が2つ以上 | 余詰として捨て、「検査で除外」に数える |
| `Unknown` | 時間切れ・中断・不正な局面・63手超の目標 | 取りこぼしを避けるためエンジンに送る |

有限深さ探索で見つかった詰みは実際の詰みなので、短い詰み・複数初手による除外は確定的である。
「目標手数以内に詰みがない」も深さ内で網羅的に調べた結果である。唯一解の最終判定は従来どおり外部エンジンと `TsumeshogiVerifier` が行う。

`generateBatch()` は `TsumeshogiPositionGenerator` で生成と選別を繰り返し、候補が `kBatchSize`（8）集まるか
`kBatchMaxGenerated`（2000）局面を生成した時点で返す。`TsumeshogiGenerator` はこれを `QtConcurrent::run` で実行し、
キューが2局面未満になったら次のバッチを先行して始める。

時間上限は `kScreenLimits`（存在探索 100ms、各初手の別詰探索 100ms、別詰の深さ上乗せ2手）。
駒除去候補の選別は `TsumeCandidateJob` が呼び出し元（UI）のスレッドで行うため `kTrimScreenLimits`（20ms、5ms、上乗せ2手）と短くし、
除去後に目標手数で詰まない・短く詰む・初手が複数ある除去はエンジンに送らず却下する。

2026-09-25 の計測（KomoringHeights 1.1.0、3手詰・攻め駒4・守り駒1・範囲3・5秒、`tsumeshogi_generation_harness --screen`）:

| 項目 | 事前選別なし | 事前選別あり（100ms・上乗せ2手） |
|---|---|---|
| 1局面あたりの選別時間 | — | 約1.5ms（ワーカースレッド） |
| エンジンに送る局面 | 全局面（平均 40〜80ms、0.3% が5秒の時間切れ） | 生成局面の約0.5%（平均 8ms、時間切れ 0） |
| 候補（3手PV）が余詰検査を通る割合 | 21% | 75〜78%（同手数〜5手の別初手を事前に除外） |
| 10局面の生成に要した時間 | 8分37秒（6505局面、アプリ実測） | 約20秒（3877〜5357局面、アプリ実測。ハーネスでは4000局面から10〜14局面を13〜20秒で採択） |

事前選別の時間切れは 4000 局面中 0 だった。目標手数が長い場合は時間切れが増えるが、
その局面は従来どおりエンジンに送られるため、生成が止まることはない。

## エンジン検証 (TsumeCandidateJob・TsumeVerificationJob)

### USIプロトコル

エンジンとの通信は USI の `go mate` コマンドを使用する。

```
→ position sfen <SFEN文字列>
→ go mate <timeout_ms>
← checkmate <PV>        … 詰みあり（PVは指し手列）
← checkmate nomate      … 詰みなし
← checkmate timeout     … 時間切れ
← checkmate notimplemented … エンジン非対応
```

### 一次フィルタと唯一性検査

返却PVの長さが目標N手と一致することは一次フィルタであり、採択条件のすべてではない。
候補発見を効率よく行うため、KomoringHeightsでは引き続き `PostSearchLevel=MinLength` を推奨する。
最終的な出力PVと手数は、検査した一意な攻手と全合法応手から再構成する。

`TsumeshogiVerifier` は次の手順で検査する。

1. 攻方の合法な王手をすべて列挙する。成・不成は別手として扱う。
2. 各王手後の玉方の全合法応手を列挙する。応手が0なら、その王手は詰み。
3. 各応手後の**攻方手番**の局面を外部エンジンの `go mate` に問い合わせる。
   玉方手番を根にした `go mate` の解釈に依存しない。
4. 応手後に1つでも `nomate` があれば、その王手は不詰。全応手で詰めば詰む攻手とし、返却PVの長さを応手後の詰み手数として記録する。
   `timeout`・不正なPV等が残れば判定不能。返却PVは合法性・連続王手・終端の詰みも検査する。
5. 詰む攻手が2つあれば余詰として棄却する（手数の長短は問わない）。詰む攻手が1つだけで、他の攻手の不詰を確認できた場合に限り、次へ進む。
6. 唯一の攻手に対する応手のうち、詰み手数が最長のもの（複数なら変化同手数としてすべて）を主手順とし、その局面で同じ検査を続ける。
   それより早く詰む応手（変化）は、攻方に別の詰め方があっても検査しない（変化別詰の許容）。
7. 主手順の最終手（応手後に1手で詰む局面）は、`allowFinalMoveAlternatives` が有効なら複数の詰手があっても採択する（最終手余詰の許容）。
   根の局面は目標1手でも複数解を許容しない。無効なら最終手も他の攻手と同じく一意性を要求する。
8. 主手順の検査が終わり、最長抵抗に対する手数が目標N手と一致したら合格する。出力PVは主手順の先頭の変化から再構成する。

別攻めの判定にN手やN+2手の深さ上限は設けない。エンジンから長い詰みPVが返れば、それも別解として数える。
不詰の根拠は `nomate` であり、有限深さで見つからなかったことではない。
エンジンが詰み・不詰を正しく回答し、`PostSearchLevel=MinLength` 相当の最短PVを返すことは前提となる。
最短でないPVが返る設定では応手後の手数が過大になり、変化を主手順として検査したり手数不一致で棄却したりするが、余詰を採択する方向には働かない。
単一の `checkmate` やMultiPV設定だけを唯一性保証として扱わない。

2026-09-25 の再現実験（KomoringHeights 1.1.0、3手詰・攻め駒4・守り駒1・範囲3・5秒）では、
ランダム3000局面のうち3手詰の候補は33局面で、全変化・最終手・成不成をすべて一意に要求する旧基準では2局面しか採択されなかった。
残りは無駄合を挟む長手数の別初手や、変化・最終手の複数解による棄却で、4分半の生成で0局面という報告の原因だった。
現在の基準（最終手許容）では同じ33局面のうち7局面を採択する。
この測定は `tests/tsumeshogi_generation_harness.cpp`（ctest 対象外の実行ファイル `build/tests/tsumeshogi_generation_harness`）で再現できる。

```bash
# ランダム3000局面を候補探索し、目標手数の候補を最終手許容・厳格の両方で検査して内訳と棄却理由を表示する
./build/tests/tsumeshogi_generation_harness --engine /path/to/KomoringHeights-by-gcc --count 3000 --explain
# 生成側と同じ事前選別を通してから測る（--screen-ms で時間上限、--alt-extra で別詰の深さ上乗せ）
./build/tests/tsumeshogi_generation_harness --engine /path/to/KomoringHeights-by-gcc --count 4000 --screen --screen-ms 100 --alt-extra 2
# 1局面だけ検査し、エンジンへの全問い合わせと応答を表示する
./build/tests/tsumeshogi_generation_harness --engine /path/to/KomoringHeights-by-gcc --sfen "<SFEN>" --target 3
```

既存の詰将棋対局で参考手順以外の詰手を受理する動作は変更しない。

`timeoutMs` は候補探索・各除去探索に加え、それぞれの余詰検査全体にも適用する。
検査中の1問い合わせは最大1秒（`TsumeVerificationJob::kQueryMaxMs`。総予算の残りが少なければその時間）とし、難しい1手に偏らないようにする。総予算は答えにかかった時間の合計で数える。
検査局面数・応答キャッシュの上限はランダム配置モードで各10,000（厳密証明 `TsumeStrictCertifier` では各200,000）。時間や資源の上限に達した場合は判定不能として採択しない。
目標手数は `TsumeshogiSettings::GeneratorLimits` で1～301の奇数に統一している（GUI・CLI `generate-tsume` / `grow-tsume` / `verify-tsume` で共通）。Hayanagiの有限深さ探索APIに依存しないため、63手への暗黙の切り詰めは行わない。

SFEN重複排除は唯一性検査と独立して行う。採択の結果（`TsumeCandidateJob::outcome()`）は、全ての1枚除去の判定を終えた局面と
その局面の検査が返した手順からしか作られない。生成器の `publish()` はその後に `m_foundSfens` で二重出力を防ぐ。

### タイマー

| タイマー | 用途 | 間隔 |
|---|---|---|
| `UsiSearchRequest::stopAfterMs`（`UsiEngineSession` の stop タイマー） | エンジン無応答ガード。期限を過ぎたら stop を送る | 各 `go mate` に指定した時間 + 5000ms（`kMateStopMarginMs`。シングルショット） |
| `UsiSearchRequest::replyTimeoutMs`（`UsiEngineSession` の応答待ち） | stop の後の応答待ち。過ぎたら `MateOracle::timedOut` を受けてエンジン固着として生成を中止する | 3000ms（`kStopReplyMs`。シングルショット） |
| `m_progressTimer` | UI進捗更新 | 500ms（リピート） |
| `MateQueryLoop` の `m_stepTimer` | 次の問い合わせを引き出す | 0ms（シングルショット） |

探索は `MateQueryLoop` が `MateOracle::ask(position, limitMs)` に頼み、`MateOracle` が `UsiSearchRequest::mate()`（打ち切りの方針を決める1か所）にまとめて `UsiEngineSession::search()` に渡す。
stop を送った後に届いた応答（`UsiCheckmate::stopped`）は `MateAnswer::fromCheckmate()` が Unknown にし、詰み・不詰の結果として扱わない。

エンジンの応答が遅れた場合、停止・応答待ちの時間だけ総予算を超えることがある。
予算超過後の結果を採択せず、既存の無応答ガードで安全に次へ進むか生成を終了する。

## 不要駒トリミング

### 目的

ランダム生成された局面には、取り除いても目標手数の詰みと主手順の一意性が保たれる不要駒が含まれることがある。盤上の玉以外の駒と攻方持駒を1枚ずつ除去して調べ、除去可能なら確定して全駒を調べ直す。最終局面の全1枚除去について判定が完了した場合だけ出力する。複数枚の同時除去や配置変更まで含めた大域的な最少駒数は保証しない。

### アルゴリズム

```
N手詰・主手順の唯一性検査に合格（SFEN, PV）
  │
  ▼
TsumeCandidateJob::startTrimming(sfen, pv)
  │
  ▼
除去候補リストを構築（enumerateRemovablePieces）
  │
  ▼
候補[0]を除去したSFENを生成（removePiece）
  │
  ▼
開始局面の王手・内蔵探索で事前選別（不詰・短い詰み・複数初手は却下して次の候補へ）
  │
  ▼
エンジンに送信（go mate）
  │
  ▼
エンジン応答:
  ├─ ちょうどN手詰 → 唯一性を再検査（VerifyTrimmed） → 合格した場合だけ除去確定
  │   → 除去後のSFENをベースに候補リストを再構築
  │   → 先頭から再開（新たに除去可能になった駒があるかもしれない）
  │
  ├─ 判定不能・時間切れ・証明が不十分な異なる長さのPV・内蔵探索と食い違う不詰 → 局面全体を不採択
  │
  └─ 確定した不詰・余詰 → 除去却下
      → 次の候補へ（インデックスを進める）
          │
          ├─ 次の候補あり → エンジンに送信
          └─ 全候補の確定判定が完了 → Accepted（outcome() に最終SFEN/PV）
              → 生成器が positionFound シグナルを emit
              → 次の候補へ
```

### 除去候補の列挙ルール

| 対象 | 除去可否 | 理由 |
|---|---|---|
| 盤上の攻方駒（玉以外） | 可 | 攻め駒が不要な可能性 |
| 盤上の守方駒（玉以外） | 可 | 守り駒が不要な可能性 |
| 攻方（先手）持駒 | 可 | 持駒が不要な可能性 |
| 守方（後手）持駒 | **不可** | 守方の防御資源を減らすと詰みやすくなり不正 |
| 玉（攻方・守方とも） | **不可** | 玉は除去不可 |

### 除去した駒の扱い

攻方玉は省略する。除去した駒は消滅させず、守方（後手）持駒に移して駒の総数を維持する:

- **盤上の駒を除去**: 生駒に戻して後手持駒に追加（成駒は持駒にできないため）
- **攻方持駒を除去**: そのまま後手持駒に移動

### 候補リスト再構築

駒を1つ除去するたびに候補リストを再構築し、先頭から再試行する。これは、ある駒の除去によって別の駒も不要になるケースに対応するためである。

例: 攻方の銀を除去 → 銀で合駒する変化が消滅 → 攻方の桂（銀合の変化でのみ必要だった）も不要になる

### 除去候補の事前検証

除去後の局面で開始時に守方玉へ王手がかかる場合（例: 飛車と玉の間にあった守り駒を除去）、詰将棋として不正なのでエンジンに送らず候補から外す。判定には `TsumeshogiPositionGenerator::isDefenderKingInCheck()` を使う。
続けて `TsumeshogiCandidateScreener::screen()` で目標手数の詰みと初手の一意性を内蔵探索で確かめ、通らない除去はエンジンに送らずに却下する。

### 中断時の扱い

トリミング中に `stop()`・エンジンエラー・エンジン固着で終了する場合、処理中の局面は出力しない。元局面の詰み・余詰を確認済みでも、その駒が不要でないことを確認したことにはならない。`Unknown` や遅延応答による判定不能も、除去を却下して元局面を採択する根拠にはしない。

採択の結果（`outcome()`）は `Accepted` のときだけ値があり、最終局面に対する全1枚除去の判定を終えてから、その局面の検査が返した手順で作る。中断した候補（`abort()`）は `Rejected` になり、結果を持たない。

### 保存済み問題集の再検査

13手詰の旧703番では4三桂・2九香を除いても同じ13手詰が成立した。このため、生成時のトリミングとは別に、保存した全局面の不要駒を再検査する。再検査ツールは `TsumeCandidateJob::onePieceRemovedPositions()` を使い、生成器と同じ除去操作を適用する。

```bash
cmake --build build --target tsumeshogi_collection_auditor --parallel 4
python3 scripts/audit_tsume_collections.py data/tsumeshogi/tsume_*ply_1000_20261001.txt \
  --engine /path/to/KomoringHeights-by-gcc \
  --output-dir /tmp/tsume-audit --workers 8 --timeout-ms 60000 --require-no-surplus
```

検証器はJSON Linesの要求を受け取り、目標手数・主手順の余詰・各1枚除去を調べる。除去したら再走査し、最後に新しい探索表による有限深さ探索と手順の合法性を確認する。`status=minimal` だけが採択可能で、`unknown` は成功に数えない。`checks` に最終局面の全除去候補と除去できない理由を残す。最終手の複数解は許容する。

Python側は監査記録 `audit.jsonl` を即時保存し、同じ検証器の実行ファイルなら再開できる。`tsume_Nply_minimal.txt` は除去後のSFENで重複を除いた確認済み結果で、元の問題集を直接上書きしない。重複や判定不能で不足した分は、新しく生成して同じ再検査に合格した問題で補充する。

## 駒を1枚除いた局面（トリミング）

トリミング処理では、SFEN を局面の値 `Position` に読み、駒を1枚除いて守方（後手）の持駒にしたあと、`Position::toSfen(Turn::Black, 1)` で SFEN を書き出す
（`TsumeCandidateJob::removePiece()`）。手番は先手、手数は1に固定する。

除去候補（`TrimCandidate`）の並び:
1. 盤上の駒（玉以外。攻方・守方とも）。上の段から、段の中は9筋から
2. 攻方（先手）の持駒。歩・香・桂・銀・金・角・飛の順

守方の持駒は除去対象外（守方の防御資源を減らすのは不正）。除いた駒は成駒なら生駒に戻して守方の持駒にする。
持駒の出力順は SFEN の慣例どおり、先手の駒を書いてから後手の駒を書き、どちらも飛・角・金・銀・桂・香・歩の順。

## 逆算・編集による長手数生成（TsumeRetrogradeGenerator）

15手〜最大301手の長手数詰将棋を現実的な時間で生成・厳密証明するための仕組み。GUIの「詰み局面から逆算・編集（長手数）」モードとCLIの `shogiboardq-cli grow-tsume` が共通して `TsumeRetrogradeGenerator` を使用する。

### 処理フェーズと優先順位

`TsumeRetrogradeGenerator::driveNext()` は、目標件数に到達するまで次の優先順位で1ステップずつ進める。

1. **進行中の厳密証明ジョブ（`m_activeCertifier`、`phase = "certifying"`）**: 最も優先して完了させる。
2. **厳密証明待ちキュー（`TsumeRetrogradeBeam::takePendingCertify()`）**: 未採択の候補を1件取り出し、確定済み厳密キャッシュ（`m_strictCache`）を注入して `TsumeStrictCertifier` を開始する（`beginCertification()`）。
3. **進行中の軽量初手余詰チェック（`m_firstCheck`、`phase = "checking_first"`）**: 目標手数未満の有望候補の初手余詰を検査する。
4. **バッチ展開（`TsumeRetrogradeBeam::expandNextBatch()`）**: 評価待ちが空なら、リペアキューまたはフロンティアから次の候補バッチを生成する。
5. **候補評価（`evaluateQueuedCandidates()`、`phase = "growing"` / `"repairing"`）**: 成長キャッシュ（`m_fastCache`）と内蔵プレスクリーニングを優先消費し、必要な候補だけを外部エンジンへ送る。

ビーム・評価待ち・リペアキュー・証明待ち・既出の記録・統計・乱数は `TsumeRetrogradeBeam` が持ち、生成器は進行中の処理（`m_activeCertifier`・`m_firstCheck`・評価中の候補 `m_activeEval`）と2つのキャッシュだけを持つ。

初期種局面（`seedSfens`。`TsumeRetrogradeBeam::addSeedLine()`）は `"SFEN moves <PV>"` の形式なら `TsumeLineMetrics::analyzeLine()` で検証して該当手数層のフロンティアへ直接投入し、0手詰み局面なら `reverseFromMateEnding()` で1手詰候補を展開、それ以外のSFENは評価キューへ入れる。種局面が空の場合は `TsumeRetrogradeOps::generateMateEndings()` で攻方持駒ゼロ・盤端/隅/中段寄りの多様な0手詰み上がり局面を自動生成する。

### 候補生成演算（TsumeRetrogradeOps・TsumePositionEdits）

純粋局面演算を、手を戻して作る逆算（[tsumeretrogradeops.cpp](../../src/analysis/tsumeretrogradeops.cpp)）と、いまある局面を変える盤面変異・余詰リペア（[tsumepositionedits.cpp](../../src/analysis/tsumepositionedits.cpp)）に分離している。どちらも盤の値と規則は [tsumeeditboard.cpp](../../src/analysis/tsumeeditboard.cpp)（`TsumeEditBoard`）を使い、全候補は `TsumeEditBoard::buildSfen()` で片玉・40駒保存則・二歩なし・行き所のない駒なし・開始時王手なしを満たす正規化SFENへ変換する。

#### 1. 2手逆算（`generatePredecessors()`）

攻方手番の局面から「玉方応手1手 → 攻方王手1手」を2段階に巻き戻し、2手前の攻方手番候補を作る。まず玉方応手を次の5類型で列挙し、各応手から均等枠（`perDefenseCap`）で攻方王手（駒打ち・盤上移動・成り戻し・駒取り戻し・開き王手・両王手）を逆算（`unwindAttackCheck()`）する。

| タグ | 巻き戻す玉方応手 | 狙い |
|---|---|---|
| `retro_escape` | 玉の隣接1マス単純移動 | 玉の移動距離・追い詰め構造を伸ばす |
| `retro_kingsac` | 玉による攻方駒取り（元の玉位置へ王手する駒を復活） | 焦点への捨駒を含む手筋を生む |
| `retro_dropint` | 玉方の盤上生駒を合駒打ちとして除去 | 遠方王手への打合を逆算する |
| `retro_moveint` | 玉方盤上駒を別マスへ戻し、王手経路を開く | 移動合を逆算する |
| `retro_recap` | 玉方盤上駒を戻し、跡地へ王手する攻方駒を復活 | 玉方駒による捨駒の取り返しを逆算する |

逆算したすべての候補は `verifyForwardRoundtrip()` で「攻方王手 → 玉方応手」を実際に順方向実行し、元の局面と完全一致する合法手であることを保証する。

#### 2. 複合盤面変異（`TsumePositionEdits::generateMutations()`）

1候補あたり **60%で1回、40%で2〜3回の連続編集（`mutate_multi`）** を適用し、単一編集では届かない別系統の局面へ跳ぶ。各編集ステップは次の確率分布で操作を選ぶ（移動・追加・変更時に行き所のない歩/香/桂は自動で成駒へ補正）。

| 操作 | 試行確率 | 内容 |
|---|---:|---|
| 盤上駒の移動（`mutate_move`） | 38% | **玉を含む**盤上駒を縦横±2マスの空きマスへ移動 |
| 盤上への駒追加（`mutate_add_atk` / `mutate_add_def`） | 22% | 攻方55%／玉方45%。攻方の飛・角・香は45%で盤全体、それ以外は玉周辺±3マスへ配置 |
| 駒種・成不成の変更（`mutate_kind`） | 18% | 玉以外の盤上駒を別の駒種・成不成へ変更 |
| 攻方持駒の増減（`mutate_hand_dec` / `mutate_hand_inc`） | 12% | 攻方持駒を1枚増減（所持していれば55%で減少） |
| 盤上駒の削除（`mutate_remove`） | 10% | 玉以外の盤上駒を1枚除去して玉方持駒へ戻す |

#### 3. 標的型余詰リペア（`TsumePositionEdits::generateCookRepairs()`）

初手余詰（`cookMove`）が検出された有望局面に対し、主手順の初手（`mainMove`）が依然として合法王手のまま保たれる候補だけを抽出する。次の6戦略を順に試行する。

1. **打駒マス閉塞・打駒削減（`repair_occupy_drop` / `repair_trim_hand`）**: `cookMove` が駒打ちかつ `cookTo != mainTo` なら、`cookTo` へ玉方駒（歩・桂・銀・金・香）を置いて塞ぐ。主手順で同じ駒を打たない場合は攻方持駒を1枚減らす。
2. **余詰マスへの守り駒追加（`repair_guard_cook`）**: `cookTo` に利く玉方駒を空きマスへ1枚追加する（`mainTo` には利かない配置を `stable_partition` で優先）。
3. **遠距離移動の経路遮断（`repair_block_line`）**: 移動余詰の途中の空きマスへ玉方駒（歩・銀・金）を置いて利きを遮る。
4. **余詰駒の除去・不成化（`repair_remove_cook_piece` / `repair_demote_cook_piece`）**: 動いた攻方駒が主手順と別駒（`cookFrom != mainFrom`）なら、その駒の除去または不成への弱体化を試す。
5. **逃げ道の開放（`repair_open_escape`）**: 玉の周囲8マスにある玉方壁駒（`mainTo` 以外）を1枚除去して余詰時の逃れ筋を作る。
6. **汎用1枚除去（`repair_trim_one`）**: `TsumeCandidateJob::onePieceRemovedPositions()` で不要駒起因の余詰を落とす。

#### 4. 接尾辞切り出し（`sliceToTargetLength()`）

目標手数を超える詰みPV（`pv.size() > targetMoves`）が得られた場合、先頭から `pv.size() - targetMoves` 手を進めてちょうど目標手数の部分局面（`SuffixSlice`）を切り出し、正規化・合法性確認のうえ目標手数候補として扱う。

### 多層ビームサーチ・評価関数・展開戦略

#### 評価関数（`TsumeLineMetrics::scoreNode()`）

長手数化と厳密条件（駒余りゼロ・最終手一意）を両立する系統を優先するため、次の各項でビームノードを採点する。

| 項目 | 計算内容 | 設計意図 |
|---|---|---|
| 手数進捗 | `min(plies, targetMoves) * 14.0` | 目標手数に近い層を最優先 |
| 駒余り | `surplusCount == 0` で `+35.0`（非厳密時 `+12.0`）、超過時は `-22.0 * 枚数`（非厳密時 `-6.0 * 枚数`） | 駒余りのない系統を強く優遇（さらに `requireNoSurplus` 時は3枚以上余る成長候補を棄却） |
| 最終手一意 | `strictFinalMove` 時、`finalImmediateMates == 1` で `+25.0`、2以上で `-55.0` | 最終手複数解の系統を育てても厳密証明で棄却されるため早期に抑止 |
| 初手一意 | `firstMoveUnique` で `+20.0` | 初手余詰のない健全な足場を優遇 |
| 長手数構造特徴 | 玉訪問マス数 `min(n,25)*2.2` ＋ 玉移動距離 `min(d,60)*0.6` ＋ 捨駒 `min(s,20)*2.5` ＋ 合駒 `min(i,20)*2.0` | 玉の広がり・捨駒・合駒を含む伸びやすい構造を優遇 |
| 盤上過密ペナルティ | `actualTotal > clamp(4 + plies/3, 5, 24)` の超過分 × `-1.4` | 手数に対して駒が多すぎる余詰爆発・探索低速化局面を抑制 |
| 展開回数ペナルティ | `-12.0 * log1p(expansionCount)` | 行き詰まった枝への固執を防ぎ自動バックトラックを促す |

#### フロンティア多様性と展開制御（`TsumeRetrogradeBeam::addToFrontier()` / `expandNextBatch()`）

- **系統多様性の上限**: 各手数層は最大 `beamWidth` 件を保持し、`TsumeLineMetrics::structuralSignature()`（玉の2×2ブロック領域・手数・先頭最大4手）が一致するノードは1層あたり最大2件までに制限する。
- **リペアキュー優先枠**: リペアキュー（`TsumeRetrogradeBeam` の `m_repairQueue`。最大256件、超過時は最短手数から破棄）が空でなければ約15%の確率で `plies * 10 - attempts * 15` が最大の候補を取り出し、最大2回まで標的型リペアを展開する（1回の分岐上限 `min(branchFactor, 16)`）。
- **親ノードの分散選択**: 約75%は上位3層から `score - 45.0 * expansions + uniform(0, 28)` で選択し、複数ワーカーが同じ上位種に集中するのを乱数ジッターと展開ペナルティで防ぐ。残り約25%は全層から6件のランダム・トーナメント選択（`score - 40.0 * expansions`）を行い、袋小路から自動バックトラックする。選択後は対象ノードの `expansions` を増やして自身のスコアも再計算する。
- **0手詰み局面の再注入抑止**: 停滞時（`stats().batchesSinceProgress >= 25` または `roll >= 92`）の0手詰み再注入はシード未指定のボトムアップ探索時だけに限定し、親種ファイルを渡した長手数生成で0手からやり直す無駄を防ぐ。
- **同手数シードの100%変異切替**: 選んだ親ノードが既に目標手数以上（`chosenLayer >= targetMoves`）のときは、2手逆算して接尾辞を切り出しても元手順へ戻りやすいため **`retroBudget = 0` として 100% 盤面変異に集中**する。目標未満の層では `branchFactor` の `2/3` を逆算、`1/3` を盤面変異に配分する。

### 高速化のための多段枝刈りと探索制御

外部エンジン（KomoringHeights）との IPC・長手数探索は1回あたりのコストが大きいため、次の4層で無駄な問い合わせと候補爆発を抑えている。

#### 1. 内蔵 Hayanagi プレスクリーニングの2段階戦略（`TsumeLocalScreen::screenCandidate()`）

`driveNext()` の評価ループでは、UIイベントループを塞がないよう **最大48件または40msごとに制御を戻しながら** キャッシュ照会と内蔵 `shogi::TsumeSearch` による事前選別を実行する。

| 目標手数帯 | 内蔵探索の深さと予算 | 判定ごとの扱い |
|---|---|---|
| 19手以下 | 目標手数まで探索。予算は 〜9手: `5ms`、〜15手: `5ms`、17手: `9ms`、19手: `15ms`（`stepTimeoutMs` 以下にクランプ） | `NoMate` は即棄却。`Mate` は高層由来の早詰（`plies < targetMoves - 2`）を弾いたうえで、埋まった置換表からほぼ0msで PV を復元し外部問い合わせなしで直接採用。`Limit` / `Timeout` は高層フロンティア保有時の15〜17手では泥沼回避のため即棄却し、それ以外は確率転送（`Limit`: 4% / 15%、`Timeout`: 10% / 20%）。 |
| 21手以上 | 浅い深さ `min(15, max(3, targetMoves - 4))` を `4ms` で探索 | `NoMate` と高層由来の早詰 `Mate` を即棄却（ボトムアップ成長足場の `Mate` は PV 復元して採用）。15手すら4msで読み切れない過密な `Timeout` 局面は35%に間引き、早詰を否定できた `Limit` 候補だけを「真の長手数候補」として外部エンジンへ送る。 |

#### 2. ローカル余詰枝刈り・軽量初手チェック・即詰問い合わせ省略

- **主手順ローカル事前余詰検査（`TsumeLocalScreen::quickMainLineCookCheck()`）**: 61手以下の目標手数候補は、重い `TsumeStrictCertifier` へ送る前に主手順の各攻方手番（1, 3, ..., N−2手目）を内蔵 Hayanagi（各別王手 `2ms`）で検査し、明白な余詰を数ミリ秒で弾く。初手余詰ならリペアキューへ戻す。
- **15手以下のローカル初手検査完結（`TsumeFirstMoveCheck::begin()`）**: 目標手数未満の成長ノードの初手余詰チェックでは、61手以下にまずローカル `TsumeLocalScreen::quickCookCheck()`（15手以下 `6ms`、それ以外 `4ms`）を掛ける。**15手以下の成長足場はローカル検査のみで完結させて外部エンジン問い合わせを温存**する。
- **17手以上の応手反駁優先度と即詰省略（`TsumeFirstMoveCheck::nextQuery()`）**: 外部問い合わせが必要な別攻手の応手は、反駁しやすい順（`defensePriority`: 非玉の駒取り `100` → 玉の駒取り `150` → 玉移動 `200` → その他盤上移動 `300` → 合駒打ち `400+`）に並べて調べ、1つでも `NoMate` が出た時点でその別攻手を不詰確定として打ち切る。さらに `TsumeLocalScreen::hasImmediateMate()` で1手詰になる応手は反駁にならないため外部問い合わせから省く（問い合わせ上限 `clamp(24 + plies/2, 24, 80)` 回、1回予算 `clamp(stepTimeoutMs/2, 200, stepTimeoutMs)`）。初手余詰が見つかった候補は目標付近（`targetMoves - 2` 以上）または最大到達層ならリペアキューへ登録し、フロンティアにはペナルティ付き（`-18.0`）の足場として残す。
- **検証器の1手詰問い合わせ省略**: `TsumeshogiVerifier::Options::skipImmediateMateQueries`（既定 `true`）により、厳密証明や通常の余詰検査でも応手後に1手詰が存在する局面は `Impl::hasImmediateMate()` で即座に1手と確定し、外部エンジンへの問い合わせを省略する。

#### 3. 1段目限定の1枚除去リトライ（`allowTrimOnReject` / `fromRepair`）

目標手数に到達した候補が次の理由でそのまま採択できない場合、余分な1枚を落とすだけで成立する例が多いため、`TsumeCandidateJob::onePieceRemovedPositions()` の先頭候補を評価待ちの先頭へ差し込んで優先評価する（`TsumeRetrogradeBeam::enqueueTrims()`）。

- 目標手数到達時の駒余り・最終手複数解（`preflight_trim`、最大4件）
- ローカル主手順余詰検出時（`precook_trim`、最大2件）
- 厳密証明での余詰・駒余り棄却時（`cert_trim_repair`、最大2件。初手余詰ならリペアキューにも登録）

これらが連鎖して候補が指数増殖しないよう、投入時に `fromRepair = true` および証明キューの `allowTrimOnReject = false` を設定し、**1段目限り**で打ち切る。

#### 4. 類似作のステッピングストーン活用と既消化親種の即時パージ

- **類似作のステッピングストーン活用（`submitTargetCandidate()`）**: 目標手数候補が採択済み・除外済みと類似（`TsumeRetrogradeBeam::isSimilarToAccepted()`、内部で `TsumeLineMetrics::diversityKeys()` を照合）している場合、厳密証明には回さない。ただし**駒余りなし・最終手一意**を満たす健全な局面であれば、スコア `-15.0` を付けて目標手数層のフロンティアへ残す。これにより同手数からの100%盤面変異で別系統へ跳ぶための探索足場（ステッピングストーン）として再利用できる。
- **既消化親種のフロンティア即時パージ（`finishCertifier()` / `addExcludeLine()`）**: `excludeSimilar` が有効なとき、9手以上の問題が採択された時点または外部オーケストレータから `exclude <SFEN> moves <PV>` を受信した時点で、その手順を2手進めた直前親種（`N-2` 手局面）を `sliceToTargetLength()` で求め、フロンティアの `N-2` 層から即時除去する（`TsumeRetrogradeBeam::removeParentOf()`）。同じ親種を再び2手逆算しても同一 `core` で類似除外されるため、無駄な再展開を防いで未消化の別系統親種へ探索資源を移す。

### 厳密証明のエンジン制御・キャッシュ分離・置換表クリーン化・自動復旧

- `TsumeMateQueryLink::start()` は `TsumeMateEngine::requiredOptions()`（`PostSearchLevel=MinLength`、`GenerateAllLegalMoves=true`、`RootIsAndNodeIfChecked=false`、`NodesLimit=0`、`MultiPV=1`）をセッション設定として自動適用し、指定があれば `USI_Hash`（`engineHashMb`）と `Threads`（`engineThreads`）も上書きする。
- 厳密証明（`strictMinLength = true`）の問い合わせは `MateOracle::askWithin()`（`go mate infinite` を送り自然終了のみ確定、期限超過時は stop を送らず打ち切り）を使用する。
- **キャッシュ分離と置換表クリーン化（`TsumeMateQueryLink::resetHashIfDirty()`）**: 成長フェーズのキャッシュ（`m_fastCache`、最大100,000件、超過時は前半半分を破棄）と厳密証明用の確定キャッシュ（`m_strictCache`、最大200,000件）は明確に分離する。さらに、成長フェーズで時間制限付きの `ask()` を実行すると窓口が置換表の汚れを覚え（`m_hashDirty`）、次に `TsumeStrictCertifier` を開始する直前に `sendUsiNewGame()` を送って時間切れ由来のエンジン置換表残骸をリセットしてから厳密証明に入る。
- 期限切れやエンジン無応答が起きた場合は `TsumeMateQueryLink::restartForRecovery()` でエンジンプロセスを自動再起動して探索を継続する。
- **応答しないエンジンの見分け**: 期限切れは、厳密証明の `go mate infinite` が期限までに終わらなかっただけのこともあり（1つの候補の検査の中でも続けて起こる）、回数だけではエンジンの異常と言えない。答えが1つも返らないまま期限切れが5回続いたら（`kProbeAfterTimeouts`）、再起動の後に1手詰の局面（`kProbeSfen`）を制限時間つきで問い合わせて生きているかを確かめる。答えが返れば（中身は問わない）数え直して続け、stop にも答えなければ（約10秒）`unresponsive` を知らせ、生成器が「エンジンが連続して応答しないため生成を中止しました。」で終了する。確かめている間は `ready` を知らせないので、生成器は再起動の途中と同じく待つ。2026-11-28 までは再起動のたびに連続回数を 0 に戻していたので、詰みの問い合わせに答えないエンジンでも再起動を繰り返し続けた。

### チェックポイント保存と再開

- `checkpointPath` を指定すると、`checkpointIntervalSec`（既定30秒）ごと・最大到達手数の更新時・厳密証明の開始/採択時・停止時に `QSaveFile` で原子的にチェックポイント JSON（`checkpoint_version = 1`）を保存する。
- 保存内容には設定、乱数シードと消費回数（`rng.seed` / `rng.draws`）、累計経過時間と統計、各手数層のフロンティア、リペアキュー、評価キュー（最大256件、中断中の評価・初手検査対象を先頭へ戻す）・証明待ちキュー、進行中の `TsumeStrictCertifier` 状態（`toJson()`）、確定済み厳密キャッシュ（`m_strictCache`）、既採択局面が含まれる。
- `resumeFromCheckpoint = true` で開始すると、これらを復元して中断箇所から再開する。

### CLI `grow-tsume` と動的制御

`shogiboardq-cli grow-tsume` は `TsumeRetrogradeGenerator` を直接駆動し、標準出力へ JSON Lines で `started`、`progress`、`position`、`checkpoint`、`finished`、`error` イベントを出力する（`--output-path` 指定時は採択ごとに `<SFEN> moves <PV>` をファイルへ追記）。

`--stdin-control` を付けると、標準入力から1行ずつ次の制御コマンドを受け付ける（`CliStopWatcher::lineReceived`）。

| コマンド | 動作 |
|---|---|
| `stop`（または EOF） | 現在の状態をチェックポイントへ保存して安全に停止する |
| `exclude <SFEN [moves PV...]>` | 実行中に既採択・除外局面と類似キーを追加し、9手以上なら直前親種（`N-2` 手局面）をフロンティアから即時除去する（`addExcludeLine()`） |
| `seed <SFEN [moves PV...]>` | 実行中に新しい種局面（PV付きならフロンティアへ直接投入）を追加する（`addSeedLine()`） |

並列オーケストレータ（`scripts/generate_long_tsume_collections.py`）はこの仕組みを用いて、常駐ワーカー間で採択結果・類似キー・同手数シードをリアルタイムに同期する。

## ダイアログ (TsumeshogiGeneratorDialog)

### UI構成

エンジン設定と生成設定を上部にまとめ、生成方式（`ランダム配置` / `詰み局面から逆算・編集（長手数）`）をコンボボックスで切り替える。目標手数・駒数上限などの左列と、生成上限・探索時間・配置範囲（または逆算用のビーム幅・分岐数・検証予算）の右列を2列で配置し、「既定値に戻す」は生成設定内に置く。逆算モード時は専用のオプション行（駒余り禁止・類似作除外）とファイル設定欄（種局面ファイル・除外ファイル・チェックポイントファイル・途中保存から再開）を表示する。エンジンの最短手順設定と採択条件の説明は展開できる。文字拡大時や小さい画面では設定領域をスクロールし、結果一覧と開始・停止ボタンを操作できる状態に保つ。

開始・停止と状態表示の下に、採択件数／生成上限の進捗バー、探索件数（逆算モードでは候補生成数・最大到達手数・採択数）、経過時間、検査除外件数（逆算モードでは判定不能・フロンティア数・修復成功数も併記）を表示する。無制限の場合は生成中のみ不定進捗表示とする。

結果一覧は `# / SFEN / 盤面 / 手数` の4列で、メイン画面と共通の `TableStyles` / `TableButtonDelegate` を使用する。出力オプション・コピー・保存は一覧の下、文字サイズと閉じるボタンは最下部に置く。生成の制御・進捗・結果操作は `tsumeshogigeneratordialog.cpp`、上段の設定欄（欄の組み立て・生成方式ごとの出し分け・設定の保存と復元・逆算用ファイルの欄・生成器の `Settings` への変換）は `tsumeshogigeneratorsettingspanel.cpp`（`TsumeshogiGeneratorSettingsPanel`）に分けている。設定欄は高さが変わると `contentHeightChanged` を出し、ダイアログが `fitSettingsArea()` で広げる。

### 設定の永続化

`TsumeshogiSettings`（`src/services/tsumeshogisettings.h` の宣言の表）を通じて以下の設定が保存・復元される:
- ダイアログサイズ（既定: 840×840）
- フォントサイズ（既定: 10pt）
- 選択エンジン（エンジンを選ぶ部品 `EngineSelector` が、`EngineSelectionSettings::saveSelection()` で名前を `TsumeshogiGenerator/engineName` に保存する。以前の版が保存した番号は、名前で見つからないときだけ読む）
- 生成方式（`TsumeshogiGenerator/mode`、0: ランダム配置 / 1: 逆算・編集、既定: 0）
- 共通・ランダム配置パラメータ（目標手数、攻め駒上限、守り駒上限、配置範囲、探索時間、生成上限）
- 逆算・編集モード用パラメータ（`retroMaxAttackPieces` 既定18、`retroMaxDefendPieces` 既定12、`beamWidth` 既定16、`branchFactor` 既定32、`verifyBudgetSec` 既定300、`requireNoSurplus` 既定true、`excludeSimilar` 既定true、`seedFile`、`excludeFile`、`checkpointFile`、`resumeCheckpoint` 既定false）
- 最終手の複数解を許容するか（`TsumeshogiGenerator/allowFinalMoveAlternatives`、既定: 有効）
- 手順出力オプション（ファイル保存・コピー時に `moves ...` を付加するか）
- エンジン設定・採択条件の説明の展開状態

### 状態表示と手順出力

- ランダム配置モードの状態ラベルは `searchPhaseStarted`（探索中）、`trimmingProgress`（トリミング中 候補 i/n）、`verificationProgress`（余詰検査中 問い合わせ回数）で更新する。逆算・編集モードは `formatRetroPhase()` により `growing`（逆算・成長探索中）、`checking_first`（初手余詰検査中）、`repairing`（余詰修復中）、`certifying`（厳密証明中／厳密証明・不要駒検査中）を表示し、チェックポイント保存時は最新の保存パスを状態ラベルのツールチップに表示する。終了時は生成完了・ユーザーによる停止・エラーを区別して表示する。
- 逆算モード開始時、GUI の「探索時間/手」は `stepTimeoutMs = timeoutSec * 1000`、`verifyQueryTimeoutMs = qBound(5000, timeoutSec * 3000, 60000)` として `TsumeRetrogradeGenerator::Settings` に渡す（`TsumeshogiGeneratorSettingsPanel::retrogradeSettings()`。種局面・除外のファイルは `TsumeRetrogradeGenerator::readInputLines()` で読む。CLI の `grow-tsume` も同じ関数を使う）。「途中保存から再開」で開始したときは、チェックポイントに保存済みの採択局面（`TsumeRetrogradeGenerator::foundPositions()`）を結果一覧と進捗バーへ即座に復元したうえで残りの生成を続行する。除外ファイルが指定されている場合は通常開始・再開のどちらでも読み込み、既出局面・類似手順・既消化親種を除外する。
- 未登録時の開始、生成中の設定変更・既定値への復帰、結果がない状態での保存・コピー、未選択時の選択コピーは無効にする。再開始時には前回の結果と進捗をリセットする。
- 盤面列のクリック、行のダブルクリック、Enterキーで盤面と手順を開く。選択コピーはCtrl+Cにも対応し、複数選択した結果を表示順に出力する。結果追加時の自動スクロールは最下部を閲覧している場合だけ行う。
- 「手順も出力」を有効にすると、ファイル保存・コピーの各行が `<SFEN> moves <USI手順>` になる。無効時は SFEN のみで、SFEN集ダイアログ等の1行1SFEN形式と互換。
- 「ファイル保存」は先頭に `# ` 始まりのコメント行で ShogiBoardQ のバージョン（`APP_VERSION`）、生成開始日時、生成設定を記録する（`TsumeshogiExportHeaderBuilder::build` はランダム配置用と逆算・編集用の両オーバーロードを持つ）。設定と日時は「開始」時に `m_lastRunSettings` / `m_lastRunRetroSettings` と `m_lastRunStartedAt` に控えたものを使うので、生成後に設定欄を変えても保存内容は変わらない。コピーにはヘッダを付けない。詰将棋問題一覧（`TsumeCollection::parse`）と局面集ビューア（`SfenCollectionDialog`）は同じ1行の規則 `PositionCollectionFile::parse`（`src/kifu/positioncollectionfile.h`）で読み、`#` 始まりの行を読み飛ばす。SFEN に添えた `moves <USI手順>` はどちらも参考手順として扱う（position コマンドの指し手だけを指した後の局面にする）。
- ファイル保存は `TsumeCollectionWriter::save()` が `QSaveFile` で書き込み完了を確認してから置き換える。閉じる・Escでも生成を停止して設定を保存する。
- ジェネレータ（`TsumeshogiGenerator` と `TsumeRetrogradeGenerator`）はダイアログの子として1つずつ生成し、開始のたびに選択方式に応じて再利用する。エンジンとの接続（`MateOracle`。逆算・編集モードでは `TsumeMateQueryLink` が持つ）は開始ごとに生成・終了時に `deleteLater` で破棄する。

## 設定パラメータ一覧

### TsumeshogiSettings::GeneratorLimits（GUI・CLI共通の範囲定義）

| 定数 | 値 | 備考 |
|---|---|---|
| `kMinTargetMoves` / `kMaxTargetMoves` | 1 / 301 | 奇数のみ有効（`isTargetMoves` / `oddTargetMoves`） |
| `kMinAttackPieces` / `kMaxAttackPieces` | 1 / 10 | ランダム配置モードの攻め駒上限 |
| `kMaxDefendPieces` | 10（下限 0） | ランダム配置モードの守り駒上限 |
| `kMinAttackRange` / `kMaxAttackRange` | 1 / 8 | ランダム配置モードの配置範囲 |
| `kMinTimeoutSec` / `kMaxTimeoutSec` | 1 / 300 | 探索時間（秒） |
| `kMaxPositions` | 10000 | 下限 0 は無制限（GUI のみ。CLI は 1 から） |
| `kMaxRetroAttackPieces` / `kMaxRetroDefendPieces` | 38 / 38 | 逆算・編集モードの攻め駒・守り駒上限 |
| `kMinBeamWidth` / `kMaxBeamWidth` | 1 / 2000 | 逆算・編集モードのビーム幅 |
| `kMinBranchFactor` / `kMaxBranchFactor` | 4 / 200 | 逆算・編集モードの1展開あたり分岐数 |
| `kMinVerifyBudgetSec` / `kMaxVerifyBudgetSec` | 5 / 86400 | 逆算・編集モードの1候補あたり厳密検証総予算（秒） |

### TsumeshogiGenerator::Settings

| フィールド | 型 | デフォルト | 説明 |
|---|---|---|---|
| `enginePath` | QString | — | エンジン実行ファイルパス |
| `engineName` | QString | — | エンジン名 |
| `targetMoves` | int | 3 | 目標手数（奇数: 1..301） |
| `timeoutMs` | int | 5000 | 各詰み探索・各余詰検査全体の時間(ms) |
| `maxPositionsToFind` | int | 10 | 見つける局面数の上限（0=無制限） |
| `allowFinalMoveAlternatives` | bool | true | 主手順の最終手の複数解を許容する（`TsumeshogiVerifier::Options` に渡す） |
| `posGenSettings` | Settings | — | 局面生成パラメータ（下記） |

### TsumeshogiPositionGenerator::Settings

| フィールド | 型 | デフォルト | 説明 |
|---|---|---|---|
| `maxAttackPieces` | int | 4 | 攻め駒上限（盤上+持駒合計） |
| `maxDefendPieces` | int | 1 | 守り駒上限（玉以外） |
| `attackRange` | int | 3 | 玉中心の攻め駒配置範囲 |
| `addRemainingToDefenderHand` | bool | true | 未使用駒を受方持駒に入れる |

### TsumeRetrogradeGenerator::Settings

| フィールド | 型 | デフォルト（CLI / 構造体） | 説明 |
|---|---|---|---|
| `enginePath` / `engineName` | QString | — | エンジン実行ファイルパス・登録名 |
| `targetMoves` | int | 101 | 目標手数（奇数: 1..301） |
| `maxPositionsToFind` | int | 1 | 発見上限（0=無制限、CLI は 1..1000） |
| `beamWidth` | int | 12（GUI既定 16） | 各手数層に保持する最大ノード数 |
| `branchFactor` | int | 24（GUI既定 32） | 1展開あたりに生成する逆算/編集候補数 |
| `maxAttackPieces` | int | 20（GUI既定 18） | 攻め駒上限（盤上+持駒、玉除く） |
| `maxDefendPieces` | int | 14（GUI既定 12） | 玉以外の玉方盤上駒上限 |
| `maxTotalBoardPieces` | int | 30 | 盤上総駒数上限（玉含む、CLI は 2..39） |
| `stepTimeoutMs` | int | 5000 | 成長フェーズの1問い合わせ上限(ms) |
| `verifyQueryTimeoutMs` | int | 15000 | 厳密検証・トリミングの1問い合わせ上限(ms) |
| `verifyBudgetMs` | qint64 | 1800000（GUI既定 300秒） | 1候補の厳密検証・トリミング総予算(ms) |
| `engineHashMb` / `engineThreads` | int | 0 / 0 | `USI_Hash`(MiB) / `Threads` の上書き（0 は登録設定のまま） |
| `addRemainingToDefenderHand` | bool | true | 未使用駒をすべて玉方持駒へ入れる |
| `allowFinalMoveAlternatives` | bool | false | 最終手の複数解を許容するか（既定は禁止で完全一意） |
| `requireNoSurplus` | bool | true | 駒余りを完全禁止するか |
| `strictMinLength` | bool | true | 厳密検証で `go mate infinite` 自然終了のみ確定するか |
| `excludeSimilar` | bool | true | 採択済み・除外済みと類似手順の候補を厳密証明前に除外するか |
| `seedSfens` / `excludeLines` | QStringList | 空 | 初期種局面（空なら0手詰み局面を自動生成）・既出除外対象 |
| `checkpointPath` / `resumeFromCheckpoint` | QString / bool | 空 / false | 途中保存ファイルパスと再開フラグ |
| `checkpointIntervalSec` | int | 30 | チェックポイント自動保存間隔（秒） |
| `rngSeed` | uint64_t | 0 | 乱数シード（0 は自動シード） |

## 応答・中断の扱いと回帰テスト

- 候補・除去探索の `checkmate` は一次フィルタを通して余詰検査へ進める。
- 検査中の `checkmate` は長さがN手かにかかわらず、現在の問い合わせへの詰み応答として扱う。
- 検査中の `nomate` はその攻手を否定するために使用し、`timeout` は判定不能として使用する。
- 無応答ガードで送ったstopの後に来る応答（`UsiCheckmate::stopped`）は結果として使わず、検査ではUnknownとして処理する（`MateAnswer::fromCheckmate()`）。
- 終了時は `MateQueryLoop::cancel()` で問い合わせを止め、調べている候補（`TsumeCandidateJob`）を破棄する。旧接続（`MateOracle`）のシグナルは切断する。

`tests/tst_tsumeshogi_verification.cpp` はエンジンを使わず、`TsumeCandidateJob`・`TsumeVerificationJob`（`TsumeshogiVerifier`）を代用エンジンの答えで直接進め、
第2・4問の初手、途中手、最終手、長い別解、判定不能、不正PV、駒除去の再検査、停止・時間切れ・遅れた応答で採択しないこと、
1回の問い合わせの上限と総予算、駒除去の事前選別で3手で詰まない除去をエンジンに送らないことを検査する。
生成器（`TsumeshogiGenerator`）は、どの終了経路（停止・上限到達・エンジンエラー・無応答・非対応）でも先行するバッチ生成を取り消して採択しないこと、
バッチが候補以外を含む生成局面数を進捗に通知し、キューに事前選別を通った局面だけを積むこと、候補の段階の進捗と棄却の数を中継することだけを検査する。
`tests/tst_tsumeshogi_screener.cpp` は事前選別の各判定（ちょうどの手数、短い詰み、複数初手、上乗せ深さによる長い別初手の検出、
不正入力・中断の Unknown、バッチの件数と中断）を検査する。
基本の唯一解正例は、王手が1種類しかなく応手が0の専用1手局面を使う。
第3問は実機照合で全攻手・全応手検査の合格を改めて確認する5手の正例とする。
過去の有限深さ検査のみを根拠に、第1・3・5問を唯一解正例とは扱わない。
最終手の複数解は、初手 `3c3b+` だけが詰み `2a1a` の後に `3b2b`/`2c2b` の2通りがある3手局面
（`7k1/9/6L+S1/9/9/9/9/9/9 b 2r2b4g3s4n3l18p 1`）で、許容時の合格・非許容時の棄却・生成側への設定伝播を検査する。
変化別詰の許容は、初手 `7e7b` の後に 8b への合駒（あと3手）と、2通りの即詰がある変化 `9b9c` を持つ5手局面
（`3+P5/k8/9/9/1G+R6/9/9/9/9 b Sr2b3g3s4n4l17p 1`）で合格を検査する。
この方針検査では、深さ5で詰みが見つからない局面を不詰と答える代用エンジンを使い、実機照合は `komoringIntegration()` で行う。
`tests/tst_tsumeshogi_export_header.cpp` はファイル保存のコメントヘッダ（各行が `# ` 始まりの1行であること、バージョン・生成日時・ランダム配置および逆算編集の各設定表記、生成上限0の「無制限」と最終手厳格・駒余り禁止の表記）を検査し、
`tests/tst_sfen_collection.cpp` は局面集ビューアがコメント行を読み飛ばすことを、`tests/tst_position_collection_file.cpp` は1行の規則（コメント・`startpos`・position コマンドの指し手と参考手順・読めない行）と最近使ったファイルの一覧を検査する。
`tests/tst_tsumeshogi_retrograde.cpp` は逆算・編集生成の単体〜統合テストを行い、0手詰み局面の生成と逆算可能性、2手逆算の順方向ラウンドトリップ、標的型余詰リペア、目標手数への接尾辞切り出し、`TsumeStrictCertifier` による最終手一意・駒余り禁止・全1枚除去の厳密証明とチェックポイント復元、101手対応と確定キャッシュ注入、モックエンジン（`tests/mock_retro_engine.cpp`）を用いた E2E 生成・再開および CLI `grow-tsume` の出力・101手再開を検査する。
GUI テスト `tests/gui/tst_tsumeshogi_generator_gui.cpp` は表示・保存・生成ライフサイクル・未登録/大フォント時のレイアウトに加え、`retrogradeModeToggleAndPersistence()` で逆算モード切替時のUI表示と全設定（除外ファイル含む）の保存・復元を、`retrogradeEndToEndResumeAndExclude()` でモックエンジンを用いた実GUI操作による5手詰生成・チェックポイント保存・既採択局面の表復元を伴う再開・除外ファイルによる既出回避を検査する。

通常の回帰テストには外部エンジンのインストールは不要。
KomoringHeights 1.1.0 の実機照合は、次の任意テストで再現できる。

```bash
SHOGIBOARDQ_TEST_KOMORING=/path/to/KomoringHeights-by-gcc \
  ./build/tests/tst_tsumeshogi_verification komoringIntegration
SHOGIBOARDQ_TEST_KOMORING=/path/to/KomoringHeights-by-gcc \
  ./build/tests/tst_tsumeshogi_retrograde komoringRetrogradeIntegration
```

実機照合では `Threads=1`、`USI_Hash=128`、`PostSearchLevel=MinLength`、
`GenerateAllLegalMoves=true`、`MultiPV=1` を指定する。
検査予算30秒・1問い合わせ1秒で、第2・4問の棄却、第3問と専用1手局面の採択、最終手複数解局面の許容時合格・非許容時棄却を確認する。
2026-09-25の実測では、第2問は57問い合わせ・約6.8秒、第4問は6問い合わせ・約0.1秒で余詰判定、
第3問は18問い合わせ・約0.1秒で合格。5秒設定では第2問は判定不能として棄却される。
検査の分だけ生成速度や採択率は下がる。

原因調査と元の再現データは [余詰調査報告](reports/tsumeshogi-yodzume-investigation.md) を参照。

## 各1000題の生成・再編で得た知見

[大量生成の作業記録](tsumeshogi-collection-generation.md) に基づき、
生成器の開発・大量生成の運用に関係する内容をまとめる。以下は2026年9月26日に初回作成した3・5・7・9・11・13手詰（各1000題・計6000題）の知見であり、その後に不要駒・類似作の再編（9月27日）、駒余り検査と差し替え（10月1日）、さらに `grow-tsume` による15〜31手詰（各1000題・計9000題、11月27日）を追加している。初回生成の条件だけで現在の全15,000題の品質を保証したものではない。

### 標準実装と作業用の変更を区別する

初回生成はPythonから `shogiboardq-cli generate-tsume` を直接並列実行した。MCPサーバーは使用していない。
標準GUI・CLIが共用する `TsumeshogiGenerator` の詰み探索・余詰検査・トリミングを使い、長手数では候補生成と事前選別を調整した一時実行版も使用した。

一時実行版は `src/analysis/tsumeshogicandidatescreener.cpp` の作業用コピーの `generateBatch()` を変更して作った。
`build/compile_commands.json` とリンクコマンドを参照し、変更したオブジェクトを既存の静的ライブラリより先にリンクしたCLIを `/tmp` に出力した。
種局面の読込・変更処理も同じ作業用ディレクトリに置き、リポジトリのアプリケーションソースや標準CLIを置き換えずに比較した。

当時の短時間選別・玉位置の制限・近傍探索は、この一時実行版への追加処理である。
`SHOGIBOARDQ_BATCH_SCREEN_MS` と `SHOGIBOARDQ_BATCH_KING_EDGE` も作業用の環境変数であり、標準CLIの設定項目ではない。
一時実行版と当時の補助検証プログラムは配布物に含まれない。現在の専用サンプラーによる駒変更・逆算は、後述の「問題集の類似作除外と補充」に記載する別の実装である。

### 手数に応じて候補生成・探索予算を変える

| 手数 | 初回作成時に試した条件 |
|---|---|
| 3 | ランダム配置で300題を作り、収集済み局面を保持して700題を追加 |
| 5 | ランダム配置を並列実行し、外部エンジンの探索時間を0.5～20秒で比較 |
| 7 | 駒数・配置範囲を比較し、一時実行版の事前選別を5msに短縮 |
| 9 | 事前選別を1・3・5・10・25msで比較。玉の盤端からの最短距離を0・1・4マス以内で比較し、並列数を8から16に変更 |
| 11 | 事前選別は主に1・3ms。生成済みの7・9手詰と収集中の11手詰を種として駒配置・持駒を変更 |
| 13 | 事前選別は主に1・3ms。7・9・11手詰と収集中の13手詰を種にし、駒移動・駒種変更の比率を増加 |

ここでの数msは内蔵探索の事前選別の予算であり、CLIの `--timeout-ms` が指定する外部エンジンの探索・検査の予算とは別である。
初回の一時実行版では、目標手数ちょうどの詰みが確認でき、複数の詰む初手が検出されなかった候補だけを外部エンジンへ渡し、`Unknown` は外した。
標準の `generateBatch()` が取りこぼしを避けるため `Unknown` もエンジンへ渡す動作とは異なる。
短時間で確認できない候補を取りこぼす方針であって、`Unknown` を不詰と証明したわけではない。事前選別後の主手順の余詰検査も省略しない。

駒数・配置範囲・玉の位置・並列数は、生成候補数だけでなく、重複を除いた採択数・判定不能数・CPUとメモリの使用量で比較した。
短い予算や盤端への限定がすべての手数で最適という意味ではなく、6000題を通じた固定設定でもない。

### 種局面の変更ではトリミング後の新規性を重視する

11手詰以降の一時実行版では、約10%をランダム配置、約90%を近傍探索とした。
近傍探索では収集中の目標手数の種があれば約60%でそこから選び、残りは既存の問題集から選んだ。
変更回数は約75%で1回、約25%で2回。駒を縦横それぞれ±2マスへ移動する、駒種・成不成を変える、玉周辺に駒を追加する、盤上駒を削除する、攻方持駒を増減する操作を試した。

変更後は行き所のない歩・香・桂を成駒に補正し、二歩・駒種ごとの総数・玉の数・初期王手を確認した。
未使用駒を玉方持駒へ戻してSFENを再構成し、元のPVを流用せずに目標手数で探索し直した。

13手詰では、追加した不要駒がトリミングで消えて種と同じ局面に戻る例を減らすため、変更操作の試行比率を調整した。

| 変更操作 | 当初 | 13手詰の調整後 |
|---|---:|---:|
| 盤上の駒の移動 | 45% | 60% |
| 駒種・成不成の変更 | 20% | 25% |
| 盤上への駒の追加 | 20% | 5% |
| 攻方持駒の増減 | 10% | 5% |
| 盤上の駒の削除 | 5% | 5% |

比率は操作を試す確率で、採択された作品の割合ではない。新規性は変更直後でなく、不要駒除去後のSFENで判定する必要がある。
さらにSFENが異なっても同じ解き方に集中するため、現在は後述の手順による類似判定も併用する。

### 採択後に最短手数と手順を独立して確認する

PVの長さが目標手数と一致するだけでは、より短い詰みがないことを証明できない。
当時の補助検証では `TsumeCollection::parse()` と `validMateLine()` で読み込み・全手の合法性・攻方の連続王手・終端の詰みを確認し、
Hayanagiの `TsumeSearch::solve()` による有限深さの反復深化探索で最短手数を調べた。

13手詰では、生成済み489題を1局面1秒の独立探索で再確認し、479題を残して不足を補充した。
その後はSFENごとに新しい探索表を使い、最短13手詰と合法な手順を確認できた場合だけ件数に加えた。
時間切れを不詰や駒の必要性の証明として扱わず、未確定の問題は完成件数に含めない。

この初回の独立確認は全駒の必要性までは検査しておらず、旧703番に不要駒が残った。
現在の完了条件には「保存済み問題集の再検査」で説明した全1枚除去の監査と、除去後の重複排除も含める。
全件が揃った後にも、正規化したSFENの重複、最短手数、合法手順、二歩・行き所のない駒・駒種ごとの総数を確認し、不足分を同じ基準で補充する。

### 並列収集は全ワーカー共通の完成件数で管理する

Python側では `asyncio.create_subprocess_exec()` でCLIを直接起動し、JSON Linesの `position` からSFENとPVを取り出した。
CLIの1回の生成上限は100題なので、全ワーカー共通で重複を除いた件数を数え、不足する間は次のバッチを起動する。
正規化したSFENの盤面・手番・持駒を共通キーにし、途中結果とワーカー別ログを保存する。設定を変える際にも収集済み局面を保持して再開する。

単に各プロセスの出力件数を合算せず、最終局面の重複排除と独立検証を通った問題数を完了判定に使う。
不要駒・類似作の整理や最終監査で件数が減った場合も同様に補充する。
CLIの登録名・エンジン設定・停止要求・途中保存を含む実行可能なPythonコードは、[生成・検証・再編の記録](tsumeshogi-collection-generation.md) を参照。

## 問題集の類似作除外と補充

盤面SFENの重複除外だけでは、金を成桂に置換した問題や、同じ追い詰めの移動座標を持つ問題が多数残る。
不要駒除去後の13手詰集では、旧722・723番のUSI手順が全て一致し、1000題の手順文字列が41種類に偏っていた。
問題集の収集側に `scripts/tsume_diversity.py` の共有索引を追加した。標準GUI・`generate-tsume` のSFEN重複検査とは別の、問題集編集用の処理である。

- 最終手を除く移動座標・駒打ちの種類・成の有無を比較する。動く盤上駒の種類は比較しないため、金と成桂などの交換でも別作に数えない。
- 玉を原点とする相対座標と左右反転の正規化により、配置の平行移動や鏡像を検出する。絶対座標でも比較し、玉の初期位置だけが違う同じ手順を採らない。
- 9手以上では、初めの2手と最終手を除く中間手順も比較する。中間手順が同じ、または1手だけ違う候補は、採択済みの代表作と同系統として除外する。
- これは保守的な編集基準であり、変化・紛れを含めた作品の同一性の数学的判定ではない。同じ座標の手順でも別の価値がある作を除外する場合がある。

`scripts/refill_diverse_tsume.py` が全ワーカーの候補を集約する。不要駒除去の前後で索引を調べ、並行検査中に他ワーカーが採択した類似作も除外する。
採択済み・除外済みのSFENも候補生成側へ定期的に共有し、既出局面に同じ有限深さ探索を繰り返さないようにする。
各ワーカーは `tsumeshogi_diversity_sampler` と `tsumeshogi_collection_auditor` を直接起動し、JSON Linesで通信する。MCPは介さない。
候補は既存の `TsumeshogiPositionGenerator` によるランダム配置、種局面の駒変更、直前の王手と玉移動・合駒の逆算から作る。
逆算候補も、想定した手順をそのまま答えにせず、改めて最短手数・余詰・全1枚除去を検査する。
逆算の出発点には検証済みの2手短い種を使い、王手と玉移動または合駒の1組を追加する。最短手数が2手伸びたことと、有限深さ内で複数の詰む初手が見つからないことを事前確認する。有限深さでの初手検査は、採択時の実エンジンによる余詰検査を代替しない。
候補の主手順を復元するときも、最終手を除く攻方の各手番で複数の詰む手が見つかった候補を除外する。ここで判定できなかった候補は実エンジンの監査へ渡し、確定検査を省略しない。

種局面は手数ごと・系統ごとの代表作を使い、不足している手数へワーカーを振り向ける。
残る手数が1種類になり、必要件数の75%以上が揃った段階では、短い種の補充担当を減らしてその手数の生成を優先する。
各手数の種は最大200題とし、その半数は最近の採択作、残りは全履歴から巡回して抽出する。巡回位置はワーカーごとにずらし、採択順の先頭200題に固定されないようにする。
駒変更には目標と同手数または2手短い種を使い、同手数の種を優先する。13手以上では候補作成の配分を逆算15%・駒変更75%・ランダム配置10%とし、それより短い手数では40%・35%・25%とする。
ランダム配置の駒数・配置範囲はワーカーごとに変える。種の変更・逆算には攻方9枚・玉以外の玉方4枚までの共通上限を使い、他ワーカーで得た種を小さい設定値だけで捨てない。駒が多い候補も不要駒検査を省略しない。
上の手数が不足している間は、9・11手詰について収録数の最大3倍まで異なる検証済みの種を保持する。公開用の作業ファイルには指定件数だけを出力し、追加の種で収録数を水増ししない。完成済みの短手数は、事前探索で手数を得た時点で候補出力を省く。
手順が変わった未採択の近傍局面も各系統3題まで探索用に保持するが、検証済み問題の件数には含めない。
新しい手順に余詰があった候補も各手数300系統まで修正探索用に保持し、種に混ぜるのは各手数50題までとする。こちらも採択数には加えない。
余詰候補には、生成器と共有する `onePieceRemovedPositions()` で駒を1枚除いた修正候補も作り、各候補最大2通りを試す。元の余詰局面を採択せず、修正後の局面に最短手数・余詰・不要駒の全検査を改めて要求する。
`progress.json` は採択数と不足数を記録し、時間切れで1000題に達しなくても達成とは表示しない。
入力ファイル・検査器・類似判定コードのハッシュを記録し、異なる検査条件の再開を防ぐ。
最終監査などで不採択にするSFENは `--exclude-sfens` で追加する。除外一覧は作業ディレクトリに永続化し、再開時に元ファイルや以前の合格ログから復活させず、不足数として補充する。

補充後は `audit_tsume_collections.py` を別プロセスで実行する。
`publish_diverse_tsume.py` は全件の確定判定・不要駒除去による変更なし・類似判定後の件数を要求し、直近10題との手順一致がなるべく少ない順序に並べる。
再編で問題番号が変わるため、保存ファイルのハッシュ、入れ替え数、類似判定前後の指標も記録する。
実行例は [問題集README](../../data/tsumeshogi/README.md) を参照。

`tests/test_tsume_diversity.py` は旧722・723番、鏡像・平行移動、玉の初期位置・最終手の差、
異なる筋の保持、出題順の再現性、並行検査後の再判定、除外を維持した再開、子プロセス異常終了、公開前の確定判定を検査する。
