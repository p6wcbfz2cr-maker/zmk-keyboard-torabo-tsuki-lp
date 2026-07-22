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
| Mouse_Slow | 8 | 低速カーソル。トリガー予定だった`a`/`s`/`d`/`f`長押しは現状使えない（後述） |
| Trackball_Gesture | 9 | トラックボールジェスチャー（現在トリガー未割当・廃止済み。詳細は後述） |

### macOS側のキー入れ替え運用（Karabiner Elements）

このキーボードはWindowsとMac（およびiPad）で併用している。OSごとに異なる`Opt`/`Cmd`配置は、キーボード側のレイヤーではなくmacOS側のKarabiner Elementsで吸収する運用にしている（torabo-tsuki使用時、Karabiner ElementsでOptとCmdを入れ替える設定）。レイヤーの違いでこの入れ替えロジックが変わってしまうことを避けたいため、キーマップ側は常にWindows配列に統一しておき、Mac/iPad側のOS設定で最終的な配置を変える。右側最下段の右から2番目のキーがMac/Winレイヤーとも`&kp RIGHT_ALT`で統一されているのはこのため（Macレイヤーであっても`&kp LEFT_GUI`などにはしない）。

### 主要なカスタムビヘイビア

- **`mt_idle`**: `require-prior-idle-ms = <150>` 付き Mod-Tap。`flavor = "hold-preferred"`（既定はbalancedだったが、割り込みキーとの相対タイミングで判定が揺れる問題があり変更。経緯は `plans/06_mt_idle_lt_flavor_hold_preferred.md` 参照）。`require-prior-idle-ms` は一度80msまで下げたが、hold-preferred化で「ローマ字入力のロールオーバー（za/zu/zo等）がShift+母音に誤爆する」問題が出たため150msへ再度引き上げた（経緯は `plans/07_mt_idle_require_prior_idle_re_raise.md` 参照）。タイピングエリアのホームポジション修飾キー（`a`=右Ctrl、`/`=右Shift、Number/Functionレイヤーの同ポジション）に使用。高速打鍵時の誤爆防止。Number/Functionレイヤーの同ポジション（`N6`/`F11`）は`mt_idle LEFT_SHIFT`のまま据え置き。Macレイヤーの`z`は別ビヘイビア`mt_z`（後述）を使用しており`mt_idle`ではない。
- **`lt_idle`**: 同じく `require-prior-idle-ms = <125>` 付き Layer-Tap。`flavor = "tap-preferred"`（既定のまま、今回変更なし）。`;` キーのスクロールレイヤー遷移に使用。
- **`mt_z`**: Macレイヤーの`z`キー専用のMod-Tap（tap=Z、hold=LEFT_SHIFT）。`flavor = "tap-preferred"`（`tapping-term-ms`満了時のみhold判定し、後続キー押下を判定に一切使わない）。`tapping-term-ms = <60>`（暫定値。50〜80msの範囲で実機タイピングしながら調整すること。短すぎると通常のZタイプでも誤ってhold判定されるリスクが増える）、`require-prior-idle-ms = <150>`（`mt_idle`と同値を流用、直前キーとの間隔が短ければ強制的にtap扱いになる安全網）。**経緯**: 過去に同じ用途を`mt_idle`（`flavor=hold-preferred`等）で実装していたが、ローマ字ロールオーバー（z+a/z+u/z+o→「ざ/ず/ぞ」）がShift+母音に誤爆する問題と、左Shiftが既に左親指キー（`&mt LEFT_SHIFT TAB`）にあり重複する点から、一度`&kp Z`（単純キー）へ差し戻した経緯がある（コミット`9e8520a`「make Mac z key a plain keypress」）。ZMK公式のhold-tap flavor仕様を確認した結果、`hold-preferred`/`balanced`/`tap-unless-interrupted`はいずれも「後続キーが押されたら即hold判定」という経路を持ち、ローマ字ロールオーバーとの衝突は原理的に避けられないと判明。後続キー押下を判定に一切使わない`tap-preferred`のみがこの衝突を避けられるため、z専用に`mt_z`として採用・再導入した（誤爆の完全解消ではなく、体感上ストレスの少ないバランスを狙う実験。経緯は `plans/08_mt_z_tap_preferred.md` 参照）。左Shiftの重複割当（zキーとposition57の左親指キー）は既知のトレードオフとして許容している。
- **`exit_mouse_macro`**: マクロ。`mouse` レイヤー（Layer 5）を OFF 専用の `tog_off 5` で解除する。mouseレイヤーの左手最下段・右から3番目のキー（Macレイヤーの`&lt 2 SPACE`に相当する位置、L layoutではposition 56）に配置。以前は右から3キー（`&lt 2 SPACE`/`&mt LEFT_SHIFT TAB`/`&kp BACKSPACE`相当、position 56/57/58）に配置していたが、右から3番目のみに絞った（2026-07-22、残り2キーは`&trans`に戻し、`excluded-positions`からも除外）。**重要**: `exit_mouse_macro`を配置したキーポジションは、必ず`zip_temp_layer`（`&pointing_listener`内、下記）の`excluded-positions`にも登録すること。登録し忘れると、そのキーを押した瞬間に（`exit_mouse_macro`が実行される前に）ZMK標準の「キー入力でauto-mouseレイヤーを即時無効化する」動作が先に働き、mouseレイヤーが外れてから下位レイヤー（Mac/Win）の元のバインディング（hold-tapの場合はそのtap側）が実行されてしまう。単純な`&kp`ならこの競合が起きても実害は小さいが、`&lt`/`&mt`のようなhold-tap系バインディングの位置に配置する場合は特に注意（タップ側の文字が誤って出力される）。**もう一つの既知の競合**: `zip_temp_layer`はレイヤーが外部から（`&tog_off`経由で）OFFにされたことを検知して内部状態を追従させるが、直後に届いたポインタ移動イベントを無条件でレイヤー再有効化のトリガーとして扱う（ZMK公式テスト`4-deactivated-layer-externally`で仕様として明示）。この再有効化は`require-prior-idle-ms`（`zip_temp_layer`側、実HIDキーコードイベント＝`zmk_keycode_state_changed`の直近発生でのみ抑制される）で防げるはずだが、`&tog_off`はトグルレイヤー系ビヘイビアでありHIDキーコードイベントを発行しないため、このガードが効かない。結果、カーソルを動かした直後（トラックボールの残留入力／イベントキューの順序競合）に`exit_mouse_macro`を押すと、レイヤー5がOFFになった直後にもう一度ONへ戻ってしまい、「押しても効いていない」ように見える。対策として、マクロに実害のないダミーキー`&kp 0`（HID usage 0 = Reserved、空きスロットに0を入れるだけでホスト側には何も送られない）を`&tog_off 5`の前に挟み、`zmk_keycode_state_changed`イベントを発生させて`require-prior-idle-ms`のガードを機能させている（`bindings = <&macro_tap>, <&kp 0>, <&tog_off 5>;`）。

