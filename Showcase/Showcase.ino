// =========================================================================
// ATAPI INTERFACE FUNCTIONS SHOWCASE
// =========================================================================

#include <Wire.h> //Working sketch with Wire1 fix

// Indirizzi I2C degli expander
const int AddrDataL = 0x20;  // PCF #1: Linee IDE D0-D7
const int AddrDataH = 0x21;  // PCF #2: Linee IDE D8-D15
const int AddrRegSel = 0x22; // PCF #3: Linee di controllo IDE

// Registri IDE
const byte DataReg = 0xF0;   // Registro 0 (Dati)
const byte ComSReg = 0xF7;   // Registro 7 (Comandi/Status)

void highZ();
void reset_IDE();
void writeIDE(byte regval, byte dataLval, byte dataHval);
byte readIDE_LowByte(byte regval);
void wait_BSY_clear();
void wait_DRQ_set();

// PLAYER FUNCTIONS
void PauseResume();
void Stop();
void Skip();
void Previous();

// Funzioni ATAPI
void eseguiComandoTray(bool apri);
void testUnitReady();
void avviaPlayTraccia1();




void setup() {
  Serial.begin(115200);

  while (!Serial);

  Serial.println("\n==================================================");
  Serial.println("    ATAPI CONTROLLER - FUNCIONS SHOWCASE      ");
  Serial.println("==================================================");
  Serial.println(" AVAILABLE SERIAL COMMANDS:");
  Serial.println("  [O] -> APRI il carrello");
  Serial.println("  [C] -> CHIUDI il carrello");
  Serial.println("  [P] -> AVVIA PLAY AUDIO (Traccia 1)");
  Serial.println("  [STOP/stop] -> STOPS current track");
  Serial.println("  [PAUSE/pause/RESUME/resume] -> PAUSES/RESUMES current track");
  Serial.println("==================================================");

  //Wire1.begin(15, 14); 
  Wire1.setSDA(14);
  Wire1.setSCL(15);
  Wire1.begin();
  Wire1.setClock(400000);
  highZ();
  reset_IDE();
}



void loop() {
  delay(10);
  if (Serial.available() > 0) {
    char comando = Serial.read();
    if (comando == '\n' || comando == '\r') return;

    if (comando == 'O' || comando == 'o') {
      Serial.println("\n[EXEC] Comando APRI CARRELLO...");
      eseguiComandoTray(true);
    }
    else if (comando == 'C' || comando == 'c') {
      Serial.println("\n[EXEC] Comando CHIUDI CARRELLO...");
      eseguiComandoTray(false);
    }
    else if (comando == 'P' || comando == 'p') {
      Serial.println("\n[EXEC] Esecuzione sequenza di PLAY AUDIO...");
      avviaPlayTraccia1();
    }
    else if (comando == 'STOP' || comando == 'stop') {
      Serial.println("STOPING READER");
      Stop();
    }
    else if (comando == 'PAUSE' || comando == 'pause' || comando == 'RESUME' || comando == 'resume') {
      Serial.println("RESUMING/PAUSING TRACK");
      PauseResume();
    }
  
  
  
  
  
  
  }
  
}

// =========================================================================
// FUNZIONI DI BASSO LIVELLO (BUS E I2C)
// =========================================================================

void highZ() {
  Wire1.beginTransmission(AddrRegSel); Wire1.write(0xFF); Wire1.endTransmission();
  Wire1.beginTransmission(AddrDataH);  Wire1.write(0xFF); Wire1.endTransmission();
  Wire1.beginTransmission(AddrDataL);  Wire1.write(0xFF); Wire1.endTransmission();
}

void reset_IDE() {
  Serial.println("[INIT] Hard Reset del lettore...");
  Wire1.beginTransmission(AddrRegSel);  Wire1.write(0xD6);  Wire1.endTransmission();
  delay(100);
  Wire1.beginTransmission(AddrRegSel);  Wire1.write(0xFF);  Wire1.endTransmission();
  delay(500);
  Serial.println("[INIT] Lettore pronto.");
}

void writeIDE(byte regval, byte dataLval, byte dataHval) {
  byte reg;
  reg = regval | 0x40; 
  Wire1.beginTransmission(AddrRegSel);  Wire1.write(reg);  Wire1.endTransmission();
  Wire1.beginTransmission(AddrDataL);   Wire1.write(dataLval); Wire1.endTransmission();
  Wire1.beginTransmission(AddrDataH);   Wire1.write(dataHval); Wire1.endTransmission();
  reg = regval & 0xBF; 
  Wire1.beginTransmission(AddrRegSel);  Wire1.write(reg);  Wire1.endTransmission();
  delayMicroseconds(15); // Incrementato leggermente per i chipset del 1999
  reg = regval | 0x40; 
  Wire1.beginTransmission(AddrRegSel);  Wire1.write(reg);  Wire1.endTransmission();
  highZ(); 
}

byte readIDE_LowByte(byte regval) {
  byte reg = regval & 0x7F; 
  Wire1.beginTransmission(AddrRegSel);  Wire1.write(reg);  Wire1.endTransmission();
  Wire1.requestFrom(AddrDataL, 1);
  byte valore = Wire1.read();
  highZ(); 
  return valore;
}

void wait_BSY_clear() {
  while (readIDE_LowByte(ComSReg) & 0x80) {
    delayMicroseconds(100); 
  }
}

