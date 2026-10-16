# 棋譜ファイル読み込み・出力・クリップボード機能

棋譜の形式は `KifuFormat`（`src/kifu/kifuformat.h`）の1つの enum で表し、読み込み・保存・変換で共通に使う。形式の判定と名前は `KifuFormats::detect()`（テキストの内容）・`detectFile()`（拡張子と内容）・`fromName()`・`name()`・`isSavable()` が受け持つ。

## 1. ファイル読み込み

### メニュー

- **ファイル(F) → 開く…** (`actionOpenKifuFile`)

### 対応フォーマット

| フォーマット | 拡張子 | 変換クラス |
|---|---|---|
| KIF | `.kif`, `.kifu` | `KifToSfenConverter` |
| KI2 | `.ki2`, `.ki2u` | `Ki2ToSfenConverter` |
| CSA | `.csa` | `CsaToSfenConverter` |
| JKF | `.jkf`（`.json` は内容で判定） | `JkfToSfenConverter` |
| USI | `.usi`, `.sfen` | `UsiToSfenConverter` |
| USEN | `.usen` | `UsenToSfenConverter` |

拡張子で形式が決まらないファイル（`.json` や未知の拡張子）は、`KifuFormats::detectFile()` がファイルの内容から判定する（棋譜の形式と分からなければ KIF とみなす）。指し手のない USI・SFEN は局面だけとして読む。

### 読めない手・指せない手・パス

どの形式も同じ扱いにする。扱いは `ParseOptions`（`src/kifu/kifparsetypes.h`）の1か所で決め、変換器は読み込み中の `ParseReport` に記録する（`rejectMove()`・`rejectPass()`）。
読み込み（`KifuLoadParser`）は既定の `UnreadableMove::TruncateLine` で読む（`FailRecord` にすると、棋譜全体を読めなかったことにする）。

- 表記が読めない手、局面に合わない手、手番と違う符号の手（CSA）、指せない手があれば、その手順はその手の手前で打ち切り、
  読み込みは成功として続ける。警告に理由と「何手目まで読んだか」（`The record was read up to move N.`。`ParseReport::truncationNotice()`）を書き、画面には警告として出す。
  指せない手の理由は全形式で同じ文面にする（`KifLineBuilder::cannotRestore()`。例: `KIF: Cannot restore move 3: Source square is empty: 7g7e`）。
- パスの手（KIF・KI2 の「パス」）も、対応していない手として同じに打ち切る（`ParseReport::rejectPass()`）。
- 変化の手順も同じ。変化の最初の手から読めなければ、その変化は手のない変化になる（本譜は残す）。
- 開始局面が読めないときだけ、読み込みを失敗にする。

### ファイルダイアログのフィルタ

```
Kifu Files (*.kif *.kifu *.ki2 *.ki2u *.csa *.jkf *.json *.usi *.sfen *.usen)
KIF Files (*.kif *.kifu)
KI2 Files (*.ki2 *.ki2u)
CSA Files (*.csa)
JKF Files (*.jkf *.json)
USI/SFEN Files (*.usi *.sfen)
USEN Files (*.usen)
```

### 主要クラス

- **`KifuFileController`** (`src/ui/coordinators/kifufilecontroller.h/.cpp`) — メニューからの読み込みの窓口
  - `chooseAndLoadKifuFile()` — ファイルを選び、未保存の棋譜を確かめてから読み込む（読み込み中は進捗ダイアログを出し、キャンセルできる）
  - `onSfenCollectionPositionSelected(sfen)` — SFEN局面集で選んだ局面を反映する
  - 読み込みの後始末（上書き保存先と未保存の状態）は、ダイアログからの非同期の読み込みも自動化 API の同期の読み込みも `finishLoad(success, source, path)` 1つで行う。読み込んだものの出どころ（`Source`）で決まる：棋譜ファイル（`File`）は上書き保存先にして変更なしとし、貼り付けた棋譜（`Text`）と自動化 API で反映した局面（`Position`）は未保存にし、局面集から反映した局面（`Collection`）は変更なしで上書き保存先を持たない（自動化 API のテキストの読み込み `loadKifuText()` は、貼り付けの後始末のうちダイアログとステータスバーを除いた `KifuDocument::markUnsavedContent()` だけを直接呼ぶ）
