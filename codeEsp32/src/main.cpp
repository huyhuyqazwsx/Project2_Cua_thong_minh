#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <Keypad.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Firebase_ESP_Client.h>
#include <TimeLib.h>
#include <NTPClient.h>

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

String doorPath = "doors/" + String(DEVICE_ID);
bool isAuth = false;
unsigned long lastInternetCheckTime = 0; 

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7 * 3600, 60000);
bool ntpInitialized = false;

//for password
String truePassword = "121233";
byte lengthpassword = truePassword.length();
String midPassword = "";
byte countReset = 0;
unsigned long lastResetTime = 0;

//Led for door
#define LED_PIN 2 // Pin for isLock
#define BUTTON_LOCK 19

bool isOpen = false;
bool isLock = false;
bool isUpdate = false;
volatile bool buttonPressed = false;
unsigned long buttonPressTime = 0;

//count time
unsigned long startTime = 0; // will store last time LED was updated


void enterPassword(){
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Enter password:");
  display.display();
}

void createNewPassword(){
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Create new password:");
  display.display();

  String newPassword = "";
  while(newPassword.length() < lengthpassword){
    char key = keypad.getKey();
    
    if(key){
      Serial.print("Key pressed: " + String(key));
      
      if(key == '*'){
        newPassword = ""; // Clear the input
        Serial.println("Clearing password input.");
        display.clearDisplay();
        display.setCursor(0,0);
        display.println("Create new password:");
        display.display();
        continue;
      }
      
      if(key == '#'){
        return;
      }

      newPassword += key;
      if(newPassword.length() == lengthpassword){
        newPassword.toUpperCase();
        truePassword = newPassword; // Update the true password
        Serial.println("New password set: " + truePassword);
        
        display.clearDisplay();
        display.setCursor(0,0);
        display.println("New password set!");
        display.display();
        
        delay(1000); // Show the message for 2 seconds
        midPassword = ""; // Reset the midPassword
        enterPassword();
        break;
      }
      else{
        display.print("*");
        display.display();
      }
      
    }
  }
}

void resetPassword(){
  String newPassword = "";
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Nhap mat khau hien tai:");
  display.display();  

  while(1){
    char key = keypad.getKey();
    if(key){
      Serial.print("Key pressed: " + String(key));
      
      if(key == '*'){
        break;
      }
      
      if(key == '#'){
        continue;
      }

      newPassword += key;
      if(newPassword.length() == lengthpassword){
        newPassword.toUpperCase();
        if(newPassword == truePassword){
          createNewPassword();
          break;
        }
        else {
          Serial.println("Mat khau hien tai khong dung!");
          display.clearDisplay();
          display.setCursor(0,0);
          display.println("Mat khau hien tai");
          display.println("khong dung!");
          newPassword = "";
          delay(200);
          Serial.println("Clearing password input.");
          display.clearDisplay();
          display.setCursor(0,0);
          display.println("Nhap mat khau hien tai:");
          display.display();
        }
      }
      else{
        display.print("*");
        display.display();
      }
    }
  }
}

void updateTimeFirebase(){
  unsigned long now = timeClient.getEpochTime();
  Firebase.RTDB.setInt(&fbdo, doorPath + "/lastTime" , now);
  Serial.println("Firebase connected and time updated.");
  Serial.println("Current time: " + timeClient.getFormattedTime());
}

void updateDoorFirebase(){
  Firebase.RTDB.setBool(&fbdo, doorPath + "/isOpen" , isOpen);
  Firebase.RTDB.setBool(&fbdo, doorPath + "/isLock" , isLock);
  Serial.println("Door status updated in Firebase.");
}

void getDoorFirebase(){
  if (Firebase.RTDB.getBool(&fbdo, doorPath + "/isOpen")) {
    isOpen = fbdo.boolData();
  }
  
  if (Firebase.RTDB.getBool(&fbdo, doorPath + "/isLock")) {
    isLock = fbdo.boolData(); 
  } 

  if(isLock){
    digitalWrite(LED_PIN, HIGH); 
  } else {
    digitalWrite(LED_PIN, LOW);
  }

  Serial.println("Door status retrieved from Firebase.");
  Serial.println("isOpen: " + String(isOpen));
  Serial.println("isLock: " + String(isLock));
}

