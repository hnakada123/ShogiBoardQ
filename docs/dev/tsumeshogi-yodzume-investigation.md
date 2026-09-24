# 詰将棋局面生成の余詰調査（2026-09-24）

本報告は修正前の `08398bbd7fd3676c66bcf9e89f9be2e024e15b00` を対象とする。
後続の修正内容・回帰テストは [生成機能の仕様](tsumeshogi-generator.md) を参照。

## 結論

根本原因は、生成側が「目標手数の詰みがあること」と「攻め方の解が一意であること」を別々に検査していない点にある。`TsumeshogiGenerator::onCheckmateSolved()` は返却PVの長さが目標手数と一致すればトリミングへ進め、除去後も同じ条件で採用する。出力までに別の詰む攻め手を調べる処理はない。

5問すべての最短詰み手数は5手だった。第2問の初手 `G*9g`・`R*9g`・`R*8h`、第4問の初手 `S*3b`・`R*4b` は、いずれも玉方の全合法応手に対して5手以内に詰ませられる。第4問は `S*3b 4a4b` の後にも3通りの3手詰がある。初手だけの一意性検査では足りない。

調査途中のユーザー申告と添付画像により、生成時の指定エンジンは **KomoringHeights 1.1.0 64ZEN2** と確認できた。指定された実行ファイルを `PostSearchLevel=MinLength` で動かしても、5問すべてに5手PVが返り、別解も再現した。最短性の設定ミスを原因とする必要はなく、今回、KomoringHeightsの詰み判定不具合を示す証拠は得られていない。

調査段階では本体機能・既存テスト・サブモジュール参照を変更せず、この報告、再現用スクリプト2本、実験記録を追加した。

## 調査対象と保全

| 対象 | 調査開始時の状態 |
|---|---|
| ShogiBoardQ | `08398bbd7fd3676c66bcf9e89f9be2e024e15b00`、作業ツリー clean |
| `ShogiBoardQ/Hayanagi` | `30cfce58e0a863f9f2d2144635f1d9e9d4e3e54f`、clean、初期化済み |
| `/home/nakada/GitHub/Hayanagi` | 同じコミット、既存の未追跡 `docs/` あり。内容・状態を変更していない |
| 適用指示 | ShogiBoardQ直下の `AGENTS.md` を確認。Hayanagi両ディレクトリ内に追加の `AGENTS.md` はなかった |
| KomoringHeights | `/home/nakada/shogi/KomoringHeights-kh-v1.1.0/source/KomoringHeights-by-gcc`。配布ソースディレクトリに `.git` はなく、コミットは未特定。実行ファイルのSHA-256で識別 |

Desktopのファイルとfixtureは、改行を含めて**バイト単位で一致**した。手順付きfixtureから ` moves ...` を除いた5行もSFENのみのfixtureと一致した。対象は10問ではなく5問である。

| Desktop → fixture | 両者に共通するSHA-256 |
|---|---|
| `5手詰.txt` → `tsume_positions.sfen` | `2fa6c3532b674a67698ab5dfe7f8c82c4c950cba3adf9675cbf56d2bf960832d` |
| `5手詰2.txt` → `tsume_positions_with_moves.sfen` | `b7c952201652632c03f90ed1cf3f56f641f27b63de76809ccbc657e40179b279` |

### 生成時の指定と今回の設定

ユーザー画像には、KomoringHeights、5手詰、攻め駒上限5、守り駒上限4、配置範囲3、探索5秒、生成上限5、手順出力あり、と表示されている。

`/home/nakada/.config/ShogiBoardQ/ShogiBoardQ.ini` の現在値を読取確認したところ、画像と一致し、KomoringHeightsの設定は次のとおりだった。設定ファイルは変更していない。

| オプション | 現在の保存値／今回の再現値 |
|---|---|
| `Threads` | 4 |
| `USI_Hash` | 4096 MB |
| `MultiPV` | 1 |
| `GenerateAllLegalMoves` | true |
| `PostSearchLevel` | MinLength |
| `NodesLimit` | 0 |
| `RootIsAndNodeIfChecked` | true |
| `PvInterval` / `ScoreCalculation` / `WriteDebugLog` | 1000 / Ponanza / 空 |
| 探索コマンド | `go mate 5000` |

