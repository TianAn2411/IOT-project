import paho.mqtt.client as mqtt
import sys

def on_connect(client, userdata, flags, rc):
    print("Connected with result code "+str(rc))
    client.subscribe("v1/devices/me/#")

def on_message(client, userdata, msg):
    print(msg.topic+" "+str(msg.payload))

client = mqtt.Client()
client.username_pw_set("user1", "12345678")
client.on_connect = on_connect
client.on_message = on_message

try:
    client.connect("127.0.0.1", 1883, 60)
    client.loop_forever()
except Exception as e:
    print(e)
    sys.exit(1)
