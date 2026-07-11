# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 会話スタイル

ユーザーとの会話は敬語でやり取りすること。

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
| Trackball_Gesture | 9 | トラックボールジェスチャー（左親指右 = `&lt 9 TAB` をホールド中にボールを上下左右へ振る） |

### 主要なカスタムビヘイビア

- **`mt_idle`**: `require-prior-idle-ms = <80>` 付き Mod-Tap。タイピングエリアのホームポジション修飾キー（`a`=右Ctrl、`z`=左Shift、Number/Functionレイヤーの同ポジション）に使用。高速打鍵時の誤爆防止。
- **`lt_idle`**: 同じく `require-prior-idle-ms = <125>` 付き Layer-Tap。`;` キーのスクロールレイヤー遷移に使用。
- **`exit_mouse_macro`**: マクロ。`mouse` レイヤー（Layer 5）を OFF 専用の `tog_off 5` で解除する。mouseレイヤーの親指行キー全体に配置。

### トラックボールジェスチャー（方向→アクション）

「あるキーを押しながらトラックボールを上下左右に振ると、方向に応じたアクションを発火する」機能。`src/trackball_gesture.c` が実装。

- **仕組み**: `src/trackball_gesture.c` が**カスタム入力プロセッサ `zip_gesture`**（compatible `zmk,input-processor-gesture`、binding は `dts/bindings/input_processors/`、ノードは shield dtsi）として実装される。`&pointing_listener` の **Layer 9 override の唯一のプロセッサ**として配置されており（`gesture { layers = <9>; input-processors = <&zip_gesture>; }`）、**「プロセッサが呼ばれた＝Layer 9 が有効」**なのでレイヤー問い合わせ不要。唯一のプロセッサゆえ**生のX/Y変位**を読め、蓄積→閾値超過で方向確定→**キーポジションを press/release**（raise はシステムワークキューに退避）。さらに `ZMK_INPUT_PROC_STOP` を返して**カーソル移動を抑制**（値は変えずにイベントを止め、base チェーンもスキップするので auto-mouse も発火しない）。アクション割り当ては通常のキーマップ側で行い、C の再コンパイル不要。
- **発火先ポジション（重要）**: **実在する未使用ポジション 0〜3**（トップ行＝全レイヤー `&none`／物理的に未実装）を UP/DOWN/LEFT/RIGHT に使う。`Trackball_Gesture`（Layer 9）の**先頭4バインド**がこれにあたる。
  - 当初は末尾に**仮想ポジション 66–69** を新設する設計だったが、ZMK は raise したポジションを**物理レイアウトの position map 経由**で解決するため、複数物理レイアウト（S/M/L）構成では新設ポジションがマップに乗らず**イベントが破棄**され発火しなかった。そこで「すでに正常にルーティングされる実在ポジション」に切り替えた。`src/trackball_gesture.c` の `GESTURE_POS_*` がこの番号（0〜3）を持つ。
  - 当初の名残（`size_l_transform`／`physical_layout_l`／全レイヤーを 66→70 に拡張した分）は**現在は未使用**。害はないが、整理するなら 66 に戻してよい。
- **「どのキーを押しながらか」**: 既存の `&mo`/`&lt`（モーメンタリ／レイヤータップ）パターンで表現。キーをホールド→ジェスチャーレイヤー有効化→そのレイヤーの仮想ポジションのバインドが選ばれる。レイヤーを分ければ「キー種別ごとに別アクション」になる。
- **現在のトリガーキー**: 左親指右の `&lt 9 TAB`（Macレイヤー、Winもフォールスルーで継承）。タップ=Tab、ホールド=ジェスチャー有効。トラックボールは右親指で操作する想定のため、反対側（左親指）をトリガーにしている。元はこのキーが `&mt LEFT_GUI TAB`（ホールド=左GUI）だったが、ホールドをレイヤー9に振り替えた（左GUIのホールド機能は無くなった／Tabタップは維持）。隣の `&lt 4 BACKSPACE`（タップ=BACKSPACE）は据え置き。
- **1モーション1アクション**: 連続した1回のボール移動につき1回だけ発火し、ボールが `GESTURE_IDLE_REARM_MS` の間アイドルになると再武装する（転がし続けても連射しない）。
- **カーソル抑制**: `zip_gesture` が `ZMK_INPUT_PROC_STOP` を返し、後続プロセッサ（transform/temp_layer/scaler）を止めてカーソルを動かさない。値をゼロ化しないため検出は壊れない。
  - かつて `gesture_suppress`（`zip_xy_scaler 0`）で抑制を試みたが、scaler 0 が検出側の読み取り値もゼロ化して失敗。プロセッサ化（リスナー内で raw を読み STOP で抑制）で解消した。
