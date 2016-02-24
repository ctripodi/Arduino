#include <SPI.h>
#include <Wire.h>
#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>
#include <SdFat.h>

#define MFRC_RST_PIN 8
#define MFRC_SS_PIN  9
#define SD_SS_PIN    10

#define STATE_STARTUP       0
#define STATE_STARTUP_ERROR 1
#define STATE_STARTING      2
#define STATE_WAITING       3
#define STATE_SCAN_INVALID  4
#define STATE_SCAN_VALID    5
#define STATE_SCAN_MASTER   6
#define STATE_ADDED_CARD    7
#define STATE_REMOVED_CARD  8

#define REDPIN 6
#define GREENPIN 7

const int cardSize = 4;
byte masterCard[cardSize] = {29,156,78,37};
byte readCard[cardSize];

// Create MFRC522 instance
MFRC522 mfrc522(MFRC_SS_PIN, MFRC_RST_PIN);
// Set the LCD I2C address
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

SdFat sd;

byte currentState = STATE_STARTUP;
unsigned long LastStateChangeTime;
unsigned long StateWaitTime;

//------------------------------------------------------------------------------------
void StartSD()
{
  pinMode(SD_SS_PIN, OUTPUT);
  pinMode(MFRC_SS_PIN, OUTPUT);

  digitalWrite(SD_SS_PIN, LOW);
  digitalWrite(MFRC_SS_PIN, HIGH);
}

//------------------------------------------------------------------------------------
void StartMFRC()
{
  pinMode(SD_SS_PIN, OUTPUT);
  pinMode(MFRC_SS_PIN, OUTPUT);

  digitalWrite(SD_SS_PIN, HIGH);
  digitalWrite(MFRC_SS_PIN, LOW);
}

//------------------------------------------------------------------------------------
void PrintCard()
{
  int index;

  Serial.print("Card - ");
  for(index = 0; index < 4; index++)
  {
    if (index > 0)
    {
      Serial.print(",");
    }
    Serial.print(readCard[index]);
  }
  Serial.println(" ");
}

//------------------------------------------------------------------------------------
boolean findCard()
{
  SdFile readFile;
  char inputChar;
  byte currentCard[cardSize];
  int cardIndex;

  //Serial.print("find ");
  //PrintCard();

  StartSD();
  if (!sd.exists("cards.txt"))
  {
    return false;
  }

  if (!readFile.open("cards.txt", O_RDWR | O_CREAT | O_AT_END))
  {
    return false;
  }

  cardIndex = 0;
  inputChar = readFile.read();
  while (inputChar != 'eof')
  {
    if (inputChar != '\n')
    {
      currentCard[cardIndex] = inputChar;
      cardIndex ++;
    }
    else
    {
      if ((memcmp(currentCard, readCard, 4)) == 0)
      {
        readFile.close();

        //Serial.println("found card");
        StartMFRC();
        return true;
      }
      cardIndex = 0;
    }
    inputChar = readFile.read();
  }
  StartMFRC();
  return false;
}

//------------------------------------------------------------------------------------
void addCard()
{
  int index;
  SdFile writeFile;

  //Serial.print("add ");
  //PrintCard();
  StartSD();
  if (writeFile.open("cards.txt", O_RDWR | O_CREAT | O_AT_END))
  { 
    for(index = 0; index < 4; index++)
    {
      writeFile.print(readCard[index]); 
    }
    writeFile.print('\n'); 
    writeFile.close();
  }
  StartMFRC();
  return;
}

//------------------------------------------------------------------------------------
void removeCard()
{
  int cardIndex;
  SdFile readFile;
  SdFile writeFile;
  byte currentCard[cardSize];
  char inputChar;
  int index;

  //Serial.print("remove ");
  //PrintCard();
  StartSD();
  if (!sd.exists("cards.txt"))
  {
    if (readFile.open("cards.txt", O_RDWR | O_CREAT | O_AT_END))
    { 
      if (writeFile.open("cards_new.txt", O_RDWR | O_CREAT | O_AT_END))
      {
        cardIndex = 0;
        inputChar = readFile.read();
        while (inputChar != 'eof')
        {
          if (inputChar != '\n')
          {
            currentCard[cardIndex] = inputChar;
            cardIndex ++;
          }
          else
          {
            if (!((memcmp(currentCard, readCard, 4)) == 0))
            {
              for(index = 0; index < 4; index++)
              {
                 writeFile.print(currentCard[index]); 
              }
              writeFile.print('\n'); 
            }
            cardIndex = 0;
          }
          inputChar = readFile.read();
        }
        writeFile.close();
      }
      readFile.close();
    }
  }
  StartMFRC();
  return;
}