これは調査時点の保存値であり、元ファイル生成時の全設定を記録したログではない。ただし、正しい最短性設定で同じ問題を再現できるため、この履歴の不足は根本原因の判断を妨げない。

## 検証方法と保証範囲

### Hayanagiによる列挙

`build/Hayanagi/hayanagi` をReleaseビルドし、`TsumeMode=true`、`USI_OwnBook=false`、`GenerateAllLegalMoves=true` を明示した。その他は起動時の既定値。詰み探索は専用ワーカースレッドで行われる。1問い合わせ10,000 msとし、各応答を受信してから次の局面を送った。

1. 5問について深さ1・3・5を探索。
2. 全合法初手のうち、王手になるものをすべて列挙し、その後の玉方局面を残り4手で探索。
3. 詰む初手について、玉方の**全合法応手**と、その先の攻め方の詰む手を再帰的に列挙。合駒・捕獲・成／不成も省略しない。
4. 第4問の `S*4b` の後は、残り4手と6手を比較し、6手の証明木も取得。

結果は3,189種類の問い合わせで、`mate` 755、`depthlimit` 2,431、`nomate` 3。`timeout`・`cancelled`・`invalid` は0だった。`depthlimit` は指定手数内に詰みがないという探索結果であり、もっと長い詰みがないことを意味しない。

### 独立した確認