- **`KifuLoadCoordinator`** (`src/ui/coordinators/kifuloadcoordinator.h/.cpp`) — 読み込んだ結果を棋譜の文書（`KifuDocument`）に載せる
  - `loadFileAsync(filePath)` / `loadTextAsync(content)` — ワーカーで `KifuLoadParser` を呼び、終わったら `loadFinished` を出す
  - `cancelLoad()` — 読み込み中の処理を捨てる
  - `loadFile(filePath)` / `loadText(content)` — 同期版（自動化 API・テスト）
  - `loadPositionFromSfen(sfen)` — SFEN局面を開始局面にする
  - 棋譜は `KifuDocument::replaceWithRecord(record, startSfen, header)`、局面だけのもの（SFEN・BOD・指し手のない USI）は `KifuDocument::replaceWithPosition(sfen)` で文書を丸ごと置き換える。置き換えると前の棋譜の対局者・持ち時間・上書き保存先は消え、対局情報は文書の `headerReset` で対局情報欄と盤の対局者名に届く
- **`KifuLoadParser`** (`src/kifu/kifuloadparser.h/.cpp`) — GUI を含まない読み込み口（結果は値 `KifuLoadResult`）
  - `parseFile(path)` — ファイルを1回だけ読み（`readFileText()`：CSA は CSA の文字コード宣言、それ以外は自動判別）、`parseContent()` に渡す
  - `parseContent(content, format)` — `KifuFormat` から形式ごとの `parse()` を表（`parserFor()`）で選んで呼び、結果（`KifuParseOutcome`）を `KifuLoadResult` に写す。指し手のない USI・SFEN と BOD は局面だけを `positionOnly` に返す
- 各形式の変換器（`src/kifu/formats/*tosfenconverter`）はテキストを読むだけで、ファイルは読まない。形式ごとに1つの関数 `parse(text, options)` だけを持ち、1回の解析で棋譜（本譜・変化）・開始局面・手合割の名前・対局情報・警告を `KifuParseOutcome` に入れて返す（`KifToSfenConverter::parse` / `Ki2ToSfenConverter::parse` / `CsaToSfenConverter::parse` / `JkfToSfenConverter::parse` / `UsenToSfenConverter::parse` / `UsiToSfenConverter::parse`。BOD は `KifTextParser::parseBod`）
  - KIF・KI2 は `KifTextParser::parse(text, Dialect::Kif / Dialect::Ki2)` が同じ処理で読む（行の分類・「変化：N手」の区切り・コメント・しおり・開始局面・指し手の文法 `KifMoveGrammar`）。KIF の手は「KI2 の手＋移動元 `(xy)`」として読み、移動元が書いてなければ局面の合法手から決める
  - どの形式も、手順は `KifLineBuilder` で組み立てる。手を読んだその場で局面を進めて棋譜の行（`RecordEntry`）を作り、棋譜欄の表記は原文を写さずに指し手から `KifMoveText::format()` で作る（書かれていた修飾語・「不成」は残す）

---

## 2. ファイル出力（保存）

### メニュー

- **ファイル(F) → 上書き保存** (`actionSave`)
- **ファイル(F) → 名前を付けて保存…** (`actionSaveAs`)
- **ファイル(F) → 将棋盤の画像をファイルに保存…** (`actionSaveBoardImage`)
- **ファイル(F) → 評価値グラフの画像をファイルに保存…** (`actionSaveEvaluationGraph`)

### 対応フォーマット（名前を付けて保存）

