#include <Arduino.h>
#include <WiFi.h>
#include "websrv.h"
#include "index.h"
#include "Audio.h"
#include "SoapESP32.h"
#include "Arduino_JSON.h"
#include <vector>

using namespace std;

#define I2S_DOUT      25
#define I2S_BCLK      27
#define I2S_LRC       26


char SSID[] = "*****";
char PASS[] = "*****";



WebSrv     webSrv;
WiFiClient client;
WiFiUDP    udp;
SoapESP32  soap(&client, &udp);
Audio audio;

uint numServers = 0;
int  currentServer = -1;
uint32_t media_downloadPort = 0;
String media_downloadIP = "";
vector<String> names{};
//----------------------------------------------------------------------------------------------------------------------
int DLNA_setCurrentServer(String serverName){
    int serverNum = -1;
    for(int i = 0; i < names.size(); i++){
        if(names[i] == serverName) serverNum = i;
    }
    currentServer = serverNum;
    return serverNum;
}
void DLNA_showServer(){ // Show connection details of all discovered, usable media servers
    String msg = "DLNA_Names=";
    soapServer_t srv;
    names.clear();
    for(int i = 0; i < numServers; i++){
        soap.getServerInfo(i, &srv);
        Serial.printf("Server[%d]: IP address: %s port: %d name: %s -> controlURL: %s\n",
           i, srv.ip.toString().c_str(), srv.port, srv.friendlyName.c_str(), srv.controlURL.c_str());
        msg += srv.friendlyName;
        if(i < numServers - 1) msg += ',';
        names.push_back(srv.friendlyName);
    }
    webSrv.send(msg);
}
void DLNA_browseServer(String objectId, uint8_t level){
    JSONVar myObject;
    soapObjectVect_t browseResult;
    soapObject_t object;

    // Here the user selects the DLNA server whose content he wants to see, level 0 is root
    if(level == 0){
        if(DLNA_setCurrentServer(objectId) < 0) {log_e("DLNA Server not found"); return;}
        objectId = "0";
    }

    soap.browseServer(currentServer, objectId.c_str(), &browseResult);
    if(browseResult.size() == 0){
        log_i("no content!"); // then the directory is empty
        return;
    }
    log_i("objectID: %s", objectId.c_str());
    for (int i = 0; i < browseResult.size(); i++){
        object = browseResult[i];
        myObject[i]["name"]= object.name;
        myObject[i]["isDir"] = object.isDirectory;
        if(object.isDirectory){
            myObject[i]["id"]  = object.id;
        }
        else {
            myObject[i]["id"]  = object.uri;
            media_downloadPort = object.downloadPort;
            media_downloadIP   = object.downloadIp.toString();
        }
        myObject[i]["size"] = (uint32_t)object.size;
        myObject[i]["uri"]  = object.id;
        log_i("objectName %s", browseResult[i].name.c_str());
        log_i("objectId %s", browseResult[i].artist.c_str());
    }
    String msg = "Level" + String(level,10) + "=" + JSON.stringify(myObject);

    log_i("msg = %s", msg.c_str());
    webSrv.send(msg);
    browseResult.clear();
}

void DLNA_getFileItems(String uri){
    soapObjectVect_t browseResult;

    log_i("uri: %s", uri.c_str());
    log_w("downloadIP: %s", media_downloadIP.c_str());
    log_w("downloadport: %d", media_downloadPort);
    String URL = "http://" + media_downloadIP + ":" + media_downloadPort + "/" + uri;
    log_i("URL=%s", URL.c_str());
    audio.connecttohost(URL.c_str());
}
void DLNA_showContent(String objectId, uint8_t level){
    log_i("obkId=%s", objectId.c_str());
    if(level == 0){
        DLNA_browseServer(objectId, level);
    }
    if(objectId.startsWith("D=")) {
        objectId = objectId.substring(2);
        DLNA_browseServer(objectId, level);
    }
    if(objectId.startsWith("F=")) {
        objectId = objectId.substring(2);
        DLNA_getFileItems(objectId);
    }
}

//----------------------------------------------------------------------------------------------------------------------
//                                      S E T U P
//----------------------------------------------------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, PASS);
    while (WiFi.status() != WL_CONNECTED) delay(1500);
    log_i("connected, IP=%s", WiFi.localIP().toString().c_str());
    webSrv.begin(80, 81); // HTTP port, WebSocket port

    soap.seekServer();
    numServers = soap.getServerCount();
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(21); // 0...21
}

//----------------------------------------------------------------------------------------------------------------------
//                                      L O O P
//----------------------------------------------------------------------------------------------------------------------
void loop() {
    if(webSrv.loop()) return; // if true: ignore all other for faster response to web
    audio.loop();
}
//----------------------------------------------------------------------------------------------------------------------
//                                    E V E N T S
//----------------------------------------------------------------------------------------------------------------------
void WEBSRV_onCommand(const String cmd, const String param, const String arg){  // called from html
    log_d("WS_onCmd:  cmd=\"%s\", params=\"%s\", arg=\"%s\"", cmd.c_str(),param.c_str(), arg.c_str());
    if(cmd == "index.html"){ webSrv.show(index_html); return;}
    if(cmd == "ping"){webSrv.send("pong"); return;}
    if(cmd == "favicon.ico") return;
    if(cmd == "DLNA_getServer")  {DLNA_showServer(); return;}
    if(cmd == "DLNA_getContent0"){DLNA_showContent(param, 0); return;}
    if(cmd == "DLNA_getContent1"){DLNA_showContent(param, 1); return;} // search for level 1 content
    if(cmd == "DLNA_getContent2"){DLNA_showContent(param, 2); return;} // search for level 2 content
    if(cmd == "DLNA_getContent3"){DLNA_showContent(param, 3); return;} // search for level 3 content
    if(cmd == "DLNA_getContent4"){DLNA_showContent(param, 4); return;} // search for level 4 content
    log_e("unknown HTMLcommand %s, param=%s", cmd.c_str(), param.c_str());
}
void WEBSRV_onRequest(const String request, uint32_t contentLength){
    log_d("WS_onReq: %s contentLength %d", request.c_str(), contentLength);
    if(request.startsWith("------")) return;      // uninteresting WebKitFormBoundaryString
    if(request.indexOf("form-data") > 0) return;  // uninteresting Info
    log_e("unknown request: %s",request.c_str());
}
void WEBSRV_onInfo(const char* info){
    log_v("HTML_info:   %s", info);    // infos for debug
}
void audio_info(const char *info){
    Serial.print("info        "); Serial.println(info);
}
void audio_id3data(const char *info){  //id3 metadata
    Serial.print("id3data     ");Serial.println(info);
}
void audio_eof_stream(const char* info){ // The webstream comes to an end
    Serial.print("end of stream:      ");Serial.println(info);
}
void audio_bitrate(const char *info){
    Serial.print("bitrate     ");Serial.println(info);
}
void audio_lasthost(const char *info){  //stream URL played
    Serial.print("lasthost    ");Serial.println(info);
}
