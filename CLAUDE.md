# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 概要

torabo-tsuki LP キーボード向け ZMK ファームウェア設定リポジトリ。ビルドは GitHub Actions で自動実行される。ローカルビルドは不要で、編集後に `master` ブランチへ push するだけでファームウェアが生成される。

## ビルド

ローカルビルド手段はない。GitHub Actions (`build.yml`) が ZMK 公式の `build-user-config.yml@v0.3` ワークフローを呼び出してビルドする。

成果物は `build.yaml` に定義された以下の 6 種（+ settings_reset）：

| artifact-name | 用途 |
|---|---|
| `torabo_tsuki_lp_left_central` | トラックボール付き左手 → 中央側 |
| `torabo_tsuki_lp_right_peripheral` | 右手 → 周辺側 |
| `torabo_tsuki_lp_left_peripheral` | 左手 → 周辺側（右手中央構成時） |
| `torabo_tsuki_lp_right_central` | トラックボール付き右手 → 中央側 |
| `torabo_tsuki_lp_double_ball_left_peripheral` | ダブルトラックボール構成の左手 |
| `torabo_tsuki_lp_double_ball_right_central` | ダブルトラックボール構成の右手中央 |

書き込み：`*_central` を トラックボール側、`*_peripheral` を反対側に書き込む。

## アーキテクチャ

### ファイル構成の役割分担

```
config/keymap.keymap        ← キーマップ（主な編集対象）
config/west.yml             ← 依存するZMKモジュールのリスト
boards/shields/torabo_tsuki_lp/
  torabo_tsuki_lp.dtsi      ← ハードウェア定義（GPIO、kscan、レイアウト）
  torabo_tsuki_lp_left.overlay   ← 左手固有のオーバーレイ
  torabo_tsuki_lp_right.overlay  ← 右手固有のオーバーレイ
  torabo_tsuki_lp_layouts.dtsi   ← 物理レイアウト (S/M/L) 定義
snippets/                   ← 入力デバイス等のオプション設定（スニペット）
src/                        ← カスタムCコード（board.c=分割電源管理, trackball_gesture.c=方向ジェスチャー, mini_trackpad=IQS7211E初期化）
                               CMakeLists.txt の zephyr_library_sources に登録して取り込む
```

### レイヤー構成

| Layer | 番号 | 役割 |
|---|---|---|
| Mac | 0 | ベースレイヤー（macOS） |
| Win | 1 | Windowsモード（`tog 1` で切替） |
| Number | 2 | 数字・記号（左親指スペース長押し） |
| Function | 3 | ファンクション・矢印・BT選択（右親指スペース長押し） |
| layer_4 | 4 | 左親指バックスペース長押し（未割当） |
| mouse | 5 | オートマウス（トラックボール動作で自動遷移） |
| Mouse_Scroll_Mac | 6 | Macスクロール（`;` 長押しで遷移） |
| Mouse_Scroll_Win | 7 | Winスクロール（`;` 長押しで遷移） |
| Mouse_Fast | 8 | 高速カーソル（mouseレイヤー中に `a`/`s`/`d`/`f` 長押し） |
| Trackball_Gesture | 9 | トラックボールジェスチャー（左親指 BACKSPACE = `&lt 9 BACKSPACE` をホールド中にボールを上下左右へ振る） |

### 主要なカスタムビヘイビア

- **`mt_idle`**: `require-prior-idle-ms = <125>` 付き Mod-Tap。タイピングエリアのホームポジション修飾キー（`a`=右Ctrl、`z`=左Shift、`;`=右Ctrl、`/`=右Shift）に使用。高速打鍵時の誤爆防止。
- **`lt_idle`**: 同じく `require-prior-idle-ms = <125>` 付き Layer-Tap。`;` キーのスクロールレイヤー遷移に使用。
- **`exit_mouse_macro`**: マクロ。`mouse` レイヤー（Layer 5）を `tog 5` でトグルオフする。mouseレイヤーの親指行キー全体に配置。

