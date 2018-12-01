//- -----------------------------------------------------------------------------------------------------------------------
// AskSin++
// 2016-10-31 papa Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
// 2018-12-01 jp112sdl Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------
// define this to read the device id, serial and device type from bootloader section
// #define USE_OTA_BOOTLOADER
#define USE_HW_SERIAL
// #define NDEBUG

#include <GxEPD.h>
//#include <GxGDEW042Z15/GxGDEW042Z15.h>    // 4.2" b/w/r
#include <GxGDEW042T2/GxGDEW042T2.h>      // 4.2" b/w
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>
#include "U8G2_FONTS_GFX.h"

#define GxRST  9
#define GxBUSY 3
#define GxDC   30
#define GxCS   23

GxIO_Class io(SPI, GxCS,  GxDC,  GxRST); // arbitrary selection of 8, 9 selected for default of GxEPD_Class
GxEPD_Class display(io , GxRST, GxBUSY ); // default selection of (9), 7

U8G2_FOR_ADAFRUIT_GFX u8g2_for_adafruit_gfx;
U8G2_FONTS_GFX u8g2Fonts(display);


#define EI_NOTEXTERNAL
#include <EnableInterrupt.h>

#include <AskSinPP.h>
#include <LowPower.h>

#include <Register.h>
#include <MultiChannelDevice.h>

#define CONFIG_BUTTON_PIN  31
#define LED_PIN_1          4
#define LED_PIN_2          5
#define BTN1_PIN           14
#define BTN2_PIN           15
#define BTN3_PIN           16
#define BTN4_PIN           17
#define BTN5_PIN           18
#define BTN6_PIN           19
#define BTN7_PIN           20
#define BTN8_PIN           21
#define BTN9_PIN           28
#define BTN10_PIN          29

#define TEXT_LENGTH        16
#define DISPLAY_LINES      10

#define DISPLAY_ROTATE     3 // 0 = 0° , 1 = 90°, 2 = 180°, 3 = 270°

// number of available peers per channel
#define PEERS_PER_CHANNEL 8
#define LOWBAT_VOLTAGE    22
#define MSG_START_KEY     0x02
#define MSG_TEXT_KEY      0x12
#define MSG_ICON_KEY      0x13

#include "Icons.h"

// all library classes are placed in the namespace 'as'
using namespace as;


// define all device properties
const struct DeviceInfo PROGMEM devinfo = {
  {0xf3, 0x43, 0x00},          // Device ID
  "JPDISEP000",                // Device Serial
  {0xf3, 0x43},                // Device Model
  0x10,                        // Firmware Version
  as::DeviceType::Remote,      // Device Type
  {0x00, 0x00}                 // Info Bytes
};

typedef struct {
  uint8_t Icon = 0xff;
  String Text = "";
} DisplayLine;
DisplayLine DisplayLines[DISPLAY_LINES + 1];

struct {
  bool LeftAligned = false;
  uint16_t clFG = GxEPD_BLACK;
  uint16_t clBG = GxEPD_WHITE;
  bool showLinesBetweenRows = false;
} DisplayConfig;

String List1Texts[(DISPLAY_LINES + 1 ) * 2];
bool mustUpdateDisplay = false;

/**
   Configure the used hardware
*/
typedef AvrSPI<10, 11, 12, 13> SPIType;
typedef Radio<SPIType, 6> RadioType;
typedef DualStatusLed<LED_PIN_1, LED_PIN_2> LedType;
typedef AskSin<LedType, BatterySensor, RadioType> Hal;
Hal hal;
BurstDetector<Hal> bd(hal);

DEFREGISTER(Reg0, MASTERID_REGS, DREG_LOCALRESETDISABLE, DREG_DISPLAY, 0x90)
class DispList0 : public RegList0<Reg0> {
  public:
    DispList0(uint16_t addr) : RegList0<Reg0>(addr) {}

    bool showLinesBetweenRows (uint8_t value) const {
      return this->writeRegister(0x90, 0x01, 0, value & 0xff);
    }
    uint8_t showLinesBetweenRows () const {
      return this->readRegister(0x90, 0x01, 0, false);
    }

    void defaults () {
      localResetDisable(false);
      displayInverting(false);
      statusMessageTextAlignmentLeftAligned(false);
      showLinesBetweenRows(false);
      clear();
    }
};