Hayanagiとコードを共有しない [python-shogi 1.1.1](https://github.com/gunyarakun/python-shogi) の盤面・合法手生成を使用した。これは別の完成済み詰将棋エンジンではなく、独立した検証手段である。

- 各初期局面で、独自の有限AND/OR探索により1手以内・3手以内の詰みがないことを確認した。1探索60秒上限、実際の時間切れは0。
- 保存した証明木では、攻め方の着手の合法性と王手、玉方の全合法応手の網羅、終端が王手かつ合法応手0であることを検証した。**755証明局面すべてが成功**した。
- Hayanagiの `perft depth 1 divide` と合法手集合を760回比較し、一致した（755証明局面＋5初期局面）。
- fixtureの参考手順5本と依頼文の別解3本も、各着手が合法・連続王手・終端詰みであることを確認した。

攻め方は少なくとも1つの証明済みの手を持ち、玉方はどの合法手を選んでも証明済み局面へ進む。このため、特定の逃げ方だけを選んだ協力手順ではない。1・3手の不存在と5手以内の証明木を合わせ、5問の最短5手も独立に確認できる。

一方、5手以内の**全非詰手の否定**をpython-shogiで独立探索したわけではない。初手候補数の網羅と非詰判定はHayanagiの結果に基づく。7手以上の全初手・全変化は未探索であり、無制限の余詰なしの証明はしていない。

### KomoringHeightsによる照合

指定実行ファイルで14件の `go mate 5000` を実行した。全件で詰みPVが返り、時間切れは0。全PVをpython-shogiで合法・連続王手・終端詰みと確認した。各問い合わせの応答は約0.032～0.043秒だった（初期化時間を除く、このマシンでの値）。

| 問い合わせ | 返却PVの長さ |
|---|---:|
| 第1～5問の出題局面 | すべて5 |
| 第2問 `G*9g` / `R*9g` / `R*8h` の後 | すべて4 |
| 第4問 `S*3b` / `R*4b` の後 | すべて4 |
| 第4問 `S*4b` の後 | 6 |
| 第4問 `S*3b 4a4b` の後に `6d5c` / `R*4a` / `R*4c` | すべて2 |

KomoringHeightsからは通常のPVを受け取った。全応手を含む証明木は前項の独立検証で確認しており、このPVのみを全応手の証明と扱ってはいない。

## 5問の結果

ここでの「最短」は、攻め方が連続王手で詰ませ、玉方が最長抵抗したときの最小手数である。人間の作品評価における無駄合・駒余り等の慣例を追加判定した値ではない。

| 問 | 最短 | 王手初手数 | 5手以内で詰む初手 | 途中手の調査結果と限界 |
|---|---:|---:|---|---|
| 1 | 5 | 9 | `3c5c+` | `3c5c+ 4d3d` 後に、1手詰と3手詰の選択肢あり。早く詰む変化内の別手順として分類する。長手数別解は未判定 |
| 2 | 5 | 20 | `G*9g`, `R*8h`, `R*9g` | 初手余詰あり。途中・最終手にも複数の詰手を確認 |
| 3 | 5 | 14 | `R*1g` | 5手以内の証明木には攻方の複数選択なし。7手以上は未判定。唯一解の正例にはしない |
| 4 | 5 | 14 | `R*4b`, `S*3b` | 初手・途中手の余詰あり。`S*4b` から7手の強制詰みも確認 |
| 5 | 5 | 13 | `R*9c` | 5手以内の証明木には攻方の複数選択なし。7手以上は未判定。唯一解の正例にはしない |

5手以内の全詰初手から展開した証明木は、順に27・88・6・96・16局面だった。第4問の `S*4b` 以下6手の木も加えた合計が755局面である。複数解の箇所を調べる際には、最短で詰む手だけでなく、残り予算内で詰む長めの手も含めた。

### 第1問の変化で見つかった複数手

`3c5c+ 4d3d` 後は `5c4c` で即詰み。一方 `1c3c` と `1c4c` も、それぞれ残り3手の強制詰みである。玉方が `4d4e` と逃げる主変化では5手かかるので、`4d3d` は早く詰む変化である。この変化内の長い詰め方を、直ちに主手順の余詰と同一視してはいけない。

第1問は「初手が1つだから完全」とも「攻方の分岐があったから不完全」とも断定しない。第3・5問を含め、作品としての完全性を今回証明したものではない。

## 第2問の再現

```text
9/9/9/R8/9/9/1k2+S4/9/3+N5 b RG2b3g3s3n4l18p 1
```

初手別のHayanagi応答：

```text
G*9g : tsume mate move 8g7f plies 4 nodes 3618
R*9g : tsume mate move 8g8f plies 4 nodes 781
R*8h : tsume mate move 8g7f plies 4 nodes 1018
```

初手直後の玉方の合法応手は以下で尽くされる。各行の攻手から先も、すべての玉方合法応手を証明木で確認した。

| 初手 | 玉方の応手 | その後、残り3手以内で詰む攻手の全候補 |
|---|---|---|
| `G*9g` | `8g7f` | `R*8f` |
| `G*9g` | `8g7g` | `R*7h`（即詰み）, `R*7i` |
| `G*9g` | `8g8h` | `R*8g` |
| `R*9g` | `8g7f` | `5g6g` |
| `R*9g` | `8g8f` | `9g9f` |
| `R*9g` | `8g8h` | `G*7h`, `G*7i`（即詰み）, `G*8g`, `G*9h`, `G*9i`（即詰み） |
| `R*8h` | `8g7f` | `G*6f`, `G*6g` |
| `R*8h` | `8g7g` | `6i7h`, `G*8g`（即詰み） |
| `R*8h` | `8g8h`（飛を取る） | `G*7h` |

依頼文の代表手順は両方とも合法な5手詰だった。

```text
R*9g 8g8f 9g9f 8f8e G*8f
R*8h 8g7f G*6f 7f7g 6i7h
```

KomoringHeightsも各初手後に4手の詰みを返した。例えば `R*9g` 後の返却は `8g7f 5g6g 7f6e G*6f`。参考手順とは異なる玉方の応手・攻め方が選ばれても、同じ強制詰みである。

## 第4問の再現

```text
5k3/9/9/3+P1B1N1/9/9/9/9/9 b RSrb4g3s3n4l17p 1
```

```text
S*3b : tsume mate move 4a5a plies 4 nodes 1398
R*4b : tsume mate move 4a5a plies 4 nodes 285
```

| 初手 | 玉方の応手 | その後、残り3手以内で詰む攻手の全候補 |
|---|---|---|
| `S*3b` | `4a4b` | `6d5c`, `R*4a`, `R*4c` |
| `S*3b` | `4a5a` | `R*3a`, `R*4a` |
| `S*3b` | `4a5b` | `4d5c+`, `6d5c`, `R*7b` |
| `R*4b` | `4a3a` | `2d3b+`（即詰み）, `4b2b+`, `4b3b`, `4b3b+`（即詰み） |
| `R*4b` | `4a4b`（飛を取る） | `6d5c` |
| `R*4b` | `4a5a` | `4b6b+` |

代表別解 `R*4b 4a5a 4b6b+ 5a4a 2d3b+` は合法な5手詰である。

参考手順の途中 `S*3b 4a4b` では、3つの攻手すべてが残り3手で詰む。各攻手後のHayanagi応答は次のとおりで、KomoringHeightsでも同じ残り2手が確認できた。

```text
6d5c : tsume mate move 4b5a plies 2 nodes 14
R*4a : tsume mate move 4b5b plies 2 nodes 7
R*4c : tsume mate move 4b5a plies 2 nodes 15
```

さらに `S*3b 4a4b R*4a 4b5b` の最終手には `4d5c+` と `6d5c` がある。これは途中手の分岐と分け、最終手複数解として記録すべきである。

長手数別解の `S*4b` は、Hayanagiで残り4手が `depthlimit`、残り6手が `mate ... plies 6`。KomoringHeightsのMinLengthでも残り6手だった。

```text
S*4b 4a4b 6d5c 4b4a R*4b 4a3a 4b3b+
```

これは7手の別攻めであり、「5手以内の別解だけを排除すれば余詰なし」という仕様では取りこぼす。全応手の6手証明木も独立検証済みである。

## コードから見た原因の切り分け

行番号は調査対象コミットのもの。

| 箇所 | 確認した実装・意味 |
|---|---|
| `src/analysis/tsumeshogigenerator.cpp:132` `onCheckmateSolved()` | 139行の `pv.size() != m_settings.targetMoves` が採否条件。通過後は探索中ならトリミングへ、トリミング中なら除去を確定。余詰検査なし |
| 同257行 `generateAndSendNext()`、427行 `sendTrimmingCheck()` | 選択された外部エンジンへ `position` と `go mate <ms>` を送る。目標手数や唯一解という要件はエンジンへ渡していない |
| 同392行 `startTrimmingPhase()`、402行 `tryNextTrimCandidate()` | ベースとPVを保持し、駒の除去を順に試す。初期王手の除外はあるが、攻方の別解は調べない |
| `src/analysis/tsumeshogigenerator_sfen.cpp:167,215` | 盤上の玉以外と攻方持駒を除去候補とし、除去駒を生駒として玉方持駒へ移す。玉方持駒は減らさない。局面自体を変えるため、以前の唯一性判定があっても流用できない |
| `tsumeshogigenerator.cpp:435` `finishTrimmingPhase()` | `processResult(true, ...)` → `registerFoundPosition()` で通常出力 |
| 同308行 `registerFoundPosition()` | `m_foundSfens` によるSFEN重複排除だけを行って `positionFound` を発火。同一局面内の解の数とは無関係 |
| 同318行 `flushTrimmingResult()` | トリミング途中のベース局面を直接登録。97行 `stop()`、232行 `onEngineError()`、185行 `onSafetyTimeout()` のstop応答待ち失敗から到達する |
| 同166行 `onCheckmateNotImplemented()` | 終了するがflushは行わない。終了経路すべてが同じ動作ではない |
| `src/engine/usi.cpp:295`、`usiprotocolhandler.cpp:126` | GUIの選択エンジンを起動し、エンジン名に対応する保存オプションを読み込む。生成機能専用の唯一性設定はない |
| `src/engine/usiprotocolhandler_ops.cpp:8` | `checkmate` をPVか状態通知に分ける。PVの別解数や証明木の情報は生成側へ渡されない |
| `src/dialogs/tsumeshogigeneratordialog_actions.cpp:26,249` | 受信SFEN/PVを表示・保存する。`moves` 出力は参考手順の付加であり、追加検証ではない |
| `src/analysis/tsumeshogipositiongenerator.cpp:26` | 駒配置・二歩・初期王手等を処理する候補生成器。唯一解を保証する生成アルゴリズムではない |

最短性依存は [既存の機能説明](tsumeshogi-generator.md) とダイアログの注意書きにも記載済みだが、最短性を満たすだけでは余詰は排除されない。

### エンジンの責務との区別

[将棋所のUSI定義](https://shogidokoro2.stars.ne.jp/usi.html) は `go mate` の結果として詰み手順、`nomate`、`timeout` 等を規定している。別解の不存在を示す情報は標準の `checkmate <PV>` には含まれない。5手PVの存在から唯一解を推論するのは生成側の追加要件の欠落である。

KomoringHeightsの [公式オプション説明](https://github.com/komori-n/KomoringHeights/blob/main/source/engine/user-engine/docs/EngineOptions.txt) と指定ディレクトリ内の同名文書では、`MinLength` は最短性を保証する設定、`MultiPV` は次善手以降も探索する設定と説明されている。ローカル `komoring_heights.cpp:185` の `SearchMainLoop()` も、詰み発見後に `len = result.Len() - 2` と短い詰みを調べる処理である。ソース中の「余詰探索」というコメントを、同手数・長手数も含めた唯一解保証と解釈してはいけない。

`MultiPV=2` に変えるだけでも根本解決にはならない。初手以外の攻手、時間切れで未探索の手、より長い別解が残り、現行ジェネレータは複数の `info` PVを余詰判定として消費していない。

Hayanagiの `src/tsume.cpp:18` は深さを増やして探索し、40～56行では攻方の王手のうち1つが詰めば打ち切り、玉方では全合法応手が詰むことを要求する。これは詰み存在判定のAND/OR探索であり、解の全列挙器ではない。同エンジンの不具合を仮定せず今回の現象を説明できる。

なお現行Hayanagiの独自 `go tsume` と、生成側の標準 `go mate` は別経路である。`Hayanagi/src/usi_engine.cpp:401` 以降を確認し、片玉局面に `go mate 1000` を送ると `info string use go tsume for kingless positions`、`bestmove resign` が返ることも記録した。このHayanagiを標準 `go mate` 対応の生成エンジンとして扱うことはできないが、今回ユーザーが指定した生成エンジンはKomoringHeightsなので、余詰混入の原因とは別件である。

### 確定していない点

- 元の乱数系列、トリミング前の候補、除去履歴は未保存である。余詰が最初からあったか、特定の除去で生じたかは断定できない。
- 今回は完成SFENへの詰み探索・採択条件の再現であり、ランダム生成を再実行して同一5問の発生履歴を再現したものではない。
- 第1・3・5問の7手以上を含む完全性、慣例に沿う全変化の作品評価は未判定。
- KomoringHeightsの配布バイナリに対応する正確なGitコミットは未特定。ただし使用バイナリ、版表示、設定、SHA-256、全通信を保存した。

## 余詰判定の仕様で決めること

詰将棋の慣例には例外や資料間の差がある。以下はソフトの採択仕様を決めるための整理であり、全分岐で攻手が厳密に1つであることを、既存作品一般の完全性の定義に置き換えるものではない。

| 分類 | 調査結果への当てはめ／仕様上の判断 |
|---|---|
| 主手順の余詰 | 別の攻めでも強制的に詰むもの。第2・4問の別初手や第4問の途中3手が該当する。生成時に排除する |
| 玉方の複数応手 | 玉が複数方向へ逃げられること自体は余詰ではない。各応手への詰みを要求する |
| 変化別詰 | 玉方が主変化より早く詰む逃げ方をした後の別攻め。第1問の `4d3d` 後が具体例。主手順の余詰と別分類にする |
| 最終手の複数解 | 第4問の最終手 `4d5c+` / `6d5c`。最後の1手に限り許容する慣例がある。短手数の教材として一意にするかは明示的な方針が必要 |
| 成・不成 | USIの `+` の有無を消して同一手にしてはいけない。王手成立・利き・応手・後続手順を比較し、非限定として許す範囲を決める |
| 作意より長い別攻め | 第4問 `S*4b` は7手。「攻方最短」の理由だけで無視しない。迂回・合流・成不成非限定などを別分類するには追加の手順解析が必要 |
| 同手数・長手数の玉方変化 | 変同・変長、駒余り、無駄合の扱いは別仕様。単一PV長と全作品評価は同一ではない |

[東京詰将棋工房のルール説明](https://tsume-kobo.org/d_ttmfaq/faq_rule.html) は、長い別攻めも余詰になり得ること、変化別詰、最終手の例外、成不成非限定を区別している。また、同ページ自身が簡略化した慣例の説明であると明記している。

[詰将棋解答選手権2023一般戦の公式解題・追記](https://shogi-problem.org/2023/DescB.pdf) には、最終手の別解の指摘を受けて正解者数を訂正した実例がある。生成品質の判定と、既存問題を解いた手の採点は分ける必要がある。

[鈴木信幸氏「迂回・成不成非限定」](https://suzukou.org/rule/rule14.html) は、同じ局面へ戻る迂回と先へ合流する迂回、飛角歩と銀桂香の成不成等を区別する。同サイトの [最終手余詰・変同・変長](https://suzukou.org/rule/rule15.html) と東京詰将棋工房の説明では変長の許容にも差があるため、慣例を一律の真偽値として埋め込まない。

実装を始める前に、少なくとも「主変化だけか全変化も厳格にするか」「最終手・成不成非限定を許容するか」「長い別解をどう証明・分類するか」を決める必要がある。教材用途に厳しい条件を設けるなら、その条件を明示し、慣例上許容される作品も落とし得ることを仕様に記載する。

## 推奨する修正方針

### 採択を1か所で管理する

`onCheckmateSolved()` のPV長判定を一次フィルタとして残し、その後に専用の検証サービスを呼ぶ。サービスは、合法性・最短手数と、攻方の別解・分類・調査範囲を別々に返す。生成オーケストレータに探索そのものを詰め込まない。

状態機械には候補検証と最終検証の状態を追加する。出力用の共通関数では、**現在のSFENと検査方針に対応する検証結果が揃っていること**を必須にする。`registerFoundPosition()` をその入口にするなら、引数をSFEN/PVだけでなく検証済み結果へ変更し、SFEN重複排除はその後に行う。

### 途中の攻手と長い別解も扱う

合法な王手を成・不成込みで全列挙し、各攻手の後は攻方を固定した玉方AND探索にする。玉方の応手数を解の数として数えない。主変化上の各攻方局面で別攻めを調べ、玉方が選ぶ他の変化も方針に応じて再帰的に分類する。第4問の例により、出題局面の初手だけを調べる設計は不十分である。

有限深さの結果は、例えば「別解あり」「指定深さ内の別解なし」「方針上の唯一性を証明」「判定不能」に分ける。時間切れ・中止・プロトコル不整合は判定不能とする。**上限N手やN+2手で見つからなかった結果を、無制限の余詰なしとして採択しない。**

厳密な余詰排除を表示するモードでは、必要な別攻めの不詰証明まで完了しなければ採択しない。有限深さしか保証できないモードを設けるなら、検証範囲を明示した別の結果として扱う。深さ上限を延ばすだけでは一般の不存在証明にはならず、循環・合流・長い別解を扱う探索と方針が必要になる。

Hayanagiを基盤にする場合、`TsumeSearch` の単一の `move` 返却だけでは足りない。全候補・全応手の調査結果と中止状態を提供するAPIを追加する。また、現行の深さ上限63と生成UIの目標上限99の差を黙って切り詰めない。外部エンジンを使う場合も、玉方ルートの解釈と返却情報を検証する必要があり、標準USIに任意の探索手除外や証明木出力があるとは仮定できない。

### トリミングと停止時出力

- 候補の検証後にトリミングする場合、除去で局面を変えるたびに検証結果を失効させる。唯一性を保持したベースが必要なら、除去確定前にも検査する。
- 少なくとも最終SFENでは改めて検証し、完了後にだけ `finishTrimmingPhase()` から出力する。
- `flushTrimmingResult()` からも同じ出力条件を通す。未検証の最新ベースは停止・エラー時に出力しない。代わりに保持するなら、最後に全検査を通過したSFEN/PVの組を出力する。
- 検査中に停止された結果、旧局面の遅延応答、前回開始時のワーカー完了を誤採択しないよう、実行世代・局面識別子を照合する。

### 探索時間とUI

列挙・追加探索はバックグラウンドで行い、キャンセルと進捗を通知する。現在の「1局面のgo mate時間」と「別解検証の総予算」を区別する。今回の短い例が速くても、全候補×全変化×長手数探索は重くなり得る。同期的な全探索や停止時の長い待機をUIスレッドへ追加しない。

UIには検証中・余詰で棄却・判定不能を区別して表示する。新しい設定は `TsumeshogiSettings` 経由で永続化し、UI文言は日英翻訳を更新する。エンジン名・版・オプション、元候補、除去履歴、検査範囲を保存できるようにすれば、次回は生成履歴まで切り分けられる。

### 変更が必要な責務とファイル

| 責務 | 主な対象 |
|---|---|
| 別解検証・分類 | 新規 `src/analysis/` の検証サービス。必要ならHayanagi側の専用API |
| 採択状態・共通出力条件・キャンセル | `src/analysis/tsumeshogigenerator.{h,cpp}` |
| トリミング後の検証 | 同上。`tsumeshogigenerator_sfen.cpp` の除去結果を新局面として扱う |
| エンジン能力・応答 | 外部方式を採る場合は `src/engine/usi*`。通常の `checkmate` を唯一性保証と解釈しない |
| 設定・進捗・結果の表示 | `src/dialogs/tsumeshogigeneratordialog*`、`src/services/tsumeshogisettings*`、`settingskeys.h`、翻訳 `.ts` |
| 保証範囲の説明・テスト | `docs/dev/tsumeshogi-generator.md`、`tests/`、`tests/CMakeLists.txt` |

Hayanagiに変更が必要なら、独立リポジトリ `/home/nakada/GitHub/Hayanagi` で管理・検証してからShogiBoardQのサブモジュール参照を更新する。今回その変更は行っていない。

`TsumeGameSession` は既存問題の着手を残り手数で評価する機能である。第2問 `R*9g` を受け入れる既存の動作と `alternativeAndRefutation()` は維持する。生成基準を厳しくすることを理由に、対局を参考PVとの文字列一致で採点する方式へ変更しない。第4問 `S*4b` の「詰まない」と「5手では足りない」の区別も維持する。

## 回帰テスト案

現行 `tests/tst_tsumeshogi_generator.cpp` は候補生成器と `TsumePositionUtil` のテストであり、`TsumeshogiGenerator` 自体の採択・トリミング・flushの経路を検査していない。別途オーケストレータと検証サービスのテストが必要である。

| ケース | 検証する動作 |
|---|---|
| 第2問の5手PVを返す | 3初手を検出し、通常生成結果に採択しない。5手PVが正しくても落とす |
| 第4問の5手PVを返す | `S*3b` と `R*4b` を検出して棄却 |
| 第4問 `S*3b 4a4b` 後を3手の検証対象にする | `6d5c`・`R*4a`・`R*4c` を検出。初手だけを調べる実装への退行を防ぐ |
| 各別初手後の玉方全応手 | 全応手で詰むことを確認。1本のPVの合法性検査だけでは合格にしない |
| 第4問 `S*4b` | 深さ4の `depthlimit` と深さ6の `mate` を区別。5手以内の別解なしを無制限の保証に変換しない |
| 第4問の最終手2候補 | 途中余詰とは別の分類となり、最終手許容方針に従う |
| 成・不成／第1問の早く詰む変化 | `+` の違いを保持し、変化別詰と主変化の余詰を区別する |
| トリミング後だけ検査不合格となるモック | 除去前の合格結果を流用しない。除去却下または最終候補棄却 |
| 検証中のstop・エラー・無応答 | `flushTrimmingResult()` を含む全経路で未検証局面を出力しない |
| timeout・cancelled・深さ上限・旧世代応答 | 判定不能または破棄。唯一解合格・発見数増加にしない |
| 重複SFEN | 唯一性の検査とは独立して、合格済み局面の二重出力を防ぐ |
| 対局機能の既存例 | `R*9g` の受理、`S*4b` の手数超過、取消・再探索を維持 |

唯一解の正例には、第1・3・5問を流用しない。検証方針の範囲内で全木を独立に確かめた小さな専用局面を用意する。単一PVや「最初の詰手だけを返す」モックを唯一性の根拠にしない。

## 再実行方法と保存記録

リポジトリ直下から実行する。サブモジュールは初期化済みだったため更新操作は不要だった。

```bash
cmake -B build -S . -DBUILD_TESTING=ON
cmake --build build -j 4
ctest --test-dir build --output-on-failure -R 'tst_tsume_play|tst_tsumeshogi_generator'
python3 Hayanagi/tests/test_engine.py build/Hayanagi/hayanagi

python3 -m pip install --target /tmp/tsumeshogi-yodzume-deps python-shogi==1.1.1
env PYTHONPATH=/tmp/tsumeshogi-yodzume-deps python3 scripts/investigate_tsumeshogi_yodzume.py \
  --output /tmp/tsumeshogi-yodzume-results.json
env PYTHONPATH=/tmp/tsumeshogi-yodzume-deps python3 scripts/investigate_komoring_yodzume.py \
  --engine /home/nakada/shogi/KomoringHeights-kh-v1.1.0/source/KomoringHeights-by-gcc \
  --output /tmp/tsumeshogi-komoring-results.json

# 保存済み証明木の独立再検証。エンジンは起動しない。
env PYTHONPATH=/tmp/tsumeshogi-yodzume-deps python3 scripts/investigate_tsumeshogi_yodzume.py \
  --verify-proof docs/dev/tsumeshogi-yodzume-evidence/hayanagi-proof.json.gz
```

Hayanagiの手動再現は以下。`usiok`、`readyok`、`tsume ...` をそれぞれ待ち、探索中に次の `position` や `quit` を送らない。

```text
usi
setoption name TsumeMode value true
setoption name USI_OwnBook value false
setoption name GenerateAllLegalMoves value true
isready
position sfen 9/9/9/R8/9/9/1k2+S4/9/3+N5 b RG2b3g3s3n4l18p 1 moves R*9g
go tsume defense depth 4 movetime 10000
```

期待する応答は `tsume mate ... plies 4 ...`。KomoringHeightsではTsumeModeを設定せず、前述のオプションで `isready` を済ませ、同じ `position` に `go mate 5000` を送る。`checkmate` の後の4手が残り手順となる。

| 保存先 | 内容 |
|---|---|
| [investigate_tsumeshogi_yodzume.py](../../scripts/investigate_tsumeshogi_yodzume.py) | Hayanagiの全初手・途中手列挙、独立短手数探索、証明木照合 |
| [investigate_komoring_yodzume.py](../../scripts/investigate_komoring_yodzume.py) | 指定版の14件のUSI再現、PV合法性確認 |
| [summary.json](tsumeshogi-yodzume-evidence/summary.json) | 版・ハッシュ・現在設定・5問の結果・全初手応答 |
| [hayanagi-proof.json.gz](tsumeshogi-yodzume-evidence/hayanagi-proof.json.gz) | 全3,189問い合わせ、755局面の証明木、合法手集合を含むJSONのgzip版 |
| [komoring.json](tsumeshogi-yodzume-evidence/komoring.json) | KomoringHeightsの設定・ハンドシェイク・全14件の送信局面と応答 |
| [validation.txt](tsumeshogi-yodzume-evidence/validation.txt) | ビルド・既存テスト・再現実行の結果 |

Pythonは3.14.7、python-shogiは1.1.1。使用した実行ファイル等のSHA-256：

```text
Hayanagi:
955a8eff734a77eade46f652bcb3396c48538ae51906948f83034cacd65a26b5
KomoringHeights:
230c5bbde4d792a8a10f4d03e6f180d1b90e775107d37a877fd2bb5343c74dba
python-shogi shogi/__init__.py:
58eee28dd9c47b2728e1d877812fcde75b890623a8ceb8bec19360a7582f6f89
```

実行結果：ビルド成功、ビルドログ中の警告・エラー0。対象Qtテスト2/2成功。HayanagiのPythonテスト9件中8件成功、旧版比較1件は `--baseline` 未指定のためskip。全テストスイートは実行していない。再現プログラムの全検査および保存後の755証明局面の再検証も成功した。
