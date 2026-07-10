# 05. マウスレイヤー（Layer 5）のホールド式への移行（オートマウス廃止）

トラックボールを動かすと自動で Layer 5（`mouse`）に一時遷移する `zip_temp_layer` 方式を廃止し、`/`（スラッシュ）キーを長押ししている間だけ Layer 5 が有効になる方式に変更しました。ブランチ (`worktree-hold-mouse-layer`) 上での試験導入です。

---

## 課題

* オートマウス（`zip_temp_layer 5 5000`）は、トラックボールを動かしてから最大5秒間 Layer 5 に居座る仕組みだった。
* トラックボール操作の直後にすぐタイピングする癖が抜けず、Layer 5 中はホームロー（`a`/`s`/`d`/`f` が `&mo 8`、`j`/`k`/`l`・`n`/`m`/`,`/`.` がクリック）が奪われるため、意図しない誤入力・クリックが頻発していた。
* CLAUDE.md には `zip_temp_layer 5 1200`（1.2秒）と記載されていたが、実際のコードは `5000`（5秒）に伸びていたことが調査で判明し、居座り時間の長さが誤入力を助長していたと考えられる。

---

## 解決策

### 1. `/` キーのホールド動作を Layer 5 遷移に差し替え

`/` キーは元々 `&mt_idle RIGHT_SHIFT SLASH`（Tap=`/`、Hold=右Shift）だった。既存の `lt_idle`（[[02_semicolon_scroll]] で導入、ホールドでレイヤー・タップでキーの構造）をそのまま流用し、`&lt_idle 5 SLASH` に変更した。

```dts
&none  &mt_idle LEFT_SHIFT Z  &kp X  &kp C  &kp V  &kp B  &kp LANGUAGE_2  &kp LANGUAGE_1  &kp N  &kp M  &kp COMMA  &kp DOT  &lt_idle 5 SLASH  &none
```

新規ビヘイビア定義は不要。Tap=`/`（変化なし）、Hold=Layer 5 起動、に変わる。`Win` レイヤーは同じ位置が `&trans` のため自動継承される。

右Shiftをホールドで出す手段は、右親指の `&mt RSHIFT ESC`（Tap=ESC、Hold=右Shift）に既に別途存在するため、実質的な機能欠落はない。

### 2. 自動遷移（`zip_temp_layer`）とその周辺コードを完全撤去

* `&pointing_listener` のベース `input-processors` から `<&zip_temp_layer 5 5000>` を削除（`zip_xy_transform` → `zip_xy_accel` の2段構成に）。
* `zip_temp_layer` ノード定義（`excluded-positions` 含む）を削除。
* `mouse` レイヤーの `exit_mouse_macro`（`tog_off 5` で強制解除するマクロ、`lt 9 TAB` と同じ列に配置）を `&trans` に戻して削除。ホールド式では離せば自動的に抜けるため不要になった。
* `exit_mouse_macro` 専用に定義していた `tog_off`（`toggle-mode = "off"` の `&mo`/`&tog` 亜種）ビヘイビアも削除。

### 3. ダブルトラックボール構成（2個目のトラックボール）も同様に修正

`snippets/input-split-listener/input-split-listener.overlay` の `pointing_device_split_listener` も同じ `&zip_temp_layer 5 5000` を phandle 参照していたため、こちらからも該当行を削除した。共有ノードを削除する前に両方の参照を消す必要がある点に注意（片方だけ消すと `undefined reference` でビルドが壊れる）。

`fast_mouse`（Layer 8）・スクロール（Layer 6/7）・ジェスチャー（Layer 9）の仕組みは変更していない。いずれも独立した momentary/layer-tap トリガーで、Layer 5 への入り方とは無関係。

---

## 効果と検証

実機（run #151 のファームウェアを central/peripheral 両方に書き込み）で確認済み。

* デフォルトレイヤでトラックボールを動かすとカーソルは常に移動するが、クリックは効かない（設計通り。カーソル移動は `pointing_listener` のベースチェーンでレイヤーに関係なく常時処理されるため、`mouse` レイヤーが入っているかどうかとは無関係）。
* `/` キーをホールドしている間は Layer 5 が有効になり、J/K/L・N/M/,/. でクリックが機能することを確認。離せば即座に通常のタイピングへ戻る。
* 当初「オートマウスレイヤが残ったままでクリックが効かない」という報告があったが、これはデフォルトレイヤでの確認（クリックバインドが存在しない状態）による誤解で、実際には自動遷移は正しく廃止されていた。`settings_reset` を試したが元々不要だった。
* ブランチでの試験運用のため、`master` にはまだマージしていない。しばらく実運用してみて、使用感に問題なければ `master` へのマージを検討する。
