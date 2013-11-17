/***************************************************
  Thermistor Calibration
 
  Find the Steinhart-Hart equation coefficients for a thermistor.
  
  See video of calibration here: http://www.youtube.com/watch?v=5qDVmvCPNdQ
  
  Copyright 2013 Tony DiCola (tony@tonydicola.com).
  Released under an MIT license: 
    http://opensource.org/licenses/MIT
  
 ****************************************************/

#define     THERMISTOR_PIN     3      // Pin between the thermistor and 
                                      // series resistor.

#define     SERIES_RESISTOR    10000  // Series resistor value in ohms.

#define     USE_FAHRENHEIT     true   // True to use Fahrenheit, false to
                                      // use celsius.

#define     ADC_SAMPLES        5      // Number of ADC samples to average
                                      // when taking a reading.

// Temperature unit conversion functions and state.
typedef float (*TempConversion)(float);
TempConversion ToKelvin; 
TempConversion FromKelvin;
char* TempUnit;

// Steinhart-Hart coefficients.
float A, B, C;

void setup(void) {
  Serial.begin(115200);
  analogReference(DEFAULT);
  
  if (USE_FAHRENHEIT) {
    ToKelvin = &fahrenheitToKelvin;
    FromKelvin = &kelvinToFahrenheit;
    TempUnit = "Fahrenheit";
  }
  else {
    ToKelvin = &celsiusToKelvin;
    FromKelvin = &kelvinToCelsius;
    TempUnit = "Celsius";
  }
  
  Serial.println(F("Thermistor probe coefficient calibration"));
  Serial.println(F("----------------------------------------\n"));
  
  Serial.println(F("To perform this calibration you will need:\n - A thermometer\n - Glass of ice water\n - Glass of luke-warm water\n - Glass of hot water"));
  
  Serial.println(F("\nSTEP ONE:"));
  Serial.println(F("Put the thermistor probe and thermometer into the ice water.\n"));
  
  waitForOk();
  
  printTempMessage();
  
  float T1 = waitForFloat();
  float R1 = readResistance();
  
  Serial.print("Got temperature value: "); Serial.println(T1, 3);
  
  Serial.println(F("\nSTEP TWO:"));
  Serial.println(F("Put the thermistor probe and thermometer into the luke-warm water.\n"));

  waitForOk();
  
  printTempMessage();
  
  float T2 = waitForFloat();
  float R2 = readResistance();
  
  Serial.print("Got temperature value: "); Serial.println(T2, 3);
  
  Serial.println(F("\nSTEP THREE:"));
  Serial.println(F("Put the thermistor probe and thermometer into the hot water.\n"));
  
  waitForOk();
  
  printTempMessage();
  
  float T3 = waitForFloat();
  float R3 = readResistance();
  
  Serial.print("Got temperature value: "); Serial.println(T3, 3);
  
  Serial.println(F("\n----------------------------------------\n"));
  
  Serial.println(F("Calibration complete!\n"));
  
  // Solve system of equations to determine coefficients for Steinhart-Hart equation.
  // See: http://en.wikipedia.org/wiki/Steinhart%E2%80%93Hart_equation
  float L1 = log(R1);
  float L2 = log(R2);
  float L3 = log(R3);
  float Y1 = 1.0 / ToKelvin(T1);
  float Y2 = 1.0 / ToKelvin(T2);
  float Y3 = 1.0 / ToKelvin(T3);
  float gamma2 = ((Y2 - Y1)/(L2 - L1));
  float gamma3 = ((Y3 - Y1)/(L3 - L1));
  C = ((gamma3 - gamma2)/(L3 - L2))*pow((L1 + L2 + L3), -1.0);
  B = gamma2 - C*(pow(L1, 2.0) + L1*L2 + pow(L2, 2.0));
  A = Y1 - (B + pow(L1, 2.0)*C)*L1;
  
  Serial.println(F("Use these three coefficients in your thermistor sketch:"));
  Serial.print("Coefficient A = "); Serial.println(A, 12);
  Serial.print("Coefficient B = "); Serial.println(B, 12);
  Serial.print("Coefficient C = "); Serial.println(C, 12);
  
  Serial.println(F("\nNow the temperature of the thermistor"));
  Serial.println(F("will be displayed every second.\n"));
  
  waitForOk();
  
  Serial.println(F(""));
}

void loop(void) {
  float temp = FromKelvin(readTemp());
  Serial.print(F("Temperature: ")); Serial.print(temp); Serial.print(F(" in ")); Serial.println(TempUnit);
  delay(1000);
}

void waitForOk() {
  Serial.println(F("Type OK <enter> to continue.\n"));
  while (!(Serial.findUntil("OK", "\n")));
}

float waitForFloat() {
  while (!(Serial.available() > 0));
  return Serial.parseFloat();
}

void printTempMessage() {
  Serial.println(F("Wait about 30 seconds for the thermistor to stabilize then,"));
  Serial.print(F("type the water temperature (in ")); Serial.print(TempUnit); Serial.println(F(") and press <enter>.\n"));
}

double readResistance() {
  float reading = 0;
  for (int i = 0; i < ADC_SAMPLES; ++i) {
    reading += analogRead(THERMISTOR_PIN);
  }
  reading /= (float)ADC_SAMPLES;
  reading = (1023 / reading) - 1;
  return SERIES_RESISTOR / reading;
}

float kelvinToFahrenheit(float kelvin) {
  return kelvin*(9.0/5.0) - 459.67;
}

float fahrenheitToKelvin(float fahrenheit) {
  return (fahrenheit + 459.67)*(5.0/9.0);
}

float kelvinToCelsius(float kelvin) {
  return kelvin - 273.15;
}

float celsiusToKelvin(float celsius) {
  return celsius + 273.15; 
}

float readTemp() {
  float R = readResistance();
  float kelvin = 1.0/(A + B*log(R) + C*pow(log(R), 3.0));
  return kelvin;
}