### トラックボールジェスチャー（方向→アクション）

「あるキーを押しながらトラックボールを上下左右に振ると、方向に応じたアクションを発火する」機能。`src/trackball_gesture.c` が実装。

- **仕組み**: `src/trackball_gesture.c` が**カスタム入力プロセッサ `zip_gesture`**（compatible `zmk,input-processor-gesture`、binding は `dts/bindings/input_processors/`、ノードは shield dtsi）として実装され、`&pointing_listener` の **input-processors 先頭**に置かれている。先頭ゆえ**生のX/Y変位**を読め、ジェスチャーレイヤー有効時だけ蓄積→閾値超過で方向確定→**キーポジションを press/release**（raise はシステムワークキューに退避）。さらに同レイヤー中は `ZMK_INPUT_PROC_STOP` を返して**カーソル移動を抑制**する（値は変えずにイベントを止めるので検出と両立）。アクション割り当ては通常のキーマップ側で行い、C の再コンパイル不要。
- **発火先ポジション（重要）**: **実在する未使用ポジション 0〜3**（トップ行＝全レイヤー `&none`／物理的に未実装）を UP/DOWN/LEFT/RIGHT に使う。`Trackball_Gesture`（Layer 9）の**先頭4バインド**がこれにあたる。
  - 当初は末尾に**仮想ポジション 66–69** を新設する設計だったが、ZMK は raise したポジションを**物理レイアウトの position map 経由**で解決するため、複数物理レイアウト（S/M/L）構成では新設ポジションがマップに乗らず**イベントが破棄**され発火しなかった。そこで「すでに正常にルーティングされる実在ポジション」に切り替えた。`src/trackball_gesture.c` の `GESTURE_POS_*` がこの番号（0〜3）を持つ。
  - 当初の名残（`size_l_transform`／`physical_layout_l`／全レイヤーを 66→70 に拡張した分）は**現在は未使用**。害はないが、整理するなら 66 に戻してよい。
- **「どのキーを押しながらか」**: 既存の `&mo`/`&lt`（モーメンタリ／レイヤータップ）パターンで表現。キーをホールド→ジェスチャーレイヤー有効化→そのレイヤーの仮想ポジションのバインドが選ばれる。レイヤーを分ければ「キー種別ごとに別アクション」になる。
- **現在のトリガーキー**: 左親指の `&lt 9 BACKSPACE`（Macレイヤー、Winもフォールスルーで継承）。タップ=BACKSPACE、ホールド=ジェスチャー有効。トラックボールは右親指で操作する想定のため、反対側（左親指）をトリガーにしている。元の `&lt 4 BACKSPACE` の Layer 4 は中身が全`&trans`で未使用だったため、ホールド先を 9 に振り替えても失う機能はない（Layer 4 自体は番号維持のため残置・孤立）。
- **1モーション1アクション**: 連続した1回のボール移動につき1回だけ発火し、ボールが `GESTURE_IDLE_REARM_MS` の間アイドルになると再武装する（転がし続けても連射しない）。
- **カーソル抑制**: ジェスチャーレイヤー中は `zip_gesture` が `ZMK_INPUT_PROC_STOP` を返し、後続プロセッサ（transform/temp_layer/scaler）を止めてカーソルを動かさない。値をゼロ化しないため検出は壊れない。
  - かつて `gesture_suppress`（`zip_xy_scaler 0`）で抑制を試みたが、入力コールバックは同一イベント実体を順に処理し、リスナーが検出より先に走って scaler 0 が検出側の読み取り値もゼロ化していたため失敗した。入力プロセッサ化（＝リスナーの先頭で処理）でこれを解消した。
