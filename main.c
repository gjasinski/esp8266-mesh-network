#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#define BASE_SSID "Lanternet1"
#define BASE_PASSWORD "swswswsw"
#define HOME_HTTP_SERVER "http://10.0.0.2:8000/"
#define MESH_INTERNAL_PASSWORD "thispasswordisnotsafe"

WiFiServer server(80);
String TEMPERATURE_TOPIC = "temperature";
String SUBSCRIBE_TOPIC_REQUEST = "0/";
String RESPOND_TO_BROKER_REQUEST = "2/";
String UNSUBSCRIBE_TOPIC_REQUEST = "3/";
String PING_REQUEST = "4/";
String PING_RESPONSE = "5";
String REGISTER_CHILD_REQUEST = "6/";
//todo we should discuss and create some mesh packet to know who should get this message
// [0/id/temperatura]esp8266 -subscribe-topic-> broker so there is new sensor and it should subscribe topic ep. temperature, humidity
// [1/id/temperatura]Broker -publish-> esp8266 sends some topic ep. humidity broadcast or unicast ep. /humidity or /18/humidity
// [2/id/temperatura/dane]esp8266 -send-data-> broker response to broker
// [3/id/temperatura]esp8266 -unsubscrive-> broker it should do also node which detect missing node(should unsubscibe missing node)?
// [3/id]esp8266 -unsubscrive-> broker it should do also node which detect missing node(should unsubscibe missing node)?
// [4/ipaddress] //ping request
// [5] //ping response
// [6/id] register childId


int myId = -1;
int myParentId = -1;
int myChildId = -1;
//==ping variables
int pingContinue = 1;
int pingCounter = 1;//we don't want to ping in first moment
int pingCounterModulo = 10000;

int getMaxNetworkId();

void connectToNetwork();

void configureAPSettings(String mySSID);

String getParentIpAddress();

void setup() {
    Serial.begin(115200);

    int maxNetworkId = getMaxNetworkId();
    myId = maxNetworkId + 1;
    myParentId = maxNetworkId;

    String mySSID = "ESPMESH-" + String(myId);
    Serial.println("I will advertise myself as " + mySSID);

    WiFi.mode(WIFI_AP_STA);
    connectToNetwork();
    configureAPSettings(mySSID);

    server.begin();
    subsribeTopics();
}

int getMaxNetworkId() {
  int numberOfNetworksFound = WiFi.scanNetworks(false, true);
  int maxnum = -1;

  while (numberOfNetworksFound == 0) {
      Serial.println("no networks found");
      delay(100);
      numberOfNetworksFound = WiFi.scanNetworks(false, true);
  }
  for (int i = 0; i < numberOfNetworksFound; ++i) {
            String ssid = WiFi.SSID(i);
            if (ssid.startsWith("ESPMESH-")) {
                int num = ssid.substring(8).toInt();
                if (num > maxnum) {
                    maxnum = num;
                }
            }
        }
    return maxnum;
}

void connectToNetwork() {
    if (myId == 0) {
        // connect to "home" base network, we are at the edge
        // todo here should be connecting to some server maybe mqtt, we shall see
        WiFi.begin(BASE_SSID, BASE_PASSWORD);
    } else {
        // connect to the nearest hop
        String parentSSID = "ESPMESH-" + String(myParentId);
        Serial.println("connecting as client to " + parentSSID);
        WiFi.begin(parentSSID.c_str(), MESH_INTERNAL_PASSWORD);
    }

    while (WiFi.status() != WL_CONNECTED) {
        Serial.println("waiting for connection");
        delay(10000);
    }
    registerMeAsChild();
}

void configureAPSettings(String mySSID) {
    IPAddress apIP(192, 168, 4 + myId, 1);
    IPAddress apGateway(192, 168, 4 + myId, 1);
    IPAddress apSubmask(255, 255, 255, 0);

    WiFi.softAPConfig(apIP, apGateway, apSubmask);
    WiFi.softAP(mySSID.c_str(), MESH_INTERNAL_PASSWORD);
}

void handleIncomingHTTPRequest(WiFiClient client) {
    String body = "";
    while (true) {
        String header = client.readStringUntil('\r');
        if (header.length() == 1) { // if only a \n is found it means the headers are done...
            client.read(); // drop the newline
            body = client.readString();
            break;
        } else if (header.length() == 0) {
            break;
        }
    }
    Serial.println(body);
    client.flush();
    String s = "HTTP/1.1 200 OK\r\n";
    s += "Content-Type: text/plain\r\n\r\n";
    s += "Great Success\r\n";
    client.print(s);
    client.close();
    incomingRequestStrategy(body);
}

