#include <Arduino.h>
#include <arduino-timer.h>
#include <NeoPixelBrightnessBus.h>
#include <ButtonDebounce.h>
#define PJON_MAX_PACKETS 4
#define PJON_PACKET_MAX_LENGTH 52
#include <PJONSoftwareBitBang.h>
#include "../../Escape Room v2 Master/src/tracks.h"

#define PIN_BEAM_LEDS       2
#define PIN_MARKER_LEDS     3
#define PIN_ENTER_BUTTON    4
#define PIN_BEAM_BUTTON     5
#define PIN_MARKER_BUTTON   6
#define PIN_POWER_LIGHT     7
// X and Y are reversed from the joystick since I need to mount these with the
// pins facing up
#define PIN_BEAM_X          A1
#define PIN_BEAM_Y          A2
#define PIN_MARKER_X        A3
#define PIN_MARKER_Y        A4
#define PIN_COMM            13

NeoGamma<NeoGammaTableMethod> colorGamma;

RgbColor white(255, 255, 255);
RgbColor red(255, 0, 0);
RgbColor green(0, 255, 0);
RgbColor blue(0, 0, 255);
RgbColor yellow(255, 255, 0);
RgbColor cyan(0, 255, 255);
RgbColor pink(255, 0, 255);
RgbColor black(0, 0, 0);
RgbColor coral = colorGamma.Correct(RgbColor(255, 127, 80));
RgbColor purple = colorGamma.Correct(RgbColor(138, 43, 226));
RgbColor olive = colorGamma.Correct(RgbColor(128, 128, 0));
RgbColor rosyBrown = colorGamma.Correct(RgbColor(188, 143, 143));
RgbColor yellowGreen = colorGamma.Correct(RgbColor(154, 205, 50));
RgbColor beige = colorGamma.Correct(RgbColor(255, 235, 205));
RgbColor darkSeaGreen = colorGamma.Correct(RgbColor(143, 188, 143));
RgbColor orange = colorGamma.Correct(RgbColor(255, 165, 0));
RgbColor turquoise = colorGamma.Correct(RgbColor(175, 238, 238));
RgbColor plum = colorGamma.Correct(RgbColor(221, 160, 221));

NeoPixelBrightnessBus<NeoRgbFeature, Neo400KbpsMethod> blackboxBeamLights(
    32, PIN_BEAM_LEDS);
NeoPixelBrightnessBus<NeoGrbFeature, Neo400KbpsMethod> blackboxMarkerLights(
    64, PIN_MARKER_LEDS);
ButtonDebounce bbBeamButton(PIN_BEAM_BUTTON, 100);
ButtonDebounce bbGuessButton(PIN_ENTER_BUTTON, 100);
ButtonDebounce bbMarkerButton(PIN_MARKER_BUTTON, 100);
Timer<1> bbBeamJoystickTimer;
Timer<1> bbMarkerJoystickTimer;

int currentBeamLight = 0, currentMarkerLight = 0;
int prevBeamLight = 0, prevMarkerLight = 0;
int nextColorIndex = 2, hitColorIndex = 0, reflectColorIndex = 1;

RgbColor beamColors[] = {red,       yellow,       blue,   green, cyan,   pink,
                         turquoise, yellowGreen,  orange, coral, purple, olive,
                         rosyBrown, darkSeaGreen, plum,   beige};

RgbColor outerLights[32];
RgbColor innerLights[64];
RgbColor markerLights[64];

int rodX[5], rodY[5];

boolean activated = false;

PJON<SoftwareBitBang> bus(14);

void send(uint8_t *msg, uint8_t len) {
  bus.send(1, msg, len);
  while (bus.update()) {};//wait for send to be completed
}

void send(const char *msg, int len) {
  uint8_t buf[35];
  memcpy(buf, msg, len);
  send(buf, len);
}

void error_handler(uint8_t code, uint16_t data, void *custom_pointer) {
  if(code == PJON_CONNECTION_LOST) {
    Serial.print("Connection lost with device id ");
    Serial.println(bus.packets[data].content[0], DEC);
  }
}

void commReceive(uint8_t *data, uint16_t len, const PJON_Packet_Info &info) {
  if (data[0] == 'A') {
    activated = true;
    digitalWrite(PIN_POWER_LIGHT, HIGH);
  } else if (data[0] == 'W') {  //player has won

  } else if (data[0] == 'L') {  //player has lost

  } else if (data[0] == 'B') {  //brightness
    blackboxBeamLights.SetBrightness(data[1]);
    blackboxBeamLights.Show();
  }
}

