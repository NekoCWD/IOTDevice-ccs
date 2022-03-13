#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>


void callback(char* topic, uint8_t* payload, unsigned int length);
EthernetClient client;
PubSubClient mqtt(client);
byte mac[6] = {0x7F,0xCF,0xD8,0xE1,0xDD,0x17};
const char MQTT_ID[] = "Test ID";
const char server_addr[] = "192.168.1.1";
const uint16_t server_port = 1883;
const int LED = 2;
enum StateEnum{
    INIT,
    NOETHCHIP,
    NOCABLECONNECTED,
    IPADDRERR,
    MQTTERR
};
class Publisher{
public:
    char* topic;
    Publisher(char topic_m[]){
        topic = topic_m;
    }
    void send(char* message){
        mqtt.publish(topic, message);
    }
};
class Subscriber{
public:
    char* topic;
    void(*callback_fn)(char*,char*);
    Subscriber(char topic_m[], void(*callback_func)(char*, char*)){
        topic = topic_m;
        callback_fn = callback_func;
        previous_instance = last_instance;
        last_instance = this;
    }
    static void handlemsg(char* topic, char* message){
        last_instance->handle(topic, message);
    }
    static void subscribe(){
        for(Subscriber* i=last_instance;i==nullptr;i=i->previous_instance){
            mqtt.subscribe(i->topic);
        }
    }
private:
    inline static Subscriber* last_instance = nullptr;
    Subscriber* previous_instance;
    void handle(char* topic_m, char* msg){
        bool is4me = true;
        for(uint16_t i=0; topic_m[i]; i++){
            if(topic_m[i]!=topic[i] || topic[i] != '#'){
                is4me = false;
                break;
            }
        }
        if(is4me){
           callback_fn(topic_m,msg); 
        }
        if(previous_instance!=nullptr){
            previous_instance->handle(topic_m,msg);
        }
    }
};
StateEnum state;


void setup(){
    state = INIT;
    Serial.begin(9600);
    Serial.println("Init...");
    Serial.print("Ethernet...");
    bool eth_state = Ethernet.begin(mac) != 0;
    if(Ethernet.hardwareStatus() == EthernetNoHardware){
        state = NOETHCHIP;
        Serial.println("FAIL: no chip!");
        Serial.println("Critical error, freezing system!");
        while(true);
    }
    Serial.println("OK");
    Serial.print("IP ADRESSING...");
    state = eth_state? state : IPADDRERR;
    Serial.println(state == IPADDRERR? "FAIL" : "OK");
    if(state!=IPADDRERR){
        Serial.println("My IP:");
        Serial.println(Ethernet.localIP());
    }
    mqtt.setServer(server_addr, server_port);
    mqtt.setCallback(callback);
    new Subscriber("/testsub/1", [](char* topic, char* msg){
                    Serial.print(topic);
                    Serial.print("<<");
                    Serial.println(msg);
                   });
}

void callback(char* topic, uint8_t* payload, unsigned int length){
    char buf[length];
    for(unsigned int i=0; i<length; i++)
        buf[i]=(char)payload[i];
    Subscriber::handlemsg(topic, buf);
}

void loop(){
    {
        static uint16_t maintain_tim;
        if(millis()-maintain_tim>=100000){
            switch (Ethernet.maintain()){
                case 4:
                    state = state == IPADDRERR? INIT:state;
                    break;
                default:
                   break; 
            }
        }
    }
    {
        static unsigned long lastMqttDisconnect;
        if(state!=IPADDRERR && !mqtt.connected()){
            if(millis()-lastMqttDisconnect>=5000){
                Serial.print("MQTT Disconnected, trying to reconnect...");
                bool statemq = mqtt.connect(MQTT_ID);
                state = statemq? INIT : MQTTERR;
            }
        }
    }
    {
        static uint16_t led_blink_tim;
        switch (state){
            case INIT:
                digitalWrite(LED, HIGH);
                break;
            case NOETHCHIP:
                if(millis()-led_blink_tim>=100/1){
                    led_blink_tim=millis();
                    digitalWrite(LED, !digitalRead(LED));
                }
                break;
            case NOCABLECONNECTED:
                if(millis()-led_blink_tim>=100/2){
                    led_blink_tim=millis();
                    digitalWrite(LED, !digitalRead(LED));
                }
                break;
            case IPADDRERR:
                if(millis()-led_blink_tim>=100/3){
                    led_blink_tim=millis();
                    digitalWrite(LED, !digitalRead(LED));
                }
                break;
            case MQTTERR:
                if(millis()-led_blink_tim>=100/4){
                    led_blink_tim=millis();
                    digitalWrite(LED, !digitalRead(LED));
                }
                break;
             
            default:
                break;
        }
    }
}
