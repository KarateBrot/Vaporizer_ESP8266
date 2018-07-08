//############################################################################//
//                                                                            //
//     VAPORIZER.CPP - Library for operating custon built vaporizer based     //
//                     on ESP8266                                             //
// -------------------------------------------------------------------------- //
//                     © 2018, Jan Post                                       //
//                                                                            //
//############################################################################//


#include <Vaporizer.h>




// ================================= TIMER =====================================

uint32_t Stopwatch::lifetime = 0;

Task::Task(fptr_t f, float tps) {

  execute      = f;
  tickRate     = tps;
  lastExecute  = micros();
}

vector<Task> Timer::_tasks;
float        Timer::_tickrate  = 0.0f;
uint32_t     Timer::_lastTick  = micros();
uint32_t     Timer::_lastWait  = micros();
uint32_t     Timer::_deltaTime = 0;
bool         Timer::_running   = true;

Timer::Timer() {

  _tick     = 0;
  _lastWait = micros();
  _lastTick = micros();
}

void Timer::add(fptr_t f, float tps) {

  _tasks.push_back(Task(f, tps));
}

void Timer::run() {

  _running = true;

  while (_running) {

    uint8_t size = _tasks.size();

    for (size_t n = 0; n < size; n++) {

      Task &task = _tasks[n];
      _deltaTime = (uint32_t)(1000000.0f/task.tickRate);

      if ((uint32_t)(micros() - task.lastExecute) >= _deltaTime) {
        task.execute();
        task.lastExecute = micros();
      }
    }

    _tickrate = 1000000.0f/(float)(micros() - _lastTick);
    _lastTick = micros();

    yield();
  }
}

void Timer::waitUntil(uint32_t timer) {

  while ((uint32_t)(micros() - _lastWait) < timer) { yield(); }
  _lastWait = micros();
}

void Timer::forceTickRate(float tps) {

  waitUntil( (uint32_t)(1000000.0f/tps + 0.5f) );
}

// --------------------------------- TIMER -------------------------------------




// ================================= SENSOR ====================================

Adafruit_INA219 Sensor_Power::_INA219;

Sensor_Power::Sensor_Power() {

  _INA219.begin();
}

void Sensor_Power::setPrecision(bool b) {

  b ? _INA219.setCalibration_16V_400mA() : _INA219.setCalibration_32V_2A();
}

double Sensor_Power::getCurrent() {

  // Current [mA]
  return _INA219.getCurrent_mA()*2.0;              // R050 instead of R100 shunt
}

double Sensor_Power::getVoltage() {

  // Voltage [mV]
  return _INA219.getBusVoltage_V()*1000.0;
}

// -------------------------------- SENSOR -------------------------------------




// ================================== DAC ======================================

Adafruit_MCP4725 DAC::_MCP4725;

DAC::DAC() {

  _MCP4725.begin(0x62);
  _MCP4725.setVoltage(4095, false);            // boot with minimal power output
}

// Applies DC voltage from 0..Vcc at pin "OUT" on MCP4725 breakout board
void DAC::setOutput(uint16_t val) {

  _MCP4725.setVoltage(4095 - val, false);
}

// ---------------------------------- DAC --------------------------------------




// ================================ PID_Ctrl ===================================

// -------------------------------------------------------------------------- //
//                          --< PID-CONTROLLER >--                            //
// -----------------:-------------------------------------------------------- //
// SIGNAL OUTPUT:   : u(t) = P*e(t) + I*∫e(t)dt + D*de(t)/dt                  //
// -----------------:-------------------------------------------------------- //
// ERROR:           :     e(t)     = T_set - T(t) = ΔT                        //
// PAST ERR:        :     ∫e(t)dt  = e_past                                   //
// PREDICTED ERR:   :     de(t)/dt = -dT/dt                                   //
// -----------------:-------------------------------------------------------- //
//       ──►        : u(t) = P*ΔT + I*e_past - D*dT/dt                        //
// -----------------:-------------------------------------------------------- //

PID_Ctrl::PID_Ctrl() {

}

void PID_Ctrl::_update(double value, double value_set) {

  // e(t)
  _error = value_set - value;

  // dt
  _dt       = ((uint32_t)(micros() - _timeLast))/1000000.0;
  _timeLast = micros();

  // de(t)/dt
  _errorDiff = (value - _valueLast)/_dt;
  _valueLast =  value;

  // ∫e(t)dt
  if (_error  <= 10) {        // only integrate shortly before reaching e(t) = 0
    _errorInt += _error*_dt;
    _errorInt  = constrain(_errorInt, 0, 1/_i);
  }
}