void sendLcd(const char *line1, const char *line2) {
  uint8_t msg[35];
  msg[0] = 'L';
  strncpy((char *)&msg[1], line1, 17);
  strncpy((char *)&msg[18], line2, 17);
  Serial.print("Sending ");
  Serial.println((char *)msg);
  send(msg, 35);
}

void sendMp3(int track) {
  uint8_t msg[2];
  msg[0] = 'M';
  msg[1] = track;
  send(msg, 2);
}

void sendTone(int tone) {
  uint8_t msg[2];
  msg[0] = 'T';
  msg[1] = tone;
  send(msg, 2);
}

void initComm() {
  bus.strategy.set_pin(PIN_COMM);
  bus.include_sender_info(false);
  bus.set_error(error_handler);
  bus.set_receiver(commReceive);
  bus.begin();
}

// Lock combo is 4219

/* ----------------- BLACKBOX ----------------------*/


void blackboxComplete() {
  activated = false;
  digitalWrite(PIN_POWER_LIGHT, LOW);
  send("D", 1);
}

void bbGuessPressed(const int state) {
  int markerCount = 0;
  if (activated && state == LOW) {
    int count = 0;
    for (int x=0;x<8;x++) {
      for (int y=0;y<8;y++) {
        int idx = y*8+x;
        if (markerLights[idx] != black) {
          markerCount++;
          for (int i=0;i<5;i++) {
            if (rodX[i] == x && rodY[i] == y) {
              count++;
            }
          }
        }
      }
    }
    if (count == 5 && markerCount == 5) { //to catch cheaters just marking every square
      //got it!
      blackboxComplete();
    } else {
      sendMp3(TRACK_WRONG);
    }
  };
}

void bbMarkerPressed(const int state) {
  if (!activated || state == HIGH) return;
  if (markerLights[currentMarkerLight] == black) {
    blackboxMarkerLights.SetPixelColor(currentMarkerLight, yellow);
    innerLights[currentMarkerLight] = yellow;
    markerLights[currentMarkerLight] = yellow;
  } else {
    blackboxMarkerLights.SetPixelColor(currentMarkerLight, black);
    innerLights[currentMarkerLight] = black;
    markerLights[currentMarkerLight] = black;
  }
  blackboxMarkerLights.Show();
}

bool hitRod(int x, int y) {
  for (int r = 0; r < 5; r++) {
    if (rodX[r] == x && rodY[r] == y) return true;
  }
  return false;
}

void placeBeamMarker(RgbColor color, int loc) {
  outerLights[loc] = color;
  int computedLight = loc;
//  if (computedLight < 8) computedLight = 7 - computedLight; //The first light strip is no longer rotated
  blackboxBeamLights.SetPixelColor(computedLight, color);
  blackboxBeamLights.Show();
}

int calcBeamLight(int x, int y) {
  // x and y are the coords in the virutal 10x10 box, with origin at lower right corner
  if (x == 8) return y;
  if (x == -1) return 23 - y;
  if (y == -1) return x + 24;
  return 15 - x;
}

