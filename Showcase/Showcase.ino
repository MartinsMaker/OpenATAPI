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
//*MEDIA
void PauseResume();
void Stop();
void Skip();
void Previous();
//*READING
int TracksCount();
int CurrentTrack();

// Funzioni ATAPI
void eseguiComandoTray(bool apri);
void testUnitReady();
void avviaPlayTraccia1();

//Pause/Resume data
  bool inPause = false;
  int TrackN = 0;
  byte pauseMin = 0;
  byte pauseSec = 0;
  byte pauseFrame = 0;





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
      avviaPlayTraccia();
    }
    else if (comando == 'S' || comando == 's') {
      Serial.println("STOPING READER");
      Stop();
    }
    else if (comando == 'f') {
      Serial.println("RESUMING/PAUSING TRACK");
      PauseResume();
    }
    else if (comando == 'T' || comando == 't') {
      Serial.println("\n[EXEC] Richiesta Numero Totale Tracce...");
      int last = TracksCount();
      if (last > 0) {
        Serial.print("Total number of tracks: ");
        Serial.println(last);
      } else {
        Serial.println("Errore di lettura o nessun disco inserito.");
      }
    }
    else if (comando == 'u') {
      Serial.println("CURRENT TRACK");
      int thisTrack = CurrentTrack();
      Serial.print("Current track number: ");
      Serial.println(thisTrack);
    }
    else if (comando == 'r') {
      Serial.println("PREVIOUS TRACK");
      //Previous();
      //PlayAudioFromLBA(1500, 50000);
      PlayTrack(2);
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
  unsigned long startTime = millis();
  while (readIDE_LowByte(ComSReg) & 0x80) {
    delayMicroseconds(100);
    if (millis() - startTime > 1000) {
      Serial.println("[TIMEOUT] BSY non si azzera.");
      break;
    }
  }
}

