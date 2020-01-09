#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

#define DHTPIN D2
#define DHTTYPE DHT22

#define MQTT_TOPIC "/XXX/XXX"

/* Certificate Authority info */
/* CA Cert in PEM format */
const char caCert[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
....
VTELMAkGA1UECAwCQVQxITAfBgNVBAodGVybmV0IFdpZGdpdHMgUHR5IEx0
....
-----END CERTIFICATE-----
)EOF";

/* MQTT broker cert SHA1 fingerprint, used to validate connection to right server */
const uint8_t mqttCertFingerprint[] = {0x58,0x62,0x5B,0xFB,0x25,0x41,0x32,0x87,0xD4,0xAA,0x72,0xA6,0xC0,0x92,0xCC,0xA1,0xAA,0xD7,0xA2,0x05};

/* Other globals */
X509List caCertX509(caCert);        /* X.509 parsed CA Cert */
WiFiClientSecure espClient;         /* Secure client connection class, as opposed to WiFiClient */
PubSubClient mqttClient(espClient); /* MQTT Client connection */
DHT dht(DHTPIN, DHTTYPE);
const char* WIFI_SSID = "XXX";
const char* WIFI_PASSWORD = "XXX";
const char* MQTT_SERVER = "XXX";
int MQTT_PORT = 8883;
String clientId = "ESP8266Client-"; /* MQTT client ID (will add random hex suffix during setup) */

float humidity;
float temperature;

void setup() 
{
  Serial.begin(115200);
  /*  Connect to local WiFi access point */
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi ");
  Serial.println(WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
  }
  Serial.println();
  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());

  /* Configure secure client connection */
  espClient.setTrustAnchors(&caCertX509);         /* Load CA cert into trust store */
  espClient.allowSelfSignedCerts();               /* Enable self-signed cert support */
  espClient.setFingerprint(mqttCertFingerprint);  /* Load SHA1 mqtt cert fingerprint for connection validation */
  
  /* Optionally do none of the above and allow insecure connections.                                             
   * This will accept any certificates from the server, without validation and is not recommended.
   */
  //espClient.setInsecure();
                                             
  /* Configure MQTT Broker settings */
  mqttClient.setServer(MQTT_SERVER,MQTT_PORT);

  /* Add random hex client ID suffix once during each reboot */
  clientId += String(random(0xffff), HEX); 

  dht.begin();
}

void loop() 
{
  /* Main loop. Attempt to re-connect to MQTT broker if connection drops, and service the mqttClient task. */
  if(!mqttClient.connected()) {
    mqttReconnect();
  }
  mqttClient.loop();
  delay(100); //reducing wifi-interferences
  // Reading DHT22 sensor data
  humidity = dht.readHumidity();
  temperature = dht.readTemperature();
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("DHT22 sensor is not ready yet");
    return;
  }
  // Publishing sensor data
  mqttPublish(MQTT_TOPIC, temperature, humidity);
  
  // Deep-Sleep
  Serial.println("going in deep-sleep for 60s");
  ESP.deepSleep(60e6); 
  delay(100);
}

void mqttReconnect() {
  /* Loop until we're reconnected */
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    /* Attempt to connect */
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("connected");
    } 
    else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void mqttPublish(char* topic, float temperature, float humidity) {
  String payload = String("{'temp': '") + String(temperature).c_str() + String ("', 'hum': '") + String(humidity).c_str() + String("'}");
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(payload);
  mqttClient.publish(topic, payload.c_str(), true);
}
