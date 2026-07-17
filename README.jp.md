[English version](README.md)

# dd_cview

[`dd_molview`](../dd_molview) のデスクトップGUIシェルをネイティブC++/Qt6で
再実装したもの。蛋白質テーブル/リガンドテーブル/3Dビュー/シーケンスビューの
同じ4パネル構成のワークベンチを、ウィンドウ・ドック・テーブル・イベント
ハンドラすべてPySide6ではなくC++/Qtで書き直している。その下にある計算ロジック
（PDB/SDF解析、コンタクト/相互作用検出、スコアリング、RMSD、鎖ごとの配列抽出、
3Dシーン用HTML生成）は**再実装していない**——`dd_cview` は
[pybind11](https://github.com/pybind/pybind11) 経由でPythonインタプリタを
埋め込み、`dd_viewer`/`dd_molview` の既存の、無改変のPythonパッケージを
そのまま呼び出す。境界は1つの狭いJSON入出力モジュール
[`python/dd_cview_backend.py`](python/dd_cview_backend.py) だけに絞っている。

`dd_molview-desktop`（PySide6版）は機能としては完成しているが、インタラ
クティブな操作では体感できるほど動作が遅い——テーブル選択、設定トグル、
残基クリックのすべてが、本当に必要なRDKit/biopython呼び出しだけでなく、
すべてのQtイベントについてPython自身のQtバインディングとシングルスレッド
インタプリタを経由してしまう。`dd_cview` はそれらの呼び出し（実際の処理
時間の大半を占める、コンタクト検出・相互作用の幾何計算・3Dシーン用HTML
生成）はそのまま残し、それ以外——ウィジェット構築、ドックレイアウト、
テーブルモデル、イベント配線、シーケンスパネル、設定パネル、カメラ状態の
管理——をすべてネイティブQt側に移すことで、UIスレッドが不要なPython/Qt
バインディングの往復を待たされないようにしている。

## アーキテクチャ

```
┌─────────────────────────────── C++ / Qt6 ────────────────────────────────┐
│ MainWindow（ドック・メニュー・テーブル・配線）                            │
│  ├─ TableModel        (QAbstractTableModel、TableDataをラップ)           │
│  ├─ DisplaySettingsPanel                                                  │
│  ├─ SequencePanel      (QTextBrowser + クリック可能な残基/鎖リンク)       │
│  ├─ Viewer3D           (QWebEngineViewラッパー、カメラJS往復)            │
│  └─ PythonBridge  ───────────────────────┐                               │
└───────────────────────────────────────────┼──────────────────────────────┘
                                             │ pybind11::embed（JSON文字列のみ）
┌────────────────────────────────────────────┼──────────────────────────────┐
│ python/dd_cview_backend.py :: Session       ▼                             │
│  load_all / *_table_json / sequence_html / build_view_html / ...          │
└──────────────────────┬──────────────────────────────────────────────────┘
                        │ 素の関数呼び出し、無改変
                        ▼
        dd_viewer（解析・相互作用・スコアリング・シーンHTML）
        dd_molview（複数受容体/リガンドのコレクション・配列・ダッシュボード）
```

`PythonBridge`（`src/PythonBridge.h`/`.cpp`）が唯一 `<pybind11/embed.h>`
（つまり `Python.h`）をincludeするファイル——他のすべてのC++ファイルは
Qt/std標準型（`QString`、`QJsonArray`、`TableData`、`DisplaySettings` など）
しか見ず、`py::object` は一切見えない。これは実際のビルド上の障害も回避
している: `Python.h` と Qt の `qobjectdefs.h` はどちらも裸の `slots`
シンボルを定義するため、両方をincludeする翻訳単位はビルドが壊れる——
pybind11のヘッダを非`QObject`の`.cpp`ファイル1つに閉じ込めることで、
あちこちで `QT_NO_KEYWORDS` と戦う代わりにこの問題自体を回避している。

`Session`（Python側の対応物）は `dd_molview.desktop.main_window.MainWindow`
自身の状態とオーケストレーション（`receptor_entries`、`ligand_entries`、
`active_receptor_idx`、`active_ligand_idx`、`reference_mol`、そして
`_refresh_view` 相当の再計算ステップ）を1対1で再現している。C++側の
`MainWindow` が持つのは*表示*状態だけ（どのドックが表示されているか、
設定パネルのチェックボックス、シーケンスパネルの選択残基、3Dビューの
現在のカメラ位置）。`PythonBridge` の各メソッドは文字列（JSON）または
プリミティブ型しかやり取りしないため、pybind11境界自体が単純なまま保たれる
——カスタム型キャスタもなく、`PythonBridge::Impl` の外に長生きする
`py::object` ハンドルも一切存在しない。

## インストール

どのプラットフォームでも必要なものは同じ3つ: CMake 3.21以上とC++20対応
コンパイラ、Qt6（`Core`、`Widgets`、`WebEngineWidgets`、`WebChannel`——
`WebEngineWidgets` が必要ということは、`qtbase` だけでなくChromium
ベースの `qtwebengine` を含むフルのQt6インストールが必要という意味）、
そして `dd_molview` 自体が動いているのと同じPython環境（`dd_viewer` +
`dd_molview` がインストール済み・editableかどうかは問わない、加えて
それらの依存——RDKit、biopython、pandas、numpy、そして `pybind11`。
これは別途システムパッケージとしてではなく、その環境からimportできれば
良い）。プラットフォーム固有なのは最初の2つだけで、Python側はどのOSでも
同じconda/miniforge env（フルセットアップは
[`../dd_molview/README.jp.md`](../dd_molview/README.jp.md)を参照）。
**以下のうち実際にビルド検証済みなのはmacOSの手順のみ**（[動作検証](#動作検証)
参照）——Ubuntu/Windowsの手順は各プラットフォームの標準的なQt6/CMakeの
慣例に従って書いているが、実機のUbuntu/Windowsマシンでは未検証。

デフォルトでは、CMake構成時に `$CONDA_PREFIX/bin/python3`
（Windowsでは `%CONDA_PREFIX%\python.exe`）が指すPythonを埋め込む
（condaが有効でなければ、そのプラットフォームのデフォルトの
miniforge配置場所の下の `dd` envにフォールバック）——どのプラット
フォームでも `-DDD_CVIEW_PYTHON=/path/to/python3`
（Windowsでは`...\python.exe`）で、`dd_viewer`/`dd_molview` が
インストールされた別の環境（別名のconda envやただのvenv）に上書き
できる。

### macOS (Homebrew)

```bash
brew install cmake qt ninja

conda activate dd
pip install pybind11   # 未インストールの場合

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Homebrewの `qt` フォーミュラはkeg-only（デフォルトの検索パスにリンク
されない）だが、`CMakeLists.txt` 自身が `brew --prefix qt` を実行して
`CMAKE_PREFIX_PATH` に追加するため、ここでは手動の
`-DCMAKE_PREFIX_PATH` は不要（下記のWindowsとは異なる）。

### Ubuntu (22.04 / 24.04)

```bash
sudo apt update
sudo apt install cmake ninja-build build-essential \
    qt6-base-dev qt6-webengine-dev qt6-webengine-dev-tools

conda activate dd
pip install pybind11   # 未インストールの場合

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

`qt6-webengine-dev` がWebEngineWidgetsのランタイム/開発ヘッダ（そして
それ自体の大きな依存チェーン——Homebrewの `qt` フォーミュラと同じ
Chromiumベースのコスト）を推移的依存として引いてくる。aptでインストール
したQt6は既にCMakeのデフォルト検索パス上にあるため、Windowsのような
`CMAKE_PREFIX_PATH` の上書きは不要。

### Windows (MSVC + Qtオンラインインストーラ)

1. Visual Studio 2022（またはスタンドアロンのBuild Tools）を
   **「C++によるデスクトップ開発」**ワークロード付きでインストールする
   ——これでCMakeが必要とするMSVCコンパイラとWindows SDKが手に入る。
2. [CMake](https://cmake.org/download/) と
   [Ninja](https://github.com/ninja-build/ninja/releases) をインストール
   する（またはVisual Studio Installerのオプションコンポーネントに
   同梱のものを使う）。
3. [Qtオンラインインストーラ](https://www.qt.io/download-qt-installer)
   経由でQt6（6.5以上）をインストールし、**MSVC 2022 64-bit** キット
   *と* **Qt WebEngine** モジュール（Homebrew/aptのパッケージとは違い、
   自動的には入らない）を選択する——これは `C:\Qt\6.7.2\msvc2022_64`
   のようなパスにインストールされる。
4. Windows用の[Miniforge](https://github.com/conda-forge/miniforge)を
   インストールし、`dd_molview` が使っているのと同じ `dd` conda env
   をセットアップ（そちらのREADMEを参照）してから、その中に
   `pip install pybind11` する。
5. **「x64 Native Tools Command Prompt for VS 2022」**からビルドする
   （`cl.exe` がPATHに乗るために必要——普通の`cmd.exe`/PowerShell
   ウィンドウにはない）:

   ```bat
   cmake -S . -B build -G Ninja ^
     -DCMAKE_BUILD_TYPE=Release ^
     -DCMAKE_PREFIX_PATH=C:\Qt\6.7.2\msvc2022_64 ^
     -DDD_CVIEW_PYTHON=%USERPROFILE%\miniforge3\envs\dd\python.exe
   cmake --build build
   ```

   Homebrew/aptとは異なり、QtオンラインインストーラはCMakeの検索パス
   に自分自身を登録しない——`-DCMAKE_PREFIX_PATH` で実際にインストール
   されたキットのディレクトリを指す必要が、フォールバックとしてでは
   なく毎回必須となる。

## ビルド済みバイナリのインストール

`cmake --install` は、ビルドされた実行ファイルと、隣接する `python/`
（`dd_cview_backend.py` モジュール）・`data/`（同梱サンプル構造）
ディレクトリを、選んだ `--prefix` の下の1つの自己完結した再配置可能な
ディレクトリにまとめてコピーする——そのディレクトリ全体をどこにコピー
/移動しても、`dd_cview` は自分自身の隣にある自分のバックエンドモジュール
をきちんと見つける（`PythonBridge.cpp` の `resolvePythonDir()` を参照）。
「インストール」とは、そのディレクトリの安定した置き場所を決め、任意で
そのバイナリを `PATH` に通すことを意味する。

**これは*埋め込まれたPython環境*自体を可搬にするものではない**: バイナリ
を*ビルド*した時点で `-DDD_CVIEW_PYTHON` が指していたconda env/venvは、
コンパイル時にその `PYTHONHOME` として埋め込まれており、インストール済み
バイナリを実際に実行するマシン上の同じパスに引き続き存在している必要が
ある。これは個人用/ローカル用のビルドであり、他のマシンや他のユーザーの
環境向けの配布可能なインストーラではない。

```bash
cmake --install build --prefix <書き込み可能な任意のディレクトリ>
```

### macOS / Ubuntu

```bash
cmake --install build --prefix ~/apps/dd_cview
ln -sf ~/apps/dd_cview/dd_cview ~/.local/bin/dd_cview   # または /usr/local/bin（書き込み可能なら）
```

（`~/.local/bin` は現行の多くのUbuntu/macOSシェルでデフォルトで`PATH`に
入っている。入っていなければシェルの設定ファイルに
`export PATH="$HOME/.local/bin:$PATH"` を追加する。）これ以降はどこから
でも `dd_cview` として実行できる。シンボリックリンクを作らず
`~/apps/dd_cview/dd_cview` を直接起動してもよい。

### Windows

```bat
cmake --install build --prefix C:\Tools\dd_cview
```

その後、`C:\Tools\dd_cview` をユーザーの `PATH` に追加する（設定 >
システム > バージョン情報 > システムの詳細設定 > 環境変数）か、
`C:\Tools\dd_cview\dd_cview.exe` へのショートカット（デスクトップや
スタートメニュー）を作成する——それ以外にインストーラ/アンインストーラ
の類は存在しない。単なる再配置可能なディレクトリ。

**ローカルでの開発/テストにはこれらは一切不要**——ビルドディレクトリの
`./build/dd_cview`（Windowsなら`build\dd_cview.exe`）をそのまま直接
実行しても全く同じように動く。`cmake --install` はビルドディレクトリの
外の安定した場所が欲しくなったときにだけ意味を持つ。

## 使い方

```bash
./build/dd_cview \
  --receptor data/6W63_receptor.pdb --poses data/6W63_redock.sdf \
  --receptor data/7L10.pdb --receptor data/7L11.pdb \
  --reference data/6W63_ligand_ref.sdf   # 任意: RMSD計算用
```

`--receptor` と `--poses` はそれぞれ**繰り返し指定可能**（ファイルごとに
フラグを1回ずつ渡す）。`--reference` と `--manifest` はそれぞれ単一指定。
ファイルは起動後にFileメニュー/ツールバー（「Open Receptor(s)...」「Add
Poses...」「Open Reference...」「Open Manifest...」）からも読み込める、
`dd_molview-desktop` と全く同じ挙動。

**posesファイルを一つも指定しなかった場合**、読み込んだ全受容体が埋め込み
リガンドをスキャンされ、結果が1つのリガンドリストにマージされる——
`dd_molview` と同じall-or-nothingな自動抽出ルール（詳細は`dd_molview`の
READMEを参照）。このロジックは `dd_molview.load_all` 内部で無改変のまま
実行され、`dd_cview` はそれを呼び出すだけ。

### アンサンブルmanifest.json

```bash
./build/dd_cview --manifest data/sample_manifest.json
```

`dd_molview` が読むのと同じdd_docking形式の `manifest.json`
（`{member_id, receptor_pdb, ...}` のプレーンなJSONリスト、duck-typing、
`dd_docking` はimportしない）——`--receptor` のパスと加算的に読み込まれる。

## 機能

各パネルは `dd_molview-desktop` と全く同じ挙動をする（同じ
`dd_viewer`/`dd_molview` の呼び出しを、PySide6ではなくネイティブの
`QTableView`/`QWebEngineView`/`QTextBrowser` ウィジェット経由で行っている
だけ）:

- **蛋白質選択パネル**: 読み込んだ受容体ごとに1行（ラベル、鎖数/残基数、
  ソースパス）。行を選ぶとその受容体が3Dビューとシーケンスパネルのアクティブ
  対象になる。
- **リガンド選択パネル**: 読み込んだリガンドごとに1行、スコア/RMSD/相互
  作用数の各列は*アクティブな*受容体に対してのみ計算される。
- **3D構造ビュー**: cartoon/stick/surfaceの切り替え、二次構造による色分け、
  リガンド近傍のみ表示オプション、相互作用オーバーレイ（水素結合、疎水性
  接触、塩橋、静電相互作用、パイスタッキング、ハロゲン結合）、残基ラベルの
  フローティング表示、更新をまたいだカメラ位置の保持——`Viewer3D` が
  `page()->runJavaScript()` の往復で直前に取得したカメラ位置を新しい
  シーンに再適用する、`dd_molview-desktop` と同じ手法。**Highlight
  interacting residues**（黄色、デフォルトON）は、上記のうち現在有効な
  相互作用タイプのいずれかを持つ残基すべてをマークする——下の「Contact
  residue cutoff」の距離内にあるだけの残基すべてではない。この2つは
  独立した別概念（下のコンタクト残基テーブルを参照）。
- **シーケンスパネル**: 1行50残基固定のresnumグリッドと行頭ルーラー、3D
  ビューと同じ黄色/マゼンタの2階層ハイライト（黄色は上記と同じ相互作用
  ベースの集合）、クリック可能な残基（Ctrl/Cmd-クリックで複数選択）と
  クリック可能な「Chain X」見出し（その鎖にカメラを再フィット）。
- **コンタクト残基テーブル**（デフォルトで折りたたみ）: Contact residue
  cutoffスライダーの距離内にあるリガンド周辺の残基すべて、上記の黄色
  ハイライトとは独立——行を選択すると、それらの残基が実際に検出された
  相互作用を持つかどうかに関わらず、3Dビューとシーケンスパネルの両方で
  マゼンタにハイライトされる（通常の手動選択の仕組み）。
- **すべてのパネルが独立したドック**——それぞれ独立して移動・フロート化・
  クローズ可能で、セッションをまたいで `QSettings`
  （`saveGeometry`/`saveState`）で状態が復元される。
- **Save 3D View Screenshot...**（Fileメニュー/ツールバー）: 3D View
  ドックに現在表示されているものそのまま——受容体、ポーズ、相互作用
  オーバーレイ、現在のカメラ位置——を、保存ダイアログで選んだPNG
  ファイルに保存する。`QWebEngineView` に対する単純な `QWidget::grab()`
  を使用（実ディスプレイでのみ意味を持つ——本プロジェクトの他の3Dビュー
  キャプチャと同様、`grab()` は `QT_QPA_PLATFORM=offscreen` 下では
  WebGLコンテンツを捉えられない——[動作検証](#動作検証)を参照）。

**カメラの挙動**と**複数残基選択**は `dd_molview-desktop` と全く同じ規約に
従う（詳細は[そちらのREADME](../dd_molview/README.jp.md)を参照）——
アクティブな蛋白質/リガンド行の切り替えや表示設定の変更ではカメラは動かず、
自分でドラッグ/ズームするか、「Center on Ligand」または「Zoom to
Highlighted Residues」を押した時だけ動く。

**`dd_cview`独自の追加機能**（`dd_molview-desktop`にはない）: Settings
ドックには「Center on Ligand」の横にもう1つカメラ用ボタン**Zoom to
Highlighted Residues**があり、現在黄色くハイライトされている残基に
カメラを再フィットする（読み込み済みシーンに対するライブの
`zoomTo({predicate: ...})` + `render()` 呼び出し、シーケンスパネルの
鎖見出しクリックと同じ直接カメラ移動の手法。何もハイライトされていな
ければ何もしない）。両方のカメラボタンは現在Settingsドックにのみ存在
する——トップレベルのViewメニュー/ツールバーの「Center on Ligand」
エントリはもう存在しない。何がハイライトされるかを決めているのは表示
設定そのものなので、両方のカメラアクションをその表示設定と一緒に
まとめる方が、片方だけをトップレベルに重複して置くよりも分かりやすい
ため。

## モジュール構成 (`src/`)

| ファイル | 内容 |
|---|---|
| `PythonBridge.h`/`.cpp` | `<pybind11/embed.h>` に触れる唯一のファイル。埋め込みインタプリタ（`py::scoped_interpreter`）と唯一の `Session` オブジェクトを保持する。各メソッドは `QJsonDocument` とQt/std標準型との間で変換する。 |
| `MainWindow.h`/`.cpp` | ドック構築、メニュー/ツールバー、レイアウト永続化、全Qtシグナルハンドラ、そして中心となる再計算・再描画メソッド `refreshView()`——`dd_molview.desktop.main_window.MainWindow` のC++版。 |
| `TableModel.h`/`.cpp` | `TableData`（列/表示行/生データ行）をラップする、3つのテーブル全てで再利用する読み取り専用の `QAbstractTableModel`。 |
| `DisplaySettingsPanel.h`/`.cpp` | 設定ドックのコントロール群——`dd_molview.desktop.controls.DisplaySettingsPanel` と1対1対応。 |
| `SequencePanel.h`/`.cpp` | シーケンスパネルのフォント/リンク処理設定を持つ `QTextBrowser` サブクラス。 |
| `Viewer3D.h`/`.cpp` | 3Dビューの `QWebEngineView` をラップ: 新しく構築したシーンの読み込み、非同期の `getView()` カメラ取得往復、鎖見出しクリック用の `zoomTo({chain})`。 |
| `main.cpp` | エントリポイント: CLI引数解析（`--receptor`/`--poses`/`--reference`/`--manifest`）、`QApplication` セットアップ、ヘッドレス検証用の任意の `DD_CVIEW_SCREENSHOT` 環境変数フック（後述）。 |

`python/dd_cview_backend.py` がPython側の対応物——詳細は上の
[アーキテクチャ](#アーキテクチャ)節を参照。

## 設計上のポイント

- **計算ロジックはC++側で一切重複させない。** PDB/SDFのパース、距離計算、
  RDKit呼び出し、HTML生成のいずれも、`dd_viewer`/`dd_molview` 内部で
  Pythonのまま、無改変で実行される。`dd_cview` はpybind11境界越しに
  プリミティブ値を渡すだけ。
- **`PythonBridge` は `py::object` を一切外に漏らさない。** その公開API
  （`PythonBridge.h` を参照）はQt/std標準型のみなので、プロジェクト内の
  他のどのファイルもpybind11の存在を知る必要がなく、`Python.h`/
  `qobjectdefs.h` の `slots` 衝突が `PythonBridge.cpp` の外で起きる余地が
  ない。
- **PYTHONHOMEは明示的に設定し、`sys.path`のcwdエントリは除去する。**
  `dd_viewer`、`dd_molview`、`dd_cview` はすべて同じ親ディレクトリ
  （`~/work`）の下にきょうだいディレクトリとして存在する。もし `dd_cview`
  がその親ディレクトリをカレントディレクトリとして起動された場合、空文字列
  `""` の `sys.path` エントリが `import dd_viewer` を、そこに実在する
  `dd_viewer` という名前のディレクトリ（その階層に `__init__.py` がない）
  を起点にした壊れた*名前空間*パッケージへと——本物のeditableインストール
  済みパッケージの代わりに——静かに解決してしまう。例外を投げるのではなく
  `dd_viewer.__file__` が `None` になって返ってくるだけなので気づきにくい。
  `PythonBridge` のコンストラクタは何かをimportする前に空文字列/cwd由来の
  `sys.path` エントリを除去するため、バイナリをどこから起動してもこの問題
  は起こらない。
- **GILは一切解放しない。** すべての `PythonBridge` 呼び出しはQtのメイン
  スレッド上で同期的に行われ、元の `dd_molview-desktop` 自身のシングル
  スレッドQtイベントループモデルと一致している——バックグラウンドスレッドが
  インタプリタに触れることは一切ないため、`py::gil_scoped_release`/
  `acquire` はどこにも必要ない。
- **クロスプラットフォームの環境まわりの処理はPOSIX限定の前提やディレクトリ
  レイアウトの前提を置かない。** `PYTHONHOME` はPOSIX限定の `setenv`
  （MSVCのCRTには存在しない）ではなく `qputenv` で設定しているため、
  同じコードがWindowsでもビルドできる。`PYTHONHOME` の*値*は
  `DD_CVIEW_PYTHON` から何段上のディレクトリかを数えて導出するのではなく、
  CMake構成時に対象インタプリタから直接 `sys.prefix` を問い合わせて得ている
  ——何段上かはプラットフォーム*と*env種別の両方で異なるため（conda-
  on-Windows: インタプリタが既にenvルートに存在、conda-on-POSIXと
  POSIX venvの`bin/`: 1段上、Windows venvの`Scripts/`: これも1段上）
  ——`sys.prefix` を使えばどの規約が当てはまるかを推測する必要がない。
  macOS/Linuxでは、`dd_cview` のCMakeターゲットに、同じenvの `lib/`
  ディレクトリを指す明示的な `INSTALL_RPATH` も設定している——これが
  ないと、インストール済み（`cmake --install`）バイナリは `main()`
  に到達する前に動的リンカのエラー（`Library not loaded: @rpath/
  libpython3.12.dylib` や `error while loading shared libraries:
  libpython3.so...`）で即座に落ちてしまう。CMakeがこれを自動的に付与
  するのは*ビルドツリー*のバイナリに対してだけで、インストール済みの
  バイナリには付与されないため。
- **インストール済みバイナリは、ソースのチェックアウトではなく自分自身
  からの相対パスで自分のバックエンドモジュールを見つける。**
  `PythonBridge.cpp` の `resolvePythonDir()` は、まず実行中の実行ファイル
  （`QCoreApplication::applicationDirPath()`）の隣にある
  `python/dd_cview_backend.py` を探す——これは `cmake --install`
  が作るレイアウト（[ビルド済みバイナリのインストール](#ビルド済みバイナリのインストール)
  を参照）——それが見つからない場合にのみコンパイル時のソースツリーの
  パス（`DD_CVIEW_PYTHON_DIR`）にフォールバックする。これにより、
  `cmake --install` したディレクトリは元のビルドディレクトリに永久に
  依存し続けるのではなく、本当の意味で再配置可能（どこにコピー/移動
  してもよい）になる。

## サンプルデータ (`data/`)

`dd_molview` と同じSARS-CoV-2 Mproサンプル一式
（`6W63_receptor.pdb` / `6W63_redock.sdf` / `6W63_ligand_ref.sdf`、
`7L10.pdb` / `7L11.pdb`、`sample_manifest.json`）——各ファイルの用途は
[dd_molviewのREADME](../dd_molview/README.jp.md)を参照。

## 動作検証

`tests/bridge_test.cpp`（CTestの `bridge_smoke_test` として登録）は
`PythonBridge` 単体の統合スモークテストで、QtのGUIは一切関与しない:
同梱の6W63 + 7L10受容体と、6W63の再ドッキングポーズ9件+参照リガンドを
読み込み、`PythonBridge` の各メソッドがそれらに対して（テーブルの行/列数、
空でないシーンHTML/シーケンスHTML、正しいリンクスキーム、埋まった
コンタクトテーブル、実際のスコア/RMSD）実際に埋め込まれたインタプリタを
通してエンドツーエンドで妥当な値を返すことを確認する——
`ctest --test-dir build` で実行できる。

GUI全体はヘッドレス環境で検証済み（`QT_QPA_PLATFORM=offscreen`、
`DD_CVIEW_SCREENSHOT` 環境変数 / `QWidget::grab()` によるスクリーン
ショット取得——`dd_molview-desktop` 自身のテストスイートと同じ手法）:
同梱の6W63 + 7L10受容体、6W63のポーズ、参照リガンドを指定して起動すると
Proteins/Ligandsテーブルと Chains パネル（コンタクト残基の黄色ハイライト
含む）が正しく表示されること、引数なしで起動すると正しい空状態（空の
テーブル、無効化された「Show reference ligand」）になること、そして
`dd_viewer`/`dd_molview`/`dd_cview` が共有する親ディレクトリを作業
ディレクトリに設定して起動しても（上記の`sys.path`修正が対処する、まさに
そのシナリオ）データが正しく読み込まれることを確認済み。
`dd_molview-desktop` と同様、実際の `QWebEngineView` によるWebGL描画は
`QT_QPA_PLATFORM=offscreen` 下では観測できない——3Dビューの実際の画面
描画とカメラ保持挙動は実ディスプレイでの目視確認が必要（これは
「Save 3D View Screenshot...」自体も、クラッシュせず*何らかの*PNGを
書き出すことしか確認できておらず、実際にレンダリングされた内容を
正しくキャプチャするかは未確認、ということでもある）。相互作用/カメラの
*ロジック*自体は `dd_viewer`/`dd_molview` 自身のテストスイートで
（無改変のまま）検証されており、本プロジェクトではそれを重複して検証
していない。

`cmake --install` の再配置可能性は実際に検証済み: 使い捨てのprefix
（`cmake --install build --prefix /tmp/...`）にインストールし、
ビルドツリーともインストールprefixとも異なる作業ディレクトリから、
インストール済みコピー自身の `data/` ディレクトリを指す
`--receptor`/`--poses` でインストール済みバイナリを実行したところ
正しく読み込めた——`resolvePythonDir()` の実行ファイル相対の探索と、
`INSTALL_RPATH` の修正（これがないと、少なくともmacOSではインストール
済みバイナリが `main()` に到達する前に動的リンカのエラーで即座に落ちる）
の両方が機能していることを確認。本プロジェクトで実際に検証したのは
macOSでのビルドと、このmacOSでのインストールフローのみ——上記の
Ubuntu/Windowsのビルド・インストール手順は標準的な慣例に従って
書いたものであり、ここでは未検証（[インストール](#インストール)参照）。

## ライセンス

MIT — [LICENSE](LICENSE) を参照。
