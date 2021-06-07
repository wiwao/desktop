//2019/11/2   LEPTON3.5の時不具合があったので修正。ESP8266でWIFIの飛距離が伸びなくなったので各所で変更。最終的にLEPTONのリセットコマンドを無効にして前に戻った
//SD使用の場合はコメントを外す。LEPTON3.0、3.5自動判別。Bluetoth GPS ONでも殆ど影響を受けなくなった。
/*SD バージョンは当初5行配信だったが、リセットはいることがありyieldを多く入れる必要がある。4行より少しスピードが落ちるので4行配信に戻した3／31
2018/12/20 High speed version for LEPTON 3.0
This is source(the scketch) for ESP-WROOM-02 with the iPhone using the infrared sensor FLIR LEPTON 3.
i add unique functions, which are not available by genuine nor other parties in the marketed for this sensor and a movie speed(frame rate) close to the limit(9 fps) of LEPTON 3 with the lowest cost.
normally distance between iphone and ESP is 50 m or more if the location is clearly visible between iphone and ESP (5 or less surrounding WIFI stations).
the speed will be dropped In the room with 10 or more WIFI stations (including the arcade where the ceiling on the top).
I recommend that you check the number of WIFI stations before you use.
Normally, even if the image stops, image distribution will start as soon as you move within the reach of the radio waves,
If the image stops for more than 30 seconds, switch off(reset) the ESP side.
If packets are lost, The part where the striped pattern in the horizontal direction appears in the image. So ESP 8266 and iPhone
Are too far between, or interference waves (BlueTooth, WIFI stations, routers, etc.).
-> the iPhone apps, you can compile with my code, which is free or purchase it at apps store. 
-> The board manager should use ESP 8266 2.2.0.
-> When compiling arduino sketch, please set to Flash 80 MHz CPU 160 MHz.
-> As memory usage reaches the upper limit, adding more sources may cause operation instability.
-> It is executable if it is an ESP 8266 board that can use SPI and i2C terminals. however SPI is very sensitive if it is longer pattern or CLK, MOIS and MISO is equal condition
-> We do not suggest to remodel the main body antenna, and we sell boards that made the best use of the theoretical radio field intensity.
Please check the following.
-> At a minimum understanding of SSID, IP address, port number, Arduino source written to ESP 8266
I make this to whom can do it programing.
-> Since it is not software for sale, please refrain from basic questions.
If you can not practice the above We may help you including installed drones, so please check. limited offer since we are two of us.

-> Please set below 1 ~ 4 according to your environment.
** You can use my scketch and code free of charge other than selling, licence is under GPL（GNU General Public License). 
* 2017/8/28,2017/10/19 Takeshi Ono
*/
//#######################################################################
//******１.download below OSC library and copy and paste to the right place*******
//　　　　　https://github.com/sandeepmistry/esp8266-OSC
//========================================================================
//*************** ２.change below for your settings. **************
//========================================================================
#define SSID_X    "KDiPhone"   //iPhone
#define PASS_X    "watanabe3"   //iPhone
#define IP_X      172,20,10,1     //iPhone 固定。修正必要なし
//========================================================================
//***************** ３.if you change port number ****************************
//========================================================================
const unsigned int outPort =    8090;      // 送信は標準で8090 iPhone側は8091
const unsigned int localPort =  8091;      // 受信は標準で8091 iPhone側は8090
//ポート番号変更により複数のiPhoneまたは複数のESP8266の使い分けが可能になります。
//========================================================================
//***************** ４.ESP-WROOM-02 GPIO pin for LED ************************
#define LEDpin 16  //16
//========================================================================

//************** SPI speed setting (no need to change) *******************
//************ below speed setting according our test date not by FLIR ***************
#define CLOCK_TAKE 38500000  //28000000 ~ 39000000 で調整　今回は33500000　で安定。39だと遅くなる　325〜345
//************************************************************************