vector<double> PID_Ctrl::getPID() const {

  vector<double> v({_p, _i, _d});
  return v;
}

double PID_Ctrl::getOutput() const {

  // u(t) = P*ΔT + I*e_past - D*dT/dt
  return constrain(_p*_error + _i*_errorInt - _d*_errorDiff, 0, 1);
}

void PID_Ctrl::regulate(double value, double value_set) {

  _update(value, value_set);
  uint16_t output = (uint16_t)( getOutput()*4095.0 + 0.5 );
  _dacPtr->setOutput(output);
}

void PID_Ctrl::autotune() {

  // TODO: Autotune algorithm
}

// -------------------------------- PID_Ctrl -----------------------------------




// ================================= Heater ====================================

Heater::Heater() {

  setTCR     (HEATER_TCR);
  setRes20   (HEATER_RES20);
  setResCable(HEATER_RESCABLE);

  resistance  = HEATER_RES20;
  temperature = 20.0;

  pid
    .attach(&dac)
    .setPID(PID_P, PID_I, PID_D);
}

void Heater::update() {

  // Voltage [mV]
  voltage =
    0.5*voltage +
    0.5*sensor.getVoltage();

  // Current [mA]
  current =
    0.5*current +
    0.5*sensor.getCurrent();

// ------------

  // Resistance [Ω]
  resistance =
    0.7*resistance +
    0.3*(voltage/constrain(current, 1, 15000) - _resCable);

  // Resistance [Ω] - if no heater connected
  if (voltage > 100 && current < 10) { resistance = _res20; }

  // Power [W]
  power =
    0.5*power +
    0.5*voltage*current/1000000.0;

  // Temperature [°C]
  temperature =
    0.7*temperature +
    0.3*(log(resistance/_res20)/_TCR + 20.0);
}

// Sets DAC pin "OUT" to DC voltage according to PID-controller
void Heater::regulate() {

  if (_running) {

    // TODO: Set ideal PID consts for each mode
    _mode == TEMP_MODE
      ? pid.regulate(temperature, temperature_set)
      : pid.regulate(power, power_set);
    sensor.setPrecision(LOW);

  } else {

    dac.setOutput(0);
    sensor.setPrecision(HIGH);
  }
}

// Make sure heater core is at room temperature before calibration!
void Heater::calibrate() {

  PID_Ctrl pid_calibration;

  pid_calibration
    .attach(&dac)
    .setPID(1.0, 0.0, 0.0);                               // TODO: Manual tuning

  sensor.setPrecision(HIGH);

  // Make sure current flow is 10mA +/- 1mA
  double currentLast = 0.0;
  while (abs(current - 10.0) > 1.0 && abs(currentLast - 10.0) > 1.0) {
    currentLast = current;
    update();
    pid_calibration.regulate(current, 10.0);
    yield();
  }

  // Calculate resistance using reference current of 10mA
  for (size_t i = 0; i < 15; i++) {
    update();
    pid_calibration.regulate(current, 10.0);
    yield();
  }

  // TODO: Room temp measurement to compensate res for temps != 20°C

  dac.setOutput(0);
  setRes20(resistance);
  sensor.setPrecision(LOW);
}

// --------------------------------- Heater ------------------------------------




// ================================ CONTROLS ===================================

const uint8_t Encoder::_stateMachine[7][4] = {

  { START,    CW_BEGIN,  CCW_BEGIN, START       },
  { CW_NEXT,  START,     CW_FINAL,  START | CW  },
  { CW_NEXT,  CW_BEGIN,  START,     START       },
  { CW_NEXT,  CW_BEGIN,  CW_FINAL,  START       },
  { CCW_NEXT, START,     CCW_BEGIN, START       },
  { CCW_NEXT, CCW_FINAL, START,     START | CCW },
  { CCW_NEXT, CCW_FINAL, CCW_BEGIN, START       }
};

Encoder::Encoder(uint8_t CLK, uint8_t DT, cmd_t cw, cmd_t ccw) {

  _pin        = CLK;
  pinMode     (_pin, INPUT);
  digitalWrite(_pin, HIGH);

  _pin2       = DT;
  pinMode     (_pin2, INPUT);
  digitalWrite(_pin2, HIGH);

  _command    = cw;
  _commandCCW = ccw;

  _state = START;
}