| フォーマット | 拡張子 | 保存ダイアログでの表示 |
|---|---|---|
| KIF | `.kifu`, `.kif` | KIF形式 UTF-8 (*.kifu) / KIF形式 Shift_JIS (*.kif) |
| KI2 | `.ki2u`, `.ki2` | KI2形式 UTF-8 (*.ki2u) / KI2形式 Shift_JIS (*.ki2) |
| CSA | `.csa` | CSA形式 (*.csa) |
| JKF | `.jkf` | JKF形式 (*.jkf) |
| USEN | `.usen` | USEN形式 (*.usen) |
| USI | `.usi` | USI形式 (*.usi) |

保存形式は拡張子で決まる（`KifuSaveFile::saveFormatForPath()`）。分岐・消費時間・コメントを書けない形式へ保存するときは、失われる情報を示して続けるかを確かめる。

### 画像出力

| 対象 | メニュー | 処理クラス |
|---|---|---|
| 盤面画像 | ファイル(F) → 将棋盤の画像をファイルに保存… | `ImageExportController::saveBoardImage()` → `BoardImageExporter::saveImage()` |
| 評価値グラフ | ファイル(F) → 評価値グラフの画像をファイルに保存… | `ImageExportController::saveEvalGraphImage()` → `BoardImageExporter::saveImage()` |

### 主要クラス

- **`KifuFileController`** (`src/ui/coordinators/kifufilecontroller.h/.cpp`) — メニューからの保存の窓口
  - `saveKifuToFile()` — 名前を付けて保存（既定のファイル名は文書の対局者と日時から作る）。保存したら `KifuDocument::markSaved(path)` で今の内容を保存済みにし、保存形式を拡張子で決められるファイルなら上書き保存先（`KifuDocument::filePath()`）にする
  - `overwriteKifuFile()` — 上書き保存先へ、拡張子で決めた形式で書く。上書き保存先が無ければ名前を付けて保存する
  - `autoSaveKifu(saveDir, extension)` — 対局の終局時の自動保存（ローカル対局・通信対局とも `GameSession::finishMatchRecord()` から。保存先と形式は `MatchAutoSave`。ダイアログなし。名前は対局者と日時から作り、同じ名前があれば連番を付ける）
  - `saveKifuToPath(filePath, errorText)` / `exportLines(format)` — 自動化 API 用（ダイアログを出さない）
- **`KifuDocument`** (`src/kifu/kifudocument.h/.cpp`) — 開いている棋譜の文書。分岐ツリー・対局情報・対局の記録（対局者・持ち時間・開始と終了の日時）・上書き保存先を持つ
  - `isModified()` — 未保存かどうか。ツリーの revision・コメントとしおりの変更・対局情報が、最後に保存（または読み込み）した時点と違うか、貼り付けた棋譜などファイルにない内容を読み込んだかで求める（変更のたびに印を付けない）
  - `markSaved(path)` / `markUnsavedContent()` — 保存した・ファイルから読み込んだ／ファイルにない内容を読み込んだ
- **`KifuSaveCoordinator`** (`src/ui/coordinators/kifusavecoordinator.h/.cpp`) — 保存で利用者に聞くこと（名前空間）
  - `saveViaDialog()` — 保存ダイアログで全6形式（KIF/KI2/CSA/JKF/USEN/USI）から選ばせる
  - `overwriteExisting()` — `.kif`/`.ki2` に Shift_JIS で表せない文字があれば UTF-8 で保存するか確かめる
  - `confirmDiscardUnsaved()` — 未保存の変更の保存・破棄・キャンセルを確かめる
- **`KifuSaveFile`** (`src/kifu/kifusavefile.h/.cpp`) — 保存形式の判定と書き出し（文字コードの選択を含む）
- **`KifuExport`** (`src/kifu/kifuexport.h/.cpp`) — 形式ごとの書き出し関数の表（名前空間）
  - `lines(document, format)` — 文書を `format` の行にする。`metadataOf(document)` で書き出しの文脈（`KifuExportMetadata`：対局情報・開始局面・対局者・持ち時間・開始と終了の日時）を作り、`lines(tree, metadata, format)` に渡す
  - 表の各関数は `src/kifu/formats/*exporter` の `exportLines(const KifuBranchTree&, const KifuExportMetadata&)`（`KifExporter`・`Ki2Exporter`・`CsaExporter`・`JkfExporter`・`UsenExporter`・`UsiExporter`）。SFEN・BOD は棋譜を書けないので KIF にする
  - GUI の保存・コピーと CLI の変換（`KifuConversionService`）が同じ表を使う
