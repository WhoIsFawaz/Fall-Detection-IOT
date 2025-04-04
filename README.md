# IoT Fall Detection System

## Overview
This is an **IoT-based fall detection system** designed to monitor elderly individuals and alert caregivers in real-time. It combines **ESP-NOW** for local communication, **LoRa** for long-range communication, and a **Flask server** for data processing and storage. The system is built for scalability, capable of handling **up to 5 devices per block (tested prototype)** in environments like **HDB blocks** or **community care facilities**.

---

## Features
- **Real-Time Fall Detection**: Simulated via button press for now, but can be extended to use **accelerometer** and **gyroscope** sensors for real fall detection.
- **Hybrid Wireless Network**: Combines **ESP-NOW** and **LoRa** for reliable, long-range communication and scalability.
- **Real-Time Alerts**: Sends immediate alerts to caregivers with no countdown timer.
- **Scalability**: Supports up to **5 devices per block (tested prototype)**.
- **AES Encryption**: Ensures secure data transmission between devices and the server.
- **Web Interface**: Caregivers and administrators can monitor real-time data and receive alerts via a web dashboard.
- **Low Power Consumption**: Designed for energy-efficient operation with **ESP-NOW** and **LoRa**.

---

## System Architecture

1. **M5StickCPlus**: 
   - Wearable fall detection device.
   - Simulated fall detection using a button click.
   
2. **LilyGO T3-S3 (SX1280 LoRa Module)**:
   - Relay node for communication via **LoRa**.
   - Forwards data from M5StickCPlus devices to the Flask server.
   - Can act as a **middleman** relay node for scalability.

3. **Flask Server**:
   - Receives data from the **LilyGO T3-S3** node.
   - Processes data and stores it in **MongoDB**.
   - Provides real-time updates and alerts to the web interface.

4. **MongoDB**:
   - Stores **sensor data**, user information, and fall detection logs.

![image](https://github.com/user-attachments/assets/7b46d8ea-89b8-437a-bffb-c9f28ad63cf5)

---

## Setup Instructions

### Hardware Requirements:
- **M5StickCPlus**: For fall detection simulation.
- **LilyGO T3-S3** (with SX1280 LoRa Module): For data relay.
- **Wi-Fi Network**: For communication between devices and the Flask server.

### Software Requirements:
- **Arduino IDE**: For programming the M5StickCPlus and LilyGO devices.
- **Python**: For Flask backend.
- **PainlessMesh Library**: For setting up the Wi-Fi mesh network.
- **LoRa Library**: For long-range communication using SX1280.
- **MongoDB**: For storing the data on the server.

---

### Setup Steps:

1. **Install Libraries**:
   - Install the necessary libraries for **Arduino IDE**: **PainlessMesh** and **LoRa**.
   - Install **Flask** and **MongoDB** for Python.

2. **Program Devices**:
   - Program the **M5StickCPlus** for button-press fall detection.
   - Program the **LilyGO T3-S3** devices for data relay using **ESP-NOW** and **LoRa**.

3. **Set Up Flask Server**:
   - Set up a **Flask server** to receive data and store it in **MongoDB**.
   - Connect the final destination **LilyGO** to the server via **local Wi-Fi** hotspot.

4. **Run System**:
   - Connect the devices in a **Wi-Fi mesh network** using **PainlessMesh**.
   - Enable **LoRa** transmission for long-range communication between nodes.
   - The final destination device sends alerts to caregivers via the web interface.

---

## Testing

1. **Device Testing**:
   - Simulate fall events using the **button press** on the M5StickCPlus.
   - Ensure data is transmitted successfully through the mesh network and LoRa communication.

2. **Performance Testing**:
   - Measure **response time** and **latency**.
   - Test scalability for **up to 5 devices (tested prototype)** and up to 50 in real-world environments.
   - Monitor **packet loss** and **data reliability**.

---

## Future Work
- Implement **real fall detection** using the **accelerometer** and **gyroscope** on M5StickCPlus.
- Improve system scalability for larger deployments.
- Optimize **battery life** for long-term monitoring.
- Enhance **LoRa network performance** to reduce latency and packet loss.

---

## Security Considerations
- **AES Encryption** is used for securing communication between devices.
- **Wi-Fi security** is enforced using strong SSID and password for local communication.
- **Data is encrypted** before being transmitted over LoRa, ensuring secure data exchange.

---

## Conclusion
This IoT-based fall detection system offers a scalable, reliable, and secure solution for real-time monitoring of elderly individuals. The combination of **ESP-NOW**, **LoRa**, and **Flask backend** allows for a robust system that can be deployed in community care facilities or residential blocks. Future work will focus on refining the fall detection algorithm, improving scalability, and reducing latency for better real-world performance.

---

## Contributors
- Chris
- Nelson
- Fawaz
- Hakam
- Vignesh

---