void wait_DRQ_set() {
  while (!(readIDE_LowByte(ComSReg) & 0x08)) {
    delayMicroseconds(100);
  }
}

// =========================================================================
// IMPLEMENTAZIONE COMANDI ATAPI
// =========================================================================

void eseguiComandoTray(bool apri) {
  wait_BSY_clear();
  writeIDE(0xF6, 0xA0, 0xFF); 
  wait_BSY_clear();

  writeIDE(ComSReg, 0xA0, 0xFF); 
  wait_DRQ_set();

  byte parametroTray = apri ? 0x02 : 0x03;

  writeIDE(DataReg, 0x1B, 0x00); 
  writeIDE(DataReg, 0x00, 0x00); 
  writeIDE(DataReg, parametroTray, 0x00); 
  writeIDE(DataReg, 0x00, 0x00); 
  writeIDE(DataReg, 0x00, 0x00); 
  writeIDE(DataReg, 0x00, 0x00); 

  wait_BSY_clear();
  Serial.println("[OK] Pacchetto Moviemento Vassoio inviato.");
}

// Interroga il lettore per svegliare la logica di decodifica interna
void testUnitReady() {
  wait_BSY_clear();
  writeIDE(0xF6, 0xA0, 0xFF);
  wait_BSY_clear();

  writeIDE(ComSReg, 0xA0, 0xFF);
  wait_DRQ_set();

  // Pacchetto nullo a 12 byte (0x00) per sbloccare la macchina a stati interna
  writeIDE(DataReg, 0x00, 0x00);
  writeIDE(DataReg, 0x00, 0x00);
  writeIDE(DataReg, 0x00, 0x00);
  writeIDE(DataReg, 0x00, 0x00);
  writeIDE(DataReg, 0x00, 0x00);
  writeIDE(DataReg, 0x00, 0x00);
  
  wait_BSY_clear();
}

void avviaPlayTraccia1() {
  Serial.println("[PLAY] 1. Sblocco e verifica presenza disco...");
  testUnitReady();
  delay(200); // Pausa di sicurezza

  Serial.println("[PLAY] 2. Selezione Drive Master...");
  writeIDE(0xF6, 0xA0, 0xFF); 
  wait_BSY_clear();

  Serial.println("[PLAY] 3. Innesco comando PACKET...");
  writeIDE(ComSReg, 0xA0, 0xFF); 
  wait_DRQ_set();

  Serial.println("[PLAY] 4. Scarico parametri MSF (00:02:00 -> 15:00:00)...");
  // La sintassi corretta esige che i byte siano passati con precisione millimetrica
  writeIDE(DataReg, 0x47, 0x00); // Word 1: Opcode PLAY AUDIO MSF
  writeIDE(DataReg, 0x00, 0x00); // Word 2: Starting Minute = 00
  writeIDE(DataReg, 0x02, 0x00); // Word 3: Starting Second = 02, Starting Frame = 00
  writeIDE(DataReg, 0x0F, 0x00); // Word 4: Ending Minute = 15 (0x0F), Ending Second = 00
  writeIDE(DataReg, 0x00, 0x00); // Word 5: Ending Frame = 00, Sub-channel/Reserved
  writeIDE(DataReg, 0x00, 0x00); // Word 6: Reserved
  
  wait_BSY_clear();
  Serial.println("[OK] Richiesta inoltrata. Osserva se il motore gira!");
}

void PauseResume(){
  Serial.println("PAUSE/RESUME");
  
  writeIDE(0xF6, 0xA0, 0xFF); // Seleziona il Drive Master
  wait_BSY_clear();

  writeIDE(ComSReg, 0xA0, 0xFF); // Avvisa il drive che arriva un pacchetto
  wait_DRQ_set();

  // Invia il pacchetto ATAPI di 12 byte (6 scritture a 16 bit)
  writeIDE(DataReg, 0x4B, 0x00); // Word 1: Opcode 0x4E (Stop) + Byte 2 (0x00)
  writeIDE(DataReg, 0x00, 0x00); // Word 2
  writeIDE(DataReg, 0x00, 0x00); // Word 3
  writeIDE(DataReg, 0x00, 0x00); // Word 4
  writeIDE(DataReg, 0x00, 0x00); // Word 5
  writeIDE(DataReg, 0x00, 0x00); // Word 6
  
  wait_BSY_clear();
  
}

void Stop(){
  Serial.println("STOP");
  
  writeIDE(0xF6, 0xA0, 0xFF); // Seleziona il Drive Master
  wait_BSY_clear();

  writeIDE(ComSReg, 0xA0, 0xFF); // Avvisa il drive che arriva un pacchetto
  wait_DRQ_set();

  // Invia il pacchetto ATAPI di 12 byte (6 scritture a 16 bit)
  writeIDE(DataReg, 0x4E, 0x00); // Word 1: Opcode 0x4E (Stop) + Byte 2 (0x00)
  writeIDE(DataReg, 0x00, 0x00); // Word 2
  writeIDE(DataReg, 0x00, 0x00); // Word 3
  writeIDE(DataReg, 0x00, 0x00); // Word 4
  writeIDE(DataReg, 0x00, 0x00); // Word 5
  writeIDE(DataReg, 0x00, 0x00); // Word 6
  
  wait_BSY_clear();
}

