/****************************************************************************
 * CSCN73060: Project VI -- Client/Server Project
 * Main client source file
 *
 * Sends a telemetry data file line-by-line over the network to a server.
 *
 * Upon starting a flight, the client shall open a telemetry file and perform
 * the following operations until EOF:
 *      a) Read a line of data that includes time and fuel remaining onboard;
 *      b) Packetize the data;
 *      c) Transmit the data to the server.
 *
 ****************************************************************************/

#include <fstream>
#include <iostream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>   // For inet_pton()
#include <sstream>
#include <cstdlib>
#include <ctime>
#pragma comment(lib, "Ws2_32.lib")

using namespace std;

const char* const SERVER_IP_ADDRESS = "127.0.0.1";
const unsigned short SERVER_PORT = 27000;
const char* const DEFAULT_FILENAME = "telemetry.txt";
const int MAX_LINE_LEN = 64;

static void exitWithError(const string& errorMsg, SOCKET* sock = nullptr) {
	cerr << "[ERROR] " << errorMsg << endl;
	if (sock && *sock != INVALID_SOCKET) {
		closesocket(*sock);
	}
	WSACleanup();
	exit(EXIT_FAILURE);
}

// Start Winsock DLLs
static void startWinsock() {
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		exitWithError("WSAStartup failed");
	}
}

static SOCKET initializeClientSocket() {
	SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (clientSocket == INVALID_SOCKET) {
		exitWithError("Socket creation failed");
	}
	return clientSocket;
}

static void connectToServer(SOCKET& clientSocket, const char* serverAddress, unsigned short serverPort) {
	sockaddr_in svrAddr = { 0 };
	svrAddr.sin_family = AF_INET;
	svrAddr.sin_port = htons(serverPort);
	if (inet_pton(AF_INET, serverAddress, &svrAddr.sin_addr) <= 0) {
		exitWithError("Invalid IP Address", &clientSocket);
	}
	if (connect(clientSocket, (struct sockaddr*)&svrAddr, sizeof(svrAddr)) == SOCKET_ERROR) {
		exitWithError("Connection to server failed", &clientSocket);
	}
}

// Send file content line-by-line
static void sendTelemetryData(SOCKET& clientSocket, const string& fileName) {
	ifstream file(fileName);
	if (!file.is_open()) {
		exitWithError("Could not open file: " + fileName, &clientSocket);
	}

	string line;
	while (getline(file, line)) {
		line += "\n";

		if (line.length() > MAX_LINE_LEN) {
			cerr << "Line is too long, skipping: " << line << endl;
			continue;
		}
		if (send(clientSocket, line.c_str(), (int)line.length(), 0) == SOCKET_ERROR) {
			file.close();
			exitWithError("Failed to send data", &clientSocket);
		}
	}
	file.close();
}

//////////////////////////////////////////////////////////////////////////////
/// Main function
//////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[]) {
	// If no arguments given, use default telemetry data filename.
	//string fileName = (argc < 2) ? DEFAULT_FILENAME : argv[1];

	// Take server IP address as a CLI argument, if one is provided
	const char* server_ip_address = (argc < 2) ? SERVER_IP_ADDRESS : argv[1];

	// Initialize random seed
	srand((unsigned int)time(NULL));

	startWinsock();
	SOCKET clientSocket = initializeClientSocket();
	connectToServer(clientSocket, server_ip_address, SERVER_PORT);

	// Generate a unique ID (for example, using a random number)
	stringstream ss;
	ss << "ClientID_" << rand();
	string uniqueID = ss.str();
	// Append a newline to clearly mark the end of the unique ID message.
	uniqueID += "\n";

	// Send the unique ID to the server
	if (send(clientSocket, uniqueID.c_str(), (int)uniqueID.length(), 0) == SOCKET_ERROR) {
		exitWithError("Failed to send unique ID", &clientSocket);
	}

	// Now send telemetry data
	sendTelemetryData(clientSocket, DEFAULT_FILENAME);

	// Clean up
	closesocket(clientSocket);
	WSACleanup();
	return 0;
}

