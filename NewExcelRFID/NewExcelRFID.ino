#include <ESP8266WiFi.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// =====================================================
// WIFI & LOCAL SERVER CONFIGURATION
// =====================================================
const char* ssid     = "JEET";        // Wi-Fi / Hotspot Name
const char* password = "12345678";    // Wi-Fi / Hotspot Password

const char* serverIP = "192.168.43.142";
const int   serverPort = 5000;            // Python Flask Port

// Set your Admin Card UID here (Upper Case)
const String ADMIN_CARD_UID = "D6 01 4F 06"; 

// Dynamic Student Memory (Supports up to 50 students)
struct Student {
  String uid;
  String name;
};
Student students[50];
int studentCount = 0;

// =====================================================
// HARDWARE PINS
// =====================================================
#define SS_PIN    4   // D2 (GPIO4)
#define RST_PIN   16  // D0 (GPIO16)
#define BUZZER_PIN 0  // D3 (GPIO0)

MFRC522 rfid(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2); 

// Function Prototypes
String getStudentName(String uid);
int markAttendance(String uid, String name);
bool registerNewStudentOnServer(String uid, String name);
void fetchStudentsFromServer();
void handleAdminRegistration();
String readUID();
String urlEncode(String text);
void beepSuccess();
void beepError();
void showReady();

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n================================");
  Serial.println("RFID LOCAL EXCEL ATTENDANCE");
  Serial.println("================================");

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Wire.begin(2, 5);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("Starting...");

  SPI.begin();
  rfid.PCD_Init();

  lcd.clear();
  lcd.print("Connecting WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
  Serial.print("ESP8266 IP: ");
  Serial.println(WiFi.localIP());

  // Fetch all saved students from Students.xlsx on server startup
  lcd.clear();
  lcd.print("Loading Data...");
  fetchStudentsFromServer();

  showReady();
}

void loop() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }

  String uid = readUID();

  Serial.print("\nCard Scanned: ");
  Serial.println(uid);

  // Check if Admin Card was scanned
  if (uid == ADMIN_CARD_UID) {
    handleAdminRegistration();
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    return;
  }

  // Normal Student Attendance Mode
  String studentName = getStudentName(uid);

  if (studentName == "") {
    lcd.clear();
    lcd.print("Unknown Card");
    lcd.setCursor(0, 1);
    lcd.print("Access Denied");
    beepError();
    delay(2000);
    showReady();
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    return;
  }

  lcd.clear();
  lcd.setCursor(0, 0); 
  lcd.print("Hey " + studentName + "!");
  delay(1000);

  lcd.clear();
  lcd.print("Updating Data...");

  int status = markAttendance(uid, studentName);

  if (status == 1) { 
    lcd.clear();
    lcd.print("Attendance");
    lcd.setCursor(0, 1);
    lcd.print("Marked!");
    beepSuccess();
  } else if (status == 2) { 
    lcd.clear();
    lcd.print("Already Marked!");
    lcd.setCursor(0, 1);
    lcd.print("Try After 5 Mins");
    beepError();
  } else { 
    lcd.clear();
    lcd.print("Server Error!");
    beepError();
  }

  delay(2500);
  showReady();

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  delay(1000);
}

// Fetch all registered students from Python Flask
void fetchStudentsFromServer() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClient client;
  client.setTimeout(5000);

  if (!client.connect(serverIP, serverPort)) {
    Serial.println("[ERROR] Failed to fetch students on boot!");
    return;
  }

  client.print(String("GET /get_students HTTP/1.1\r\n") +
               "Host: " + String(serverIP) + "\r\n" +
               "User-Agent: ESP8266\r\n" +
               "Connection: close\r\n\r\n");

  delay(200);

  // Skip HTTP headers
  while (client.connected() || client.available()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break; // Headers end with an empty line
  }

  studentCount = 0;
  
  // Read student records
  while (client.available()) {
    String line = client.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    int colonIndex = line.indexOf(':');
    if (colonIndex > 0 && studentCount < 50) {
      students[studentCount].uid = line.substring(0, colonIndex);
      students[studentCount].name = line.substring(colonIndex + 1);
      Serial.println("Loaded: " + students[studentCount].name + " (" + students[studentCount].uid + ")");
      studentCount++;
    }
  }

  client.stop();
  Serial.print("Total Students Loaded into RAM: ");
  Serial.println(studentCount);
}

