# 調査依頼：詰将棋局面生成機能が余詰のある局面を出力する原因

ShogiBoardQの詰将棋局面生成機能で作成した5手詰の局面集に、異なる攻め方でも詰む局面が含まれていました。以下の具体例を再現し、生成機能が余詰を排除できなかった原因を、ソースコードと実験結果に基づいて調査してください。

この依頼は、過去の会話を参照できない状態でも実行できるようにまとめています。作業範囲は、原因調査・再現検証・修正方針の提示です。検証に必要なスクリプトや調査報告は作成して構いません。本体機能の修正、コミット、pushは今回の依頼には含みません。

## 作業環境と対象ファイル

- ShogiBoardQ：`/home/nakada/GitHub/ShogiBoardQ`
- Hayanagiの独立した開発用リポジトリ：`/home/nakada/GitHub/Hayanagi`
- ShogiBoardQ内のサブモジュール：`/home/nakada/GitHub/ShogiBoardQ/Hayanagi`
- 調査前に各リポジトリの状態と適用される `AGENTS.md` を確認し、既存の変更を保全してください。説明・報告は日本語でお願いします。

問題が見つかった元ファイルは次の2つです。いずれもShogiBoardQの詰将棋局面生成機能から作成しました。

- `/home/nakada/Desktop/5手詰.txt`：5局面のSFENのみ。
- `/home/nakada/Desktop/5手詰2.txt`：同じ5局面に `moves ...` の参考手順を付加したもの。

2ファイルは別々の10問ではなく、同一の5問です。リポジトリにも相当するデータがあります。

- `tests/fixtures/tsume_positions.sfen`
- `tests/fixtures/tsume_positions_with_moves.sfen`

元ファイルとfixtureの一致を確認してください。Desktopのファイルがなくても、fixtureと本依頼のSFENを使って再現を進められます。

## これまでに確認したこと

- Hayanagiによる検証では、5問すべての最短詰み手数は5手でした。
- 第2問は、5手以内で詰ませられる初手が `G*9g`、`R*9g`、`R*8h` の3通りありました。
- 第4問は、5手以内で詰ませられる初手が `S*3b`、`R*4b` の2通りありました。
- 第1・3・5問について、全変化を含めた余詰なしの証明は行っていません。これらを無条件に「完全な詰将棋」と扱わないでください。
- 過去の確認は主にHayanagiを用いており、独立した別エンジンによる余詰検証は未実施です。
- 第2問の `R*9g` は詰将棋対局機能の「参考手順と違っても詰む手を受け入れる」テストとして既に使用されています。対局でその手を正解とすることと、生成時に余詰のある問題を採択してよいかは別の要件です。

以下の別解は代表的な応手を示す一手順です。再検証では、この一手順が合法で詰むことに加え、別解の初手以降、玉方が別の合法応手を選んでも攻め方に詰ませる手があることを確認してください。

## 再現例1：第2問

両ファイルの2行目です。

```text
9/9/9/R8/9/9/1k2+S4/9/3+N5 b RG2b3g3s3n4l18p 1
```

参考手順：

```text
G*9g 8g8h R*8g 8h9i 9g9h
```

▲９七金打 → △８八玉 → ▲８七飛打 → △９九玉 → ▲９八金、の5手です。

確認された別解の代表手順：

```text
R*9g 8g8f 9g9f 8f8e G*8f
R*8h 8g7f G*6f 7f7g 6i7h
```

- ▲９七飛打 → △８六玉 → ▲９六飛 → △８五玉 → ▲８六金打。
- ▲８八飛打 → △７六玉 → ▲６六金打 → △７七玉 → ▲７八成桂。

参考手順の初手と異なる飛車打ちからも5手で詰むため、少なくとも初手の一意性が失われています。

## 再現例2：第4問

両ファイルの4行目です。

```text
5k3/9/9/3+P1B1N1/9/9/9/9/9 b RSrb4g3s3n4l17p 1
```

参考手順：

```text
S*3b 4a4b R*4a 4b5b 4d5c+
```

▲３二銀打 → △４二玉 → ▲４一飛打 → △５二玉 → ▲５三角成、の5手です。

確認された別解の代表手順：

```text
R*4b 4a5a 4b6b+ 5a4a 2d3b+
```

▲４二飛打 → △５一玉 → ▲６二飛成 → △４一玉 → ▲３二桂成、の5手です。

さらに、参考手順の `S*3b 4a4b` の後にも、残り3手で詰む攻め手として `6d5c`、`R*4a`、`R*4c` が見つかっています。初手だけを調べる検査で十分かを検討する材料にしてください。

また、既存テストでは初手 `S*4b` について、5手以内では詰まないが7手以内では詰むことも確認されています。作意より長い詰め手順をどう検査・分類するかも調査対象です。

## 現時点のコード上の所見

以下は調査の出発点です。現在のコードを読み直し、確定した事実と仮説を分けて報告してください。

