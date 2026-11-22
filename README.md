# DJFX Standard

DJミキサー(DJM-A9)とプレーヤー(CDJ-3000)のエフェクト操作を、DAW上のVST3で再現するプラグイン。
Windows / Mac のVST3。出力ファイルは `DJFX Standard.vst3`(CMakeのターゲット名は内部名のまま `NamiDJFX`)。UIは実機(ミキサー/プレーヤー)の面板をイメージした見た目で、フォントは Ricty Diminished Discord を埋め込んでいます。配色は #0B132B #1C2541 #3A506B #5BC0BE #F3C969 を基調にしています。拍の同期はDAWのテンポに合わせます。

**公式の製品ではありません。** 音・パラメータの範囲は、取扱説明書の記載をもとに耳と数値で寄せた近似です。
配布する場合は、製品名や商標になりうる名前(DJM・CDJ・Pioneer、SPIRAL/HELIX/MOBIUS など)を
`Source/dsp/Engine.h` の `fxName()` と `colorName()` で別の名前に変えてください。

## 入っているもの

### BEAT FX(14種)
DELAY / ECHO / PING PONG / SPIRAL / HELIX / REVERB / FLANGER / PHASER / FILTER / TRIPLET FILTER / TRANS / ROLL / TRIPLET ROLL / MOBIUS

- BPM同期(1/16〜64拍)と、ミリ秒指定の切り替え
- FX FREQUENCY: LOW(〜250Hz) / MID / HI(2.5kHz〜)を組み合わせて選べます(実機と同じ)。選んだ帯域だけにFXがかかります
- 液晶の拍表示、BEAT ◀▶、TAP(タップテンポ)で時間を選びます。TIMEノブを回すと自由な時間(ms)になります
- エフェクトは周りに名前が並んだロータリーで選びます(名前のクリック、つまみのドラッグ、ホイール)
- X-PAD: 触っている間だけON。左右に動かすと時間(拍)が変わります
- LEVEL/DEPTH: 効果の量

### SOUND COLOR FX(6種)
SPACE / DUB ECHO / SWEEP / NOISE / CRUSH / FILTER
COLORノブ(中央でバイパス、CENTER LOCKで中央付近をロック)とPARAMETERノブの2つで操作します。

### JOG(CDJの操作)
バックスピン / ブレーキ(VINYL STOP) / リバース / ビートループ / スクラッチ

- 常に直近10秒を録音しておき、押している間だけ再生位置を操作します。離すと、裏で進んでいた現在位置にクロスフェードで戻ります(CDJのSLIPモードと同じ動き)。
- スクラッチは画面のジョグをドラッグします(ドラッグの回転速度が再生速度になります)。オートメーションで `Scratch Touch` と `Scratch Speed` を書くこともできます。
- 前方向へのスクラッチは「まだ来ていない音」を読めないので、既定では現在位置より先へ進めません。
  `LOOKAHEAD` を上げると、その分だけ全体が遅れて(PDCで補正されます)、先へ進める余白ができます。

### MIDI
| ノート | 動作 |
|---|---|
| C1 (36) | バックスピン(押している間) |
| C#1 | ブレーキ |
| D1 | リバース |
| D#1 | スクラッチ(タッチ) |
| E1 | ビートループ |
| F1 | BEAT FX ON |

## ビルド

```
git clone --depth 1 --branch 8.0.8 https://github.com/juce-framework/JUCE.git external/JUCE
cmake -S . -B build
cmake --build build --config Release --target NamiDJFX_VST3
```

GitHubにpushすると、Actionsが Windows と Mac のVST3をビルドして `Artifacts` に出します。

### テスト
- `test/offline_test.cpp`: DSPの単体テスト(JUCE不要)。全エフェクトの数値確認(エコーの遅延時間、ロールの周期、バックスピンの減速、モビウスの上昇/下降 など)
  `g++ -std=c++17 -O2 -o dsp_test test/offline_test.cpp && ./dsp_test`
- `test/proc_test.cpp`: プラグイン本体の結合テスト(MIDI・パラメータ・モノ・状態保存)

## 近似にしている点

- BEATの値の並びは取扱説明書の範囲から決めています。TRIPLET系(1/12〜12/3)は並びを推定しています。
- SPIRAL は「ピッチが上がっていくフィードバック残響」、MOBIUS は「入力の大きさに追従する、上がり続ける/下がり続ける音」としています。
- HELIX は録音して重ねるループです。LEVEL/DEPTHを右へ回し切ると固定します。
- LEVEL/DEPTHの混ぜ方(DELAY系は足し算、FILTER・FLANGER・PHASER・ROLLなどは原音との入れ替え)は聴感で決めています。

## SOUND COLOR FX (DJM-A9 説明書 p.38-41 準拠)
| FX | COLOR 左 / 右 | PARAMETER |
|---|---|---|
| SPACE | 中+低域 / 中+高域にリバーブ | フィードバック |
| DUB ECHO | 中域 / 高域に反響 | フィードバック |
| SWEEP | ノッチ(帯域が広がる) / バンドパス(帯域が狭まる) | 中心周波数 |
| NOISE | ノイズ用フィルターのカットオフ 下げる / 上げる | ノイズ音量 |
| CRUSH | クラッシュのみ / クラッシュ後にHPF | クラッシュ量 |
| FILTER | LPF / HPF | レゾナンス |

CENTER LOCK: ON のときCOLORを速く回しても中央で止まり、手を離すと解除(止まった側へ戻せば続けて回せる)。
未確認/仮定: DUB ECHOの遅延時間(説明書に記載なし → 1拍で仮置き)、NOISEのEQ/ISOによる音質調整(チャンネルEQなしのため未対応)。

## 再生速度系の長さを拍で決める
- SPIN TIME / BRAKE TIME のつまみを右クリック → 1/16, 1/8, 1/8., 1/4, 1/4., 1/2, 1/2., 1bar, 2bar, 4bar(1/4=四分音符=1拍)。そのときのBPMでmsに換算してつまみを動かし、以後はBPMが変わっても拍の長さに追従。つまみを回すか数字を打つとFREE(ms)に戻る。
- BEAT FX の TIME も右クリックでそのFXの拍リストから選べる(BEATボタンと同じく拍同期になる)。
- 値の欄はダブルクリックで数字だけ入力。単位(ms)は欄の外に固定表示。
