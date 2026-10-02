# アプリケーションアイコン

| ファイル | 用途 |
| --- | --- |
| `shogiboardq.png` | 元画像。macOS等のウィンドウとバージョン情報ダイアログで使用 |
| `shogiboardq.icns` | macOSアプリケーションバンドル用 |
| `linux/shogiboardq.png` | Linux用512×512px。控えめな角丸と透明背景。絵柄の幅は画像の約95%（元画像は約80%） |
| `windows/shogiboardq.png` | Windows用512×512px。外側の余白なし。高DPI表示とICOの生成元 |
| `shogiboardq.ico` | Windows実行ファイルとウィンドウ用。16/20/24/32/40/48/64/96/128/256pxを収録 |

Linux用はQtリソース、CMakeのhicolor/512x512へのインストール、AppImageで共用します。
KDEパネルの小さなアイコンでも図柄が小さく見えないよう、Linux用の透明な余白を
左右それぞれ約2〜3%に抑えています。角の外側も透明で、木目・黒いQ・「将」を維持しています。
Windows用はICOに加えて512pxのPNGもQIconに登録します。
macOS用のPNG・ICNSは今回変更していません。

Linux・Windows用PNGは元画像を参照して内蔵 image_gen で再生成し、
ImageMagickのLanczosフィルターで512×512pxに縮小しています。
ICOはリポジトリのルートで次のコマンドを実行して再生成できます。

```sh
magick resources/icons/windows/shogiboardq.png \\
  -define icon:auto-resize=256,128,96,64,48,40,32,24,20,16 \\
  resources/icons/shogiboardq.ico
```

## Linux用の生成プロンプト

```text
Use case: precise-object-edit
Asset type: ShogiBoardQ Linux desktop and KDE taskbar application icon, transparent PNG.
Input image 1 is the EDIT TARGET: the original production ShogiBoardQ icon supplied by the user.
Primary request: Keep this SAME logo and artwork, enlarge the wooden square to remove almost all empty outer padding, and gently round only the four outside corners.
Composition/framing: Square 1024x1024 canvas. Wooden board centered from approximately x=12 to x=1012 and y=12 to y=1012, occupying 97.6% of the canvas width and height. Just 12 pixels of transparent margin on each side. Corner radius approximately 55 pixels (subtle rounding, NOT a circle or squircle). Preserve the entire design inside the board.
Invariants: Keep the straight-on honey-gold wooden board, fine wood grain, original thin dark grid, large black capital Q with its elegant long tail, and exact central black Japanese character "将" in the same calligraphic style, stroke shapes, proportions, and relative placement as the input. The black Q and 将 must remain clearly legible at 32x32 pixels. This is a padding and outer-corner edit, not a new logo design.
Background: genuine transparent alpha outside the rounded wooden board, including the four corner cutouts.
Avoid: checkerboard, solid background, shadows, glow, bevel, 3D perspective, new colors, extra strokes, additional letters, watermark, mockup. Keep the logo itself sharp and unchanged; round only the outside wood silhouette.
Return a single final transparent icon.
```

## Windows用の生成プロンプト

```text
Use case: precise-object-edit
Asset type: ShogiBoardQ Windows application icon master, square PNG.
Input image 1 is the original production icon and the edit target.
Primary request: Preserve this existing ShogiBoardQ logo design and recreate it to completely FILL THE ENTIRE SQUARE CANVAS. Remove ALL exterior blank/transparent margins. The finished PNG must be opaque wood and logo from edge to edge, with absolutely ZERO padding.
Composition/framing: Exact square 1024x1024, straight-on square golden wooden board. The four wooden board edges coincide exactly with the four image/canvas edges. Cropping away the transparent exterior is the only intended change; keep the original entire board and internal composition. No inset square, no empty framing around the wood.
Invariants: Keep the original honey-gold wooden board grain and thin dark grid lines, the large black elegant capital Q ring and tail, and exact black calligraphic Japanese character "将" centered within it. Preserve recognizable original shape, character strokes, colors and overall design.
Constraints: edge-to-edge/full-bleed square icon, NO transparent margins, NO white margins, NO black margins, no rounding, shadow, new text, ornaments, or mockup. Optimize crisp edges for small Windows taskbar display. Return only the finished icon.
```
