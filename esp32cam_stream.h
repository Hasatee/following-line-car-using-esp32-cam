#ifndef _ESP32CAM_STREAM_H_
#define _ESP32CAM_STREAM_H_

#include "esp_camera.h"
#include <WebServer.h>

extern WebServer server; // Khai báo server từ sketch chính

void startCameraServer() {
  server.on("/stream", HTTP_GET, []() {
    WiFiClient client = server.client();

    String response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
    server.sendContent(response);

    while (1) {
      camera_fb_t * fb = esp_camera_fb_get();
      if (!fb) {
        Serial.println("Camera capture failed");
        return;
      }

      response = "--frame\r\n";
      response += "Content-Type: image/jpeg\r\n\r\n";
      server.sendContent(response);

      client.write(fb->buf, fb->len);
      server.sendContent("\r\n");

      esp_camera_fb_return(fb);

      if (!client.connected()) {
        break;
      }

      delay(50); // Khoảng thời gian giữa các frame
    }
  });
}

#endif