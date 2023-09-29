/*
   -------------------------------------------------------------------------------------
   HX711_ADC
   Biblioteca de Arduino para el Convertidor Analógico-Digital de 24 bits HX711 para básculas
   Olav Kallhovd septiembre de 2017
   -------------------------------------------------------------------------------------
*/

/*
   Este archivo de ejemplo muestra cómo calibrar la celda de carga y, opcionalmente, almacenar el valor de calibración
   en la EEPROM, y también cómo cambiar el valor manualmente.
   El valor resultante luego se puede incluir en su proyecto de sketch o recuperarse de EEPROM.

   Para implementar la calibración en su proyecto de sketch, el procedimiento simplificado es el siguiente:
       LoadCell.tare();
       // Coloque una masa conocida
       LoadCell.refreshDataSet();
       float nuevoValorDeCalibración = LoadCell.getNewCalibration(masa_conocida);
*/

#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x3F,16,2);

#include <HX711_ADC.h>
#if defined(ESP8266)|| defined(ESP32) || defined(AVR)
#include <EEPROM.h>
#endif

// Pines:
const int HX711_dout = 4; // Pin dout de la MCU al HX711
const int HX711_sck = 5; // Pin sck de la MCU al HX711

// Constructor de HX711:
HX711_ADC LoadCell(HX711_dout, HX711_sck);

const int calVal_eepromAdress = 0;
unsigned long t = 0;

