#include <winsock2.h>
#include <iostream>
#include <thread>
#include <unistd.h>
#include <cstring>
#include <string>
#include <fstream>
#include <ws2tcpip.h>
#include <map>
#include <utility>
#include <sstream>

#pragma comment(lib, "ws2_32.lib")

#define CONTROL_COMMAND_PORT 8090
#define TELEMETRY_PORT 8091
#define FILE_TRANSFER_PORT 8082
#define BUFFER_SIZE 1024

std::map<std::string, std::pair<std::string, int>> mp;

std::string xorEncryptDecrypt(const std::string &message, char key)
{
    std::string result = message;
    for (int i = 0; i < message.size(); i++)
    {
        result[i] = message[i] ^ key;
    }
    return result;
}

// Function to handle control commands (UDP)

void handleControlCommands()
{

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed." << std::endl;
        exit(EXIT_FAILURE);
    }

    // Create a UDP socket
    SOCKET udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpSocket == INVALID_SOCKET)
    {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t addrLen = sizeof(clientAddr);

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(CONTROL_COMMAND_PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket
    if (bind(udpSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
        closesocket(udpSocket);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    // Set up the client address

    while (1)
    {
        std::string drone_name;
        std::cin >> drone_name;
        std::cout << drone_name << std::endl;
        char control_command[1000];
        std::cin.getline(control_command, 1000);
        if (mp.find(drone_name) == mp.end())
        {
            std::cout << "INVALID DRONE_NAME \n";
            continue;
        }
        std::string cc = control_command;
        if (cc.find("update") != std::string::npos)
        {
            std::string valueStr = cc.substr(7); // Extract substring after "update"
            try
            {
                int newSpeed = std::stoi(valueStr); // Convert to integer
                
            }
            catch (std::invalid_argument &e)
            {
                std::cout << "INVALID COMMAND 1\n";
                continue;
            }
        }
        else if(cc==" send pic"){
            
        }
        else
        {
            std::cout << "INVALID COMMAND " <<std::endl;
            continue;
        }
        std::cout << "Sending Control Command to: " << drone_name << ' ' << mp[drone_name].second << ' ' << mp[drone_name].first << '\n';
        sockaddr_in clientAddr;
        clientAddr.sin_family = AF_INET;
        clientAddr.sin_port = htons(mp[drone_name].second);
        inet_pton(AF_INET, (mp[drone_name].first).c_str(), &clientAddr.sin_addr.s_addr);
        std::string encryptedCommand = xorEncryptDecrypt(control_command, 'K');
        const char *encryptedCommand_cstr = encryptedCommand.c_str();

        if (sendto(udpSocket, encryptedCommand_cstr, strlen(encryptedCommand_cstr), 0,
                   (struct sockaddr *)&clientAddr, sizeof(clientAddr)) == SOCKET_ERROR)
        {
            std::cerr << "Send failed: " << WSAGetLastError() << std::endl;
            closesocket(udpSocket);
            WSACleanup();
            exit(EXIT_FAILURE);
        }

        std::cout << "Control command sent successfully " << std::endl;
    }

    // Clean up
    closesocket(udpSocket);
    WSACleanup();
}

void connectClient(SOCKET new_socket, std::string clientIP, int clientPort)
{
    char buffer[BUFFER_SIZE] = {0};
    int valread;
    std::string drone_name;
    while ((valread = recv(new_socket, buffer, BUFFER_SIZE, 0)) > 0)
    {
        // Decrypt telemetry data
        std::string encryptedTelemetry(buffer, valread);
        std::string telemetry = xorEncryptDecrypt(encryptedTelemetry, 'K');
        std::cout << "Received Telemetry: " << telemetry << std::endl;
        if(telemetry[0]=='1'){
            drone_name = telemetry.substr(2);
            mp[drone_name] = {clientIP, clientPort};
            std::cout << "Connected To Drone: " << drone_name << ' ' << clientIP << ' ' << clientPort << '\n';
            continue;
        }
        std::istringstream iss(telemetry);
        std::string data1, data2;
        std::string x, y;

        // Extract components from the string
        iss >> data1 >> x >> data2 >> y;

        
        // std::cout << "Telemetry data received: " << data1 << ": " << x << ' ' << data2 << ": " << y << std::endl;

        memset(buffer, 0, BUFFER_SIZE);
    }

    mp.erase(drone_name);
    std::cout << "Connection closed by client." << std::endl;
    closesocket(new_socket);
}

// Function to handle telemetry data (TCP)
void handleTelemetry()
{
    WSADATA wsaData;
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed: " << WSAGetLastError() << std::endl;
        exit(EXIT_FAILURE);
    }

    // Create a TCP socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
    {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(TELEMETRY_PORT);

    // Bind the socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR)
    {
        std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
        closesocket(server_fd);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 5) == SOCKET_ERROR)
    {
        std::cerr << "Listen failed: " << WSAGetLastError() << std::endl;
        closesocket(server_fd);
        WSACleanup();
        exit(EXIT_FAILURE);
    }
    std::cout << "Listening...\n";
    while (true)
    {
        // Accept a new client connection

        struct sockaddr_in client_address;
        int client_addrlen = sizeof(client_address);

        if ((new_socket = accept(server_fd, (struct sockaddr *)&client_address, &client_addrlen)) == INVALID_SOCKET)
        {
            std::cerr << "Accept failed: " << WSAGetLastError() << std::endl;
            closesocket(server_fd);
            WSACleanup();
            return;
        }

        // Get client IP address and port
        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_address.sin_addr), clientIP, INET_ADDRSTRLEN);
        int clientPort = ntohs(client_address.sin_port);

        std::cout << "Found a new client, creating a new thread for it: " << clientIP << ":" << clientPort << std::endl;

        // Create a new thread to handle the client connection, pass IP and port
        std::thread clientThread(connectClient, new_socket, std::string(clientIP), clientPort);
        clientThread.detach(); // Detach the thread to let it run independently
    }

    closesocket(server_fd);
    WSACleanup();
}

