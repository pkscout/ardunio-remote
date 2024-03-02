/*********************************************************************
 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 Copyright (c) 2019 Ha Thach for Adafruit Industries
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/

// The USB bits of this are all from the hid_device_report example
// Adafruit provided (see notice above)
// the rest is custom development also licensed under the MIT license

// arduino_secrets.h needs to contain #define statements for
// SECRET_SSID - the SSID of the wifi network
// SECRET_PASS - the password of the wifi network
// BROKER_ADDR - the IP address of the broker in format IPAddress(127,0,0,1)
// DEVICE_NAME - the name of the remote
// LONG_PRESS_THRESHOLD - an integer of the milliseconds a key must be held to be considered a long press

#define SOFTWARE_VERSION "1.0.0"
#define MANUFACTURER "pkscout"
#define MODEL "Adafruit RP2040 USB Host with AirLift FeatherWing"
#define CONFIGURL "https://github.com/pkscout/hid_remote_rp2040"

#define SPIWIFI       SPI  // The SPI port
#define SPIWIFI_SS    13   // Chip select pin
#define ESP32_RESETN  12   // Reset pin
#define SPIWIFI_ACK   11   // a.k.a BUSY or READY pin
#define ESP32_GPIO0   -1

#include <SPI.h>
#include <WiFiNINA.h>
#include <ArduinoHA.h>
#include "usbh_helper.h"
#include "arduino_secrets.h"

// intialize Wifi, Web loggins and HA connection
WiFiClient CLIENT;
HADevice DEVICE;
HAMqtt MQTT(CLIENT, DEVICE);

// global variables
uint8_t KEY_DOWN = 0;
unsigned long DOWN_START = 0;
unsigned long LAST_UPDATE_AT = 0;
char UPTIME_CHAR[40];
char MAC_CHAR[18];
HASensor KEY_PRESS("key_press");
HASensor UPTIME("uptime");
HASensor MAC_ADDRESS("mac_address");
const int QUEUE_LENGTH = 128;

queue_t KEY_QUEUE;
typedef struct keyQueueItem {
  char value[10];
  bool available;
} item;

void setup() {
  // make WiFi connection
  Serial.print("Waiting for Wifi");
  WiFi.setPins(SPIWIFI_SS, SPIWIFI_ACK, ESP32_RESETN, ESP32_GPIO0, &SPIWIFI);
  WiFi.begin(SECRET_SSID, SECRET_PASS);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // setup HA device
  byte mac[6];
  WiFi.macAddress(mac);
  sprintf(MAC_CHAR, "%2X:%2X:%2X:%2X:%2X:%2X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.printf("Mac address: ");
  Serial.println(MAC_CHAR);
  DEVICE.setUniqueId(mac, sizeof(mac));
  DEVICE.setName(DEVICE_NAME);
  DEVICE.setSoftwareVersion(SOFTWARE_VERSION);
  DEVICE.setManufacturer(MANUFACTURER);
  DEVICE.setModel(MODEL);
  DEVICE.setConfigurationUrl(CONFIGURL);
  DEVICE.enableExtendedUniqueIds();
  KEY_PRESS.setName("Key Press");
  KEY_PRESS.setForceUpdate(true);
  KEY_PRESS.setIcon("mdi:button-pointer");
  KEY_PRESS.setAvailability(true);
  UPTIME.setName("Uptime");
  UPTIME.setExpireAfter(30);
  UPTIME.setEntityCategory("diagnostic");
  UPTIME.setIcon("mdi:clock-check-outline");
  MAC_ADDRESS.setName("MAC Address");
  MAC_ADDRESS.setIcon("mdi:ethernet");
  MAC_ADDRESS.setEntityCategory("diagnostic");
  
  // start MQTT connection
  Serial.print("Starting connection to MQTT broker at ");
  Serial.println(BROKER_ADDR);
  MQTT.begin(BROKER_ADDR);
}

void loop() {
  MQTT.loop();

  keyQueueItem key;
  while (queue_try_remove(&KEY_QUEUE, &key)) { //retrieving item from the queue and deleting it from the queue
    Serial.print("VALUE RETRIEVED:\t");
    Serial.println(key.value);
    KEY_PRESS.setValue(key.value);
    Serial.print("AVAILABILITY RETRIEVED:\t");
    Serial.println(key.available);
    if (strcmp(key.value,"None") == 0) {
      KEY_PRESS.setAvailability(key.available);
    }
  }

  if ((millis() - LAST_UPDATE_AT) > 2000) { // update in 2s interval
    String uptime_value = "";
    unsigned long seconds = millis() / 1000;
    int days = seconds / (24 * 3600);
    seconds = seconds % (24 * 3600);
    int hours = seconds / 3600;
    seconds = seconds % 3600;
    int minutes = seconds /  60;
    seconds = seconds % 60;
    if ( days > 3650 ) {
      sprintf(UPTIME_CHAR, "%ds", 0);
    } else if ( days ) {
      sprintf(UPTIME_CHAR, "%%dd %dh %dm %ds", days,hours,minutes,seconds);
    } else if ( hours ) {
      sprintf(UPTIME_CHAR, "%dh %dm %ds", hours,minutes,seconds);
    } else if ( minutes ) {
      sprintf(UPTIME_CHAR, "%dm %ds", minutes,seconds);
    } else {
      sprintf(UPTIME_CHAR, "%ds", seconds);
    }
    UPTIME.setValue(UPTIME_CHAR);
    MAC_ADDRESS.setValue(MAC_CHAR);
    LAST_UPDATE_AT = millis();
  }
}

//------------- Core1 -------------//
void setup1() {
  //initialize the queues to pass data to main loop to send to HA
  queue_init(&KEY_QUEUE, sizeof(item), QUEUE_LENGTH); 

  // configure pio-usb: defined in usbh_helper.h
  rp2040_configure_pio_usb();

  // run host stack on controller (rhport) 1
  // Note: For rp2040 pico-pio-usb, calling USBHost.begin() on core1 will have most of the
  // host bit-banging processing works done in core1 to free up core0 for other works
  // start USB
  Serial.println("Waiting for USB device to mount...");
  USBHost.begin(1);

}

void loop1() {
  USBHost.task();
}

extern "C" {

  void send_key_availability(bool available) {
    keyQueueItem key;
    strncpy(key.value, "None", sizeof(key.value));
    key.available = available;
    if (queue_try_add(&KEY_QUEUE, &key)) { //adding items to the queue
      Serial.print("ADDING:\t");
      Serial.println(key.value);
     }
  }

  // Invoked when device with hid interface is mounted
  // Report descriptor is also available for use.
  // tuh_hid_parse_report_descriptor() can be used to parse common/simple enough
  // descriptor. Note: if report descriptor length > CFG_TUH_ENUMERATION_BUFSIZE,
  // it will be skipped therefore report_desc = NULL, desc_len = 0
  void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *desc_report, uint16_t desc_len) {
    (void)desc_report;
    (void)desc_len;
    uint16_t vid, pid;
    tuh_vid_pid_get(dev_addr, &vid, &pid);

    Serial.printf("HID device address = %d, instance = %d is mounted\r\n", dev_addr, instance);
    Serial.printf("VID = %04x, PID = %04x\r\n", vid, pid);
    Serial.println("Listening for remote codes...");

    send_key_availability(true);

    if (!tuh_hid_receive_report(dev_addr, instance)) {
      Serial.println("Error: cannot request to receive report");
      send_key_availability(false);
    }
  }

  // Invoked when device with hid interface is un-mounted
  void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    Serial.printf("HID device address = %d, instance = %d is unmounted\r\n", dev_addr, instance);
    send_key_availability(false);
  }

  // Invoked when received report from device via interrupt endpoint
  void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t len) {
    keyQueueItem key;
    key.available = true;

    if (KEY_DOWN == 0) {
      Serial.print("HID report: ");
      for (uint16_t i = 1; i < len; i++) {
        Serial.printf("%d ", report[i]);
      }
      Serial.println("");

      for (uint16_t i = 1; i < len; i++) {
        if (report[i] > 0) {
          KEY_DOWN = report[i];
          break;
        }
      }
      DOWN_START = millis();
    } else {
      unsigned long down_end = millis();
      unsigned long total_time = down_end - DOWN_START;

      if (total_time > LONG_PRESS_THRESHOLD) {
        sprintf(key.value, "%d-L", KEY_DOWN);
      } else {
        sprintf(key.value, "%d", KEY_DOWN);
      }
      
      Serial.printf("Keycode : %d held for %dms\r\n", KEY_DOWN, total_time);
      
      if (queue_try_add(&KEY_QUEUE, &key)) { //adding items to the queue
        Serial.print("ADDING:\t");
        Serial.println(key.value);
      }

      KEY_DOWN = 0;
      DOWN_START = 0;
    }

    // continue to request to receive report
    if (!tuh_hid_receive_report(dev_addr, instance)) {
      Serial.println("Error: cannot request to receive report");
      send_key_availability(false);
    }
  }

}  // extern C