DEFREGISTER(RemoteReg1, CREG_AES_ACTIVE, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55)
class RemoteList1 : public RegList1<RemoteReg1> {
  public:
    RemoteList1 (uint16_t addr) : RegList1<RemoteReg1>(addr) {}

    bool TEXT1 (uint8_t value[TEXT_LENGTH]) const {
      for (int i = 0; i < TEXT_LENGTH; i++) {
        this->writeRegister(0x36 + i, value[i] & 0xff);
      }
      return true;
    }
    String TEXT1 () const {
      String a = "";
      for (int i = 0; i < TEXT_LENGTH; i++) {
        byte b = this->readRegister(0x36 + i, 0x20);
        if (b == 0) b = 32;
        a += char(b);
      }
      return a;
    }

    bool TEXT2 (uint8_t value[TEXT_LENGTH]) const {
      for (int i = 0; i < TEXT_LENGTH; i++) {
        this->writeRegister(0x46 + i, value[i] & 0xff);
      }
      return true;
    }
    String TEXT2 () const {
      String a = "";
      for (int i = 0; i < TEXT_LENGTH; i++) {
        byte b = this->readRegister(0x46 + i, 0x20);
        if (b == 0) b = 32;
        a += char(b);
      }
      return a;
    }

    void defaults () {
      clear();
      uint8_t initValues[TEXT_LENGTH] = {32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32};
      TEXT1(initValues);
      TEXT2(initValues);
    }
};

class DispChannel : public Channel<Hal, RemoteList1, EmptyList, DefList4, PEERS_PER_CHANNEL, DispList0>,
  public Button {

  private:
    uint8_t       repeatcnt;
    volatile bool isr;
    uint8_t       commandIdx;
    uint8_t       command[112];

  public:

    DispChannel () : Channel(), repeatcnt(0), isr(false), commandIdx(0) {}
    virtual ~DispChannel () {}

    Button& button () {
      return *(Button*)this;
    }

    void configChanged() {
      List1Texts[(number() - 1)  * 2] = this->getList1().TEXT1();
      List1Texts[((number() - 1) * 2) + 1] = this->getList1().TEXT2();
      DDEC(number()); DPRINT(F(" - TEXT1 = ")); DPRINTLN(this->getList1().TEXT1());
      DDEC(number()); DPRINT(F(" - TEXT2 = ")); DPRINTLN(this->getList1().TEXT2());
    }

    uint8_t status () const {
      return 0;
    }

    uint8_t flags () const {
      return 0;
    }

    bool process (const Message& msg) {
      DPRINTLN("process Message");
      return true;
    }

    bool process (const ActionCommandMsg& msg) {
      static bool getText = false;
      String Text = "";
      for (int i = 0; i < msg.len(); i++) {
        command[commandIdx] = msg.value(i);
        commandIdx++;
      }

      if (msg.eot(AS_ACTION_COMMAND_EOT)) {
        static uint8_t currentLine = 0;
        if (commandIdx > 0 && command[0] == MSG_START_KEY) {
          DPRINT("RECV: ");
          for (int i = 0; i < commandIdx; i++) {
            DHEX(command[i]); DPRINT(" ");

            if (getText) {
              if (command[i] >= 0x20 && command[i] < 0x80) {
                char c = command[i];
                Text += c;
              } else {
                getText = false;
                DisplayLines[currentLine].Text = Text;
                //DPRINTLN(""); DPRINT("DisplayLines[" + String(currentLine) + "].Text = "); DPRINTLN(Text);
              }
            }

            if (command[i] == MSG_TEXT_KEY) {
              if (command[i + 1] < 0x80) {
                getText = true;
              } else {
                uint8_t textNum = command[i + 1] - 0x80;
                //DPRINTLN(""); DPRINT("USE PRECONF TEXT NUMBER "); DDEC(textNum); DPRINT(" = "); DPRINTLN(List1Texts[textNum]);
                DisplayLines[currentLine].Text =  List1Texts[textNum];
              }
            }

            if (command[i] == MSG_ICON_KEY) {
              DisplayLines[currentLine].Icon = command[i + 1] - 0x80;
            }

            if (command[i] == AS_ACTION_COMMAND_EOL) {
              if (i > 1) currentLine++;
              Text = "";
              getText = false;
            }
          }
        }
        DPRINTLN("");
        currentLine = 0;
        commandIdx = 0;
        memset(command, 0, sizeof(command));

        for (int i = 0; i < DISPLAY_LINES; i++) {
          DPRINT("LINE "); DDEC(i + 1); DPRINT(" ICON = "); DDEC(DisplayLines[i].Icon); DPRINT(" TEXT = "); DPRINT(DisplayLines[i].Text); DPRINTLN("");
        }
        mustUpdateDisplay = true;
      }

      return true;
    }

    bool process (const RemoteEventMsg& msg) {
      return true;
    }

    virtual void state(uint8_t s) {
      DHEX(Channel::number());
      Button::state(s);
      RemoteEventMsg& msg = (RemoteEventMsg&)this->device().message();
      msg.init(this->device().nextcount(), this->number(), repeatcnt, (s == longreleased || s == longpressed), this->device().battery().low());
      if ( s == released || s == longreleased) {
        this->device().sendPeerEvent(msg, *this);
        repeatcnt++;
      }
      else if (s == longpressed) {
        this->device().broadcastPeerEvent(msg, *this);
      }
    }

    uint8_t state() const {
      return Button::state();
    }

    bool pressed () const {
      uint8_t s = state();
      return s == Button::pressed || s == Button::debounce || s == Button::longpressed;
    }
};