### `&mt`/`&lt` のグローバルoverride

`mt_idle`/`lt_idle`とは別に、ZMK標準ビヘイビアそのもの（`&mt`/`&lt`）にもグローバルoverrideを設定している（`config/keymap.keymap`の`behaviors {}`ブロック直後）。タイピングエリア外（親指キー等）で使われる素の`&mt`/`&lt`バインディング全般に適用される。

- **`&mt`**: `tapping-term-ms = <200>`, `flavor = "balanced"`。
- **`&lt`**: `tapping-term-ms = <200>`（ZMK既定と同値）, `flavor = "hold-preferred"`（既定のtap-preferredから変更）。`&lt 2 SPACE`/`&lt 3 SPACE`/`&lt 6 ENTER`/`&lt 7 ENTER`の4箇所に適用。`mt_idle`のflavor変更と同じ仮説（balanced/tap-preferredは割り込みキーとの相対タイミングで判定が揺れる）で導入した。誤ホールド（意図せずレイヤーが起動する）リスクは上がるトレードオフがある。詳細は `plans/06_mt_idle_lt_flavor_hold_preferred.md` を参照。

### トラックボールジェスチャー（方向→アクション）

「あるキーを押しながらトラックボールを上下左右に振ると、方向に応じたアクションを発火する」機能。`src/trackball_gesture.c` が実装。

**現在の状態**: トリガーキーは左手親指キーの再配置（position57/58を `&mt LEFT_SHIFT TAB` / `&kp BACKSPACE` に変更）に伴い廃止済みで、キーマップ上どこからもLayer 9へ入れない。実装（`src/trackball_gesture.c`、`zip_gesture`、Layer 9定義）自体は残っているが未使用。再度使う場合は下記「設定手順」の1.に従ってトリガーキーを割り当てる。

