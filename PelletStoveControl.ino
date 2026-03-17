#include <xcape_network.h>
#include <xcape_ota.h>
#include <DHT11.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <xcape_digitalinput.h>

#include "c:/Users/jeanp/Documents/Arduino/wifi-maison.h"

//https://docs.cirkitdesigner.com/component/f334e0a6-54f4-48e6-9e27-c51db1c94614/lcd2004i2c

const int FLASH_NVM_OFFSET = 0x290000;

const int STOVE_STATE_UNKNOWN  = -1;
const int STOVE_STATE_STOPPED  = 0;
const int STOVE_STATE_STARTED  = 1;
const int STOVE_STATE_INACTIVE = 2;

const int PULLUP_PIN     = 25;
const int SENSOR_PIN     = 5;
const int STOVE_PIN      = 4;
const int ACTIVATION_PIN = 32;

DHT11             _sensor(SENSOR_PIN); // tempreature and humidity sensor on pin 2
LiquidCrystal_I2C _lcd(0x27, 20, 4);
xcape::DigitalInput _activation(32);

struct State {
  int curr_temp;
  int start_temp;
  int stop_temp;
  int stove_state;
} _state;


void setup() {
  Serial.begin(115200);
  xcape::network::initialize_wifi(WIFI_SSID, WIFI_PASS);
  xcape::ota::initialize("StoveThermostat", "StoveThermostatAdmin");
  
  pinMode(PULLUP_PIN, OUTPUT);
  pinMode(STOVE_PIN, OUTPUT);
  digitalWrite(PULLUP_PIN, HIGH);

  _activation.initialize();
  if (digitalRead(_activation.get_id()) == HIGH) {
    _state.stove_state = STOVE_STATE_INACTIVE;
  }
  else {
      _state.stove_state = STOVE_STATE_UNKNOWN;
  }

  ESP.flashRead(FLASH_NVM_OFFSET, (uint32_t*)&_state, sizeof(_state));
  _state.curr_temp = 99;
  if (_state.start_temp == -1) _state.start_temp = 20;
  if (_state.stop_temp == -1) _state.stop_temp   = 23;

  Serial.printf("Start temp: %i\tStop temp: %i\n", _state.start_temp, _state.stop_temp);

  _lcd.init(); // Initialize the LCD
  _lcd.backlight(); // Turn on the backlight

  print_ip();
  print_interval();  
  print_temp();
  print_state();
}


void loop() {
  xcape::ota::handle();
  update_stove();
}


void print_ip() {
  char str_buf[21];
  _lcd.setCursor(0, 0); // Set cursor to column 0, row 0
  snprintf(str_buf, sizeof(str_buf), "  IP: %s", WiFi.localIP().toString().c_str());
  _lcd.print(str_buf);
}

void print_interval() {
  char str_buf[21];
  _lcd.setCursor(0, 1);
  snprintf(str_buf, sizeof(str_buf), " Dem. %iC  Arr. %iC", _state.start_temp, _state.stop_temp);
  _lcd.print(str_buf);
}

void print_temp() {
  char str_buf[21];
  _lcd.setCursor(0, 2);
  snprintf(str_buf, sizeof(str_buf), "Temp. %iC", _state.curr_temp);
  _lcd.print(str_buf);
  Serial.printf("Temperature: %iC\n", _state.curr_temp);  
}


void print_state() {
  char str_buf[21];
  _lcd.setCursor(0, 3);
  if (_state.stove_state == STOVE_STATE_UNKNOWN) {
    _lcd.print("Foyer ???      ");
    Serial.println("Stove unknown.");
  }
  else if (_state.stove_state == STOVE_STATE_STOPPED) {
    _lcd.print("Foyer a l'arret");
    Serial.println("Stove stopped.");
  } 
  else if (_state.stove_state == STOVE_STATE_STARTED) {
    _lcd.print("Foyer en marche");
    Serial.println("Stove started.");
  }
  else if (_state.stove_state == STOVE_STATE_INACTIVE) {
    _lcd.print("Foyer inactif  ");
    Serial.println("Stove stopped.");
  }
}


void update_stove() {
  int new_temp = _sensor.readTemperature();
  
  int active_changed = _activation.sample();
  if (active_changed > 1) {
    delay(10);
    return;
  }
  if (active_changed != 0) Serial.println(active_changed);
  if (active_changed == 1) {
    if (_state.stove_state == STOVE_STATE_STARTED) {
      stop_stove();
    }
    _state.stove_state = STOVE_STATE_INACTIVE;
    print_state();
  }
  else if (active_changed == -1) {
    _state.stove_state = STOVE_STATE_UNKNOWN;
  }

  if (_state.stove_state == STOVE_STATE_INACTIVE) {
    if (new_temp != _state.curr_temp) {
      _state.curr_temp = new_temp;
      print_temp();
    }
    return;
  }

  if(_state.stove_state == STOVE_STATE_UNKNOWN) {
    if (new_temp < _state.stop_temp) {
      start_stove();
    }
    else {
      stop_stove();
    }
  }

  if (_state.stove_state == STOVE_STATE_STOPPED) {
    if (new_temp <= _state.start_temp) {
      start_stove();
    }
  }
  else if (_state.stove_state == STOVE_STATE_STARTED) {
    if (new_temp >= _state.stop_temp) {
      stop_stove();
    }
  }
  if (new_temp != _state.curr_temp) {
    _state.curr_temp = new_temp;
    print_temp();
  }
}


void start_stove() {
  if (_state.stove_state == STOVE_STATE_INACTIVE) {
    return;
  }
  _state.stove_state = STOVE_STATE_STARTED;
  digitalWrite(STOVE_PIN, HIGH);
  print_state();
}

void stop_stove() {
  _state.stove_state  = STOVE_STATE_STOPPED;
  digitalWrite(STOVE_PIN, LOW);
  print_state();
}