void openDoor(){
  Serial.println("Opening door...");

  digitalWrite(LED_PIN, LOW);
  isLock = false; 
  if(WiFi.status() == WL_CONNECTED) updateDoorFirebase();

  Serial.println("Welcome!");
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Welcome!");
  display.display();
  delay(1000);

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
    display.display();

    delay(200); // Show the message for 2 seconds

    enterPassword(); // Reset the password input
    midPassword = "";
  }
}

void checkFireBaseConnect(){

  //Dang nhap voi firebase

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

void buttonISR() {
  buttonPressed = true;
  buttonPressTime = millis();
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  //Led
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_LOCK, INPUT_PULLUP);
  digitalWrite(LED_PIN, LOW);
  attachInterrupt(digitalPinToInterrupt(BUTTON_LOCK), buttonISR, FALLING);

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
  
  // NTP Client
  timeClient.begin();

  // Man hinh chinh
  enterPassword();

}

void checkByKeypad(){
  char key = keypad.getKey();

  if(key){
    Serial.print("Key pressed: " + String(key));
    
    if(key == '*'){
      countReset = 0;
      Serial.println("Clearing password input.");
      display.clearDisplay();
      enterPassword(); // Reset the password input
      midPassword = ""; // Reset the midPassword
      return;
    }

    // Change password
    if(key == '#'){
      Serial.println("Changing password...");
      if(millis() - lastResetTime < 3000){
        countReset++;
      }
      else{
        countReset = 1;
      }
      Serial.println("Count reset attempts: " + String(countReset));
      lastResetTime = millis();
      
      if(countReset >= 3){
        resetPassword();
        countReset = 0; // Reset the count after successful reset
        return;
      }
      return;
    }

    // Reset password
    countReset = 0;

    //nhan so
    midPassword += key;

    if(midPassword.length() == lengthpassword){
      Serial.println("Checking password: " + midPassword);
      handlePassword();
      midPassword = "";
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

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }
  else{
    // Serial.println("No new card present or read error.");
  }

  
}

void checkByInternet(){

  unsigned long currentMillis = millis();
  if(currentMillis - lastInternetCheckTime < 2000){
    return;
  }
  lastInternetCheckTime = currentMillis;

  //Kiem tra ket noi wifi
  if(WiFi.status() == WL_CONNECTED ) {
    //Kiem tra thoi gian ket noi internet
    if(!isUpdate){
      updateDoorFirebase;
      isUpdate = true;
    }

    //Kiem tra ntp
    if(!ntpInitialized) {
      Serial.println("Initializing NTP client...");
      timeClient.begin();
      
      for(int i = 1; i <= 3 ; i++){
        if(timeClient.update()) {
          setTime(timeClient.getEpochTime());
          Serial.println("NTP time updated: " + timeClient.getFormattedTime());
          ntpInitialized = true;
          break;
        } else {
          Serial.println("Failed to update NTP time.");
        }
      }
      
    }
    // Serial.println("NTP time updated: " + timeClient.getFormattedTime());
    

    //Kiem tra dang nhap Firebase
    if(!isAuth){
      Serial.println("Not authenticated. Attempting to connect to Firebase...");
      checkFireBaseConnect();
    }
    else{
      // Gui thoi gian len Firebase
      updateTimeFirebase();
      getDoorFirebase();
    }
  }
  else{
    WiFi.begin(WiFi_SSID, WiFi_PASSWORD);
    ntpInitialized = false;
    isUpdate = false;
  }
}

void readDoor(){
  if (buttonPressed && (millis() - buttonPressTime) > 50) {
    buttonPressed = false;
    
    isLock = !isLock;
    digitalWrite(LED_PIN, isLock ? HIGH : LOW);
    if(WiFi.status() == WL_CONNECTED) updateDoorFirebase();
    
    Serial.println("Button interrupt triggered! Door is now " + String(isLock ? "LOCKED" : "UNLOCKED"));
    
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  readDoor();

  checkByKeypad();
  checkByRFID();
  checkByInternet();
  delay(100);
}
