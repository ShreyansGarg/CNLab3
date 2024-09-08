# Autonomous Drone Monitoring and Control System

This project is a sophisticated monitoring and control system for a fleet of autonomous drones. The system supports three types of communication:

1. *Control Commands:* Allows the server to send real-time control commands to the drones with minimal delay.
2. *Telemetry Data:* Drones periodically send telemetry data such as position and speed back to the server reliably and in order.
3. *File Transfers:* Drones occasionally send large files (like images or videos) to the server using a protocol that handles efficient file transfers with low latency and high reliability.

## Features

- *Tri-Mode Communication System:*
  - *Mode 1 (Control Commands):* High-performance, low-latency communication for sending commands to drones.
  - *Mode 2 (Telemetry Data):* Reliable data transmission from drones to the server for diagnostics and analysis.
  - *Mode 3 (File Transfers):* Efficient file transfer mode using QUIC (Quick UDP Internet Connections).
- *Multi-Drone Support:* Handles multiple drones communicating with the server simultaneously.
- *Security:* All messages are encrypted and decrypted using a simple XOR cipher.

## Prerequisites

- Windows Operating System
- Visual Studio or any C++ compiler that supports Winsock2.
- Git for cloning the repository.

## Getting Started

### Installation

1. Clone the repository:
   ```bash
   git clone https://github.com/yourusername/drone-control-system.git
   cd drone-control-system

2. Compile and Run Server & Drone
  ```bash
  g++ Server1.cpp -o server -lws2_32 -pthreads
  ./server
  g++ Drone.cpp -o Drone -lws2_32 -pthreads
  ./Drone <drone_name> <drone_port>
