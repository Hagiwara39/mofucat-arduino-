/*
  もふもふ猫ボール - 触ったら「にゃー」(Phase 2)

  配線:
    静電容量センサー: D2 → 1MΩ → ホイル、ホイル → D4
    DFPlayer: TX→D10, RX←(1kΩ)←D11, VCC→5V, GND→GND, SPK1/SPK2→スピーカー

  使い方:
    ★ファーを重ねた「完成形」の状態で電源を入れる★
    (起動時の値を「触れていない基準」として覚えるため)
    シリアルモニタ(9600)で value / baseline / しきい値 を見て、
    MARGIN を調整する。
*/
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include <CapacitiveSensor.h>

CapacitiveSensor sensor = CapacitiveSensor(2, 4);   // (送信, 受信)
SoftwareSerial mySerial(10, 11);                    // RX, TX
DFRobotDFPlayerMini myDFPlayer;

// ---- 改良スイッチ(1つずつ true にして効果を試す。全部falseが前の安定版) ----
const bool USE_WARMUP   = true;  // 起動直後の測定を捨てる(効果あり)
const bool USE_DEBOUNCE = false; // 3回連続で超えたときだけ「触れた」(悪化したのでオフ)
const bool USE_TRACKING = true;  // 触れていない間、基準をゆっくり追従(効果あり)

const long MARGIN = 300;              // 基準よりこれだけ増えたら「触れた」。調整する
const unsigned long COOLDOWN_MS = 1500; // 連続再生を防ぐ待ち時間

long baseline = 0;
bool wasTouched = false;
int touchCount = 0;
unsigned long lastPlay = 0;

void setup() {
  Serial.begin(9600);
  mySerial.begin(9600);

  // 基準値が勝手に取り直されないようにする
  sensor.set_CS_AutocaL_Millis(0xFFFFFFFF);

  // DFPlayer初期化(失敗したら数回やり直す)
  delay(1500);
  bool ok = false;
  for (int i = 0; i < 5 && !ok; i++) {
    ok = myDFPlayer.begin(mySerial);
    if (!ok) {
      Serial.println(F("DFPlayer init failed, retry..."));
      delay(1000);
    }
  }
  if (!ok) {
    Serial.println(F("DFPlayer init NG: 配線・SDカードを確認"));
    while (true);
  }
  Serial.println(F("DFPlayer init OK"));
  myDFPlayer.volume(30);

  // 起動直後は値が不安定なので、最初の30回は捨てる(ウォームアップ)
  Serial.println(F("ウォームアップ中... 触らないでください"));
  if (USE_WARMUP) {
    for (int i = 0; i < 30; i++) {
      sensor.capacitiveSensor(30);
      delay(50);
    }
  }

  // 基準値を測る(この間も触らないこと)
  long sum = 0;
  for (int i = 0; i < 40; i++) {
    sum += sensor.capacitiveSensor(30);
    delay(50);
  }
  baseline = sum / 40;
  Serial.print(F("baseline = "));
  Serial.println(baseline);
}

void loop() {
  long value = sensor.capacitiveSensor(30);
  bool above = (value > baseline + MARGIN);

  // 連続して3回超えたときだけ「触れた」とする(一瞬の揺れを無視)
  if (above) touchCount++; else touchCount = 0;
  bool touched = USE_DEBOUNCE ? (touchCount >= 3) : above;

  // 触れていないあいだは、基準をゆっくり現在値に近づける(自然なずれに追従)
  if (USE_TRACKING && !above) {
    baseline += (value - baseline) / 50;
  }

  // 「触れていない→触れた」に変わった瞬間だけ鳴らす
  if (touched && !wasTouched && millis() - lastPlay > COOLDOWN_MS) {
    myDFPlayer.play(1);
    lastPlay = millis();
    Serial.println(F(">>> にゃー"));
  }
  wasTouched = touched;

  Serial.print(F("value="));
  Serial.print(value);
  Serial.print(F("  threshold="));
  Serial.println(baseline + MARGIN);
  delay(50);
}