class DisplayDevice : public ChannelDevice<Hal, VirtBaseChannel<Hal, DispList0>, 11, DispList0> {
  public:
    VirtChannel<Hal, DispChannel, DispList0> c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11;
  public:
    typedef ChannelDevice<Hal, VirtBaseChannel<Hal, DispList0>, 11, DispList0> DeviceType;
    DisplayDevice (const DeviceInfo& info, uint16_t addr) : DeviceType(info, addr) {
      DeviceType::registerChannel(c1, 1);
      DeviceType::registerChannel(c2, 2);
      DeviceType::registerChannel(c3, 3);
      DeviceType::registerChannel(c4, 4);
      DeviceType::registerChannel(c5, 5);
      DeviceType::registerChannel(c6, 6);
      DeviceType::registerChannel(c7, 7);
      DeviceType::registerChannel(c8, 8);
      DeviceType::registerChannel(c9, 9);
      DeviceType::registerChannel(c10, 10);
      DeviceType::registerChannel(c11, 11);
    }
    virtual ~DisplayDevice () {}

    DispChannel& disp1Channel ()  {
      return c1;
    }
    DispChannel& disp2Channel ()  {
      return c2;
    }
    DispChannel& disp3Channel ()  {
      return c3;
    }
    DispChannel& disp4Channel ()  {
      return c4;
    }
    DispChannel& disp5Channel ()  {
      return c5;
    }
    DispChannel& disp6Channel ()  {
      return c6;
    }
    DispChannel& disp7Channel ()  {
      return c7;
    }
    DispChannel& disp8Channel ()  {
      return c8;
    }
    DispChannel& disp9Channel ()  {
      return c9;
    }
    DispChannel& disp10Channel ()  {
      return c10;
    }

    virtual void configChanged () {
      DPRINTLN(F("CONFIG LIST0 CHANGED"));
      DPRINT(F("showLinesBetweenRows                  : ")); DDECLN(this->getList0().showLinesBetweenRows());
      DPRINT(F("displayInverting                      : ")); DDECLN(this->getList0().displayInverting());
      DPRINT(F("statusMessageTextAlignmentLeftAligned : ")); DDECLN(this->getList0().statusMessageTextAlignmentLeftAligned());

      DisplayConfig.showLinesBetweenRows = this->getList0().showLinesBetweenRows();

      if (this->getList0().displayInverting()) {
        DisplayConfig.clFG = GxEPD_WHITE;
        DisplayConfig.clBG = GxEPD_BLACK;
      } else {
        DisplayConfig.clFG = GxEPD_BLACK;
        DisplayConfig.clBG = GxEPD_WHITE;
      }
      DisplayConfig.LeftAligned = this->getList0().statusMessageTextAlignmentLeftAligned();
      //mustUpdateDisplay = true;
    }
};
DisplayDevice sdev(devinfo, 0x20);
ConfigButton<DisplayDevice> cfgBtn(sdev);