- **仕組み**: `src/trackball_gesture.c` が**カスタム入力プロセッサ `zip_gesture`**（compatible `zmk,input-processor-gesture`、binding は `dts/bindings/input_processors/`、ノードは shield dtsi）として実装される。`&pointing_listener` の **Layer 9 override の唯一のプロセッサ**として配置されており（`gesture { layers = <9>; input-processors = <&zip_gesture>; }`）、**「プロセッサが呼ばれた＝Layer 9 が有効」**なのでレイヤー問い合わせ不要。唯一のプロセッサゆえ**生のX/Y変位**を読め、蓄積→閾値超過で方向確定→**キーポジションを press/release**（raise はシステムワークキューに退避）。さらに `ZMK_INPUT_PROC_STOP` を返して**カーソル移動を抑制**（値は変えずにイベントを止め、base チェーンもスキップするので auto-mouse も発火しない）。アクション割り当ては通常のキーマップ側で行い、C の再コンパイル不要。
- **発火先ポジション（重要）**: **実在する未使用ポジション 0〜3**（トップ行＝全レイヤー `&none`／物理的に未実装）を UP/DOWN/LEFT/RIGHT に使う。`Trackball_Gesture`（Layer 9）の**先頭4バインド**がこれにあたる。
  - 当初は末尾に**仮想ポジション 66–69** を新設する設計だったが、ZMK は raise したポジションを**物理レイアウトの position map 経由**で解決するため、複数物理レイアウト（S/M/L）構成では新設ポジションがマップに乗らず**イベントが破棄**され発火しなかった。そこで「すでに正常にルーティングされる実在ポジション」に切り替えた。`src/trackball_gesture.c` の `GESTURE_POS_*` がこの番号（0〜3）を持つ。
  - 当初の名残（`size_l_transform`／`physical_layout_l`／全レイヤーを 66→70 に拡張した分）は**現在は未使用**。害はないが、整理するなら 66 に戻してよい。
- **「どのキーを押しながらか」**: 既存の `&mo`/`&lt`（モーメンタリ／レイヤータップ）パターンで表現。キーをホールド→ジェスチャーレイヤー有効化→そのレイヤーの仮想ポジションのバインドが選ばれる。レイヤーを分ければ「キー種別ごとに別アクション」になる。
- **旧トリガーキー**: 左親指右（position58）の `&lt 9 TAB`（Macレイヤー、Winもフォールスルーで継承）。タップ=Tab、ホールド=ジェスチャー有効という割り当てだったが、左手親指キーの再配置に伴い廃止（position58は `&kp BACKSPACE` に変更）。トラックボールは右親指で操作する想定だったため、反対側（左親指）をトリガーにしていた。元はこのキーが `&mt LEFT_GUI TAB`（ホールド=左GUI）だったが、ホールドをレイヤー9に振り替えた経緯があった（左GUIのホールド機能は無くなった／Tabタップは維持）。隣のposition57は現在 `&mt LEFT_SHIFT TAB`（タップ=Tab、ホールド=左Shift）。
- **1モーション1アクション**: 連続した1回のボール移動につき1回だけ発火し、ボールが `GESTURE_IDLE_REARM_MS` の間アイドルになると再武装する（転がし続けても連射しない）。
- **カーソル抑制**: `zip_gesture` が `ZMK_INPUT_PROC_STOP` を返し、後続プロセッサ（transform/temp_layer/scaler）を止めてカーソルを動かさない。値をゼロ化しないため検出は壊れない。
  - かつて `gesture_suppress`（`zip_xy_scaler 0`）で抑制を試みたが、scaler 0 が検出側の読み取り値もゼロ化して失敗。プロセッサ化（リスナー内で raw を読み STOP で抑制）で解消した。