- **方向の注意**: `zip_gesture` は `zip_xy_transform`（X/Y反転）より前で生値を読むため、カーソルの向きと検出の向きが反転している。逆に感じる場合は `src/trackball_gesture.c` の `GESTURE_POS_*` の対応を入れ替える。
- **モジュール越しのリンク制約（重要）**: このリポジトリは ZMK の外部モジュール（`ZMK_EXTRA_MODULES`）として読み込まれる。`zmk_keymap_layer_active()` など ZMK 本体内の一部関数は**外部モジュールからリンクできず**（`undefined reference`）、一方でイベント機構（`ZMK_LISTENER`/`ZMK_SUBSCRIPTION`/`raise_*`）は `board.c` 同様リンクできる。そのため `zip_gesture` はレイヤー判定を **`zmk_layer_state_changed` イベントの購読**で自前追跡している（直接 `zmk_keymap_layer_active` を呼ばない）。新規 C コードで ZMK 本体の関数を呼ぶ際はこの制約に注意。
- **モジュールの dts_root**: カスタム binding を認識させるため `zephyr/module.yml` に `dts_root: .` が必要（`dts/bindings/` を検索対象に追加）。
- **調整パラメータ**（`src/trackball_gesture.c` 冒頭の `#define`）: `GESTURE_THRESHOLD`（発火に必要な変位量／生値ベース）, `GESTURE_IDLE_REARM_MS`（このms間アイドルで再武装）, `GESTURE_RELEASE_DELAY_MS`（press→release 間隔）, `gesture_layers[]`（ジェスチャー有効レイヤーの配列）。

**設定手順（新しいジェスチャーを足す場合）**:
1. トリガーキーを割り当てる。現状は左親指 `&lt 9 BACKSPACE`。別のキーにしたい場合は `&mo 9`（押している間だけ）か `&lt 9 <tap-key>`（タップで通常キー／ホールドでジェスチャー）を使う。
2. `Trackball_Gesture` レイヤー（Layer 9）の**先頭4バインド**（position 0–3＝UP/DOWN/LEFT/RIGHT）を好みのアクションに書き換える。
3. キーごとに別アクションにしたい場合は、新レイヤーを追加し、`gesture_layers[]` にそのレイヤー番号を足して再ビルド。

### トラックボール入力処理（`pointing_listener`）

通常時は `zip_xy_scaler 40 100`（40%速度）＋ `zip_temp_layer 5 1200`（1200ms で Layer 5 へ一時遷移）。

| プロファイル | 発火レイヤー | 速度 |
|---|---|---|
| デフォルト | — | 40% |
| `fast_mouse` | Layer 8 | 100%（通常の約2.5倍） |
| `scroll_mac` | Layer 6 | スクロール（Mac方向） |
| `scroll_win` | Layer 7 | スクロール（Win方向） |

### 物理レイアウト

キーボード本体はキー数によって S / M / L の 3 サイズを持つ。デフォルトは L レイアウト（5行14列）。`torabo_tsuki_lp.dtsi` の `chosen { zmk,physical-layout = &physical_layout_l; }` で切替可能。

### 外部依存（`west.yml`）

- `zmkfirmware/zmk@v0.3` — ZMK 本体
- `sekigon-gonnoc/zmk-component-bmp-boost@v0.2` — BMP Boost ボードサポート
- `sekigon-gonnoc/zmk-driver-paw3222` (branch: `torabo-tsuki`) — トラックボールドライバ
- `sekigon-gonnoc/zmk-driver-iqs7211e` — トラックパッドドライバ
- `kot149/zmk-scroll-snap@v1` — スクロールスナップ機能
- その他 sekigon-gonnoc 製フィーチャーモジュール多数

## 主な変更箇所

キーリマップは `config/keymap.keymap` のみ編集すればよい。ハードウェア構成（GPIO ピンアサイン、トラックボール感度）を変更する場合は `boards/shields/` 配下の `.dtsi` / `.overlay` を編集する。

`plans/` ディレクトリに過去の設計判断の記録がある（実装の意図を把握するために参照すること）。