extern "C" void esp_yield();
//void(* resetFunction) (void) = 0;  
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <SPI.h>
#include <OSCBundle.h>
#include <OSCMessage.h>
#include <Wire.h>
#include <SD.h>

#define WDT_YIELD_TIME_MICROS 10000    //これが効いてるかどうかは？おまじない。
//************LEPTON command *************** 
#define ADDRESS  (0x2A)
//#define AGC (0x01)  //以下必要に応じてコメント外す
#define SYS (0x02)
//#define VID (0x03)
#define OEM (0x08)
#define GET (0x00)
//#define SET (0x01)
#define RUN (0x02)
//**************CS信号高速化用*****************
#define PIN_OUT *(volatile uint32_t *)0x60000300
#define PIN_ENABLE *(volatile uint32_t *)0x6000030C
#define PIN_15  *(volatile uint32_t *)0x60000364
//******************************************


//int read_data1();

//iPhoneテザリングLAN設定用
//  SDカードの場合は下記２行をコメントアウト
char ssid[] = SSID_X;          // your iPhone Name SSID (name)
char pass[] = PASS_X;          // your iPhone テザリングパスワード

WiFiUDP Udp;
// 機種IPアドレス設定
const   IPAddress outIp(IP_X); //iphone、iPad テザリング用アドレス。全機種共通
float   diff;
char    ip_ESP[16];
uint8_t lepton_image1[39360]; //イメージ用メモリ39,360バイト(39K)確保
int     touchX = int(79 % 80), touchY = int(59 / 30) * 60 + int(59 % 30) * 2  + int( 79 / 80);
float   sl_256 = 0.0f;
float   aux;
int     LEPTON;
float   value_min = 0, value_max = 0, tempXY = 0;
float    th_l,th_h;
int     huri1=0,tcount=0,spi_t2=5000;
//*************OSCアドレス********************
  OSCMessage msg1("/m");
  OSCMessage msg2("/i");
