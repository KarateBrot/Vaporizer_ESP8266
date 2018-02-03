//############################################################################//
//                                                                            //
//     VAPORIZER.H - Library for operating custon built vaporizer based       //
//                   on ESP8266                                               //
// -------------------------------------------------------------------------- //
//                   © 2018, Jan Post                                         //
//                                                                            //
//############################################################################//


#ifndef VAPORIZER_H
#define VAPORIZER_H


// -------------------------------------------------------------------------- //
  #include <Arduino.h>                                                        //
  #include <Adafruit_INA219.h>                                                //
  #include <Adafruit_MCP4725.h>                                               //
  #include <Adafruit_GFX.h>                                                   //
  #include <Adafruit_SSD1306.h>                                               //
// -------------------------------------------------------------------------- //
  #include <vector>                                                           //
    using namespace std;                                                      //
// -------------------------------------------------------------------------- //
  #include <img/splash.h>                                                     //
// -------------------------------------------------------------------------- //
  #define V_FIRMWARE_VERSION     "0.1-a"                                      //
// -------------------------------------------------------------------------- //
  #define HEATER_TCR_SS316L       0.00092                         // at 20°C  //
// -------------------------------------------------------------------------- //
  #define HEATER_RES20            0.51                                        //
  #define HEATER_RESCABLE         0.27                                        //
// -------------------------------------------------------------------------- //
  #define PID_P                   1.0                                         //
  #define PID_I                   0.1                                         //
  #define PID_D                   0.08                                        //
// -------------------------------------------------------------------------- //




class Service {

  uint32_t _runtime = 0;
  uint32_t _timer_lastWaitCall = 0;

 public:

  Service(void);

  static String getVersion  (void)       { return V_FIRMWARE_VERSION; }
  void          startRuntime(void)       { _runtime = micros(); }
  uint32_t      getRuntime  (void) const { return micros() - _runtime; }
  void          waitUntil   (uint32_t);
};




class Sensor {

  Adafruit_INA219 _INA219;

 public:

  double current, voltage;

  Sensor(void);

  void setPrecision(bool);
  void read        (void);
};




class DAC {

  Adafruit_MCP4725 _MCP4725;

 public:

  DAC(void);

  void setOutput(uint16_t);
};




class PID_Ctrl {

  double  _p, _error, _i, _errorInt, _d, _errorDiff;
  double  _dt, _timeLast, _valueLast;

 public:

  PID_Ctrl(void);

  PID_Ctrl&   setP  (double p) { _p = p; return *this; }
  PID_Ctrl&   setI  (double i) { _i = i; return *this; }
  PID_Ctrl&   setD  (double d) { _d = d; return *this; }
  PID_Ctrl&   setPID(double p, double i, double d) { _p = p; _i = i; _d = d; return *this; }

  double getOutput     (double, double);
  void   setOutputOfDAC(DAC&, double, double);
  void   autotune      (void);
};




class Heater {

  double _TCR, _res20, _resCable;

 public:

  Sensor   sensor;
  DAC      dac;
  PID_Ctrl pid;

  double   resistance;
  double   power, temperature;
  uint16_t power_set, temperature_set;

  Heater(void);

  Heater& setRes20   (double res) { _res20    = res; return *this; }
  Heater& setResCable(double res) { _resCable = res; return *this; }
  Heater& setTCR     (double tcr) { _TCR      = tcr; return *this; }

  void calculate(void);
  void regulate (void);
  void calibrate(void);
};




class Input {


};




class GUI {

  vector<uint8_t> _state;

 public:

  Adafruit_SSD1306 display;

  GUI(void);

  void clear(void);
  void draw (void);
};




// =============================== VAPORIZER ================================ //

class Vaporizer {

 public:

  Heater heater;
  Input  input;
  GUI    gui;

  Vaporizer(void);
};

// -------------------------------- VAPORIZER ------------------------------- //




#endif // VAPORIZER_H