void fireBeam() {
  // -1 -1 is lower right corner of the 10x10 that surrounds the 8x8 blackbox
  int row = 0;
  int col = 0;
  int deltaX = 0;
  int deltaY = 0;
  if (currentBeamLight < 8) {
    row = currentBeamLight;
    col = 8;
    deltaX = -1;
  } else if (currentBeamLight > 23) {
    row = -1;
    col = currentBeamLight - 24;
    deltaY = 1;
  } else if (currentBeamLight >= 8 && currentBeamLight < 16) {
    row = 8;
    col = 15 - currentBeamLight;
    deltaY = -1;
  } else {
    col = -1;
    row = 23 - currentBeamLight;
    deltaX = 1;
  }

  while (true) {
    //calc square in front of beam
    int x = col + deltaX;
    int y = row + deltaY;
    //check square in front of us
    if (hitRod(x, y)) {
      placeBeamMarker(beamColors[hitColorIndex], currentBeamLight);
      break;
    }
    //x1/y1 is square in front of and to one side of beam, x2/y2 is the square in front of and on the other side of the beam
    int x1 = x + deltaY;
    int y1 = y + deltaX;
    int x2 = x - deltaY;
    int y2 = y - deltaX;
    if (hitRod(x1, y1)) {  // rod on first side
      if (hitRod(x2, y2)) { // rod on second side
      //reflection
        placeBeamMarker(beamColors[reflectColorIndex], currentBeamLight);
        break;
      }
      if (row < 0 || row == 8 || col < 0 || col == 8) { //start on perimeter but trying to deflect, this is reflection
        placeBeamMarker(beamColors[reflectColorIndex], currentBeamLight);
        break;
      }
      //deflect negative
      int temp = -deltaX;
      deltaX = -deltaY;
      deltaY = temp;
      // row += deltaY;
      // col += deltaX;
//      continue;
    } else if (hitRod(x2, y2)) {  // rod on other side
      if (row < 0 || row == 8 || col < 0 || col == 8) { //start on perimeter but trying to deflect, this is reflection
        placeBeamMarker(beamColors[reflectColorIndex], currentBeamLight);
        break;
      }
      //deflect positive
      int temp = deltaX;
      deltaX = deltaY;
      deltaY = temp;
      // row += deltaY;
      // col += deltaX;
//      continue;
    }
    row += deltaY;  // advance
    col += deltaX;
    if (row < 0 || row == 8 || col < 0 || col == 8) {  // hit perimeter
      placeBeamMarker(beamColors[nextColorIndex], currentBeamLight);
      //? is calc beam light from row/col
      placeBeamMarker(beamColors[nextColorIndex++], calcBeamLight(col, row));
      break;
    }

  }
}

void checkBeamButton(int state) {
  if (!activated || state == HIGH) return;
  if (nextColorIndex == 15) {
    //out of guesses - play message and flash lights?
    return;
  }
  if (black == outerLights[currentBeamLight]) {
    fireBeam();
  }
}

bool checkBeamJoystick(void *t) {
  if (!activated) return true;
  if (nextColorIndex == 15) {
    //out of guesses - play message and flash lights?
    return true;
  }
  int x = analogRead(PIN_BEAM_X);
  int y = analogRead(PIN_BEAM_Y);
  int diffX = 512 - x;
  int diffY = 512 - y;
  if (diffX < -100) {  // moving left
    if (currentBeamLight >= 8 && currentBeamLight <= 16) {
      currentBeamLight--;
    } else if (currentBeamLight >= 23 && currentBeamLight <= 31) {
      currentBeamLight++;
      if (currentBeamLight == 32) currentBeamLight = 0;
    }
  } else if (diffX > 100) {  // moving right
    if (currentBeamLight == 0)
      currentBeamLight = 31;
    else if (currentBeamLight >= 7 && currentBeamLight <= 15) {
      currentBeamLight++;
    } else if (currentBeamLight >= 24) {
      currentBeamLight--;
    }
  }
  if (diffY < -100) {  // moving down
    if (currentBeamLight >= 0 && currentBeamLight <= 8) {
      currentBeamLight--;
      if (currentBeamLight < 0) currentBeamLight = 31;
    } else if (currentBeamLight >= 15 && currentBeamLight <= 23) {
      currentBeamLight++;
    }
  } else if (diffY > 100) {  // moving up
    if (currentBeamLight <= 7) {
      currentBeamLight++;
    } else if (currentBeamLight >= 16 && currentBeamLight <= 24) {
      currentBeamLight--;
    } else if (currentBeamLight == 31)
      currentBeamLight = 0;
  }
  if (prevBeamLight != currentBeamLight) {
    int computedLight = currentBeamLight;
//    if (computedLight < 8) computedLight = 7 - computedLight;
    if (outerLights[prevBeamLight] == black) {
      int prevComputed = prevBeamLight;
//      if (prevComputed < 8) prevComputed = 7 - prevComputed;
      blackboxBeamLights.SetPixelColor(prevComputed, black);
      blackboxBeamLights.Show();
    }
    prevBeamLight = currentBeamLight;
    if (outerLights[currentBeamLight] == black) {
      blackboxBeamLights.SetPixelColor(computedLight, white);
      blackboxBeamLights.Show();
    }
  }
  return true;
}

