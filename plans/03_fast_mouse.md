# 03. オートマウスレイヤーにおける一時的なカーソルスピード高速化の設定

オートマウスレイヤー（`mouse`、Layer 5）を使用中、左手の特定のキー（`a`, `s`, `d`, `f`）のいずれかを長押ししている間だけ、トラックボールのカーソルスピードを劇的（通常の約2.5倍）に高速化し、キーを離すと即座に通常速度（40%）に戻る機能を実装しました。

---

## 課題
トラックボールを動かしたときに一時的に遷移するオートマウスモードは、細かいカーソル移動を行いやすくするために、速度スケールを通常の40%に抑える設定（`&zip_xy_scaler 40 100`）になっています。
しかし、ディスプレイの端から端へ大きくカーソルを動かしたい場合など、一時的にカーソルの移動速度を速くしたい局面がありました。
直感的かつシームレスに速度を可変にするため、「左手の押しやすいホームポジション周辺キー（`a`, `s`, `d`, `f`）を押している間だけ高速化する」という操作体系を構築しました。

---

## 解決策

### 1. 高速化レイヤー `Mouse_Fast` （Layer 8）の定義
キーを押している間だけ遷移させるための、新しい一時的レイヤー `Mouse_Fast` （Layer 8）を定義しました。
このレイヤーのバインドは他の操作の邪魔をしないよう、すべて透過（`&trans`）に設定しています。

### 2. オートマウスレイヤー（Layer 5 `mouse`）におけるトリガー設定
オートマウスレイヤーの左手ホームポジションキー `a`, `s`, `d`, `f` （インデックス 2, 3, 4, 5）の定義を、Layer 8 への一時遷移を行う `&mo 8` に書き換えました。

```dts
        mouse {
            bindings = <
&trans  &trans  &trans  &trans  &trans  &trans                  &trans  &trans     &trans     &trans     &trans  &trans
&trans  &trans  &trans  &trans  &trans  &trans                  &trans  &trans     &trans     &trans     &trans  &trans
&trans  &mo 8   &mo 8   &mo 8   &mo 8   &trans  &trans  &trans  &trans  &mkp LCLK  &mkp MCLK  &mkp RCLK  &trans  &trans
&trans  &trans  &trans  &trans  &trans  &trans  &trans  &trans  &trans  &mkp LCLK  &mkp MCLK  &mkp RCLK  &trans  &trans
&trans  &trans  &trans  &trans  &trans  &trans  &trans  &trans  &trans  &trans     &trans     &trans     &trans  &trans
            >;
        };
```

### 3. `pointing_listener` への高速プロファイル定義
ZMKの `pointing_listener` 機能を利用し、Layer 8（`Mouse_Fast`）がアクティブになっている間のみ、速度補正を変更するプロファイル `fast_mouse` を追加しました。

通常のオートマウスでは `40 100`（40%）が適用されますが、Layer 8 では `100 100`（100%、つまり通常の2.5倍速）が適用されます。

```dts
&pointing_listener {
    input-processors =
        <&zip_xy_transform (INPUT_TRANSFORM_X_INVERT | INPUT_TRANSFORM_Y_INVERT)>,
        <&zip_temp_layer 5 5000>,
        <&zip_xy_scaler 40 100>;

    fast_mouse {
        layers = <8>;
        input-processors =
            <&zip_xy_transform (INPUT_TRANSFORM_X_INVERT | INPUT_TRANSFORM_Y_INVERT)>,
            <&zip_xy_scaler 100 100>; // 100/100 = 1.0 (通常の約2.5倍速)
    };

    // ... scroll_mac ...
```

---

## 効果と検証
* トラックボールを動かすとオートマウスモード（Layer 5、低速40%）に入ります。
* トラックボールを動かしたまま、左手の `a`, `s`, `d`, `f` のいずれかのキーを長押しすると、瞬時にカーソルが高速移動に切り替わります。
* キーを離せば、遅延なくオートマウスモードの低速（40%）に戻ります。
* これにより、画面全体の大きな移動とピンポイントの細かい操作を、極めてスムーズかつ直感的に両立できるようになりました。