- **方向の調整（カーソル＝矢印を一致）**: `zip_gesture` の override にはカーソル base と**同じ `zip_xy_transform` フラグ**を前段に置いてあり（現状 `INPUT_TRANSFORM_X_INVERT | INPUT_TRANSFORM_Y_INVERT`）、矢印の向きは**常にカーソルの向きと一致**する。C 側の方向マッピングは画面座標規約（post-transform +X=右=RIGHT、+Y=下=DOWN）で固定なので、**向きの調整は `src/trackball_gesture.c` ではなく `&pointing_listener` の3か所の `zip_xy_transform` フラグ**（base / `slow_mouse` / `gesture` override）を**同一に**変更して行う。当初「矢印が逆」だったのは、ジェスチャー検出が transform を通らない生値を読んでいたため。override に同じ transform を追加して解消した（カーソルの `INVERT|INVERT` はそのまま正しい向き）。scroll レイヤー（6/7）の transform は別管理。
- **ジェスチャー中のカーソル抑制**: `zip_gesture` は `event->value = 0` にしたうえで `ZMK_INPUT_PROC_STOP` を返す（値ゼロ化＋チェーン停止の二重）。ただし抑制が効くのは**ジェスチャーレイヤーが実際に有効な間だけ**。トリガーをホールドタップ（例: 旧 `&lt 9 TAB`）にする場合、押してから hold 判定が確定する（tapping-term≈200ms）まではレイヤー9が未有効なので、その間にボールを動かすとカーソルが少し動く。押して一拍おいてから回すと完全に止まる。
- **モジュール越しのリンク制約（重要）**: このリポジトリは ZMK の外部モジュール（`ZMK_EXTRA_MODULES`）として読み込まれる。実測では **`zmk_keymap_layer_active()` や `keymap.c` 由来のイベント（例 `zmk_layer_state_changed`）は外部モジュールからリンクできない**（`undefined reference`）。一方 `position_state_changed`/`activity_state_changed` 系や `raise_*`、入力プロセッサ API はリンクできる（`board.c` も同系統を使用）。そのため `zip_gesture` は**レイヤー問い合わせを一切せず**、「Layer 9 override の唯一のプロセッサとして配置されている＝呼ばれた時点で Layer 9」という構造でこれを回避している。新規 C コードで ZMK 本体関数を呼ぶ際はこの制約に注意。
- **モジュールの dts_root**: カスタム binding を認識させるため `zephyr/module.yml` に `dts_root: .` が必要（`dts/bindings/` を検索対象に追加）。
- **調整パラメータ**（`src/trackball_gesture.c` 冒頭の `#define`）: `GESTURE_THRESHOLD`（発火に必要な変位量／生値ベース）, `GESTURE_IDLE_REARM_MS`（このms間アイドルで再武装）, `GESTURE_RELEASE_DELAY_MS`（press→release 間隔）。

**設定手順（新しいジェスチャーを足す場合）**:
1. トリガーキーを割り当てる。現状はどこにも割り当てられていない（廃止済み）。`&mo 9`（押している間だけ）か `&lt 9 <tap-key>`（タップで通常キー／ホールドでジェスチャー）を使う。
2. `Trackball_Gesture` レイヤー（Layer 9）の**先頭4バインド**（position 0–3＝UP/DOWN/LEFT/RIGHT）を好みのアクションに書き換える。
3. キーごとに別アクションにしたい場合は、新レイヤー（例 Layer 10）を追加し、そのレイヤーへ入るトリガーキー（`&mo 10` 等）を割り当て、`&pointing_listener` に override（`gesture10 { layers = <10>; input-processors = <&zip_gesture>; };`）を足す。`zip_gesture` はどのレイヤーかを問わないので C の変更は不要。

### トラックボール入力処理（`pointing_listener`）

通常時は `zip_xy_accel 65 120`（速度65%〜120%の可変、旧`zip_xy_scaler 65 100`を置き換え）＋ `zip_temp_layer 5 5000`（ポインタ操作でLayer 5へ遷移し、5000ms操作がなければ自動解除。`require-prior-idle-ms`とキー入力による即時解除の仕様は[ZMK公式ドキュメント](https://zmk.dev/docs/keymaps/input-processors/temp-layer)参照。`excluded-positions`に入れていないキー位置を押すと、そのキー入力だけでLayer 5が即時解除される点に注意——`exit_mouse_macro`の項も参照）。

| プロファイル | 発火レイヤー | 速度 |
|---|---|---|
| デフォルト | — | 40% |
| `slow_mouse` | Layer 8 | 40%（低速） |
| `scroll_mac` | Layer 6 | スクロール（Mac方向） |
| `scroll_win` | Layer 7 | スクロール（Win方向） |

`slow_mouse`は`mouse`レイヤー（Layer5）内の`a`/`s`/`d`/`f`位置（L layoutでposition 25/26/27/28）に配置された`&mo 8`。**現在は`zip_temp_layer`の`excluded-positions`にあえて登録していない**（`exit_mouse_macro`の項の注意点と同じ理由で、登録するとこのキーを押した瞬間にLayer5が即時解除されず`&mo 8`が発動してしまう）。一度registerして実機投入した結果、「トラックボールを操作した直後（5000ms以内）にタイピングエリアの`a`（Macレイヤーで`mt_idle RIGHT_CONTROL A`＝右Ctrl）を押しても、Layer5経由で`&mo 8`が発動しCtrlとして機能しない」という副作用が発覚し、タイピングへの影響を避けるため撤回した（2026-07-22）。結果として`a`/`s`/`d`/`f`を押すと通常のキー入力と同様にLayer5が即時解除されるため、**この4キー経由でのMouse_Slow起動は現状事実上使えない**。再度使いたい場合は、これらとは別のトリガー位置を検討すること。

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
