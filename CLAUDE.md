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

- **仕組み**: `src/trackball_gesture.c` が `INPUT_CALLBACK_DEFINE(NULL, ...)`（central限定）でトラックボールの生X/Y変位を購読・蓄積し、ジェスチャーレイヤーが有効な間だけ、閾値超過で方向を確定して**仮想キーポジション**を press/release する。アクションの割り当ては通常のキーマップ側（その仮想ポジションのバインド）で行うため、C の再コンパイル不要。
- **仮想ポジション**: キーマップ位置 **66=UP, 67=DOWN, 68=LEFT, 69=RIGHT**。確保には **3か所を同じ数（70）に揃える必要がある**:
  1. `torabo_tsuki_lp.dtsi` の `size_l_transform`（マトリクストランスフォーム）末尾に未配線の `RC()` を4つ追加（物理スキャンからは発火しない）。
  2. `torabo_tsuki_lp_layouts.dtsi` の `physical_layout_l` の `keys`（物理キー定義）末尾に `key_physical_attrs` を4つ追加。**ここが重要**: ZMK は raise したポジションを物理レイアウトの position map 経由で変換し、`位置 >= マップ長`（=物理レイアウトのキー数）だと**イベントを破棄**する。トランスフォームだけ増やして物理レイアウトを増やし忘れると「レイヤーは有効だがキーが出ない」状態になる。
  3. `keymap.keymap` の**全レイヤーのバインド数を 66→70** に拡張。
  （ZMK は最大／chosen レイアウト=L にキーマップ長を合わせる仕様。S/M は未使用のため拡張不要。）
- **「どのキーを押しながらか」**: 既存の `&mo`/`&lt`（モーメンタリ／レイヤータップ）パターンで表現。キーをホールド→ジェスチャーレイヤー有効化→そのレイヤーの仮想ポジションのバインドが選ばれる。レイヤーを分ければ「キー種別ごとに別アクション」になる。
- **現在のトリガーキー**: 左親指の `&lt 9 BACKSPACE`（Macレイヤー、Winもフォールスルーで継承）。タップ=BACKSPACE、ホールド=ジェスチャー有効。トラックボールは右親指で操作する想定のため、反対側（左親指）をトリガーにしている。元の `&lt 4 BACKSPACE` の Layer 4 は中身が全`&trans`で未使用だったため、ホールド先を 9 に振り替えても失う機能はない（Layer 4 自体は番号維持のため残置・孤立）。
- **カーソル抑制**: `&pointing_listener` の `gesture_suppress`（`layers = <9>`）で Layer 9 中はカーソル移動を 0 にスケールし、ジェスチャー中の意図しないカーソルドリフトと auto-mouse（Layer 5）の発火を防ぐ。生X/Y はリスナー処理前に取得するため検出に影響しない。
- **調整パラメータ**（`src/trackball_gesture.c` 冒頭の `#define`）: `GESTURE_THRESHOLD`（発火に必要な変位量）, `GESTURE_COOLDOWN_MS`（連続発火間隔＝押し続けた時の自動リピート速度）, `GESTURE_RELEASE_DELAY_MS`（press→release 間隔）, `gesture_layers[]`（ジェスチャー有効レイヤーの配列）。方向が逆に感じる場合は `GESTURE_POS_*` の対応を入れ替える。

**設定手順（新しいジェスチャーを足す場合）**:
1. トリガーキーを割り当てる。現状は左親指 `&lt 9 BACKSPACE`。別のキーにしたい場合は `&mo 9`（押している間だけ）か `&lt 9 <tap-key>`（タップで通常キー／ホールドでジェスチャー）を使う。
2. `Trackball_Gesture` レイヤー（Layer 9）の末尾4バインド（UP/DOWN/LEFT/RIGHT）を好みのアクションに書き換える。
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