//################################# WIFI CONNECT #############################################
void wificonnect(){
  while (WiFi.status() != WL_CONNECTED) {   //WDTリセット時、復旧を早くするためdelayを標準の1/10に設定。短くしてもあまり影響なし
                            digitalWrite(LEDpin, HIGH);
                            delay(5);
                            digitalWrite(LEDpin, LOW);
  }
}
//################################# SET UP ########################################
void setup()
{
//Serial.begin(9600);

//***************SD CARD ID,PASS Read!!**********************
digitalWrite(LEDpin, HIGH);
pinMode(1, OUTPUT);
/*
File myFile;
  if (!SD.begin(2))  return;   //SDカードのCSは　IO2に割り当て　LEPTONは15 SDが認識できなかっらた終了
  myFile = SD.open("iphone.txt");
  if (myFile) {
      String line = myFile.readStringUntil('\n');
      String line1 = myFile.readStringUntil('\n');
      int ssidlen=line.length()+1;
      int passlen=line1.length()+1;
      if( ssidlen ==0 || passlen ==0) return; //書き込まれてなかったら終了
      line.toCharArray(ssid,ssidlen);  //&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&$$$$$$$$$$$$$$$$$$$$$$$$$$$$$#######################################
      
      //Serial.println(ssid);
      line1.toCharArray(pass,passlen);  //&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&$$$$$$$$$$$$$$$$$$$$$$$$$$$$$#######################################
      //Serial.println(pass);
      myFile.close();
     
   } 
   else return;  //iphone.txt がオープンできなかったら終了
   SPI.end();
  
  */ 
//***************LEPTON Read!!**********************
  Wire.begin();
  pinMode(LEDpin, OUTPUT);  
  SPI.begin();                    //SPIの設定はSPI.begin()の後に記述すること！！ システム上何もしなければSSは15
  SPI.setFrequency(CLOCK_TAKE);   //SPIクロック設定
  SPI.setDataMode(SPI_MODE2);    //esp8266のみMODE2 他は MODE3
  PIN_OUT =     (1 << 15);       //SSピン高速化の前処理
  PIN_ENABLE =  (1 << 15);
  delay(100);         //SPI設定を確実にするため
  PIN_15 = 1; //LOW
  //***************
  SPI.setHwCs(true);
  //***************
  WiFi.mode(WIFI_STA);  //WIFI_AP_STA
  WiFi.begin(ssid, pass);  //SDから読み取ったssid と passを設定してWIFIスタート
  wificonnect();
  sprintf(ip_ESP, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]); //ローカルipを文字列に変換iPhoneへ送る下準備
  Udp.begin(localPort);    //OSC受信ポートを開始させておく
  aux=30000;   //LEPTON チップ温度初期値
  lepton_command(OEM, 0x1C >> 2 , GET);  //LEPTON 3.0 or 3.5  FLIR Systems Part Number
  LEPTON=read_data1() ;  //LEPTON3.5=1、LEPTON3.0=2 error=0
  if(LEPTON!=2){spi_t2=4880;}  //lepton3.5の場合6970
}
//################################# SPI date reading command ########################################
//Loop中、どの段階でも画面の先頭を見つけ出して、一挙に　4セグメント＝１画面　分読み込む。２バイト整数配列にしてしまうと
//ESP8266のメモリオーバーとなるため、計算は複雑になるが1バイト１次元配列以外方法なし。ソースはテストに基づくオリジナル。
void read_lepton_frame(void) //<---SPIリードコマンド。
{
  
start:  
  delayMicroseconds(6000);  //********************************LEPTON3.5と3.0共存には6000でないとダメ<-----この数値でなければダメ！！3.0 6000 3.5 7000
  lepton_image1[3280] = 0;      //164*20=3280
  int D_T=0;
  while ((lepton_image1[3280] & 0b01110000) != 0x10) {    //確実に１セグメント目を捕まえるための処理。２０行目にセグメント番号が出力される。
    delayMicroseconds(1150);    //　ESP.wdtFeed()使うとき、ここに絶対必要　しかもdelay(1)である必要あり。yieldではダメ。
    uint16_t data = 0x0F;
    while (data != 0 ) //エラー行を読み飛ばすための処理 セグメントの一番最初の行を捕まえる。0x0fよりこの方が早い
    {
      D_T++;
      SPI.transferBytes(0x0000, lepton_image1, 164);    //標準SDKではバイト単位で処理しているがスピードが間に合わないため1行分一挙に読み込む
      yield();          //最初の行を探すとき永遠にエラーになる場合があるので一応yieldを入れる
      data =  (lepton_image1[1]);
      
      if(D_T >500)    //エラー行が多数続くと止まる可能性があるので、適度に刺激を与える700以下だと接続時切れる可能性あり。3.0と3.5の最適値900
      {
        delayMicroseconds(1000);
        D_T=0;
      }
    }
    SPI.transferBytes(0x0000, &lepton_image1[164], 9676);  //1セグメントの残り（164バイト以降）を一挙に読み込む。
  }
  for (int kk = 9840; kk < 39360; kk+= 9840) {       //セグメント2~4はdelay（５.X）を挟んで一挙に読み込む。delayMicrosecondsを使った方がいくらか良い。4900
   ESP.wdtFeed();
      delayMicroseconds(spi_t2);   // 5153 5150 セグメント間で5000以上が絶対条件。ここで微調整する 3.0 5000 3.5 4850
      SPI.transferBytes(0x0000, &lepton_image1[kk],  9840);
    }

  // ノイズ除法処理追加　SPIノイズをここで除去　効果あり。ただしLEPTONリセットコマンドを実行すると電波が伸びず、妨害電波にも弱い
  int ii=0;  
   for (int i= 0;i<39360;i+=164)         // ノイズ除去処理　ノイズあある場合は再度読み込み
    {
      yield();ESP.wdtFeed();
      if (lepton_image1[i+1] != ii) {
    //      lepton_command(OEM, 0x40 >> 2 , RUN);  //REBOOT コマンド　ここでは復旧する
          delayMicroseconds(20000);
          goto start;
      }
    ii++;
    if (ii==60) ii=0;
    }
  delayMicroseconds(50);
}