- **`BoardImageExporter`** (`src/board/boardimageexporter.h/.cpp`) — 盤面・グラフの画像保存

---

## 3. クリップボードコピー

### メニュー

- **編集(E) → 棋譜コピー** （サブメニュー）
  - KIF形式 (`actionCopyKIF`)
  - KI2形式 (`actionCopyKI2`)
  - CSA形式 (`actionCopyCSA`)
  - USI形式（現在の指し手まで）(`actionCopyUSICurrent`)
  - USI形式（全て）(`actionCopyUSIAll`)
  - JSON棋譜フォーマット (`actionCopyJKF`)
  - USEN (`actionCopyUSEN`)
- **編集(E) → 局面コピー** （サブメニュー）
  - SFEN形式 (`actionCopySFEN`)
  - BOD形式 (`actionCopyBOD`)
- **編集(E) → 将棋盤画像コピー** (`actionCopyBoardToClipboard`)
- **編集(E) → 評価値グラフ画像コピー** (`actionCopyEvalGraphToClipboard`)

### 棋譜コピー

| フォーマット | 内容 | メソッド |
|---|---|---|
| KIF | 棋譜全体をKIF形式でコピー | `KifuFileController::copyKifToClipboard()` |
| KI2 | 棋譜全体をKI2形式でコピー | `KifuFileController::copyKi2ToClipboard()` |
| CSA | 棋譜全体をCSA形式でコピー（分岐は書けないので本譜のみ） | `KifuFileController::copyCsaToClipboard()` |
| USI（現在の指し手まで） | 表示中の手順の、表示中の手までをUSI形式でコピー | `KifuFileController::copyUsiCurrentToClipboard()` |
| USI（全て） | 棋譜全体をUSI形式でコピー（分岐は書けないので本譜のみ） | `KifuFileController::copyUsiToClipboard()` |
| JKF | 棋譜全体をJKF形式でコピー | `KifuFileController::copyJkfToClipboard()` |
| USEN | 棋譜全体をUSEN形式でコピー | `KifuFileController::copyUsenToClipboard()` |

棋譜のコピーは保存と同じ `KifuFileController::exportLines()`（`KifuExport::lines(document, format)`）で行を作り、クリップボードに書く。

### 局面コピー

| フォーマット | 内容 | メソッド |
|---|---|---|
| SFEN | 盤の局面をSFEN文字列でコピー | `KifuFileController::copySfenToClipboard()` |
| BOD | 盤の局面をBOD（テキスト盤面図）でコピー（`BodTextGenerator`）。駒落ちの棋譜は KIF の局面図と同じく持駒・手番を下手・上手で書く（対局情報の見出しが先手・後手ならそれに合わせる） | `KifuFileController::copyBodToClipboard()` |

### 画像コピー

| 内容 | メソッド |
|---|---|
| 盤面を画像としてクリップボードにコピー | `ImageExportController::copyBoardToClipboard()` → `BoardImageExporter::copyToClipboard()` |
| 評価値グラフを画像としてクリップボードにコピー | `ImageExportController::copyEvalGraphToClipboard()` → `BoardImageExporter::copyToClipboard()` |

### 主要クラス

- **`KifuFileController`** (`src/ui/coordinators/kifufilecontroller.h/.cpp`) — 棋譜・局面のコピー（`KifuExport` で作った行・盤の局面をクリップボードに書き、結果を `statusMessage` でステータスバーに出す）
- **`ImageExportController`** (`src/ui/controllers/imageexportcontroller.h/.cpp`) — 画像の保存・コピーの窓口
- **`BoardImageExporter`** (`src/board/boardimageexporter.h/.cpp`) — 盤面・グラフ画像のクリップボードコピー

