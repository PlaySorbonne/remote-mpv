#!/bin/python3

import argparse
import socket
import json
import time
import threading
import os
import subprocess
import shutil
from flask import Flask, request, jsonify, send_from_directory

app = Flask(__name__)

# Internal database (dictionary) to store property values
property_values = {}


def command_exists(command):
    # Check if the command exists in the system
    return shutil.which(command) is not None


def is_writable(path):
    # Check if the path is writable
    return os.access(path, os.W_OK)


def can_create_mpv_socket(filepath):
    try:
        # Try to create the file
        with open(filepath, 'w') as f:
            pass
        # If the file was created successfully, remove it
        os.remove(filepath)
        return True
    except (OSError, IOError) as e:
        return False


def process_event(event):
    if 'name' in event and 'data' in event:
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


# Route for serving index.html at the root URL
@app.route('/')
def serve_index():
    return send_from_directory(app.static_folder, 'index.html')


# Route for serving files from the app.static_folder directory
@app.route('/<path:filename>')
def serve_static(filename):
    return send_from_directory(app.static_folder, filename)


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
        time.sleep(2)
        start_mpv_listener(mpvsocketpath)
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
        '-H', '--host', type=str, default='0.0.0.0',
        help='Host IP address')
    parser.add_argument(
        '-p', '--port', type=int, default=8000,
        help='Port number for Flask server')
    parser.add_argument(
        '-s', '--socket', type=str, default="/tmp/mpvsocket",
        help='Path to the MPV socket')
    parser.add_argument(
        '-d', '--directory', type=str, default="web",
        help='Directory for serving HTML pages')
    args = parser.parse_args()

    if not command_exists("mpv"):
        print("mpv not installed")
        exit()

    if not is_writable(args.socket):
        if not can_create_mpv_socket(args.socket):
            print(f"{args.socket} is not writable")
            exit()

    # Additional arguments for mpv
    mpv_arguments = ['--profile=pseudo-gui',
                     '--input-ipc-server=' + args.socket, '--idle']

    # Command to launch mpv player witH additional arguments
    command = ['mpv'] + mpv_arguments

    # Launch the mpv player process
    subprocess.Popen(command)

    # Run the MPV listener in a separate thread
    mpv_thread = threading.Thread(
        target=start_mpv_listener, args=(args.socket,))
    mpv_thread.daemon = True
    mpv_thread.start()

    # Set directory for serving HTML pages
    args.directory = os.path.abspath(args.directory)

    # Set the static folder for Flask application
    app.static_folder = args.directory

    # Start the Flask web server
    app.run(debug=False, host=args.host, port=args.port)
