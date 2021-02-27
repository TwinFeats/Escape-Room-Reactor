#include <Arduino.h>
#include <arduino-timer.h>
#include <NeoPixelBrightnessBus.h>
#include <ButtonDebounce.h>
#define PJON_MAX_PACKETS 2
#define PJON_PACKET_MAX_LENGTH 33
#include <PJONSoftwareBitBang.h>
#include "../../Escape Room v2 Master/src/tracks.h"

#define PIN_BEAM_LEDS       1
#define PIN_MARKER_LEDS     2
#define PIN_ENTER           3
#define PIN_BEAM_BUTTON     4
#define PIN_MARKER_BUTTON   5
#define PIN_POWER_LIGHT     6
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

int oneShotCount = 0;
Timer<10, millis, int> oneShotTimers;

boolean activated = false;

PJON<SoftwareBitBang> bus(14);

void send(uint8_t *msg, uint8_t len) {
  bus.send(1, msg, len);
  bus.update();
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

  }
}

void sendLcd(char *line1, char *line2) {
  uint8_t msg[33];
  msg[0] = 'L';
  strncpy((char *)&msg[1], line1, 16);
  strncpy((char *)&msg[17], line2, 16);
  send(msg, 33);
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

NeoPixelBrightnessBus<NeoRgbFeature, Neo400KbpsMethod> blackboxBeamLights(
    32, PIN_BEAM_LEDS);
NeoPixelBrightnessBus<NeoGrbFeature, Neo400KbpsMethod> blackboxMarkerLights(
    64, PIN_MARKER_LEDS);
ButtonDebounce bbBeamButton(PIN_BEAM_BUTTON, 100);
ButtonDebounce bbGuessButton(PIN_ENTER, 100);
ButtonDebounce bbMarkerButton(PIN_MARKER_BUTTON, 100);
Timer<1> bbBeamJoystickTimer;
Timer<1> bbMarkerJoystickTimer;
Timer<1> beamButtonTimer;

int currentBeamLight = 0, currentMarkerLight = 0;
int prevBeamLight = 0, prevMarkerLight = 0;
int nextColorIndex = 2, hitColorIndex = 0, reflectColorIndex = 1;

RgbColor beamColors[] = {red,       yellow,       blue,   green, cyan,   pink,
                         turquoise, yellowGreen,  orange, coral, purple, olive,
                         rosyBrown, darkSeaGreen, plum,   beige};

RgbColor outerLights[32];
RgbColor innerLights[64];

int rodX[5], rodY[5];

void bbGuessPressed(const int state) {
  if (activated && state == LOW) {

  };
}

void bbMarkerPressed(const int state) {
  if (!activated || state == HIGH) return;
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
  if (computedLight < 8) computedLight = 7 - computedLight;
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
      row += deltaY;
      col += deltaX;
      continue;
    } else if (hitRod(x2, y2)) {  // rod on other side
      if (row < 0 || row == 8 || col < 0 || col == 8) { //start on perimeter but trying to deflect, this is reflection
        placeBeamMarker(beamColors[reflectColorIndex], currentBeamLight);
        break;
      }
      //deflect positive
      int temp = deltaX;
      deltaX = deltaY;
      deltaY = temp;
      row += deltaY;
      col += deltaX;
      continue;
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

bool checkBeamButton(void *t) {
  int state = digitalRead(PIN_BEAM_BUTTON);
  if (!activated || state == HIGH) return true;
  if (nextColorIndex == 15) {
    //out of guesses - play message and flash lights?
    return true;
  }
  if (black == outerLights[currentBeamLight]) {
    fireBeam();
  }
  return true;
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
    if (computedLight < 8) computedLight = 7 - computedLight;
    if (outerLights[prevBeamLight] == black) {
      int prevComputed = prevBeamLight;
      if (prevComputed < 8) prevComputed = 7 - prevComputed;
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
    blackboxMarkerLights.SetPixelColor(prevMarkerLight,
                                       black);  // is default color black?
    prevMarkerLight = currentMarkerLight;
    //    prevMarkerColor =
    //    blackboxMarkerLights.GetPixelColor(currentMarkerLight);
    blackboxMarkerLights.SetPixelColor(currentMarkerLight, yellow);
    blackboxMarkerLights.Show();
  }

  return true;
}

void initBlackbox() {
  pinMode(PIN_ENTER, INPUT_PULLUP);
  pinMode(PIN_BEAM_BUTTON, INPUT_PULLUP);
  pinMode(PIN_MARKER_BUTTON, INPUT_PULLUP);
  blackboxMarkerLights.Begin();
  blackboxMarkerLights.Show();
  blackboxBeamLights.Begin();
  blackboxBeamLights.Show();
  blackboxBeamLights.SetBrightness(100);
  blackboxMarkerLights.SetBrightness(100);

  for (int i = 0; i < 5; i++) {
  repeat:
    int x = random(8);
    int y = random(8);
    for (int j = 0; j < i; j++) {
      if (rodX[j] == x && rodY[j] == y) goto repeat;
    }
    rodX[i] = x;
    rodY[i] = y;
  }
  for (int i = 0; i < 32; i++) {
    outerLights[i] = black;
  }
  blackboxBeamLights.Show();
  for (int i = 0; i < 64; i++) {
    innerLights[i] = black;
  }
  blackboxMarkerLights.Show();
  //  bbBeamButton.setCallback(bbBeamPressed);
  bbGuessButton.setCallback(bbGuessPressed);
  bbMarkerButton.setCallback(bbMarkerPressed);
  bbBeamJoystickTimer.every(200, checkBeamJoystick);
  bbMarkerJoystickTimer.every(200, checkMarkerJoystick);
  beamButtonTimer.every(200, checkBeamButton);
}
/* --------------END BLACKBOX ----------------------*/


void setup() {
  Serial.begin(9600);
  //  while (!Serial)
  //    ;  // wait for serial attach
  Serial.println("Starting");
  randomSeed(analogRead(0));
  pinMode(PIN_POWER_LIGHT, OUTPUT);
  digitalWrite(PIN_POWER_LIGHT, LOW);
  delay(2000);
  initComm();
  initBlackbox();
}

void loop() {
  bbBeamJoystickTimer.tick();
  bbMarkerJoystickTimer.tick();
  beamButtonTimer.tick();

  bus.update();
  bus.receive(750);
}