---

## 4. クリップボード貼り付け

### メニュー

- **編集(E) → 棋譜貼り付け…** (`actionPasteKifu`)

`KifuFileController::pasteKifuFromClipboard()` が `KifuPasteDialog` を開く。ダイアログでクリップボードの内容を貼り付けて「取り込む」を押すと、`importRequested` を受けた `KifuFileController::onKifuPasteImportRequested()` が未保存の棋譜を確かめてから `KifuLoadCoordinator::loadTextAsync()` で読み込む。貼り付けた棋譜はファイル由来ではないので、上書き保存先を持たず未保存とする（`KifuDocument::markUnsavedContent()`）。

### 自動判定対応フォーマット

貼り付けたテキストの形式は `KifuFormats::detect()` が次の順に判定する（`KifuLoadParser::parseText()` から呼ぶ）。

| フォーマット | 判定基準 |
|---|---|
| SFEN | テキスト全体が SFEN 局面（盤・手番・持駒・手数）だけ |
| JKF | `{` または `[` で始まるJSON形式 |
| USI | `position`、`startpos`、`sfen ` または SFEN の盤で始まる行を含む |
| USEN | テキスト全体が USEN の構造（`~` で区切った指し手列） |
| CSA | `V2`/`V3`、`PI`、`P1`〜`P9`、`N+`/`N-`、`$EVENT:` などのヘッダ行、`'` で始まる、または `+`/`-` と数字で始まる指し手行を含む |
| KIF | `手数----` の見出し、または数字で始まる指し手行を含む |
| KI2 | `▲`・`△` などで始まる指し手行を含む |
| BOD | 持駒の行（`先手の持駒` など）、盤の枠（`+---+`）、`|v`・`| ・` を含む |

どれにも当たらなければ KIF として読む。SFEN と BOD、指し手のない USI は局面だけとして読む。

### 主要クラス

- **`KifuPasteDialog`** (`src/dialogs/kifupastedialog.h/.cpp`) — 貼り付けるテキストの入力（クリップボードからの貼り付けボタンを持つ）
- **`KifuFileController`** (`src/ui/coordinators/kifufilecontroller.h/.cpp`)
  - `pasteKifuFromClipboard()` — ダイアログを開く
  - `onKifuPasteImportRequested(content)` — 取り込む
- **`KifuLoadCoordinator`** (`src/ui/coordinators/kifuloadcoordinator.h/.cpp`)
  - `loadTextAsync(content)` — 自動判定した形式で読み込み
- **`KifuLoadParser`** (`src/kifu/kifuloadparser.h/.cpp`)
  - `parseText(content)` — `KifuFormats::detect()` で形式を判定し、`parseContent(content, format)` でテキストのまま解析する（一時ファイルは作らない）

---

## 5. クラス構成図

```
ファイル読み込み:
  MenuActionsWiring → KifuFileController → KifuLoadCoordinator → KifuLoadParser (ワーカー)
                                                                 → 各形式 Converter (KifToSfenConverter 等)
                                         KifuLoadCoordinator → KifuDocument (文書の置き換え。KifuBranchTree の作り直し)

ファイル保存:
  MenuActionsWiring → KifuFileController → KifuExport (各形式への変換) → 各形式 Exporter
                                         → KifuSaveCoordinator → KifuSaveFile
                                         → KifuDocument::markSaved (保存した時点・上書き保存先)

クリップボードコピー:
  MenuActionsWiring → KifuFileController → KifuExport (各形式への変換)
                                         → QClipboard

クリップボード貼り付け:
  MenuActionsWiring → KifuFileController → KifuPasteDialog (入力)
                                         → KifuLoadCoordinator → KifuLoadParser (形式判定と読み込み)

画像出力:
  MenuActionsWiring → ImageExportController → BoardImageExporter (保存/クリップボードコピー)
```