//################################## MAIN ROUTINE #####################################################
void loop()
{
  //iPhone 側がプログラム停止している時頻繁にリセットが掛かり、SPIとのバランスが崩れて最終的に画像が止まってしまうので、iPhoneから信号が来ていない時は、LEPTONから
  //データを読み込まないようにした。Udp.endPacket()でパケットの行き場所がなくなりエラーになるのが原因の模様
  yield();
  //*****************Send osc data*****************
  //自分のipを含めた4つのデータをiPhoneへ送信。iPhoneからは設定値が送られてくる

  msg1.add(value_min).add(value_max).add(tempXY).add(ip_ESP);
  sendOSC(msg1);
  yield(); yield();
  //**********Osc Receive**********　OSCデータ受信は最後に処理しないと途中で全体が止まる
  OSCBundle bundle;   //iPhoneから送られてきた設定データを待って、画像送信を開始する
  int size = Udp.parsePacket();
  if (size > 0) {
    while (size--) {
          bundle.fill(Udp.read());
          yield();
    }
    if (!bundle.hasError()) {
      bundle.dispatch("/t", touch);
      send_ios();  //画像読み取り送信ルーチンの本体を実行
    }
  }
  else {digitalWrite(LEDpin, HIGH);delay(20);digitalWrite(LEDpin, LOW);delay(20);}//iPhone側アプリが動いていない場合の処理
  yield();
  //************** LEPTON SIDE STATIONARY TRANSACTION *************************
}

