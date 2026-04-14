#include <WiFi.h>
#include <PubSubClient.h>

/* ================= WIFI + MQTT ================= */
const char* ssid = "Phan Quyen T3";
const char* password = "88888888";
const char* mqtt_server = "192.168.1.194";

WiFiClient espClient;
PubSubClient client(espClient);

/* ================= UART ================= */
HardwareSerial mySerial(2); // RX=16, TX=17

/* ================= COUNTERS ================= */
int ga_count = 0;
int gb_count = 0;
int ba_count = 0;
int bb_count = 0;

/* ================= CALLBACK MQTT ================= */
void callback(char* topic, byte* payload, unsigned int length) {
  String msg;

  for (int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  Serial.print("Nhan duoc MQTT: ");
  Serial.println(msg);

  /* Đếm nhãn */
  if (msg == "good_apple") {
    ga_count++;
  } 
  else if (msg == "good_banana") {
    gb_count++;
  } 
  else if (msg == "bad_apple") {
    ba_count++;
  } 
  else if (msg == "bad_banana") {
    bb_count++;
  }

  /* Khi không còn fruit → quyết định */
  else if (msg == "no_fruit") {

    Serial.println("=== KET QUA ===");
    Serial.print("GA: "); Serial.println(ga_count);
    Serial.print("GB: "); Serial.println(gb_count);
    Serial.print("BA: "); Serial.println(ba_count);
    Serial.print("BB: "); Serial.println(bb_count);

    /* Tìm max */
    int max_count = ga_count;
    String label = "GA";

    if (gb_count >= max_count) {
      max_count = gb_count;
      label = "GB";
    }
    if (ba_count >= max_count) {
      max_count = ba_count;
      label = "BA";
    }
    if (bb_count >= max_count) {
      max_count = bb_count;
      label = "BB";
    }

    /* Gửi UART theo yêu cầu */
    if (label == "GA") {
      Serial.println("Gui 'A' (good_apple)");
      mySerial.write('A');
    } 
    else if (label == "GB") {
      Serial.println("Gui 'B' (good_banana)");
      mySerial.write('B');
    } 
    else {
      Serial.println("Khong gui (bad fruit)");
    }

    /* 🔥 Reset counter */
    ga_count = 0;
    gb_count = 0;
    ba_count = 0;
    bb_count = 0;

    Serial.println("Reset counter\n");
  }
}

/* ================= RECONNECT MQTT ================= */
void reconnect() {
  while (!client.connected()) {
    Serial.print("Dang ket noi MQTT...");
    
    if (client.connect("ESP32Client")) {
      Serial.println("OK");
      client.subscribe("esp32/control");
    } else {
      Serial.print("Loi, rc=");
      Serial.println(client.state());
      delay(2000);
    }
  }
}

/* ================= SETUP ================= */
void setup() {
  Serial.begin(115200);

  mySerial.begin(9600, SERIAL_8N1, 16, 17);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi OK");

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

/* ================= LOOP ================= */
void loop() {
  if (!client.connected()) {
    reconnect();
  }

  client.loop();
}
