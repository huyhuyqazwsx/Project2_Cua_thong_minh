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
#include <vector>
#include <BLEServer.h>
#include <BLEDevice.h>
#include <BLEUtils.h>

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

std::vector<String> passId = {"B1161505"};
short maxPassRfid = passId.size();

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

//For resetWifi
#define SERVICE_UUID "5ced6bfc-5390-495f-b0f6-8dddc487c58a"
#define SSID_UUID "cb9d145e-0837-4209-8fd9-d9e5eee0a323"
#define PASSWORD_UUID "b8c0f1d2-3e4f-5a6b-7c8d-9e0f1a2b3c4d"

String WiFi_SSID ="Bắt Con Sóc";
String WiFi_PASSWORD ="12121212";
bool gotSSID = false;
bool gotPassword = false;
bool readyToConnect = false;

BLEServer *pServer;
BLEService *pService;
BLECharacteristic *pSSID;

BLECharacteristic *pPass;
BLEAdvertising *pAdvertising;

//For firebase

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
bool isLock = true;
bool isUpdate = false;
volatile bool buttonPressed = false;
unsigned long buttonPressTime = 0;

//count time
unsigned long startTime = 0; // will store last time LED was updated

//callback su dung cho character
class myCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    std::string uuid = pCharacteristic->getUUID().toString();

    if (uuid == SSID_UUID) {
      WiFi_SSID = value.c_str();
      gotSSID = true;
      Serial.println("Received SSID: " + WiFi_SSID);

    } else if (uuid == PASSWORD_UUID) {
      WiFi_PASSWORD = value.c_str();
      gotPassword = true;
      Serial.println("Received Password: " + WiFi_PASSWORD);
    }

    if (gotSSID && gotPassword) {
      gotSSID = false;
      gotPassword = false;
      readyToConnect = true;
    }
  }
};

void initBLE(){
  BLEDevice::init("MyESP32");

  pServer = BLEDevice::createServer();
  pService = pServer->createService(SERVICE_UUID);

  pSSID = pService->createCharacteristic(
    SSID_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );

  pPass = pService->createCharacteristic(
    PASSWORD_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );

  pSSID->setCallbacks(new myCallbacks());
  pPass->setCallbacks(new myCallbacks());

  pService->start();

  pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);

  // Serial.println("Doi ket noi ble");
}

void enterPassword(){
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Enter password:");
  display.display();
}

void changeLock(){
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Thay doi khoa");
  display.println("1. Xoa toan bo khoa");
  display.println("2. Them khoa");
  display.println("An * de thoat");
  display.display();

  while(1){
    char key4 = keypad.getKey();
    if(key4 == '1'){
      //Xoa toan bo
      passId.clear();
      maxPassRfid = passId.size();

      display.clearDisplay();
      display.setCursor(0,0);
      display.println("Da xoa thanh cong");
      display.display();
      delay(200);
      return;
    }
    else if(key4 ==  '2'){
      display.clearDisplay();
      display.setCursor(0,0);
      display.println("Vui long them khoa");
      display.display();
      String midId = "";

      while(1){
        if(rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()){
          Serial.print("Card UID: ");

          for(byte i = 0; i < rfid.uid.size ; i++){
            if(rfid.uid.uidByte[i] < 0x10){
              midId += "0";
            }
            midId += String(rfid.uid.uidByte[i], HEX);
          }

          midId.toUpperCase();

          for(short i = 0; i< maxPassRfid ;  i++){
            if(midId == passId[i]){
              display.clearDisplay();
              display.setCursor(0,0);
              display.println("Khoa da ton tai");
              display.display();

              Serial.println("Khoa da ton tai");
              delay(500);
              return;
            }
          }

          passId.push_back(midId);
          maxPassRfid++;
          display.clearDisplay();
          display.setCursor(0,0);
          display.println("Da them the thanh cong");
          display.display();

          Serial.println("Da them the thanh cong");
          delay(500);          
          return;
        }
      }

    }
    else if(key4 == '*'){
      return;
    }
  }
}

