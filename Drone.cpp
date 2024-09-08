#include <iostream>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <fstream>
#include <string>
#include <chrono>
#include <sstream>

#pragma comment(lib, "ws2_32.lib")

#define CONTROL_COMMAND_PORT 8090
#define TELEMETRY_PORT 8091
#define FILE_TRANSFER_PORT 8082
#define BUFFER_SIZE 1024

std::string drone_name;
int drone_port;
bool running = true;

class Drone
{
public:
    std::string name;
    int port;
    int position;
    int speed;

    // Default constructor
    Drone() : name(""), port(0), position(0), speed(0) {}

    // Parameterized constructor
    Drone(std::string name, int port) : name(name), port(port), speed(0)
    {
        position = 0;
    }

    // Function to update drone's position and speed
    void Speed(int speed)
    {
        this->speed = speed;
    }

    // Function to get telemetry data as a string
    std::string getTelemetryData() const
    {
        return "Position: " +
               std::to_string(position) + " Speed: " +
               std::to_string(speed);
    }

    void UpdatePosition()
    {
        while (running)
        {
            position += speed;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
};

Drone drone;

void ltrim(std::string &str)
{
    // Find the first character that is not a whitespace
    size_t start = str.find_first_not_of(" \t\n\r\f\v");
    if (start != std::string::npos)
    {
        str.erase(0, start); // Erase leading whitespaces
    }
    else
    {
        str.clear(); // If the string contains only whitespaces, clear it
    }
}

void rtrim(std::string &str)
{
    // Find the last character that is not a whitespace
    size_t end = str.find_last_not_of(" \t\n\r\f\v");
    if (end != std::string::npos)
    {
        str.erase(end + 1); // Erase trailing whitespaces
    }
    else
    {
        str.clear(); // If the string contains only whitespaces, clear it
    }
}

std::string
xorEncryptDecrypt(const std::string &message, char key)
{
    std::string result = message;
    for (int i = 0; i < message.size(); i++)
    {
        result[i] = message[i] ^ key;
    }
    return result;
}

// Function to receive control commands (UDP)
void QUICFileTransferClient() {
    WSADATA wsaData;
    SOCKET client_fd;
    struct sockaddr_in serverAddr,clientAddr;
    char buffer[BUFFER_SIZE] = {0};
    const std::string fileName = "DroneData.txt";
    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed: " << WSAGetLastError() << std::endl;
        exit(EXIT_FAILURE);
    }

    // Create a TCP socket
    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    clientAddr.sin_family = AF_INET;
    clientAddr.sin_port = htons(drone_port+1); // Client port to receive commands
    clientAddr.sin_addr.s_addr = INADDR_ANY; // Listen on any available network interface

    // Bind the socket to the client address and port
    if (bind(client_fd, (struct sockaddr *)&clientAddr, sizeof(clientAddr)) == SOCKET_ERROR)
    {
        std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
        closesocket(client_fd);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    // Configure the server address
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(FILE_TRANSFER_PORT);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");  // Localhost or replace with server IP

    // Connect to the server
    if (connect(client_fd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Connection failed: " << WSAGetLastError() << std::endl;
        closesocket(client_fd);
        WSACleanup();
        exit(EXIT_FAILURE);
    }
    
    
    // Open the file to read its content
    std::ifstream file(fileName, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << fileName << std::endl;
        closesocket(client_fd);
        WSACleanup();
        return;
    }

    // Send the file in chunks
    while (!file.eof()) {
        file.read(buffer, BUFFER_SIZE);
        std::streamsize bytesRead = file.gcount();

        // Encrypt the chunk before sending
        std::string encryptedChunk = xorEncryptDecrypt(std::string(buffer, bytesRead), 'K');

        // Send the encrypted chunk
        if (send(client_fd, encryptedChunk.c_str(), encryptedChunk.size(), 0) == SOCKET_ERROR) {
            std::cerr << "Error sending file: " << WSAGetLastError() << std::endl;
            break;
        }
    }

    std::cout << "File sent successfully." << std::endl;

    // Close file and clean up
    file.close();
    closesocket(client_fd);
    WSACleanup();
}

void receiveControlCommands()
{
    std::cout << "Control Command Thread Started " << std::endl;
    SOCKET sockfd;
    char buffer[BUFFER_SIZE];
    sockaddr_in serverAddr, clientAddr;
    int addrLen = sizeof(clientAddr);

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed." << std::endl;
        exit(EXIT_FAILURE);
    }

    // Create a UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == INVALID_SOCKET)
    {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    // Configure client socket to listen on CONTROL_COMMAND_PORT
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_port = htons(drone_port); // Client port to receive commands
    clientAddr.sin_addr.s_addr = INADDR_ANY; // Listen on any available network interface

    // Bind the socket to the client address and port
    if (bind(sockfd, (struct sockaddr *)&clientAddr, sizeof(clientAddr)) == SOCKET_ERROR)
    {
        std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
        closesocket(sockfd);
        WSACleanup();
        exit(EXIT_FAILURE);
    }
    fd_set readfds;
    struct timeval timeout;
    timeout.tv_sec = 1; // 1-second timeout
    timeout.tv_usec = 0;

    while (running)
    {
        // Clear the set and add the socket to it
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);

        // Wait for data or timeout
        int activity = select(0, &readfds, NULL, NULL, &timeout);

        if (activity == SOCKET_ERROR)
        {
            std::cerr << "select error: " << WSAGetLastError() << std::endl;
            break;
        }

        // Check if there's data to receive
        if (activity > 0 && FD_ISSET(sockfd, &readfds))
        {
            // Receive control command from the server
            int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&serverAddr, &addrLen);
            if (n == SOCKET_ERROR)
            {
                std::cerr << "recvfrom failed: " << WSAGetLastError() << std::endl;
                continue;
            }
            buffer[n] = '\0'; // Null-terminate the received data

            // Decrypt the received control command
            std::string encryptedMessage(buffer);
            std::string command = xorEncryptDecrypt(encryptedMessage, 'K'); // Assuming 'K' as encryption key

            // Output the received control command
            std::cout << "Received Control Command: " << command << std::endl;
            ltrim(command); // Remove leading whitespaces
            rtrim(command); // Remove trailing whitespaces
            if (command.find("update") != std::string::npos)
            {
                std::string valueStr = command.substr(6); // Extract substring after "update"
                try
                {
                    int newSpeed = std::stoi(valueStr); // Convert to integer
                    drone.Speed(newSpeed);              // Update drone speed
                    std::cout << "Updated drone speed to: " << newSpeed << std::endl;
                }
                catch (std::invalid_argument &e)
                {
                    std::cerr << "Invalid speed value received: " << valueStr << std::endl;
                }
            }
            else if(command=="send pic"){
                std::thread fileTransferThread(QUICFileTransferClient);
                fileTransferThread.detach();
            }
            else
            {
                std::cout << "Received Weird Control Command: " << command << std::endl;
            }
        }

        // If no data and running is false, break the loop
        if (!running)
        {
            std::cout << "Control command thread exiting..." << std::endl;
            break;
        }
    }

    // Clean up after use
    closesocket(sockfd);
    WSACleanup();
}

// Function to send telemetry data (TCP)
void sendTelemetryData()
{

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed." << std::endl;
        exit(EXIT_FAILURE);
    }