// Admin Mode Function
void handleAdminRegistration() {
  lcd.clear();
  lcd.print("ADMIN MODE");
  lcd.setCursor(0, 1);
  lcd.print("Tap New Card...");
  beepSuccess();
  
  Serial.println("\n--- ADMIN REGISTRATION MODE ACTIVE ---");
  Serial.println("Please tap the new card/tag on the RFID reader...");

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  String newUID = "";
  unsigned long startTime = millis();
  
  while (millis() - startTime < 30000) {
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      newUID = readUID();
      
      if (newUID == ADMIN_CARD_UID) {
        lcd.clear();
        lcd.print("Admin Card!");
        lcd.setCursor(0, 1);
        lcd.print("Tap New Student");
        beepError();
        Serial.println("[WARNING] Admin card tapped! Please tap a new student card.");
        
        rfid.PICC_HaltA();
        rfid.PCD_StopCrypto1();
        delay(2000);
        
        lcd.clear();
        lcd.print("ADMIN MODE");
        lcd.setCursor(0, 1);
        lcd.print("Tap New Card...");
        
        newUID = "";
        continue;
      }

      String existingName = getStudentName(newUID);
      if (existingName != "") {
        lcd.clear();
        lcd.print("Already Exists!");
        lcd.setCursor(0, 1);
        lcd.print("Name: " + existingName);
        beepError();
        Serial.println("[ERROR] Card already assigned to: " + existingName);
        delay(3000);
        showReady();
        return;
      }

      break;
    }
    delay(100);
  }

  if (newUID == "") {
    lcd.clear();
    lcd.print("Admin Timeout!");
    beepError();
    delay(2000);
    showReady();
    return;
  }

  Serial.print("New Card Detected UID: ");
  Serial.println(newUID);

  lcd.clear();
  lcd.print("Card Scanned!");
  lcd.setCursor(0, 1);
  lcd.print("Type Name on PC");
  beepSuccess();

  Serial.println(">>> TYPE STUDENT NAME IN SERIAL MONITOR AND PRESS ENTER <<<");
  
  while (Serial.available()) { Serial.read(); }

  String newName = "";
  startTime = millis();

  while (millis() - startTime < 30000) {
    if (Serial.available() > 0) {
      newName = Serial.readStringUntil('\n');
      newName.trim();
      if (newName.length() > 0) break;
    }
    delay(100);
  }

  if (newName == "") {
    lcd.clear();
    lcd.print("Name Timeout!");
    beepError();
    delay(2000);
    showReady();
    return;
  }

  Serial.println("Registering: " + newName + " (" + newUID + ")");

  if (registerNewStudentOnServer(newUID, newName)) {
    if (studentCount < 50) {
      students[studentCount].uid = newUID;
      students[studentCount].name = newName;
      studentCount++;
    }
    lcd.clear();
    lcd.print("Saved Student:");
    lcd.setCursor(0, 1);
    lcd.print(newName);
    beepSuccess();
  } else {
    lcd.clear();
    lcd.print("Failed To Save!");
    beepError();
  }

  delay(3000);
  showReady();
}

String readUID() {
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
    if (i < rfid.uid.size - 1) uid += " ";
  }
  uid.toUpperCase();
  return uid;
}

String getStudentName(String uid) {
  for (int i = 0; i < studentCount; i++) {
    if (students[i].uid == uid) {
      return students[i].name;
    }
  }
  return "";
}

int markAttendance(String uid, String name) {
  if (WiFi.status() != WL_CONNECTED) return 0;

  WiFiClient client;
  client.setTimeout(5000);

  if (!client.connect(serverIP, serverPort)) return 0;

  String path = "/log?uid=" + urlEncode(uid) + "&name=" + urlEncode(name);

  client.print(String("GET ") + path + " HTTP/1.1\r\n" +
               "Host: " + String(serverIP) + "\r\n" +
               "User-Agent: ESP8266\r\n" +
               "Connection: close\r\n\r\n");

  delay(100);

  int result = 0;
  unsigned long timeout = millis();

  while (client.connected() || client.available()) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      if (line.indexOf("ATTENDANCE_SUCCESS") >= 0) result = 1;
      else if (line.indexOf("ATTENDANCE_DUPLICATE") >= 0) result = 2;
    }
    if (millis() - timeout > 3000) break;
    delay(10);
  }

  client.stop();
  return result;
}

bool registerNewStudentOnServer(String uid, String name) {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClient client;
  client.setTimeout(5000);

  if (!client.connect(serverIP, serverPort)) return false;

  String path = "/add_student?uid=" + urlEncode(uid) + "&name=" + urlEncode(name);

  client.print(String("GET ") + path + " HTTP/1.1\r\n" +
               "Host: " + String(serverIP) + "\r\n" +
               "User-Agent: ESP8266\r\n" +
               "Connection: close\r\n\r\n");

  delay(100);

  bool success = false;
  unsigned long timeout = millis();

  while (client.connected() || client.available()) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      if (line.indexOf("STUDENT_ADDED") >= 0) {
        success = true;
      }
    }
    if (millis() - timeout > 3000) break;
    delay(10);
  }

  client.stop();
  return success;
}

String urlEncode(String text) {
  String encoded = "";
  const char* hex = "0123456789ABCDEF";
  for (unsigned int i = 0; i < text.length(); i++) {
    char c = text[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else {
      encoded += '%';
      encoded += hex[(c >> 4) & 0x0F];
      encoded += hex[c & 0x0F];
    }
  }
  return encoded;
}

void beepSuccess() {
  digitalWrite(BUZZER_PIN, HIGH); delay(150);
  digitalWrite(BUZZER_PIN, LOW);  delay(100);
  digitalWrite(BUZZER_PIN, HIGH); delay(150);
  digitalWrite(BUZZER_PIN, LOW);
}

void beepError() {
  digitalWrite(BUZZER_PIN, HIGH); delay(500);
  digitalWrite(BUZZER_PIN, LOW);
}

void showReady() {
  lcd.clear();
  lcd.print("RFID ATTENDANCE");
  lcd.setCursor(0, 1);
  lcd.print("Scan Card...");
}