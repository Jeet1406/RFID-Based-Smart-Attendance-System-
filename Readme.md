# Smart IoT RFID Attendance System with Automated Student Enrollment

An automated, real-time attendance management system built using an **ESP8266 (NodeMCU)** microcontroller, **MFRC522 RFID Reader**, and a **Python Flask** backend. Attendance records and student registration data are automatically logged into dynamic Excel sheets (`Attendance.xlsx` and `Students.xlsx`).

---

## 🛠️ System Architecture & Workflow

```text
┌─────────────────┐       Wi-Fi (HTTP GET)       ┌───────────────────────┐
│                 │ ───────────────────────────> │  Python Flask Server  │
│ ESP8266 NodeMCU │                              │      (Port 5000)      │
│                 │ <─────────────────────────── │                       │
└────────┬────────┘      Response & Data         └───────────┬───────────┘
         │                                                   │
  ┌──────┴──────┐                                     ┌──────┴──────┐
  │ Peripherals │                                     │ Excel Files │
  ├─────────────┤                                     ├─────────────┤
  │ MFRC522 RFID│                                     │ Attendance  │
  │ 16x2 LCD    │                                     │ Students    │
  │ Buzzer      │                                     └─────────────┘
  └─────────────┘




  [ Power On / Boot ]
                          │
            [ Connect to Local Wi-Fi ]
                          │
          [ GET /get_students from Flask ]
                          │
            [ Load Student Data into RAM ]
                          │
               [ Ready: Wait for Card ]
                          │
                    ┌─────┴─────┐
                    │ Card Scan │
                    └─────┬─────┘
                          │
          ┌───────────────┴───────────────┐
          │                               │
  [ Admin Card ]                   [ Student Card ]
          │                               │
  [ Enter Admin Mode ]            [ Check Student RAM ]
          │                               │
  [ Scan New Card ]           ┌───────────┴───────────┐
          │                   │                       │
  [ Type Name in Serial ] [ Unknown ]             [ Recognized ]
          │                   │                       │
  [ GET /add_student ]   [ Access Denied ]     [ GET /log ]
          │                                           │
  [ Saved to Excel ]                     ┌────────────┴────────────┐
                                         │                         │
                                  [ Success ]               [ Duplicate (< 5m) ]
                                         │                         │
                                [ Logged to Excel ]       [ Try After 5 Mins ]