static void isr1 () {
  sdev.disp1Channel().irq();
}
static void isr2 () {
  sdev.disp2Channel().irq();
}
static void isr3 () {
  sdev.disp3Channel().irq();
}
static void isr4 () {
  sdev.disp4Channel().irq();
}
static void isr5 () {
  sdev.disp5Channel().irq();
}
static void isr6 () {
  sdev.disp6Channel().irq();
}
static void isr7 () {
  sdev.disp7Channel().irq();
}
static void isr8 () {
  sdev.disp8Channel().irq();
}
static void isr9 () {
  sdev.disp9Channel().irq();
}
static void isr10 () {
  sdev.disp10Channel().irq();
}

void initISR() {
  if ( digitalPinToInterrupt(BTN1_PIN) == NOT_AN_INTERRUPT ) enableInterrupt(BTN1_PIN, isr1, CHANGE); else attachInterrupt(digitalPinToInterrupt(BTN1_PIN), isr1, CHANGE);
  if ( digitalPinToInterrupt(BTN2_PIN) == NOT_AN_INTERRUPT ) enableInterrupt(BTN2_PIN, isr2, CHANGE); else attachInterrupt(digitalPinToInterrupt(BTN2_PIN), isr2, CHANGE);
  if ( digitalPinToInterrupt(BTN3_PIN) == NOT_AN_INTERRUPT ) enableInterrupt(BTN3_PIN, isr3, CHANGE); else attachInterrupt(digitalPinToInterrupt(BTN3_PIN), isr3, CHANGE);
  if ( digitalPinToInterrupt(BTN4_PIN) == NOT_AN_INTERRUPT ) enableInterrupt(BTN4_PIN, isr4, CHANGE); else attachInterrupt(digitalPinToInterrupt(BTN4_PIN), isr4, CHANGE);
  if ( digitalPinToInterrupt(BTN5_PIN) == NOT_AN_INTERRUPT ) enableInterrupt(BTN5_PIN, isr5, CHANGE); else attachInterrupt(digitalPinToInterrupt(BTN5_PIN), isr5, CHANGE);
  if ( digitalPinToInterrupt(BTN6_PIN) == NOT_AN_INTERRUPT ) enableInterrupt(BTN6_PIN, isr6, CHANGE); else attachInterrupt(digitalPinToInterrupt(BTN6_PIN), isr6, CHANGE);
  if ( digitalPinToInterrupt(BTN7_PIN) == NOT_AN_INTERRUPT ) enableInterrupt(BTN7_PIN, isr7, CHANGE); else attachInterrupt(digitalPinToInterrupt(BTN7_PIN), isr7, CHANGE);
  if ( digitalPinToInterrupt(BTN8_PIN) == NOT_AN_INTERRUPT ) enableInterrupt(BTN8_PIN, isr8, CHANGE); else attachInterrupt(digitalPinToInterrupt(BTN8_PIN), isr8, CHANGE);
  if ( digitalPinToInterrupt(BTN9_PIN) == NOT_AN_INTERRUPT ) enableInterrupt(BTN9_PIN, isr9, CHANGE); else attachInterrupt(digitalPinToInterrupt(BTN9_PIN), isr9, CHANGE);
  if ( digitalPinToInterrupt(BTN10_PIN) == NOT_AN_INTERRUPT ) enableInterrupt(BTN10_PIN, isr10, CHANGE); else attachInterrupt(digitalPinToInterrupt(BTN10_PIN), isr10, CHANGE);
}

void setup () {
  for (int i = 0; i < DISPLAY_LINES; i++) {
    DisplayLines[i].Icon = 0xff;
    DisplayLines[i].Text = "";
  }

  initIcons();

  display.init(57600);
  DINIT(57600, ASKSIN_PLUS_PLUS_IDENTIFIER);
  sdev.init(hal);
  sdev.disp1Channel().button().init(BTN1_PIN);
  sdev.disp2Channel().button().init(BTN2_PIN);
  sdev.disp3Channel().button().init(BTN3_PIN);
  sdev.disp4Channel().button().init(BTN4_PIN);
  sdev.disp5Channel().button().init(BTN5_PIN);
  sdev.disp6Channel().button().init(BTN6_PIN);
  sdev.disp7Channel().button().init(BTN7_PIN);
  sdev.disp8Channel().button().init(BTN8_PIN);
  sdev.disp9Channel().button().init(BTN9_PIN);
  sdev.disp10Channel().button().init(BTN10_PIN);
  buttonISR(cfgBtn, CONFIG_BUTTON_PIN);
  initISR();
  bd.enable(sysclock);
  hal.activity.stayAwake(seconds2ticks(15));
  hal.battery.low(LOWBAT_VOLTAGE);
  // measure battery every 12 hours
  hal.battery.init(seconds2ticks(60UL * 60 * 12 * 0.88), sysclock);
  sdev.initDone();
  DDEVINFO(sdev);
  sdev.disp1Channel().changed(true);
  display.drawPaged(showInitDisplay);
}

