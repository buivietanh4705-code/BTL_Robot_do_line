#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// --- CẤU HÌNH MẠNG ---
const char* ssid_AP = "";     
const char* password_AP = "";  

// Điền WiFi bạn đang dùng (Điện thoại phát hoặc WiFi trường)
const char* ssid_STA = "";
const char* password_STA = "";     

// Điền địa chỉ IP của Laptop (Vừa lấy từ lệnh ipconfig)
const char* mqtt_server = ""; 

WiFiClient espClient;
PubSubClient client(espClient);

void reconnect() {
  while (!client.connected()) {
    String clientId = "Robot-Wagur-ESP-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      client.subscribe("uet/robot/control"); 
    } else {
      delay(5000); // Đợi 5 giây rồi thử lại nếu rớt mạng
    }
  }
}


void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  if (String(topic) == "uet/robot/control") {
    // ĐÂY LÀ DÒNG PRINT DUY NHẤT - GỬI THẲNG JSON XUỐNG STM32
    Serial.println(message); 
  }
}

void setup() {
  Serial.begin(115200); 
  Serial.setTimeout(50); 
  delay(100); // Đợi serial ổn định
  
  // Khởi động WiFi Dual Mode
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ssid_AP, password_AP);
  WiFi.begin(ssid_STA, password_STA);

  // Đợi kết nối WiFi (Tăng thời gian lên 40 vòng lặp ~ 20 giây)
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 40) {
    delay(500);
    retries++;
  }

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); 

  // Đọc dữ liệu từ STM32 gửi lên và bắn lên Node-RED
  if (Serial.available() > 0) {
    String payload = Serial.readStringUntil('\n'); 
    client.publish("uet/robot/telemetry", payload.c_str());
  }
}