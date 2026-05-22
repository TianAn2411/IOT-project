import json
import time
import random
from flask import Flask, render_template
from flask_socketio import SocketIO
import paho.mqtt.client as mqtt
import threading

app = Flask(__name__)
app.config['SECRET_KEY'] = 'iot-dashboard-secret'
socketio = SocketIO(app, cors_allowed_origins="*", async_mode='threading')

# MQTT Configuration
import socket
MQTT_BROKER = socket.gethostbyname(socket.gethostname())
MQTT_PORT = 1883
MQTT_USER = "user1"
MQTT_PASS = "12345678"

# Topics
TOPIC_TELEMETRY = "v1/devices/me/telemetry"
TOPIC_ATTRIBUTES = "v1/devices/me/attributes"
TOPIC_RPC_RES = "v1/devices/me/rpc/response/+"
TOPIC_RPC_REQ_PREFIX = "v1/devices/me/rpc/request/"

def on_connect(client, userdata, flags, rc):
    print(f"Connected to MQTT Broker with result code {rc}", flush=True)
    client.subscribe("v1/devices/me/#")

def on_message(client, userdata, msg):
    topic = msg.topic
    payload = msg.payload.decode('utf-8')
    try:
        data = json.loads(payload)
    except json.JSONDecodeError:
        data = payload

    print(f"Received MQTT [{topic}]: {data}", flush=True)

    if topic == TOPIC_TELEMETRY:
        socketio.emit('telemetry', data)
    elif topic == TOPIC_ATTRIBUTES:
        socketio.emit('attributes', data)
    elif topic.startswith("v1/devices/me/rpc/response/"):
        socketio.emit('rpc_response', {'topic': topic, 'data': data})

mqtt_client = mqtt.Client()
mqtt_client.username_pw_set(MQTT_USER, MQTT_PASS)
mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message

def start_mqtt():
    try:
        mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
        mqtt_client.loop_forever()
    except Exception as e:
        print(f"Failed to connect to MQTT: {e}")

# Start MQTT in a separate thread so it doesn't block Flask
mqtt_thread = threading.Thread(target=start_mqtt, daemon=True)
mqtt_thread.start()


@app.route('/')
def index():
    return render_template('index.html')

@socketio.on('send_rpc')
def handle_send_rpc(data):
    # data expects: {'method': 'setPwm2', 'params': {'enabled': True, 'duty': 50}}
    request_id = str(int(time.time() * 1000) + random.randint(0, 1000))
    topic = f"{TOPIC_RPC_REQ_PREFIX}{request_id}"
    payload = json.dumps(data)
    print(f"Sending RPC [{topic}]: {payload}")
    mqtt_client.publish(topic, payload)

if __name__ == '__main__':
    socketio.run(app, host='0.0.0.0', port=5000, debug=True)
