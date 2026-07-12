# トラックボール慣性スクロール

## 目的

ゆっくりしたスクロールの精密さは保ちつつ、速いフリックが終わった後だけ縦・横スクロールを自然に減速継続させる。

通常カーソル、Slow Mouse、オートマウス、トラックボールジェスチャー、クリックには慣性を適用しない。

## 入力経路

スクロール時の処理順は次のとおり。

1. `zip_xy_transform`
2. `zip_xy_scaler`
3. `zip_xy_to_scroll_mapper`
4. `zip_scroll_snap`
5. `zip_scroll_inertia`
6. ZMK HIDスクロールレポート

`zip_scroll_inertia` は、倍率変換と方向固定を終えた `REL_HWHEEL` / `REL_WHEEL` だけを観測する。慣性によるスクロールはプロセッサ自身を仮想入力デバイスとして `input_report_rel()` で発行し、専用の `inertia_listener` からHIDへ送る。

この専用経路により、慣性イベントがtransform、scaler、mapper、scroll-snapを再度通ることを防ぐ。

## OSごとの速度正規化

MacとWindowsでは既存のスクロール倍率が異なるため、プロセッサの2セル引数で処理済み速度を共通の基準へ戻してから発火判定する。

| 経路 | 既存scaler | 慣性プロセッサ | 用途 |
|---|---:|---:|---|
| Mac | `2 / 100` | `<&zip_scroll_inertia 100 2>` | 通常・split |
| Windows | `40 / 100` | `<&zip_scroll_inertia 100 40>` | 通常 |
| Windows split | `2 / 100` | `<&zip_scroll_inertia 100 2>` | 既存倍率を維持 |

scalerを変更する場合は、慣性プロセッサの第2引数も同じ値へ揃える。

## 動作

- `start-speed` 未満のゆっくりした操作では慣性を開始しない。
- 実スクロールが来るたびに保留中の慣性tickを無効化し、最新サンプルから速度を再評価する。
- 逆方向へ `reverse-cancel-threshold` 以上動かすと以前の慣性を即座に破棄する。
- `start-delay-ms` の間、実入力が来なければ慣性を開始する。
- `tick-ms` ごとに `friction-permille / 1000` を速度へ掛け、`stop-speed` 未満で終了する。
- セミコロンキーを離しても、慣性は自然停止するまで継続する。
- 速度と移動端数は固定小数点で保持し、低速なMac経路でも整数丸めによる早期停止を防ぐ。

## 初期設定

設定場所は `boards/shields/torabo_tsuki_lp/torabo_tsuki_lp.dtsi` の `zip_scroll_inertia`。

| プロパティ | 初期値 | 意味 |
|---|---:|---|
| `start-speed` | 1200 counts/s | 慣性を開始する正規化後の最低速度 |
| `stop-speed` | 35 counts/s | 慣性を終了する正規化後の速度 |
| `max-speed` | 6000 counts/s | 取り込む正規化後速度の上限 |
| `friction-permille` | 940 | 1 tick後に残す速度（1000分率） |
| `tick-ms` | 8 ms | 慣性レポートの周期 |
| `start-delay-ms` | 12 ms | 実入力停止から慣性開始までの待ち時間 |
| `sample-timeout-ms` | 50 ms | 速度計算をリセットする入力間隔 |
| `reverse-cancel-threshold` | 1 count | 逆入力キャンセルの最低変位 |

開始速度付近では約0.45秒、上限速度付近では約0.65秒で停止する想定。

## 実機調整

1. 細かい操作でも発動する場合は `start-speed` を上げ、フリックしても発動しない場合は下げる。
2. 滑りすぎる場合は `friction-permille` を下げ、短すぎる場合は上げる。
3. フリック直後の距離が大きすぎる場合は `max-speed` を下げる。

一度に複数の値を変えず、この順番で一項目ずつ実機確認する。