    // Create a TCP socket
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET)
    {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    sockaddr_in clientAddr;
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_port = htons(drone_port); // Use the drone_port variable
    clientAddr.sin_addr.s_addr = INADDR_ANY; // Bind to any local IP address

    if (bind(clientSocket, (struct sockaddr *)&clientAddr, sizeof(clientAddr)) == SOCKET_ERROR)
    {
        std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    sockaddr_in serverAddr;

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(TELEMETRY_PORT);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        std::cerr << "Connection failed: " << WSAGetLastError() << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        exit(EXIT_FAILURE);
    }
    std::string td = "1 "+ drone_name;
    std::string ed = xorEncryptDecrypt(td, 'K');
    send(clientSocket, ed.c_str(), ed.size(), 0);
    std::cout << "Established Connection With Server: " <<td<< std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(10));

    while (running)
    {
        std::string telemetryData = drone.getTelemetryData();
        std::string encryptedData = xorEncryptDecrypt(telemetryData, 'K');
        send(clientSocket, encryptedData.c_str(), encryptedData.size(), 0);

        std::cout << "Sent Telemetry Data: " << telemetryData << std::endl;

        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
    running = false;
    closesocket(clientSocket);
    WSACleanup();

    // std::string Data = (std::string) "1 " + drone_name;
    // std::string telemetryData = xorEncryptDecrypt(Data, 'K');
    // const char *telemetryData_cstr = telemetryData.c_str();
    // // char telemetryData[BUFFER_SIZE] = "Telemetry data " + drone_id;

    // send(clientSocket, telemetryData_cstr, strlen(telemetryData_cstr), 0);

    // std::cout << "Send Sucessfully ";

    // std::cin.get();

    // running = false;

    // closesocket(clientSocket);
    // WSACleanup();
}

// Function to receive file transfers (TCP)


int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        std::cerr << "Usage: " << argv[0] << " <drone_name> <drone_port>" << std::endl;
        return 1;
    }

    drone_name = argv[1];
    drone_port = atoi(argv[2]);

    drone = Drone(drone_name, drone_port);

    std::thread updatePositionThread(&Drone::UpdatePosition, &drone);
    std::thread controlCommandThread(receiveControlCommands);
    std::thread telemetryThread(sendTelemetryData);


    // Wait for all threads to finish
    controlCommandThread.join();
    telemetryThread.join();
    updatePositionThread.join();


    return 0;
}