//################################# iPhone Touch XY #################################
void touch(OSCMessage &msg) {
  float sli = sl_256;
  touchX  = msg.getInt(0);    //iPhone上のカーソル位置　X
  touchY  = msg.getInt(1);    //iPhone上のカーソル位置　Y
  sl_256  = msg.getFloat(2);  //ダイナミックレンジスラーダーの数値
  int huri=   msg.getInt(3);  //予備
  th_l    =   msg.getFloat(4);  //カラーレンジ設定下限温度
  th_h    =   msg.getFloat(5);  //カラーレンジ設定上限温度

  if (sl_256 > 0.95 || sl_256 < -0.95) sl_256 = sli;
}
//################################# image sending to iPhone #################################
void sendOSC(OSCMessage &msgx) {
  int upe=0;
    if (WiFi.status() == WL_CONNECTED)  {
       while(Udp.beginPacket(outIp, outPort) != 1){yield(); delay(10);} 
       msgx.send(Udp); 
       while(Udp.endPacket()!=1 && upe<3) {   //WIFIが切れて6個貯まるとリセットがかかるの3個以上で止まりやすい。以前は3個であまり上手くいかなかった。　メモリー残量の関係？バッファがメモリー量を超えるからか？
        
              yield();
              if (WiFi.status() != WL_CONNECTED) {upe=2;goto Loop1;}
              while(Udp.beginPacket(outIp, outPort) != 1)yield();
              digitalWrite(LEDpin, HIGH);delay(9);digitalWrite(LEDpin, LOW);delay(10); 
              yield();msgx.send(Udp);
              upe++;
              
      }   //パケットが紛失するとエラー＝０になる。全体のWDTリセットはこれに起因する模様。
        //再送してもシステムが止まってしまうので、delay調整が最良。9と１０にすればiPhoneのBlueToothにいくらか強くなる。
        //その後エラーが起きたらdelayの後パケットを再送出ことで、ほとんどノイズなくなる！！！！！！！！
    }   
Loop1: if (upe>=2){wificonnect();delayMicroseconds(100);} //もしリセットかかるようならsetup();入れる！！                                                      
    msgx.empty();
}
//################################# image sending to iPhone #################################
void send_ios()
{
  int av5,p, tempXY1, reading = 0;
  int col;
  unsigned int min = 65536;
  unsigned int max = 0;
  float av3;

yomikomi:  read_lepton_frame();   //とにかくLEPTON3からデータを読み込む。
  //**********　temp calculation　**********
  for (int frame_number = 0; frame_number < 240; ++frame_number) {
    int pxx=frame_number * 164;  
    for (int i = 2; i < 82; ++i)
    {
      int px = pxx + 2 * i;
      p = (lepton_image1[px] << 8 | lepton_image1[px + 1]);
      if (p < min) min = p;
      else if(p > max) max = p;
    }
  }
  int touch_A=touchY * 164 + 2 * (touchX+2);
  tempXY1= (lepton_image1[touch_A] << 8 | lepton_image1[touch_A+ 1]); //iPhoneをタッチしたアドレスの温度データ
  diff = (max - min) * (1.0f - abs(sl_256))/256.0f; //高温側を256等分 iPhoneスライダーに連動 標準は0。最高で0.9
  if(diff < 1.0f) diff = 1.0f;  //256階調で表現できなくなったらノイズを減らすため強制的に256階調演算をさせる
  int rrr = (max - min) * sl_256;         //開始低温度側温度
  if (sl_256 > 0) min = min + rrr; //最低温度表示も開始温度に連動させる
  else max = max + rrr; //最高温度表示も開始温度に連動させる minはそのまま
  //if (min<=0) goto yomikomi;
  float keisu,fpatemp_f ;
    if(LEPTON==2){fpatemp_f = - 535.4999f + aux / 100.0f ;keisu=0.03262;}
    else{fpatemp_f = -273.15f ;keisu=0.01;}
  
  value_min = keisu * min     + fpatemp_f;
  value_max = keisu * max     + fpatemp_f;
  tempXY    = keisu * tempXY1 + fpatemp_f;
  if(th_l != 999){   //一応エラーチェック　温度範囲指定の時の事前計算
            float av1=255.0f/(th_h-th_l);
                  av3=av1/(255.0f/(value_max-value_min));
                  av5=(value_min-th_l)*av1;
            yield();
            }
  else {av3=1; av5=0;}   //iPhoneから送られてくる温度差が0以下なら通常の色設定にする
  float avx=av3/diff;
  int   avxx=int(-(int) min*avx+av5);
  //グレースケール変換データ送信。型はStringだが実質バイナリデータとして送信している。数値をASCII変換すると最小で３倍のデータ量になるため、通信条件が悪化する。
  //問題は送信数値が0になったとき。今回は視覚上のデータなので255色中0が1になったところでまったく問題がない。従って0の場合は、1に強制変換。
  ESP.wdtFeed();

  for (int frame_number = 0; frame_number < 240; frame_number += 4) {
    if( ! frame_number % 14) delayMicroseconds(100); //適度にdelayを入れて混線しにくくする（相手に余裕を与える！！）
    for (int ii = 0; ii < 4 ; ++ii) //5行ずつ送信 String型は450文字前後が実験による最大値。よって５行が限界。
    {
      int iix = ii * 81;
      for (int i = 2; i < 82; ++i)
      {
        int ax = (frame_number + ii) * 164 + 2 * i;
        col = (int)(lepton_image1[ax] << 8 | lepton_image1[ax + 1])*avx ; //(int)を入れないと符号無し演算になる!!
        col = col+avxx;   //別計算にしないとシステムダウンする-->メモリーオーバー
        if (col <= 0)   col = 1; //グレーカラー0の場合Stringの終端扱いになるため1にする。視覚上は許容範囲。
        else if(col > 255) col = 255;   
        lepton_image1[iix + i - 1] = col;  //メモリー節約のため　同じ配列を使う。最初の406文字だけを使うので重複しない。
        ESP.wdtFeed();   //ESP8266 CPUを160MHzとした場合ここにもESP.wdtFeed()が必要。2個である必要あり
        ESP.wdtFeed();   //<<<<<<---これを入れないのが諸悪の原因。yieldは遅くなるだけ。
      }
      lepton_image1[iix] = frame_number + ii; //一番最初のその他の行にはを行番号代入
      ESP.wdtFeed();  //ハイスピードではこれを入れるNo1 
      yield();        //ハイスピードではこれを入れるNo2
    }
    if ( frame_number == 0 ) lepton_image1[0] = 240; //最初の行の初めにに240を書き込んで　MacまたはiPhoneに渡す。0では改行になってしまうため
    lepton_image1[325] = '\0'; //ストリング型の終端処理
    //yield();yield();yield();//安定化のために必要
    //*****************Send osc data********************
    msg2.add( (char *)lepton_image1 );
    sendOSC(msg2);
    yield();      // <----絶対必要
    //*****************Send osc data********************
    delayMicroseconds(4);//入れた方が良いが入れなくてもいい
  }
    //チップ温度測定。 
    if(LEPTON == 2){
      lepton_command(SYS, 0x15 >> 2 , GET); //i2C通信。温度読み取り : 0x15 = aux LEPTON3 0x14 = chip LEPTON3 センサーチップ温度LEPTON1と3では微妙に違う。マニュアルミス！！
      aux = read_data() ;
    }
    
}
//################################# FLIR SDK　i2C command ########################################
void lepton_command(unsigned int moduleID, unsigned int commandID, unsigned int command) 
{
  byte error;
  Wire.beginTransmission(ADDRESS);
  // Command Register is a 16-bit register located at Register Address 0x0004
  Wire.write(0x00);
  Wire.write(0x04);
  if (moduleID == 0x08) Wire.write(0x48);  //OEM module ID
  else Wire.write(moduleID & 0x0f);
  Wire.write( ((commandID << 2 ) & 0xfc) | (command & 0x3));
  Wire.endTransmission();
 // if (Wire.endTransmission() != 0) Serial.println(error);
}
//################################# FLIR SDK　i2C command ########################################
void set_reg(unsigned int reg)
{
  byte error;
  Wire.beginTransmission(ADDRESS); // transmit to device #4
  Wire.write(reg >> 8 & 0xff);
  Wire.write(reg & 0xff);            // sends one byte
  Wire.endTransmission();
 // if (Wire.endTransmission() != 0) Serial.println(error);
}
//################################# FLIR SDK　i2C command ########################################
int read_reg(unsigned int reg)
{
  int reading = 0;
  set_reg(reg);
  Wire.requestFrom(ADDRESS, 2);
  reading = Wire.read();  // receive high byte (overwrites previous reading)
  reading = reading << 8;    // shift high byte to be high 8 bits
  reading |= Wire.read(); // receive low byte as lower 8 bits
  return reading;
}

