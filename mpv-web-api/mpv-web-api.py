import argparse
import socket
import json
import time
import threading
from flask import Flask, request, jsonify

app = Flask(__name__)

# Internal database (dictionary) to store property values
property_values = {}


def process_event(event):
    if event.get('event') == 'property-change' and 'name' in event and 'data' in event:
        property_name = event['name']
        property_value = event['data']
        property_values[property_name] = property_value


def send_command(client, command):
    try:
        message = json.dumps(command).encode() + b'\n'
        client.sendall(message)
        print(f"Sent command: {command}")
    except Exception as e:
        print(f"Error sending command: {e}")


@app.route('/properties', methods=['GET'])
def get_properties():
    return jsonify(property_values)


@app.route('/properties/<property_name>', methods=['GET'])
def get_property(property_name):
    return jsonify({property_name: property_values.get(property_name,
                                                       'Unknown')})


@app.route('/command', methods=['POST'])
def command():
    data = request.data
    if not data:
        return jsonify({'error': 'No command provided'}), 400

    try:
        send_command(client_set, json.loads(data))
        return jsonify({'status': 'command sent'}), 200
    except json.JSONDecodeError as e:
        return jsonify({'error': 'Invalid JSON format'}), 400
    except Exception as e:
        return jsonify({'error': str(e)}), 500


# Connect to the MPV IPC socket
def start_mpv_listener(mpvsocketpath):
    global client_get
    global client_set
    client_get = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    client_set = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    try:
        client_get.connect(mpvsocketpath)
        client_set.connect(mpvsocketpath)
        print("Connected to MPV socket")
    except Exception as e:
        print(f"Failed to connect to MPV socket: {e}")
        return

    # List of properties to observe
    properties_to_observe = [
        "pause",
        "duration",
        "volume",
        "mute",
        "filename",
        "playlist-pos",
        "playlist-count",
        "media-title",
        "playback-time",
        "idle-active"
    ]

    # Send commands to MPV to observe property changes
    for prop in properties_to_observe:
        send_command(client_get, {"command": ["observe_property", 1, prop]})

    buffer = ""

    while True:
        try:
            data = client_get.recv(1024).decode()
            buffer += data

            # Split the buffer by newline character
            # to get individual JSON objects
            while '\n' in buffer:
                line, buffer = buffer.split('\n', 1)
                if line.strip():
                    try:
                        event = json.loads(line)
                        process_event(event)
                    except json.JSONDecodeError as e:
                        print(f"Failed to decode JSON: {e}")
        except Exception as e:
            print(f"Error receiving data from MPV: {e}")
            break


if __name__ == '__main__':
    # Parse command-line arguments
    parser = argparse.ArgumentParser(
        description='Start Flask server and MPV listener.')
    parser.add_argument(
        '-p', '--port', type=int, default=5000,
        help='Port number for Flask server')
    parser.add_argument(
        '-s', '--socket', type=str, required=True,
        help='Path to the MPV socket')
    args = parser.parse_args()

    # Run the MPV listener in a separate thread
    mpv_thread = threading.Thread(
        target=start_mpv_listener, args=(args.socket,))
    mpv_thread.daemon = True
    mpv_thread.start()

    # Start the Flask web server
    app.run(debug=False, port=args.port)
