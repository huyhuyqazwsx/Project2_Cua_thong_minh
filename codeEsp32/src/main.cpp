#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <Keypad.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>

//Screen
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define SCREEN_ADDRESS 0x3C // 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);


//MFRC522
#define SS_PIN 4      // SDA pin for MFRC522
#define RST_PIN 18    // Reset pin for MFRC522
#define SCK 16        // SPI Clock pin
#define MOSI 17      // SPI Master Out Slave In pin
#define MISO 5       // SPI Master In Slave Out pin

//RFID
MFRC522 rfid(SS_PIN, RST_PIN);

String passId[10] ={
  "B1161505" // true password
};

unsigned long lastRfidCheckTime = 0;

//Keypad
#define ROW_NUM     4 // four rows
#define COLUMN_NUM  3 // three columns

char keys[ROW_NUM][COLUMN_NUM] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};

byte pin_rows[ROW_NUM] = { 13, 12, 14, 27 }; // Connect to the row pinouts of the keypad
byte pin_column[COLUMN_NUM] = { 26, 25, 33 }; // Connect to the column pinouts of the keypad
Keypad keypad = Keypad(makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM);

//For firebase
#define WiFi_SSID "Bắt Con Sóc"
#define WiFi_PASSWORD "12121212"

#define API_KEY "AIzaSyA-4jl9_VxBc9COVA0mOVBZV2586ApfobM"
#define DATABASE_URL "https://smart-door-2-7d605-default-rtdb.asia-southeast1.firebasedatabase.app/"

#define EMAIL "esp32_121233@gmail.com"
#define PASSWORD "121233"

#define DEVICE_ID "121233"
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

String doorPathStatus = "doors/" + String(DEVICE_ID) + "/status";
bool isAuth = false;
unsigned long lastInternetCheckTime = 0; 

//for password
String truePassword = "121233";
int lengthpassword = truePassword.length();
String midPassword = "";


//Led for door
#define LED_PIN 2 // Pin for the door LED

//count time
unsigned long startTime = 0; // will store last time LED was updated

void enterPassword(){
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Enter password:");
  display.display();
}

void openDoor(){
  unsigned long currentMillis = millis();
  startTime = currentMillis; // Reset the timer
  Serial.println("Opening door...");

  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Door opened!");
  display.display();
  digitalWrite(LED_PIN, HIGH); // Turn on the LED

  while (millis() - startTime < 5000) { // Keep the door open for 5 seconds
    // Do nothing, just wait
  }

  digitalWrite(LED_PIN, LOW); // Turn off the LED
  Serial.println("Door closed.");
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Door closed.");

  enterPassword();
  midPassword = "";
  
}

void displayInit(){
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)){
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  display.clearDisplay();
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0,0);     // Start at top-left corner
  display.display();
}

void handlePassword(){
  if(midPassword == truePassword){
    Serial.println("Password correct!");
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Password correct!");
    display.display();

    openDoor();
  }
  else{
    Serial.println("Password incorrect!");
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Password incorrect!");

    delay(200); // Show the message for 2 seconds

    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Enter password:");
    midPassword = ""; // Reset the password input
    display.display();
  }
}

void checkFireBaseConnect(){
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;

    Serial.println("Logging in to Firebase...");
    auth.user.email = EMAIL;
    auth.user.password = PASSWORD;

    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);


    if (Firebase.ready()){
    Serial.println("Firebase authentication successful");
    if(auth.token.uid.length() > 0) {
      Serial.print("User UID: ");
      Serial.println(auth.token.uid.c_str());
    }
    isAuth = true;

    }
    else{
      Serial.println("Firebase authentication failed");
      isAuth = false;
    }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

  //Led
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // Turn off the LED initially
  // Screen
  displayInit();

  // RFC522
  SPI.begin(SCK, MISO, MOSI, SS_PIN); // Initialize SPI with custom pins
  rfid.PCD_Reset(); // Reset the RFID reader
  rfid.PCD_Init(); 
  
  //For firebase
  WiFi.begin(WiFi_SSID, WiFi_PASSWORD);

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  fbdo.setBSSLBufferSize(1024, 1024);
  fbdo.setResponseSize(1024);
  Firebase.setDoubleDigits(5);
  config.timeout.serverResponse = 10000;

  // Man hinh chinh
  enterPassword();

}

void checkByKeypad(){
  char key = keypad.getKey();

  if(key){
    Serial.print("Key pressed: " + String(key));
    
    if(key == '*'){
      Serial.println("Clearing password input.");
      display.clearDisplay();
      enterPassword(); // Reset the password input
      midPassword = ""; // Reset the midPassword
      return;
    }

    // Change password
    if(key == '#'){
      return;
    }


    //nhan so
    midPassword += key;

    if(midPassword.length() == lengthpassword){
      Serial.println("Checking password: " + midPassword);
      handlePassword(); // kiem tra mat khau
      midPassword = ""; // reset mat khau
    } 
    else{
      display.print("*");
      display.display();
    }
  }
}

void checkByRFID(){

  unsigned long currentMillis = millis();
  if(currentMillis - lastRfidCheckTime < 200) { 
    return; 
  }

  lastRfidCheckTime = currentMillis;

  String midId ="";

  if(rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()){

    Serial.print("Card UID: ");

    for(byte i = 0; i < rfid.uid.size ; i++){
      if(rfid.uid.uidByte[i] < 0x10){
        midId += "0";
      }
      midId += String(rfid.uid.uidByte[i], HEX);
    }

    midId.toUpperCase(); // Convert to uppercase for consistency

    for(byte i = 0 ; i < 10 ;i++){
      if(midId == passId[i]){
        Serial.println("Success with RFID: " + midId);
        display.clearDisplay();
        display.setCursor(0,0);
        display.println("RFID Access Granted!");
        display.display();
        openDoor(); // Open the door
        return;
      }
    }

    Serial.println("Wrong RFID");
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Wrong RFID!");
    display.display();
    delay(500);
   
    enterPassword();

    rfid.PICC_HaltA(); // Halt the current card
    rfid.PCD_StopCrypto1(); // Stop encryption on the card
  }
  else{
    Serial.println("No new card present or read error.");
  }

  
}

void checkByInternet(){
  //Neu ket noi internet

  unsigned long currentMillis = millis();
  if(WiFi.status() == WL_CONNECTED && (currentMillis - lastInternetCheckTime >= 2000)) { // Check every 10 seconds
    lastInternetCheckTime = currentMillis;

    if(!isAuth){
      Serial.println("Not authenticated. Attempting to connect to Firebase...");
      checkFireBaseConnect();
    }
    else{
      boolean doorStatus = false;

      if(Firebase.RTDB.getBool(&fbdo , doorPathStatus.c_str(),&doorStatus)){
        if(doorStatus){
          openDoor(); // Open the door
        } else {
          Serial.println("Door is CLOSED");
        }
      }
      else{
        Serial.print("Error getting door status: ");
        Serial.println(fbdo.errorReason());
      }
    }
  }
  else{
    // Serial.println("WiFi disconnected. Reconnecting...");
    WiFi.begin(WiFi_SSID, WiFi_PASSWORD);
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  checkByKeypad();
  checkByRFID();
  checkByInternet();
  delay(100); // Add a small delay to avoid overwhelming the loop
}