- **方向の調整（カーソル＝矢印を一致）**: `zip_gesture` の override にはカーソル base と**同じ `zip_xy_transform` フラグ**を前段に置いてあり（現状 `INPUT_TRANSFORM_X_INVERT | INPUT_TRANSFORM_Y_INVERT`）、矢印の向きは**常にカーソルの向きと一致**する。C 側の方向マッピングは画面座標規約（post-transform +X=右=RIGHT、+Y=下=DOWN）で固定なので、**向きの調整は `src/trackball_gesture.c` ではなく `&pointing_listener` の3か所の `zip_xy_transform` フラグ**（base / `fast_mouse` / `gesture` override）を**同一に**変更して行う。当初「矢印が逆」だったのは、ジェスチャー検出が transform を通らない生値を読んでいたため。override に同じ transform を追加して解消した（カーソルの `INVERT|INVERT` はそのまま正しい向き）。scroll レイヤー（6/7）の transform は別管理。
- **ジェスチャー中のカーソル抑制**: `zip_gesture` は `event->value = 0` にしたうえで `ZMK_INPUT_PROC_STOP` を返す（値ゼロ化＋チェーン停止の二重）。ただし抑制が効くのは**ジェスチャーレイヤーが実際に有効な間だけ**。トリガーが `&lt 9 TAB`（ホールドタップ）の場合、押してから hold 判定が確定する（tapping-term≈200ms）まではレイヤー9が未有効なので、その間にボールを動かすとカーソルが少し動く。押して一拍おいてから回すと完全に止まる。
- **モジュール越しのリンク制約（重要）**: このリポジトリは ZMK の外部モジュール（`ZMK_EXTRA_MODULES`）として読み込まれる。実測では **`zmk_keymap_layer_active()` や `keymap.c` 由来のイベント（例 `zmk_layer_state_changed`）は外部モジュールからリンクできない**（`undefined reference`）。一方 `position_state_changed`/`activity_state_changed` 系や `raise_*`、入力プロセッサ API はリンクできる（`board.c` も同系統を使用）。そのため `zip_gesture` は**レイヤー問い合わせを一切せず**、「Layer 9 override の唯一のプロセッサとして配置されている＝呼ばれた時点で Layer 9」という構造でこれを回避している。新規 C コードで ZMK 本体関数を呼ぶ際はこの制約に注意。
- **モジュールの dts_root**: カスタム binding を認識させるため `zephyr/module.yml` に `dts_root: .` が必要（`dts/bindings/` を検索対象に追加）。
- **調整パラメータ**（`src/trackball_gesture.c` 冒頭の `#define`）: `GESTURE_THRESHOLD`（発火に必要な変位量／生値ベース）, `GESTURE_IDLE_REARM_MS`（このms間アイドルで再武装）, `GESTURE_RELEASE_DELAY_MS`（press→release 間隔）。

**設定手順（新しいジェスチャーを足す場合）**:
1. トリガーキーを割り当てる。現状は左親指右 `&lt 9 TAB`。別のキーにしたい場合は `&mo 9`（押している間だけ）か `&lt 9 <tap-key>`（タップで通常キー／ホールドでジェスチャー）を使う。
2. `Trackball_Gesture` レイヤー（Layer 9）の**先頭4バインド**（position 0–3＝UP/DOWN/LEFT/RIGHT）を好みのアクションに書き換える。
3. キーごとに別アクションにしたい場合は、新レイヤー（例 Layer 10）を追加し、そのレイヤーへ入るトリガーキー（`&mo 10` 等）を割り当て、`&pointing_listener` に override（`gesture10 { layers = <10>; input-processors = <&zip_gesture>; };`）を足す。`zip_gesture` はどのレイヤーかを問わないので C の変更は不要。

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
