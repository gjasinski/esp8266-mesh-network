#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#define BASE_SSID "Lanternet1"
#define BASE_PASSWORD "swswswsw"
#define HOME_HTTP_SERVER "http://10.0.0.2:8000/"
#define MESH_INTERNAL_PASSWORD "thispasswordisnotsafe"

WiFiServer server(80);
//todo we should discuss and create some mesh packet to know who should get this message
// esp8266 -subscribe-topic-> broker so there is new sensor and it should subscribe topic ep. temperature, humidity
// Broker -publish-> esp8266 sends some topic ep. humidity broadcast or unicast ep. /humidity or /18/humidity
// esp8266 -send-data-> broker response to broker
// esp8266 -unsubscrive-> broker it should do also node which detect missing node(should unsubscibe missing node)?


int myId = -1;
int masterId = -1;

int getMaxNetworkId();

void connectToNetwork();

void configureAPSettings(String mySSID);

String getParentIpAddress();

void setup() {
    Serial.begin(115200);

    int maxNetworkId = getMaxNetworkId();
    myId = maxNetworkId + 1;

    String mySSID = "ESPMESH-" + String(myId);
    Serial.println("I will advertise myself as " + mySSID);

    WiFi.mode(WIFI_AP_STA);
    connectToNetwork();
    configureAPSettings(mySSID);

    server.begin();
}

int getMaxNetworkId() {
    int numberOfNetworksFound = WiFi.scanNetworks(false, true);
    Serial.println("scan done networks = " + numberOfNetworksFound);

    int maxnum = -1;
    if (numberOfNetworksFound == 0) {
        Serial.println("no networks found");
        // todo in my opinion, we should search for network until we will find it
    } else {
        for (int i = 0; i < numberOfNetworksFound; ++i) {
            String ssid = WiFi.SSID(i);
            if (ssid.startsWith("ESPMESH-")) {
                int num = ssid.substring(8).toInt();
                if (num > maxnum) {
                    maxnum = num;
                }
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
        String parentSSID = "ESPMESH-" + String(myId - 1);
        Serial.println("connecting as client to " + parentSSID);
        WiFi.begin(parentSSID.c_str(), MESH_INTERNAL_PASSWORD);
    }

    while (WiFi.status() != WL_CONNECTED) {
        Serial.println("waiting for connection");
        delay(10000);
    }
}

void configureAPSettings(String mySSID) {
    IPAddress apIP(192, 168, 4 + myId, 1);
    IPAddress apGateway(192, 168, 4 + myId, 1);
    IPAddress apSubmask(255, 255, 255, 0);

    WiFi.softAPConfig(apIP, apGateway, apSubmask);
    WiFi.softAP(mySSID.c_str(), MESH_INTERNAL_PASSWORD);
}

void handleIncomingHTTPRequest(WiFiClient client) {
    Serial.println("got incoming http request");

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
    //todo it's too data in simple ok response, make it shorter maybe some code or sth
    //todo discuss
    String s = "HTTP/1.1 200 OK\r\n";
    s += "Content-Type: text/plain\r\n\r\n";
    s += "Great Success\r\n";
    client.print(s);
    //client.close();

    if (body.length() > 0) {
        // TODO: put this in a transmit queue or something like that, now this will create a blocking process on all the hops until the edge of the network
        transmitSensorData(body);
    }
}

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
    Serial.printf("transmitting sensor data, return code: %d\r\n", code);
}

String getParentIpAddress() {
    return "http://192.168." + String(4 + myId - 1) + ".1/";
}

void pingYourMaster() {

}

int sleep = 0;
int messageId = 0;

void loop() {
    WiFiClient client = server.available();
    if (client) {
        handleIncomingHTTPRequest(client);
    } else {
        delay(1);  // small delay so we don't wait one second before handling incoming http traffic
        sleep += 1;
        if (sleep >= 1000) { //todo extract magic number and adopt sleep time, that it should in some resonable intervals send sensor data or if it device will subscribe to topic, then it should be deleted and respond only for pubish
            //todo discuss: sensors: one, two, more? and then get data and send
            //todo create strategy for different type of sensors
            //todo I don't know if esp8266 can detect type of sensor, we should configure each node from server? and here is research about mqtt
            transmitSensorData("client with distance: " + String(myId) + " - messageId: " + String(messageId));
            messageId += 1;
            sleep = 0;
        }
    }
}