void changeWifi(){
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Su dung app de cau hinh");
  display.println("An * de thoat");
  display.display();

  WiFi.disconnect();

  //Quang ba
  pAdvertising->addServiceUUID(SERVICE_UUID);
  BLEDevice::startAdvertising();

  while(1){
    char key = keypad.getKey();
    if(key == '*'){

      if (pServer->getConnectedCount() > 0) {
        pServer->disconnect(0);
      }

      BLEDevice::getAdvertising()->stop();
      delay(300);
      return;
    }

    // Serial.println("test ble");
    if(readyToConnect){
      gotSSID = false;
      gotPassword = false;
      readyToConnect = false;

      unsigned long startTime = millis();

      display.clearDisplay();
      display.setCursor(0,0);
      display.println("Ket noi wifi...");
      display.display();

      WiFi.begin(WiFi_SSID.c_str(), WiFi_PASSWORD.c_str());

      while (WiFi.status() != WL_CONNECTED && (millis() - startTime < 5000)) {
        delay(1000);
        Serial.println(".");
      }

      if(WiFi.status() == WL_CONNECTED){
        
        display.clearDisplay();
        display.setCursor(0,0);
        display.println("Ket noi thanh cong");
        display.println(WiFi_SSID);
        display.display();

        Serial.println("Connected to WiFi!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        
        delay(300);
        //Ngat ket noi voi client
        if (pServer->getConnectedCount() > 0) {
          pServer->disconnect(0);
        }

        BLEDevice::getAdvertising()->stop();
        delay(700);
        return;

      } else{
        Serial.println("Failed to connect to WiFi.");
        
        display.clearDisplay();
        display.setCursor(0,0);
        display.println("Ket noi that bai");
        display.display();
        delay(100);

        display.clearDisplay();
        display.setCursor(0,0);
        display.println("Su dung app de cau hinh");
        display.println("An * de thoat");
        display.display();

        WiFi.disconnect();
      }
    }
    delay(100);
  }
}

void createNewPassword(){
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Create new password:");
  display.display();

  String newPassword = "";
  while(newPassword.length() < lengthpassword){
    char key3 = keypad.getKey();
    
    if(key3){
      Serial.print("Key pressed: " + String(key3));
      
      if(key3 == '*'){
        newPassword = ""; // Clear the input
        Serial.println("Clearing password input.");
        display.clearDisplay();
        display.setCursor(0,0);
        display.println("Create new password:");
        display.display();
        continue;
      }
      
      if(key3 == '#'){
        return;
      }

      newPassword += key3;
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
        return;
      }
      else{
        display.print("*");
        display.display();
      }
    }
  }
}

void selectMode(){
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Chon che do");
  display.println("1. Thay doi mat khau");
  display.println("2. Thay doi khoa tu");
  display.println("3. Thay doi wifi");
  display.println("An # de thoat");
  display.display();
  delay(100);

  while(1){
    char key1 = keypad.getKey();
    if(key1){
      Serial.print("Key pressed: " + String(key1));
      
      if(key1 == '1'){
        createNewPassword();
        return;
      }
      else if(key1 == '2'){
        changeLock();
        return;
      }
      else if(key1 == '3'){
        changeWifi();
        return;
      }
      else if(key1 == '#'){
        return;
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
    char key2 = keypad.getKey();
    if(key2){
      Serial.print("Key pressed: " + String(key2));
      
      if(key2 == '*'){
        return;
      }
      
      if(key2 == '#'){
        continue;
      }

      newPassword += key2;
      if(newPassword.length() == lengthpassword){
        newPassword.toUpperCase();
        if(newPassword == truePassword){
          selectMode();
          return;
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
  if(Firebase.RTDB.setInt(&fbdo, doorPath + "/lastTime" , now)){
    Serial.println("Cap nhat lastTime thanh cong");
    Serial.println("Thoi gian: " + timeClient.getFormattedTime());
  }
  else{
    Serial.println("Cap nhat lastTime that bai");
  }
  
}

void updateDoorFirebase(){
  Firebase.RTDB.setBool(&fbdo, doorPath + "/isOpen" , isOpen);
  Firebase.RTDB.setBool(&fbdo, doorPath + "/isLock" , isLock);
  Serial.println("Door status updated in Firebase.");
}

void getDoorFirebase(){
  if (Firebase.RTDB.getBool(&fbdo, doorPath + "/isOpen")) {
    isOpen = fbdo.boolData();

    //Kiem tra isLock
    if(Firebase.RTDB.getBool(&fbdo, doorPath + "/isLock")){
      isLock = fbdo.boolData();

      if(isLock){
        digitalWrite(LED_PIN, HIGH); 
      } else {
        digitalWrite(LED_PIN, LOW);
      }

      Serial.println("Thong tin tu firebase");
      Serial.println("isOpen: " + String(isOpen));
      Serial.println("isLock: " + String(isLock));
    }
    else{
      Serial.println("Loi lay thong tin isLock");
    }
    
  }
  else{
    Serial.println("Loi lay thong tin isOpen");
  }
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
    fbdo.setBSSLBufferSize(1024, 1024);
    fbdo.setResponseSize(1024);
    Firebase.setDoubleDigits(5);
    config.timeout.serverResponse = 10000;


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
  WiFi.begin(WiFi_SSID.c_str(), WiFi_PASSWORD.c_str());

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  fbdo.setBSSLBufferSize(1024, 1024);
  fbdo.setResponseSize(1024);
  Firebase.setDoubleDigits(5);
  config.timeout.serverResponse = 10000;
  
  // NTP Client
  timeClient.begin();

  //ble
  initBLE();

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
      Serial.println("Chuc nang");
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
        countReset = 0; 
        enterPassword();
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
      enterPassword();
      return;
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

    for(byte i = 0 ; i < maxPassRfid ;i++){
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
    WiFi.begin(WiFi_SSID.c_str(), WiFi_PASSWORD.c_str());
    // ntpInitialized = false;
    // isUpdate = false;
    // isAuth = false;
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
}