uint8_t Encoder::read() {

  // state machine naturally debounces encoder
  uint8_t stateTemp = (digitalRead(_pin) << 1) | digitalRead(_pin2);
  _state = _stateMachine[_state & 0xf][stateTemp];

  return _state & 0x30;
}

// - - - - -

Button::Button(uint8_t pin, cmd_t c) {

  _pin = pin;

  pinMode     (_pin, INPUT);
  digitalWrite(_pin, LOW);

  _command = c;

  _state = digitalRead(_pin);
}

uint8_t Button::read() {

  // easy debouncing (1000/50 button state changes per second max.)
  if ((uint32_t)(millis() - _lastRead) >= 50) {

    _state    = digitalRead(_pin);
    _lastRead = millis();
  }

  return _state;
}

// - - - - -

Switch::Switch(uint8_t pin) {

  _pin = pin;

  pinMode     (_pin, INPUT);
  digitalWrite(_pin, LOW);

  _state = digitalRead(_pin);
}

uint8_t Switch::read() {

  // easy debouncing (1000/50 switch state changes per second max.)
  if ((uint32_t)(millis() - _lastRead) >= 50) {

    _state    = digitalRead(_pin);
   *_ptr      = _state;
    _lastRead = millis();
  }

  return _state;
}

// -------------------------------- CONTROLS -----------------------------------




// ================================= Input =====================================

vector<Encoder> Input::_encoders;
vector<Button>  Input::_buttons;
vector<Switch>  Input::_switches;

vector<cmd_t>   Input::_commands;

Input::Input() {

}

void Input::_ISR_encoder() {

  uint8_t state = 0;

  for (Encoder enc : _encoders) {

    state = enc.read();

    switch (state) {
      case Encoder::CW:  _addCommand(enc.getCommand());    break;
      case Encoder::CCW: _addCommand(enc.getCommandCCW()); break;
      default: break;
    }
  }
}

void Input::_ISR_button() {

  uint8_t state = 0;

  for (Button btn : _buttons) {

    state = btn.read();

    switch (state) {
      case Button::DOWN: _addCommand(btn.getCommand()); break;
      default: break;
    }
  }
}

void Input::_handleCommand(cmd_t t) {

  switch (t) {                 // TODO: Implement functionality in _handleTask()
    case UP:     Serial.println("+1"); break;
    case DOWN:   Serial.println("-1"); break;
    case ENTER:  Serial.println( "0"); break;
    case ENTER2: Serial.println("00"); break;
    default:     break;
  }
}

void Input::add(Encoder enc) {

  _encoders.push_back(enc);
  attachInterrupt(_encoders.back().getPin(),  _ISR_encoder, CHANGE);
  attachInterrupt(_encoders.back().getPin2(), _ISR_encoder, CHANGE);
}

void Input::add(Button btn) {

  _buttons.push_back(btn);
  attachInterrupt(_buttons.back().getPin(), _ISR_button, CHANGE);
}

void Input::add(Switch sw) {

  _switches.push_back(sw);
}

void Input::update() {

  for (Switch sw : _switches) { sw.read(); }
  for (cmd_t  c  : _commands) { _handleCommand(c); }
  _commands.clear();
}

// --------------------------------- Input -------------------------------------




// ================================== GUI ======================================

Adafruit_SSD1306 GUI::display;

GUI::GUI() {

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextColor(WHITE);
}

void GUI::clear() {

  display.clearDisplay();
  display.display();
}

// ---------------------------------- GUI --------------------------------------




// =============================== VAPORIZER ===================================

Vaporizer::Vaporizer() {


}

// Needs to be called lastly in Setup() to overwrite Wire (I2C) settings
void Vaporizer::begin(uint8_t scl, uint8_t sda) {

  Wire.begin(sda, scl);                      // Select I2C pins
  Wire.setClock(WIRE_FREQ);                  // Faster I2C transmission (800kHz)
  analogWriteRange(PWM_RANGE);
  analogWriteFreq(PWM_FREQ);
}

void Vaporizer::run(uint8_t tickrate) {

  timer.tick();
  heater.update();
  heater.regulate();
  input.update();
  timer.forceTickRate(tickrate);
}

// ------------------------------- VAPORIZER -----------------------------------
