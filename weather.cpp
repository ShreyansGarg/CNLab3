#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <zlib.h>
#include <thread>
#include <chrono>
#include <sstream>
#include <random>
#include <unordered_map>
#pragma comment(lib, "Ws2_32.lib")  // Link with Ws2_32.lib
#define SERVER_PORT 5000
#define BUFFER_SIZE 1024
using namespace std;

unordered_map<int, int> ack_count;  
int last_acked_id = -1;  
int cwnd = 1;  // Congestion window (in packets)
int ssthresh = 32;  // Slow start threshold

string compress_data(const string& data) {
    int compression_level = Z_BEST_COMPRESSION; // Default to highest compression
    if (data.size() < 100) {
        // cout<<"NO compression"<<"\n";
        compression_level = Z_NO_COMPRESSION;   // No compression for small data
    } else if (data.size() < 150) {
        
        compression_level = Z_BEST_SPEED;       // Prioritize speed for medium-sized data
    }

    string compressed(BUFFER_SIZE, '\0');
    z_stream strm = {0};
    strm.total_in = strm.avail_in = data.size();
    strm.total_out = strm.avail_out = BUFFER_SIZE;
    strm.next_in = (Bytef *)data.data();
    strm.next_out = (Bytef *)compressed.data();

    if (deflateInit(&strm, compression_level) != Z_OK) {
        throw runtime_error("zlib: Failed to initialize compression.");
    }
    deflate(&strm, Z_FINISH);
    deflateEnd(&strm);

    compressed.resize(strm.total_out); 
    return compressed;
}


string generate_weather_data(int client_id) {
    stringstream ss;
    ss << "Client " << client_id << ": ";


    ss << "Temperature: " << 275 + client_id % 10 << " K, "
    << "Humidity: " << 50 + (client_id % 20) << "%, "
    << "Air Pressure: " << 1000 + (client_id % 15) << " hPa";
    

    return ss.str();
}
    // TCP Reno congestion control
void tcp_reno_congestion_control(bool ack_received) {
    if (ack_received) {
        if (cwnd < ssthresh) {
            // Slow start phase: exponential growth
            cwnd *= 2;
        } else {
            // Congestion avoidance phase: linear growth
            cwnd += 1;
        }
    } else {
        // Congestion detected: reduce cwnd and enter congestion avoidance
        ssthresh = max(cwnd / 2, 2);
        cwnd = 1;
    }
}

void send_weather_data(SOCKET server_socket, int client_id) {
    
    int packet_id = 0;  // Initialize the packet ID

    while (true) {
        bool flag=false;
        for(int i=0;i<cwnd;i++){
        

        // Generate and compress weather data
        string weather_data = generate_weather_data(client_id);
        string compressed_data = compress_data(weather_data);

            // Add packet ID and cwnd to the compressed data
        string packet_with_id_and_cwnd = to_string(packet_id) + "|" + to_string(cwnd) + "|"+to_string(ssthresh) +"|"+compressed_data;

        // Simulate network transmission based on bandwidth and congestion window
        size_t data_to_send = min(packet_with_id_and_cwnd.size(), static_cast<size_t>(cwnd * 1024));  // Simulate packet size using cwnd

        if (send(server_socket, packet_with_id_and_cwnd.c_str(), data_to_send, 0) == SOCKET_ERROR) {
            cerr << "Client " << client_id << " failed to send data." << endl;
        } else {
          
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(server_socket, &read_fds);
            
            struct timeval timeout;
            timeout.tv_sec = 1;  
            timeout.tv_usec = 0; // 0 microseconds

            int select_result = select(server_socket + 1, &read_fds, NULL, NULL, &timeout);

            if (select_result > 0 && FD_ISSET(server_socket, &read_fds)) {
                // Server response received
                char ack_buffer[10];  // Buffer for receiving ACK (with ID)
                int ack_received = recv(server_socket, ack_buffer, sizeof(ack_buffer), 0);

                if (ack_received > 0) {
                    int ack_id = stoi(string(ack_buffer, ack_received));
                    cout<<ack_id <<"  "<<packet_id<<endl;
                    // Handle ACK reception
                    if (ack_id == packet_id) {
                        // Normal ACK received
                        ack_count[ack_id] = 0;  // Reset the duplicate ACK count for this packet ID
                      
                        last_acked_id = ack_id;
                       //cout << "ACK received for packet " << ack_id << ". Increasing cwnd.\n";
                    } else if (ack_id == last_acked_id) {
                        // Duplicate ACK received
                        ack_count[ack_id] += 1;
                        cout << "Duplicate ACK received for packet " << ack_id << ". Count: " << ack_count[ack_id] << "\n";
                        if (ack_count[ack_id] == 3) {
                            // Triple duplicate ACKs: trigger congestion control
                            cerr << "Triple duplicate ACKs for packet " << ack_id << ". Reducing cwnd.\n";
                            tcp_reno_congestion_control(false);  // Trigger congestion control, reduce cwnd
                            packet_id=ack_id;
                            flag=true;
                            break;
                        }
                    }
                } else {
                    // No ACK received, simulate congestion and reduce congestion window
                    cerr << "No ACK received. Reducing cwnd.\n";

                }
            } else if (select_result == 0) {
                // Timeout occurred
                cerr << "Timeout occurred ."+to_string(packet_id+1)<<endl;
                
            } else {
                // Select failed, treat as an error
                cerr << "Error occurred during select().\n";
            //    tcp_reno_congestion_control(false);  // Handle as congestion
            }
        
        }
        // Move to the next packet
        packet_id++;


        size_t sleep_duration = static_cast<size_t>((data_to_send ) );  // Milliseconds
        this_thread::sleep_for(chrono::milliseconds(sleep_duration > 0 ? sleep_duration : 1));
    }
       

   // cout<<" cwnd:" <<cwnd<<endl;

    if(!flag)
    {
        cout<<"doubled cw"<<endl;
        tcp_reno_congestion_control(true);  // Trigger congestion control, reduce cwnd

    }
     cout<<" cwnd:" <<cwnd<<endl;
    }
}



int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <client_id> " << endl;
        return 1;
    }

    int client_id = stoi(argv[1]);


    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        cerr << "WSAStartup failed with error: " << iResult << endl;
        return 1;
    }

    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        cerr << "Socket creation error.\n";
        WSACleanup();
        return 1;
    }

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(SERVER_PORT);

    inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr);

    // Connect to the server
    if (connect(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) == SOCKET_ERROR) {
        cerr << "Connection failed.\n";
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    cout << "Client " << client_id << " connected to the server\n";

    // Send weather data continuously with network adaptation and dynamic data size
    send_weather_data(server_socket, client_id);

    // Cleanup
    closesocket(server_socket);
    WSACleanup();
    return 0;
}