void wait_DRQ_set() {
  unsigned long startTime = millis();
  while (!(readIDE_LowByte(ComSReg) & 0x08)) {
    delayMicroseconds(100);
    if (millis() - startTime > 1000) {
      Serial.println("[TIMEOUT] DRQ non si alza.");
      break;
    }
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

void avviaPlayTraccia() {
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

//DOESN'T WORK BECAUSE OF FIRMWARE CRIPPLING, IT SHOULD NORMALLY WORK
void PauseResume(){
  Serial.println("PAUSE/RESUME");
  
  if(inPause == false){

    wait_BSY_clear();
    writeIDE(0xF6, 0xA0, 0xFF); 
    wait_BSY_clear();
    writeIDE(ComSReg, 0xA0, 0xFF); 
    wait_DRQ_set();
    
    // 1. Chiediamo il Sub-channel Q (Formato Posizione 0x01) per leggere il tempo esatto
    writeIDE(DataReg, 0x42, 0x02); // Opcode 0x42 + MSF attivo
    writeIDE(DataReg, 0x40, 0x01); // SubQ + Position Data Format
    writeIDE(DataReg, 0x00, 0x00); 
    writeIDE(DataReg, 0x00, 0x10); // Allocation length 16 byte
    writeIDE(DataReg, 0x00, 0x00); 
    writeIDE(DataReg, 0x00, 0x00); 
  
    delay(50);
    wait_DRQ_set();
  
    // Leggiamo i dati di posizione
    uint16_t word0 = readIDE_Word(DataReg); // Status
    uint16_t word1 = readIDE_Word(DataReg); // Length
    uint16_t word2 = readIDE_Word(DataReg); // Format / Track
    uint16_t word3 = readIDE_Word(DataReg); // Index / Minuto assoluto? 
  
    // Nota: Nei byte successivi (parole 4 e 5) troviamo l'Absolute Address (Min, Sec, Frame)
    uint16_t word4 = readIDE_Word(DataReg); // Contiene Minuto e Secondo assoluto
    uint16_t word5 = readIDE_Word(DataReg); // Contiene Frame e altro
  
    pauseMin   = (byte)(word4 & 0xFF);         // Minuto
    pauseSec   = (byte)((word4 >> 8) & 0xFF);  // Secondo
    pauseFrame = (byte)(word5 & 0xFF);         // Frame

    // Svuotiamo il resto del buffer
    for(int i = 0; i < 2; i++) {
       readIDE_Word(DataReg);
    }
    wait_BSY_clear();


   //Pause Command
   wait_BSY_clear();
   writeIDE(0xF6, 0xA0, 0xFF); 
   wait_BSY_clear();
   writeIDE(ComSReg, 0xA0, 0xFF); 
   wait_DRQ_set();

   writeIDE(DataReg, 0x4B, 0x00); 
   writeIDE(DataReg, 0x00, 0x00); 
   writeIDE(DataReg, 0x00, 0x00); 
   writeIDE(DataReg, 0x00, 0x00); 
   writeIDE(DataReg, 0x00, 0x00); // Resume bit = 0 (Pausa)
   writeIDE(DataReg, 0x00, 0x00); 
   wait_BSY_clear();

   inPause = true;
   
   Serial.print("[PAUSE] Pausing current track: ");
   Serial.print(pauseMin); Serial.print(":");
   Serial.print(pauseSec); Serial.print(":");
   Serial.println(pauseFrame);
   
   
   
   
  }
  else{
  
    Serial.print("[RESUME] Resuming track from stored temporal coordinates ");
  
    // 1. Inviamo un comando di STOP rapido per sbloccare lo stato di "Pausa Hardware" del drive
    wait_BSY_clear();
    writeIDE(0xF6, 0xA0, 0xFF); 
    wait_BSY_clear();
    writeIDE(ComSReg, 0xA0, 0xFF); 
    wait_DRQ_set();

    writeIDE(DataReg, 0x4E, 0x00); // Opcode 0x4E (Stop)
    writeIDE(DataReg, 0x00, 0x00); 
    writeIDE(DataReg, 0x00, 0x00); 
    writeIDE(DataReg, 0x00, 0x00); 
    writeIDE(DataReg, 0x00, 0x00); 
    writeIDE(DataReg, 0x00, 0x00); 
    wait_BSY_clear();

    delay(20); // Breve pausa per lasciare respirare il firmware

    // 2. Ora inviamo il comando PLAY AUDIO MSF con le coordinate esatte
    wait_BSY_clear();
    writeIDE(0xF6, 0xA0, 0xFF); 
    wait_BSY_clear();
    writeIDE(ComSReg, 0xA0, 0xFF); 
    wait_DRQ_set();

    writeIDE(DataReg, 0x47, 0x00);         // Word 1: Opcode 0x47
    writeIDE(DataReg, 0x00, pauseMin);     // Word 2: Starting Minute
    writeIDE(DataReg, pauseSec, pauseFrame); // Word 3: Starting Second & Frame
    writeIDE(DataReg, 0x59, 0x59);         // Word 4: Ending Minute/Second
    writeIDE(DataReg, 0x00, 0x00);         // Word 5: Padding
    writeIDE(DataReg, 0x00, 0x00);         // Word 6: Padding
  
    wait_BSY_clear();
    inPause = false;
    Serial.print(pauseMin); Serial.print(":");
    Serial.print(pauseSec); Serial.print(":");
    Serial.println(pauseFrame);
    RequestSense();
  } 
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

int TracksCount() {
  wait_BSY_clear();
  writeIDE(0xF6, 0xA0, 0xFF); 
  wait_BSY_clear();

  writeIDE(ComSReg, 0xA0, 0xFF); 
  wait_DRQ_set();

  // Pacchetto SCSI READ TOC (0x43)
  writeIDE(DataReg, 0x43, 0x02); 
  writeIDE(DataReg, 0x00, 0x00); 
  writeIDE(DataReg, 0x00, 0x00); 
  writeIDE(DataReg, 0x00, 0x0C); 
  writeIDE(DataReg, 0x00, 0x00); 
  writeIDE(DataReg, 0x00, 0x00); 
  
  delay(50); 
  wait_DRQ_set();
  
  // Parola 0: Lunghezza dati (Byte 0 e 1)
  uint16_t word0 = readIDE_Word(DataReg);
  // Parola 1: Byte basso = Prima traccia, Byte alto = Ultima traccia
  uint16_t word1 = readIDE_Word(DataReg);
  
  byte firstTrack = (byte)(word1 & 0xFF);
  byte LastTrack  = (byte)((word1 >> 8) & 0xFF);
  
  // reading remaining word to empty the buffer
  for(int i = 0; i < 6; i++) {
    readIDE_Word(DataReg);
  }

  wait_BSY_clear();
  
  if (LastTrack == 0 || LastTrack > 99) {
    Serial.println("[ERROR] TOC illeggibile.");
    return -1;
  }
  
  return (int)LastTrack;
}

int CurrentTrack(){
  
  wait_BSY_clear();
  writeIDE(0xF6, 0xA0, 0xFF); 
  wait_BSY_clear();

  writeIDE(ComSReg, 0xA0, 0xFF); 
  wait_DRQ_set();

  writeIDE(DataReg, 0x42, 0x02);  
  writeIDE(DataReg, 0x40, 0x01); 
  writeIDE(DataReg, 0x00, 0x00);  
  writeIDE(DataReg, 0x00, 0x10); // Allocation lenghth (16 byte)
  writeIDE(DataReg, 0x00, 0x00);
  writeIDE(DataReg, 0x00, 0x00);
  
  wait_DRQ_set();
  
  // Reading bytes from the the dataRegister (byte pointer is automatically moved with each reading)
  uint16_t word0 = readIDE_Word(DataReg); // Byte 0 (Reserved), Byte 1 (Audio Status)
  uint16_t word1 = readIDE_Word(DataReg); // Byte 2 (Len MSB), Byte 3 (Len LSB)
  uint16_t word2 = readIDE_Word(DataReg); // Byte 4 (Data Format), Byte 5 (ADR/Control)
  uint16_t word3 = readIDE_Word(DataReg); // Byte 6 (CURRENT TRACK!), Byte 7 (Index)
  
  byte audioStatus = (byte)((word0 >> 8) & 0xFF);
  byte currTrack   = (byte)(word3 & 0xFF); // La traccia corrente è nel byte basso della word 3

  // Svuotiamo le restanti 4 parole (8 byte) per chiudere il buffer
  for(int i = 0; i < 4; i++) {
    readIDE_Word(DataReg);
  }
  int currTrackNumber = int(currTrack);
  wait_BSY_clear();
  
  return currTrackNumber;

}

void PreviousMIO(){
  
  
  
  
  int currentTrack = CurrentTrack();
  int MaxTracksN = TracksCount();
  int TrackToPlay;

  if(currentTrack >= 2){
    TrackToPlay = currentTrack - 1;

  }
  else{
    TrackToPlay = 1;
  }
  
  Stop();
  delay(300);

  testUnitReady();
  Serial.print("Playing previous track n: ");
  Serial.println(TrackToPlay);
  // 1. Assicuriamoci che il bus sia libero e selezioniamo il Master
  wait_BSY_clear();
  writeIDE(0xF6, 0xA0, 0xFF); 
  wait_BSY_clear();

  // 2. Segnaliamo l'arrivo di un pacchetto ATAPI
  writeIDE(ComSReg, 0xA0, 0xFF); 
  wait_DRQ_set();

  // 3. Invio del pacchetto ATAPI PLAY AUDIO TRACK/INDEX (0x48) - 12 byte totali
  writeIDE(DataReg, 0x48, 0x00);           // Word 0: Opcode 0x48 + Byte 2 (0x00)
  writeIDE(DataReg, 0x00, TrackToPlay);           // Word 1: Reserved (Byte 3 e 4)
  writeIDE(DataReg, 0x01, (byte)MaxTracksN);  // Word 2: Byte 5 (Reserved), Byte 6 = Traccia di inizio (Starting Track)
  writeIDE(DataReg, 0x01, 0x00);           // Word 3: Byte 7 = Indice di inizio (solitamente 1), Byte 8 = Reserved
  writeIDE(DataReg, 0x00, 0x00);  // Word 4: Byte 9 = Traccia di fine (Ending Track - mettiamo la stessa per non fermarci), Byte 10 = Res.
  writeIDE(DataReg, 0x01, 0x00);           // Word 5: Byte 11 = Indice di fine (1), Byte 12 = Reserved
  
  wait_BSY_clear();
  RequestSense();
}

void wait_DRQ_or_Error() {
  unsigned long startTime = millis();
  while (true) {
    byte status = readIDE_LowByte(ComSReg);
    
    // Se DRQ si alza, i dati sono pronti
    if (status & 0x08) break;
    
    // Se il lettore segnala un errore (ERR bit attivo), usciamo dal ciclo
    if (status & 0x01) {
      Serial.println("[ERRORE IDE] Il lettore ha rifiutato il comando (ERR bit attivo)!");
      break;
    }
    
    if (millis() - startTime > 1000) {
      Serial.println("[TIMEOUT] Timeout in attesa di risposta dal drive.");
      break;
    }
    delayMicroseconds(100);
  }
}

uint16_t readIDE_Word(byte regval) {
  byte reg = regval & 0x7F; 
  Wire1.beginTransmission(AddrRegSel);  Wire1.write(reg);  Wire1.endTransmission();
  
  Wire1.requestFrom(AddrDataL, 1);
  byte lowByte = Wire1.read();
  
  Wire1.requestFrom(AddrDataH, 1);
  byte highByte = Wire1.read();
  
  highZ(); 
  return (uint16_t)((highByte << 8) | lowByte);
}

//ERROR DETECTION FUNCTION REFER TO ERROR TABLES 
void RequestSense() {
  Serial.println("[DIAGNOSTICA] Richiesta Sense Data al lettore...");

  wait_BSY_clear();
  writeIDE(0xF6, 0xA0, 0xFF); // Seleziona Master
  wait_BSY_clear();

  writeIDE(ComSReg, 0xA0, 0xFF); 
  wait_DRQ_set();

  // Pacchetto SCSI REQUEST SENSE (0x03) - 12 byte totali (6 parole)
  writeIDE(DataReg, 0x03, 0x00); // Word 0: Opcode 0x03
  writeIDE(DataReg, 0x00, 0x00); // Word 1: Reserved
  writeIDE(DataReg, 0x00, 0x00); // Word 2: Reserved
  writeIDE(DataReg, 0x00, 0x12); // Word 3: Allocation Length (18 byte di risposta standard)
  writeIDE(DataReg, 0x00, 0x00); // Word 4: Padding
  writeIDE(DataReg, 0x00, 0x00); // Word 5: Padding
  
  delay(50);
  wait_DRQ_set();
  
  // Leggiamo i dati di risposta (18 byte = 9 parole)
  uint16_t word0 = readIDE_Word(DataReg); // Byte 0 (Error Code), Byte 1 (Reserved)
  uint16_t word1 = readIDE_Word(DataReg); // Byte 2 (Sense Key), Byte 3 (Information...)
  uint16_t word2 = readIDE_Word(DataReg); // ...
  uint16_t word3 = readIDE_Word(DataReg); // ...
  uint16_t word4 = readIDE_Word(DataReg); // ...
  uint16_t word5 = readIDE_Word(DataReg); // Byte 10, 11
  uint16_t word6 = readIDE_Word(DataReg); // Byte 12 (ASC), Byte 13 (ASCQ)
  
  // Estrazione dei parametri chiave
  byte senseKey = (byte)((word1 >> 8) & 0x0F); // I 4 bit bassi del byte 2
  byte asc      = (byte)(word6 & 0xFF);        // Additional Sense Code (Byte 12)
  byte ascq     = (byte)((word6 >> 8) & 0xFF); // Additional Sense Code Qualifier (Byte 13)

  // Svuotiamo le ultime parole del buffer
  readIDE_Word(DataReg);
  readIDE_Word(DataReg);

  wait_BSY_clear();

  // Stampiamo il responso in chiaro sul monitor seriale
  Serial.print("-> Sense Key (SK): 0x"); Serial.println(senseKey, HEX);
  Serial.print("-> Addr. Sense Code (ASC): 0x"); Serial.println(asc, HEX);
  Serial.print("-> Sense Code Qual. (ASCQ): 0x"); Serial.println(ascq, HEX);
}
uint32_t getTrackLBA(int trackNum) {
  wait_BSY_clear();
  writeIDE(0xF6, 0xA0, 0xFF); // Seleziona Master
  wait_BSY_clear();

  writeIDE(0xF4, 0x00, 0x02); 
  writeIDE(ComSReg, 0xA0, 0xFF); 
  
  unsigned long startTime = millis();
  bool drqReady = false;
  while (millis() - startTime < 300) {
    if (readIDE_Word(ComSReg) & 0x08) {
      drqReady = true;
      break;
    }
  }

  if (!drqReady) {
    // Timeout DRQ: usiamo direttamente la stima senza bloccare il bus
    return 150 + ((trackNum - 1) * 20000);
  }

  // Invio pacchetto SCSI READ TOC (0x43)
  writeIDE(DataReg, 0x43, 0x00);           
  writeIDE(DataReg, 0x00, 0x00);           
  writeIDE(DataReg, 0x00, 0x00);           
  writeIDE(DataReg, 0x00, (byte)trackNum); 
  writeIDE(DataReg, 0x00, 0x12);           
  writeIDE(DataReg, 0x00, 0x00);           
  
  delay(15);
  
  uint16_t dataLen   = readIDE_Word(DataReg);
  uint16_t trackInfo = readIDE_Word(DataReg); 
  uint16_t descW0    = readIDE_Word(DataReg); 
  uint16_t descW1    = readIDE_Word(DataReg); 
  uint16_t lbaHigh   = readIDE_Word(DataReg); 
  uint16_t lbaLow    = readIDE_Word(DataReg); 

  readIDE_Word(DataReg); 
  wait_BSY_clear();

  uint32_t lba = ((uint32_t)lbaHigh << 16) | lbaLow;

  // CONTROLLO DI VALIDITÀ RIGOROSO:
  // Un LBA valido per un CD audio deve essere compreso tra 150 e 350.000
  if (lba < 150 || lba > 350000) {
    Serial.print("[TOC FALLBACK] LBA non valido letto (");
    Serial.print(lba);
    Serial.println("), uso stima standard.");
    
    // Stima affidabile basata sulla posizione della traccia (es. 20.000 blocchi per traccia)
    return 150 + ((trackNum - 1) * 20000);
  }

  Serial.print("[TOC PIVOT] Traccia ");
  Serial.print(trackNum);
  Serial.print(" -> LBA Reale: ");
  Serial.println(lba);

  return lba;
}



uint32_t getTrackLength(int trackNum) {
  int totalTracks = TracksCount();
  uint32_t currentLBA = getTrackLBA(trackNum);
  uint32_t nextLBA;

  // Se è l'ultima traccia, puntiamo al Lead-out (0xAA)
  if (trackNum >= totalTracks) {
    nextLBA = getTrackLBA(0xAA); 
  } else {
    nextLBA = getTrackLBA(trackNum + 1);
  }

  uint32_t lengthInBlocks = (nextLBA > currentLBA) ? (nextLBA - currentLBA) : 10000;
  return lengthInBlocks;
}



void PreviousV(){
  int currentTrack = CurrentTrack();
  int MaxTracksN = TracksCount();
  int TrackToPlay;
  
  
 
  if(currentTrack >= 2){
    TrackToPlay = currentTrack - 1;
  } else {
    TrackToPlay = 1;
  }
  
  Serial.print("Saltando alla traccia precedente n: ");
  Serial.println(TrackToPlay);

  // Ricaviamo l'LBA e la durata esatta tramite la TOC
  uint32_t targetLBA = getTrackLBA(TrackToPlay);
  uint16_t trackLen  = getTrackLength(TrackToPlay);

  // Stop di sicurezza e riproduzione tramite Play Audio (10) [0x45]
  Stop();
  delay(100);

  wait_BSY_clear();
  writeIDE(0xF6, 0xA0, 0xFF); 
  wait_BSY_clear();

  writeIDE(ComSReg, 0xA0, 0xFF); 
  wait_DRQ_set(); 

  // Invio comando PLAY AUDIO (10) [0x45] basato sulla documentazione ufficiale
  writeIDE(DataReg, 0x45, 0x00);                             // Word 0: Opcode 0x45 + Reserved
  writeIDE(DataReg, (targetLBA >> 24) & 0xFF, (targetLBA >> 16) & 0xFF); // Word 1: LBA (Byte 2 & 3)
  writeIDE(DataReg, (targetLBA >> 8) & 0xFF, targetLBA & 0xFF);          // Word 2: LBA (Byte 4 & 5)
  writeIDE(DataReg, 0x00, (trackLen >> 8) & 0xFF);           // Word 3: Reserved + Play Length MSB
  writeIDE(DataReg, trackLen & 0xFF, 0x00);                  // Word 4: Play Length LSB + Control
  writeIDE(DataReg, 0x00, 0x00);                             // Word 5: Padding
  
  wait_BSY_clear();
}
void resetATAPIDrive() {
  Serial.println("[RESET] Tentativo di sblocco bus ATAPI...");
  
  wait_BSY_clear();
  writeIDE(0xF6, 0xA0, 0xFF); // Seleziona Master
  wait_BSY_clear();

  // Invia il comando di Device Reset ATAPI (0x08)
  writeIDE(ComSReg, 0x08, 0xFF); 
  delay(100); // Attesa di ricalibrazione del firmware del lettore
  wait_BSY_clear();
  
  Serial.println("[RESET] Bus sbloccato.");
}

void PlayAudioFromLBA(uint32_t targetLBA, uint32_t playLength) {
  // 1. Stop di sicurezza per fermare la meccanica in corso
  Stop();
  delay(120);

  Serial.print("[PLAYER] Play Audio (10) [0x45] - LBA: ");
  Serial.print(targetLBA);
  Serial.print(" - Durata blocchi: ");
  Serial.println(playLength);

  // 2. Preparazione standard del bus IDE per ricezione pacchetto ATAPI (0xA0)
  wait_BSY_clear();
  writeIDE(0xF6, 0xA0, 0xFF); // Seleziona il Master
  wait_BSY_clear();

  // Impostiamo il limite di byte del buffer PIO (necessario per non far bloccare il DRQ)
  writeIDE(0xF4, 0x00, 0x02); // Cylinder Low/High = 512 bytes

  // Invio del comando di inizio pacchetto ATAPI
  writeIDE(ComSReg, 0xA0, 0xFF); 
  
  // Attendiamo che il lettore alzi il flag DRQ (Data Request)
  wait_DRQ_set(); 

  // 3. Invio del pacchetto SCSI di 10 byte suddiviso in 5 parole da 16 bit:
  // Word 0 (Byte 0 & 1): Opcode 0x45 + Reserved (RelAdr = 0)
  writeIDE(DataReg, 0x45, 0x00); 
  
  // Word 1 (Byte 2 & 3): LBA MSB e Byte successivo
  writeIDE(DataReg, (targetLBA >> 24) & 0xFF, (targetLBA >> 16) & 0xFF); 
  
  // Word 2 (Byte 4 & 5): LBA Byte intermedio e LSB
  writeIDE(DataReg, (targetLBA >> 8) & 0xFF, targetLBA & 0xFF);          
  
  // Word 3 (Byte 6 & 7): Reserved (0x00) + Play Length MSB
  writeIDE(DataReg, 0x00, (playLength >> 8) & 0xFF);          
  
  // Word 4 (Byte 8 & 9): Play Length LSB + Control (0x00)
  writeIDE(DataReg, playLength & 0xFF, 0x00);                 
  
  // Word 5 (Padding finale di chiusura pacchetto a 12 byte totali per il bus)
  writeIDE(DataReg, 0x00, 0x00);                             
  
  // 4. Attendiamo che il lettore completi l'accettazione del comando
  wait_BSY_clear();
}
void PlayTrack(int trackNum) {
  int maxTracks = TracksCount();

  // Controllo di sicurezza sui limiti del CD
  if (maxTracks > 0 && (trackNum < 1 || trackNum > maxTracks)) {
    Serial.print("[ERRORE] Traccia ");
    Serial.print(trackNum);
    Serial.println(" non valida!");
    trackNum = 1; 
  }

  Serial.print("[PLAYER] Traduzione e avvio traccia n: ");
  Serial.println(trackNum);

  // Ricaviamo LBA e durata in blocchi al volo
  uint32_t targetLBA = getTrackLBA(trackNum);
  uint32_t trackLen  = getTrackLength(trackNum);

  // Eseguiamo il play tramite il comando 0x45 collaudato
  PlayAudioFromLBA(targetLBA, trackLen);
}