void incomingRequestStrategy(String body){
  char packetType = body[0];
  switch (packetType){
    case '0'://subscrive topic -> forward message to parent
      sendPacketToIp(packet, getParentIpAddress());
      break;
    case '1':
      respondeToPublishTypePacket(body);
      if (shouldIForwardPublishTypePacket(body) == 1){
        sendPacketToIp(packet, convertIdToAddress(String(myChildId)));
      }
      break;
    case '2'://esp sends sensor data - forward message to parent
      sendPacketToIp(packet, getParentIpAddress());
      break;
    case '3'://esp unsubscibe - forward message to parent
      sendPacketToIp(packet, getParentIpAddress());
      break;
    case '4'://ping request
      String sourceId = body.substring(1);
      String sourceAddress = convertIdToAddress(sourceId);
      String response = PING_RESPONSE;
      sendPacketToIp(response, sourceAddress);
      Serial.println("RECEIVED PING REQUEST");
      break;
    case '5'://ping response
      pingContinue = 1;
      Serial.println("RECEIVED PING RESPONSE");
      break;
    case '6'://register childId
      myChildId = body.substring(1);
      Serial.println("REGISTER CHILD:" + myChildId);
    default:
      Serial.println("Not known packet: " + body);
      /*
       *    if (body.length() > 0) {
             // TODO: put this in a transmit queue or something like that, now this will create a blocking process on all the hops until the edge of the network
             transmitSensorData(body);
         }
         */
  }
}
/*
void transmitSensorData(String data) {
    HTTPClient http;
    if (myId == 0) {
        http.begin(HOME_HTTP_SERVER);
    } else {
        String parentIpAddress = getParentIpAddress();
        http.begin(parentIpAddress);
    }
    Serial.println(data);
    int code = http.POST(data);
    //Serial.printf("transmitting sensor data, return code: %d\r\n", code);
}*/

int sleep = 0;
int messageId = 0;

void loop() {
    WiFiClient client = server.available();
    if (client) {
        handleIncomingHTTPRequest(client);
    } else {
        handlePing();
        delay(1);  // small delay so we don't wait one second before handling incoming http traffic
    }
}

void handlePing(){
  pingCounter = pingCounter % pingCounterModulo;
  if(pingCounter == 0){
    if(pingContinue == 0){
      initializeSearchingForNewParent();
    }
    else{
      sendPing();
      pingContinue = 0;
    }
  }
}

void initializeSearchingForNewParent(){
  Serial.println("initializeSearching for new parent");
  unSubscribeToTopicWithNode(myParentId);
  int newCandidateParentId = getNetworkCandidateToNewParent();
  myParentId = newCandidateParentId;
  connectToNetwork();
}

int getNetworkCandidateToNewParent(){
  int numberOfNetworksFound = WiFi.scanNetworks(false, true);
  int maxnum = -1;

  while (numberOfNetworksFound == 0) {
      Serial.println("no networks found");
      delay(100);
      numberOfNetworksFound = WiFi.scanNetworks(false, true);
  }
  for (int i = 0; i < numberOfNetworksFound; ++i) {
          String ssid = WiFi.SSID(i);
          if (ssid.startsWith("ESPMESH-")) {
              int num = ssid.substring(8).toInt();
              if (num > myId){
                return maxnum;
              }
              if (num > maxnum) {
                  maxnum = num;
              }
          }
      }
  return maxnum;
}

void registerMeAsChild(){
  String data = REGISTER_CHILD_REQUEST + String(myId);
  sendPacketToIp(data, getParentIpAddress());
}
void subsribeTopics(){
  subscribeToTopic("temperature");
}
void subscribeToTopic(String topic){
  String data = SUBSCRIBE_TOPIC_REQUEST + String(myId) + "/" + topic;
  sendPacketToIp(data, getParentIpAddress());
}

void unSubscribeToTopicWithNode(String nodeId){
  String data = UNSUBSCRIBE_TOPIC_REQUEST + nodeId;
  sendPacketToIp(data, getParentIpAddress());
}

int shouldIForwardPublishTypePacket(String body){
  String bodyWithoutHeader = String.substring(2);
  if(strstr(bodyWithoutHeader, "/") == NULL){
    return 1;
  }
  if(bodyWithoutHeader == String(myId)){
    Serial.println("shouldIForwardPublishTypePacket=true contains=" + bodyWithoutHeader)
    return 1;
  }
  else{
    Serial.println("shouldIForwardPublishTypePacket=false contains=" + bodyWithoutHeader)
    return 0;
  }
}

void respondeToPublishTypePacket(String body){
  respondeToPublishTemperaturePacket(body);
  //when adding new topics support write methods like above
}

void respondeToPublishTemperaturePacket(String body){
  if(strstr(body, TEMPERATURE_TOPIC) != NULL){
    String data = "19.99";//getTemerature()
    String packet = RESPOND_TO_BROKER_REQUEST + String(myId) + "/" + data;
    sendPacketToIp(packet, getParentIpAddress());
  }
}

void sendPing(){
  String request = PING_REQUEST + myId;
  String address = getParentIpAddress();
  sendPacketToIp(request, getParentIpAddress);
}

String getParentIpAddress() {
    return convertIdToAddress(String(4 + myParentId));
}

String convertIdToAddress(String id){
    return "http://192.168." + id + ".1/";
}

void sendPacketToIp(String body, String address){
  HTTPClient http;
  http.begin(address);
  int code = http.POST(body);
}