//################################# FLIR SDK　i2C command ########################################
int read_data()
{
  int data;
  int payload_length;
  yield();
  while (read_reg(0x2) & 0x01) int damy=1; //Serial.println("busy");
  payload_length = read_reg(0x6);
  Wire.requestFrom(ADDRESS, payload_length);
  for (int i = 0; i < (payload_length / 2); i++) data = Wire.read() << 8 | Wire.read();
  return data;
}
//################################# FLIR SDK　i2C command ########################################
  int read_data1()
{
  int i;
  int data;
  int payload_length;
  char lp_no[32];

  while (read_reg(0x2) & 0x01) int damy=1;//Serial.println("busy");
  payload_length = read_reg(0x6);
  //Serial.print("Length="); 
  //Serial.println(payload_length);
  Wire.requestFrom(ADDRESS, payload_length);
  for (i = 0; i < payload_length; i++)
  {
    lp_no[i] = Wire.read() ;
  }
  //i2Cなので 16bit中上位下位が逆なので注意。この場合は逆のまま判断してる。本来なら 71=LEPTON3.5  62=LEPTON3
  if (lp_no[6]=='1' && lp_no[7]=='7')return 1; //LEOTON3.5
  else if (lp_no[6]=='6' && lp_no[7]=='2') return 2;   //LEPTON3.0
  else return 0;
}