//------------------------------------------------------------------------------------
int readCardState()
{
  int index;

  for(index = 0; index < 4; index++)
  {
    readCard[index] = mfrc522.uid.uidByte[index];
  }
  //PrintCard();

  //Check Master Card
  if ((memcmp(readCard, masterCard, 4)) == 0)
  {
    return STATE_SCAN_MASTER;
  }

  if (findCard() == true)
  {
    return STATE_SCAN_VALID;
  }

 return STATE_SCAN_INVALID;
}

//------------------------------------------------------------------------------------
void DisplayInfo(char *Line1Str, char *Line2Str, int RedPinState, int GreenPinState)
{
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(Line1Str);
  lcd.setCursor(0,1);
  lcd.print(Line2Str);
  digitalWrite(REDPIN, RedPinState);
  digitalWrite(GREENPIN, GreenPinState);
}

//------------------------------------------------------------------------------------
void updateState(byte aState)
{
  if (aState == currentState)
  {
    return;
  }

  // do state change
  switch (aState)
  {
    case STATE_STARTING:
      DisplayInfo("RFID Scanner", "Starting up", HIGH, HIGH);
      StateWaitTime = 1000;
      break;
      
    case STATE_STARTUP_ERROR:
      DisplayInfo("Error", "SD card", HIGH, HIGH);
      StateWaitTime = 1000;
      break;
      
    case STATE_WAITING:
      DisplayInfo("Waiting for Card", "to be swiped", LOW, LOW);
      StateWaitTime = 1000;
      break;
      
    case STATE_SCAN_INVALID:
      if (currentState == STATE_SCAN_MASTER)
      {
        addCard();
        aState = STATE_ADDED_CARD;

        DisplayInfo("Card Scanned", "Card Added", LOW, HIGH);
        StateWaitTime = 2000;
      }
      else if (currentState == STATE_REMOVED_CARD)
      {
        return;
      }
      else
      {
        DisplayInfo("Card Scanned", "Invalid Card", HIGH, LOW);
        StateWaitTime = 2000;
      }
      break;
      
    case STATE_SCAN_VALID:
      if (currentState == STATE_SCAN_MASTER)
      {
        removeCard();
        aState = STATE_REMOVED_CARD;

        DisplayInfo("Card Scanned", "Card Removed", LOW, HIGH);
        StateWaitTime = 2000;
      }
      else if (currentState == STATE_ADDED_CARD)
      {
        return;
      }
      else
      {
        DisplayInfo("Card Scanned", "Valid Card", LOW, HIGH);
        StateWaitTime = 2000;
      }
      break;
      
    case STATE_SCAN_MASTER:
      DisplayInfo("Master Card", "", LOW, HIGH);
      StateWaitTime = 5000;
      break;
  }

  //Serial.print("Current State - ");
  //Serial.print(currentState);
  //Serial.print(", New State - ");
  //Serial.println(aState);

  currentState = aState;
  LastStateChangeTime = millis();
}

//------------------------------------------------------------------------------------
void setup() 
{
  SPI.begin();         // Init SPI Bus
  mfrc522.PCD_Init();  // Init MFRC522

  if (!sd.begin(SD_SS_PIN, SPI_HALF_SPEED))
  {
    updateState(STATE_STARTUP_ERROR);   
  }

  lcd.begin(20,4);

  LastStateChangeTime = millis();
  updateState(STATE_STARTING);

  pinMode(REDPIN, OUTPUT);
  pinMode(GREENPIN, OUTPUT);

  //Serial.begin(9600);
}

//------------------------------------------------------------------------------------
void loop() 
{
  byte cardState;

  if ((currentState != STATE_WAITING) &&
      (currentState != STATE_STARTUP_ERROR) &&
      (StateWaitTime > 0) &&
      (LastStateChangeTime + StateWaitTime < millis()))
  {
    updateState(STATE_WAITING);
  }

  // Look for new cards 
  if ( ! mfrc522.PICC_IsNewCardPresent()) 
  { 
    return; 
  } 
  
  // Select one of the cards 
  if ( ! mfrc522.PICC_ReadCardSerial()) 
  { 
    return; 
  }

  if (currentState != STATE_STARTUP_ERROR)
  {
    cardState = readCardState();
    updateState(cardState);
  }
}