void loop() {
  bool worked = hal.runready();
  bool poll = sdev.pollRadio();
  if ( worked == false && poll == false ) {
    hal.activity.savePower<Sleep<>>(hal);
  }
  updateDisplay(mustUpdateDisplay);
}

void showInitDisplay() {
  HMID devid;
  sdev.getDeviceID(devid);
  uint8_t serial[11];
  sdev.getDeviceSerial(serial);
  serial[10] = 0;
  initDisplay(serial);
}

void updateDisplay(bool doit) {
  if (doit) {
    mustUpdateDisplay = false;
    display.drawPaged(updateDisplay);
  }
}

void updateDisplay() {
  u8g2Fonts.setFont(u8g2_font_helvB18_tf);
  display.fillScreen(DisplayConfig.clBG);
  for (uint16_t i = 0; i < 10; i++) {
    DisplayLines[i].Text.replace("}", "ü");
    DisplayLines[i].Text.trim();
    uint16_t left = (DisplayConfig.LeftAligned == true) ? 40 : (display.width() - 40 -  u8g2Fonts.getUTF8Width((DisplayLines[i].Text).c_str()));
    u8g2Fonts.setCursor(left, (i * 40) + 30);
    u8g2Fonts.print(DisplayLines[i].Text);

    if (DisplayConfig.showLinesBetweenRows) display.drawLine(0, (i * 40), display.width(), (i * 40), DisplayConfig.clFG);

    uint8_t icon_number = DisplayLines[i].Icon;
    if (icon_number != 255) {
      if (DisplayConfig.LeftAligned == true)
        display.drawExampleBitmap(Icons[icon_number].Icon, (( 24 - Icons[icon_number].width ) / 2) + 8, (24 - ( Icons[icon_number].height / 2)) + (i * 40) - 4, Icons[icon_number].width, Icons[icon_number].height, DisplayConfig.clFG);
      else
        display.drawExampleBitmap(Icons[icon_number].Icon, display.width() - 32 + (( 24 - Icons[icon_number].width ) / 2) , (24 - ( Icons[icon_number].height / 2)) + (i * 40) - 4, Icons[icon_number].width, Icons[icon_number].height, DisplayConfig.clFG);
    }
  }

  if (DisplayConfig.showLinesBetweenRows) display.drawLine(0, 399, display.width(), 399, DisplayConfig.clFG);
}

uint16_t centerPosition(char * text) {
  return (display.width() / 2) - (u8g2Fonts.getUTF8Width(text) / 2);
}

void initDisplay(uint8_t serial[11]) {
  u8g2_for_adafruit_gfx.begin(display);
  display.setRotation(DISPLAY_ROTATE);
  u8g2Fonts.setFontMode(1);
  u8g2Fonts.setFontDirection(0);
  u8g2Fonts.setForegroundColor(DisplayConfig.clFG);
  u8g2Fonts.setBackgroundColor(DisplayConfig.clBG);
  display.fillScreen(DisplayConfig.clBG);
  u8g2Fonts.setFont(u8g2_font_helvB24_tf);


  char * title PROGMEM = "HB-Dis-EP-42BW";
  char * asksinpp PROGMEM = "AksinPP";
  char * version PROGMEM = "V " ASKSIN_PLUS_PLUS_VERSION;
  char * ser = (char*)serial;

  u8g2Fonts.setCursor(centerPosition(title), 95);
  u8g2Fonts.print(title);

  u8g2Fonts.setCursor(centerPosition(asksinpp), 170);
  u8g2Fonts.print(asksinpp);

  u8g2Fonts.setCursor(centerPosition(version), 210);
  u8g2Fonts.print(version);

  u8g2Fonts.setFont(u8g2_font_helvB18_tf);
  u8g2Fonts.setCursor(centerPosition(ser), 270);
  u8g2Fonts.print(ser);

  display.drawRect(60, 138, 175, 80, DisplayConfig.clFG);
}