void setup() {
  lcd.init();
  lcd.clear();
  lcd.backlight();
  lcd.setCursor(3, 0);
  lcd.print("Bienvenido");
  lcd.setCursor(3, 1);
  lcd.print("Bascula");
  delay(2000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Configurando");
  lcd.setCursor(0, 1);
  lcd.print("Via Serial");
  Serial.begin(57600); delay(10);
  Serial.println();
  lcd.clear();
  Serial.println("Comenzando...");
  lcd.setCursor(0, 0);                    
  lcd.print("Comenzando...");
  delay(500);
  LoadCell.begin();
  //LoadCell.setReverseOutput(); // Descomente para convertir un valor de salida negativo en positivo
  unsigned long stabilizingtime = 2000; // Precisión justo después de la activación puede mejorarse agregando unos segundos de tiempo de estabilización
  boolean _tare = true; // Establezca esto en false si no desea que se realice la tara en el próximo paso
  LoadCell.start(stabilizingtime, _tare);
  if (LoadCell.getTareTimeoutFlag() || LoadCell.getSignalTimeoutFlag()) {
    Serial.println("Tiempo de espera, verifique el cableado MCU > HX711 y las designaciones de los pines");
    while (1);
  }
  else {
    LoadCell.setCalFactor(1.0); // Valor de calibración configurado por el usuario (float), valor inicial 1.0 que se puede usar para este sketch
    Serial.println("La inicialización está completa");
  }
  while (!LoadCell.update());
  calibrar(); // Iniciar el procedimiento de calibración
lcd.clear();
}

void loop() {
  static boolean newDataReady = 0;
  const int serialPrintInterval = 0; // Aumentar el valor para reducir la actividad de impresión en serie

  // Verificar nuevos datos / iniciar próxima conversión:
  if (LoadCell.update()) newDataReady = true;
  // Obtener el valor suavizado del conjunto de datos:
  if (newDataReady) {
    if (millis() > t + serialPrintInterval) {
      float i = LoadCell.getData();
      Serial.print("Valor de salida de la celda de carga: ");
      /////////////////////////////////////////////////////////////////////////////////////////
      Serial.println(i);
      lcd.setCursor(0, 0);                    
      lcd.print("Valor de Lectura");
      lcd.setCursor(5, 1);                    //Salida LCD//
      lcd.print(i);
      lcd.setCursor(9, 1);                    
      lcd.print("kg");
      
      //delay(200);
      /////////////////////////////////////////////////////////////////////////////////////////
      
      newDataReady = 0;
      t = millis();
    }
    
  }

  // Recibir comando desde el terminal en serie
  if (Serial.available() > 0) {
    char inByte = Serial.read();
    if (inByte == 't') LoadCell.tareNoDelay(); // Tara
    else if (inByte == 'r') calibrar(); // Calibrar
    else if (inByte == 'c') cambiarValorDeCalibracionGuardado(); // Editar manualmente el valor de calibración
  }

  // Verificar si la última operación de tara está completa
  if (LoadCell.getTareStatus() == true) {
    Serial.println("Tara completa");
  }

}

void calibrar() {
  lcd.clear();
  lcd.setCursor(0,0);                    
  lcd.print("Calibrando....");
  delay(500);
  Serial.println("***");
  Serial.println("Iniciar calibración:");
  Serial.println("Coloque la celda de carga en una superficie nivelada y estable.");
  Serial.println("Retire cualquier carga aplicada a la celda de carga.");
  Serial.println("Envíe 't' desde el monitor en serie para establecer el desplazamiento de la tara.");

  boolean _resume = false;
  while (_resume == false) {
    LoadCell.update();
    if (Serial.available() > 0) {
      if (Serial.available() > 0) {
        char inByte = Serial.read();
        if (inByte == 't') LoadCell.tareNoDelay();
      }
    }
    if (LoadCell.getTareStatus() == true) {
      Serial.println("Tara completa");
      _resume = true;
    }
  }

  Serial.println("Ahora, coloque su masa conocida en la celda de carga.");
  Serial.println("Luego, envíe el peso de esta masa (por ejemplo, 100.0) desde el monitor en serie.");

  float masa_conocida = 0;
  _resume = false;
  while (_resume == false) {
    LoadCell.update();
    if (Serial.available() > 0) {
      masa_conocida = Serial.parseFloat();
      if (masa_conocida != 0) {
        Serial.print("La masa conocida es: ");
        Serial.println(masa_conocida);
        _resume = true;
      }
    }
  }

  LoadCell.refreshDataSet(); // Actualizar el conjunto de datos para asegurarse de que la masa conocida se mida correctamente
  float nuevoValorDeCalibracion = LoadCell.getNewCalibration(masa_conocida); // Obtener el nuevo valor de calibración

  Serial.print("El nuevo valor de calibración se ha configurado en: ");
  Serial.print(nuevoValorDeCalibracion);
  Serial.println(", use esto como valor de calibración (calFactor) en su proyecto de sketch.");
  Serial.print("¿Guardar este valor en la dirección de EEPROM ");
  Serial.print(calVal_eepromAdress);
  Serial.println("? s/n");

  _resume = false;
  while (_resume == false) {
    if (Serial.available() > 0) {
      char inByte = Serial.read();
      if (inByte == 's') {
#if defined(ESP8266)|| defined(ESP32)
        EEPROM.begin(512);
#endif
        EEPROM.put(calVal_eepromAdress, nuevoValorDeCalibracion);
#if defined(ESP8266)|| defined(ESP32)
        EEPROM.commit();
#endif
        EEPROM.get(calVal_eepromAdress, nuevoValorDeCalibracion);
        Serial.print("Valor ");
        Serial.print(nuevoValorDeCalibracion);
        Serial.print(" guardado en la dirección de EEPROM: ");
        Serial.println(calVal_eepromAdress);
        _resume = true;

      }
      else if (inByte == 'n') {
        Serial.println("Valor no guardado en EEPROM");
        _resume = true;
      }
    }
  }

  Serial.println("Fin de la calibración");
  Serial.println("***");
  Serial.println("Para recalibrar, envíe 'r' desde el monitor en serie.");
  Serial.println("Para editar manualmente el valor de calibración, envíe 'c' desde el monitor en serie.");
  Serial.println("***");
  lcd.clear();
  lcd.setCursor(0, 0);                    
  lcd.print("Fin de la ");
  lcd.setCursor(0, 1);                    
  lcd.print("calibración");
  delay(800);
  lcd.clear();
}

void cambiarValorDeCalibracionGuardado() {
  float antiguoValorDeCalibracion = LoadCell.getCalFactor();
  boolean _resume = false;
  Serial.println("***");
  Serial.print("El valor actual es: ");
  Serial.println(antiguoValorDeCalibracion);
  Serial.println("Ahora, envíe el nuevo valor desde el monitor en serie, por ejemplo, 696.0");
  float nuevoValorDeCalibracion;
  while (_resume == false) {
    if (Serial.available() > 0) {
      nuevoValorDeCalibracion = Serial.parseFloat();
      if (nuevoValorDeCalibracion != 0) {
        Serial.print("El nuevo valor de calibración es: ");
        Serial.println(nuevoValorDeCalibracion);
        LoadCell.setCalFactor(nuevoValorDeCalibracion);
        _resume = true;
      }
    }
  }
  _resume = false;
  Serial.print("¿Guardar este valor en la dirección de EEPROM ");
  Serial.print(calVal_eepromAdress);
  Serial.println("? s/n");
  while (_resume == false) {
    if (Serial.available() > 0) {
      char inByte = Serial.read();
      if (inByte == 's') {
#if defined(ESP8266)|| defined(ESP32)
        EEPROM.begin(512);
#endif
        EEPROM.put(calVal_eepromAdress, nuevoValorDeCalibracion);
#if defined(ESP8266)|| defined(ESP32)
        EEPROM.commit();
#endif
        EEPROM.get(calVal_eepromAdress, nuevoValorDeCalibracion);
        Serial.print("Valor ");
        Serial.print(nuevoValorDeCalibracion);
        Serial.print(" guardado en la dirección de EEPROM: ");
        Serial.println(calVal_eepromAdress);
        _resume = true;
      }
      else if (inByte == 'n') {
        Serial.println("Valor no guardado en EEPROM");
        _resume = true;
      }
    }
  }
  Serial.println("Fin de cambiar el valor de calibración");
  Serial.println("***");
}