// origin is lower right corner!
bool checkMarkerJoystick(void *t) {
  if (!activated) return true;
  int row = currentMarkerLight / 8;
  int col = (currentMarkerLight % 8);
  int x = analogRead(PIN_MARKER_X);
  int y = analogRead(PIN_MARKER_Y);
  int diffX = 512 - x;
  int diffY = 512 - y;
  if (diffX < -100) {  // left
    if (col < 7) {
      currentMarkerLight += 1;
    }

  } else if (diffX > 100) {  // right
    if (col > 0) {
      currentMarkerLight -= 1;
    }
  }
  if (diffY < -100) {  // down
    if (row > 0) {
      currentMarkerLight -= 8;
    }

  } else if (diffY > 100) {  // up
    if (row < 7) {
      currentMarkerLight += 8;
    }
  }
  if (prevMarkerLight != currentMarkerLight) {
    /*
    Restore light from saved value
    */
    blackboxMarkerLights.SetPixelColor(prevMarkerLight, markerLights[prevMarkerLight]);
    innerLights[prevMarkerLight] = markerLights[prevMarkerLight];

    prevMarkerLight = currentMarkerLight;
    blackboxMarkerLights.SetPixelColor(currentMarkerLight, yellow);
    innerLights[currentMarkerLight] = yellow;
    blackboxMarkerLights.Show();
  }

  return true;
}

void initBlackbox() {
  pinMode(PIN_ENTER_BUTTON, INPUT_PULLUP);
  pinMode(PIN_BEAM_BUTTON, INPUT_PULLUP);
  pinMode(PIN_MARKER_BUTTON, INPUT_PULLUP);
  blackboxMarkerLights.Begin();
  blackboxMarkerLights.Show();
  blackboxBeamLights.Begin();
  blackboxBeamLights.Show();
  blackboxBeamLights.SetBrightness(128);
  blackboxMarkerLights.SetBrightness(128);

  for (int i = 0; i < 5; i++) {
  repeat:
    int x = random(8);
    int y = random(8);
    for (int j = 0; j < i; j++) {
      if (rodX[j] == x && rodY[j] == y) goto repeat;
    }
    rodX[i] = x;
    rodY[i] = y;
//    int idx = y*8+x;
//    markerLights[idx] = yellow;
//    blackboxMarkerLights.SetPixelColor(idx, yellow);
  }
  blackboxMarkerLights.Show();
  for (int i = 0; i < 32; i++) {
    outerLights[i] = black;
  }
  blackboxBeamLights.Show();
  for (int i = 0; i < 64; i++) {
    innerLights[i] = black;
    markerLights[i] = black;
  }
  blackboxMarkerLights.Show();
  bbGuessButton.setCallback(bbGuessPressed);
  bbMarkerButton.setCallback(bbMarkerPressed);
  bbBeamButton.setCallback(checkBeamButton);
  bbBeamJoystickTimer.every(200, checkBeamJoystick);
  bbMarkerJoystickTimer.every(200, checkMarkerJoystick);
}
/* --------------END BLACKBOX ----------------------*/


void startup() {
  delay(8000*3 + 1000);  //wait for modem, firewall, control
  digitalWrite(PIN_POWER_LIGHT, HIGH);
  for (int i=0;i<32;i++) {
    blackboxBeamLights.SetPixelColor(i, red);
    blackboxBeamLights.Show();
    delay(500);
    blackboxBeamLights.SetPixelColor(i, black);
    blackboxBeamLights.Show();
  }
  blackboxBeamLights.Show();
  blackboxMarkerLights.SetPixelColor(0, yellow);
  blackboxMarkerLights.Show();
  delay(500);
  blackboxMarkerLights.SetPixelColor(0, black);
  blackboxMarkerLights.Show();
  char line1[17], line2[17];
  for (int i=0;i<5;i++) {
    sprintf(line1, "Rod %i", (i+1));
    sprintf(line2, "row %i, col %i", rodY[i], rodX[i]);
    sendLcd(line1, line2);    
  }
  digitalWrite(PIN_POWER_LIGHT, LOW);
}

void setup() {
  Serial.begin(9600);
  Serial.println("Starting");
  randomSeed(analogRead(0));
  pinMode(PIN_POWER_LIGHT, OUTPUT);
  digitalWrite(PIN_POWER_LIGHT, LOW);
  delay(2000);
  initComm();
  initBlackbox();

  startup();
}

void loop() {
  bbBeamJoystickTimer.tick();
  bbMarkerJoystickTimer.tick();
  bbBeamButton.update();
  bbMarkerButton.update();
  bbGuessButton.update();

  bus.update();
  bus.receive(750);
}