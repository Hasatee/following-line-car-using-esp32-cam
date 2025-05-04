#include <WiFi.h>
#include <WebServer.h>
#include "esp_camera.h"

// ==== CONFIG CAMERA ====
// ESP32-CAM AI Thinker Module
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ==== Motor Driver L298N Pin Setup ====
#define enA 4
#define in1 2
#define in2 14
#define in3 15
#define in4 13
#define enB 12

// ==== WiFi Credentials ====
const char* ssid = "Chi";
const char* password = "chi71124!";

WebServer server(80);

// ==== Motor Control ====
void motorStop() {
  analogWrite(enA, 0);
  analogWrite(enB, 0);
  digitalWrite(in1, LOW);
  digitalWrite(in2, LOW);
  digitalWrite(in3, LOW);
  digitalWrite(in4, LOW);
}

void motorForward() {
  analogWrite(enA, 90);
  analogWrite(enB, 90);
  digitalWrite(in1, HIGH);
  digitalWrite(in2, LOW);
  digitalWrite(in3, HIGH);
  digitalWrite(in4, LOW);
}

void motorBackward() {
  analogWrite(enA, 90);
  analogWrite(enB, 90);
  digitalWrite(in1, LOW);
  digitalWrite(in2, HIGH);
  digitalWrite(in3, LOW);
  digitalWrite(in4, HIGH);
}

void motorLeft() {
  analogWrite(enA, 85);
  analogWrite(enB, 90);
  digitalWrite(in1, LOW);
  digitalWrite(in2, HIGH);
  digitalWrite(in3, HIGH);
  digitalWrite(in4, LOW);
}

void motorRight() {
  analogWrite(enA, 90);
  analogWrite(enB, 85);
  digitalWrite(in1, HIGH);
  digitalWrite(in2, LOW);
  digitalWrite(in3, LOW);
  digitalWrite(in4, HIGH);
}

// ==== Camera Setup ====
void startCameraServer();

void detectLineAndMove(); // Function to detect line and control motors

// ==== Setup Function ====
void setup() {
  Serial.begin(115200);
  
  // Motor pins
  pinMode(enA, OUTPUT);
  pinMode(enB, OUTPUT);
  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);
  pinMode(in3, OUTPUT);
  pinMode(in4, OUTPUT);

  // Camera config
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // Init with high specs to capture better images
  if(psramFound()){
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QQVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println(WiFi.localIP());

  startCameraServer();

  // Routes for motor control
  server.on("/forward", HTTP_GET, [](){
    motorForward();
    server.send(200, "text/plain", "Moving Forward");
  });
  
  server.on("/backward", HTTP_GET, [](){
    motorBackward();
    server.send(200, "text/plain", "Moving Backward");
  });
  
  server.on("/left", HTTP_GET, [](){
    motorLeft();
    server.send(200, "text/plain", "Turning Left");
  });
  
  server.on("/right", HTTP_GET, [](){
    motorRight();
    server.send(200, "text/plain", "Turning Right");
  });

  server.on("/stop", HTTP_GET, [](){
    motorStop();
    server.send(200, "text/plain", "Stopped");
  });

  server.on("/", HTTP_GET, [](){
    server.send(200, "text/html", R"rawliteral(
      <html>
        <head>
          <title>ESP32-CAM Line Follower</title>
          <style>
            button {
              width: 100px;
              height: 50px;
              font-size: 18px;
              margin: 5px;
            }
          </style>
        </head>
        <body>
          <h1>ESP32 Line Follower Control</h1>
          <img src="http://)" + WiFi.localIP().toString() + R"rawliteral(/stream" width="320"><br><br>
          <button onclick="location.href='/forward'">Forward</button><br>
          <button onclick="location.href='/left'">Left</button>
          <button onclick="location.href='/stop'">Stop</button>
          <button onclick="location.href='/right'">Right</button><br>
          <button onclick="location.href='/backward'">Backward</button>
        </body>
      </html>
    )rawliteral");
  });

  server.begin();
}

// ==== Loop Function ====
void loop() {
  server.handleClient();
  startCameraServer();
  detectLineAndMove();
}

// ==== Camera Streaming ====
#include "esp32cam_stream.h" // Create your esp32cam_stream.h separately to manage streaming

// ==== Line Detection (Simple thresholding) ====
void detectLineAndMove() {
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  // Tạm thời bỏ qua header JPEG, tìm line đơn giản bằng phân tích brightness
  // Vì ảnh là JPEG -> muốn làm chuẩn hơn phải decode JPEG -> ESP32 không đủ RAM
  // => ta xử lý sơ bộ bằng cách lấy mẫu các byte raw trong vùng giữa

  int left = 0, center = 0, right = 0;
  int total = 0;

  // Chia ảnh làm 3 vùng: trái - giữa - phải (giả định mỗi vùng chiếm 1/3 độ dài ảnh)
  for (size_t i = fb->len / 2; i < fb->len / 2 + 150; i++) {
    uint8_t pixel = fb->buf[i];
    if (pixel < 30) { // Pixel tối (đen)
      if (i % fb->len < fb->len / 3) {
        left++;
      } else if (i % fb->len < (2 * fb->len) / 3) {
        center++;
      } else {
        right++;
      }
    }
    total++;
  }

  // Debug
  //Serial.printf("L:%d C:%d R:%d\n", left, center, right);

  // Quyết định hướng di chuyển
  if (center > left && center > right && center > 100) {
    motorForward();
  } else if (left > right && left > 80) {
    motorLeft();
  } else if (right > left && right > 80) {
    motorRight();
  } else {
    motorStop();
  }

  esp_camera_fb_return(fb);
}