- `src/analysis/tsumeshogigenerator.cpp` の `TsumeshogiGenerator::onCheckmateSolved()` は、エンジンが返したPVの長さを `pv.size() != m_settings.targetMoves` で検査しています。確認時点では、別の詰む攻め手が存在するかの検査はありませんでした。
- `generateAndSendNext()` と `sendTrimmingCheck()` は、選択された外部USIエンジンへ `go mate` を送信します。生成機能が当時使ったエンジンの種類・設定は未確認です。詰将棋対局でHayanagiを使っていることから、生成にもHayanagiを使ったと推測しないでください。
- 不要駒トリミングでは、駒を除去した後も同じ手数のPVが返ると除去を採用しています。唯一解が維持されるかを別途検証しているか確認してください。
- `registerFoundPosition()` のSFEN重複排除は、同一局面の二重出力を防ぐ処理です。1局面に複数の詰め方があることの検査とは区別してください。
- `finishTrimmingPhase()` による通常出力に加え、停止・エラー等の際の `flushTrimmingResult()` にも出力経路があります。将来検査を加える場合、未検証局面が別経路から出力されないか確認してください。
- `docs/dev/tsumeshogi-generator.md` は、返却PVの長さによる採択と、使用エンジン側の最短性保証への依存を説明しています。最短性の保証と唯一解の保証を分けて評価してください。

主な参照先：

- `src/analysis/tsumeshogigenerator.{h,cpp}`
- `src/analysis/tsumeshogigenerator_sfen.cpp`
- `src/analysis/tsumeshogipositiongenerator.{h,cpp}`
- `src/dialogs/tsumeshogigeneratordialog*.cpp`
- `docs/dev/tsumeshogi-generator.md`
- `tests/tst_tsumeshogi_generator.cpp`
- `tests/tst_tsume_play.cpp` の `alternativeAndRefutation()`
- `Hayanagi/src/tsume.{h,cpp}` と `Hayanagi/src/position.{h,cpp}`
- `Hayanagi/tests/test_engine.py`

## 調査・再現検証で確認する事項

1. 5問の最短詰み手数を再確認し、第2・4問の複数初手による強制詰みを再現してください。可能なら別エンジンや独立した検証手段でも照合し、使用したエンジン・コミット・設定を記録してください。
2. 初手だけでなく、攻め方の途中の着手にも複数の詰む選択肢があるか調べてください。玉方が複数の応手を選べること自体を余詰と判定しないでください。
3. 余詰、変化別詰、最終手の複数解、成・不成の違い、作意より長い別解の扱いを、信頼できる資料を参照して整理してください。どこまで排除する仕様にするか、判断が必要な点を明示してください。
4. 「5手以内の別解が見つからない」ことだけで、7手以上を含む余詰なしと断定しないでください。探索深さの上限・時間切れ・中止は、余詰なしの証明と区別してください。
5. 原因が生成側の検査不足、外部エンジンの応答仕様・設定、トリミング、あるいは検証側の不具合のどこにあるかを切り分けてください。特定のエンジンの不具合と先に決めつけないでください。
6. 修正案では、採択前の検査箇所、トリミング後の再検査、停止時の出力、探索時間・UI応答性、判定不能時の扱い、既存の詰将棋対局への影響を検討してください。実装変更が必要な責務とファイルを挙げてください。
7. 第2・4問を使った回帰テスト案を提示してください。余詰検査が完了していない第1・3・5問を、唯一解の正例としてそのまま採用しないでください。

## 再現手段とリポジトリ管理

必要に応じて、次のコマンドでビルド・既存テストを確認できます。

```bash
cd /home/nakada/GitHub/ShogiBoardQ
git submodule update --init --recursive
cmake -B build -S . -DBUILD_TESTING=ON
cmake --build build -j 4
ctest --test-dir build --output-on-failure -R 'tst_tsume_play|tst_tsumeshogi_generator'
python3 Hayanagi/tests/test_engine.py build/Hayanagi/hayanagi
```

Hayanagi単独エンジンは `build/Hayanagi/hayanagi` です。独自USI拡張の使用例：

```text
usi
setoption name TsumeMode value true
setoption name USI_OwnBook value false
isready
position sfen 9/9/9/R8/9/9/1k2+S4/9/3+N5 b RG2b3g3s3n4l18p 1 moves R*9g
go tsume defense depth 4 movetime 10000
```

`usiok`、`readyok`、`tsume ...` の応答を順に待ってください。探索は非同期なので、結果を受け取る前に次の `position` や `quit` を送ると探索が破棄されます。この例では、初手の後に残り4手で詰むことを示す `tsume mate ... plies 4 ...` が期待されます。

調査時点のShogiBoardQはHayanagiの `30cfce58e0a863f9f2d2144635f1d9e9d4e3e54f` を参照しています。実際の参照コミットは `git submodule status` で確認してください。将来Hayanagiのソース修正を行う場合は独立リポジトリ `/home/nakada/GitHub/Hayanagi` で管理し、ShogiBoardQ側では検証済みコミットへの参照を更新する方針です。

## 期待する成果物

`docs/dev/tsumeshogi-yodzume-investigation.md` に調査結果をまとめてください。

- 結論と根本原因。コード上の関数・該当箇所を示すこと。
- 5問ごとの検証結果と検証範囲。最短手数、確認した別解、未判定事項を分けること。
- 第2・4問の再現手順と結果。特定の玉方応手に協力してもらっただけの手順ではないことを示すこと。
- 余詰を排除するための修正方針と回帰テスト案。
- 使用したコマンド・検証プログラム・エンジンの情報、および実行したテスト結果。

再現用コードを作成した場合は、後から同じ確認ができるよう保存先と実行方法を記載してください。最終回答では、判明した原因、未解決事項、推奨する修正範囲を日本語で簡潔に説明してください。