void fileRecieve(SOCKET new_socket, std::string clientIP, int clientPort){
    char buffer[BUFFER_SIZE] = {0};
    std::string filename ="";

    std::cout<<clientIP<<' '<<clientPort<<'\n';
    for(auto x:mp){
        if(((x.second).first == clientIP)&&(((x.second).second + 1)== clientPort)){
            filename = x.first + ".txt";
            break;
        }
    }

    if(filename==""){
        std::cout<<"Drone sending file is not Active\n";
        closesocket(new_socket);
        return;
    }
    std::cout<<filename<<'\n';
    std::ofstream file(filename, std::ios::out | std::ios::binary);

    // Read file in chunks
    int valread;
    while ((valread = recv(new_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        std::string encryptedChunk(buffer, valread);
        std::string chunk = xorEncryptDecrypt(encryptedChunk, 'K');
        file.write(chunk.c_str(), chunk.size());
    }

    file.close();
    std::cout << "File received successfully." << std::endl;

    closesocket(new_socket);
}

// Function to handle file transfers (TCP)
void handleQuicFileTransfer() {
    WSADATA wsaData;
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed: " << WSAGetLastError() << std::endl;
        exit(EXIT_FAILURE);
    }

    // Create a TCP socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(FILE_TRANSFER_PORT);

    // Bind the socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR) {
        std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
        closesocket(server_fd);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) == SOCKET_ERROR) {
        std::cerr << "Listen failed: " << WSAGetLastError() << std::endl;
        closesocket(server_fd);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    while (true) {
        struct sockaddr_in client_address;
        int client_addrlen = sizeof(client_address);

        if ((new_socket = accept(server_fd, (struct sockaddr *)&client_address, &client_addrlen)) == INVALID_SOCKET) {
            std::cerr << "Accept failed: " << WSAGetLastError() << std::endl;
            closesocket(server_fd);
            WSACleanup();
            return;
        }


        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_address.sin_addr), clientIP, INET_ADDRSTRLEN);
        int clientPort = ntohs(client_address.sin_port);

        std::thread fileRecieveThread(fileRecieve, new_socket, std::string(clientIP), clientPort);
        fileRecieveThread.detach();
        
    }

    closesocket(server_fd);
    WSACleanup();
}


int main()
{
    // Start threads for each mode of communication
    std::thread controlCommandThread(handleControlCommands);
    std::thread telemetryThread(handleTelemetry);
    std::thread QUICfileTransferThread(handleQuicFileTransfer);

    // Wait for all threads to finish
    controlCommandThread.join();
    telemetryThread.join();
    QUICfileTransferThread.join();

    return 0;
}