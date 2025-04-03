/****************************************************************************
 * CSCN73060: Project VI -- Client/Server Project
 * Server main source file.
 *
 * This server listens on a TCP port, accepts an unlimited number of client
 * connections (each in its own thread), and for each connection:
 *   - Reads the unique airplane ID transmitted by the client.
 *   - Reads telemetry data (each line includes a timestamp and fuel remaining).
 *   - Parses the telemetry data and calculates current fuel consumption.
 *   - When the client disconnects, computes the final average fuel consumption.
 ****************************************************************************/

#include <iostream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <vector>
#include <mutex>

#include "Parse.h"

#pragma comment(lib, "Ws2_32.lib")

#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma warning(disable : 4996)
using namespace std;

const unsigned short SERVER_PORT = 27000;
const int BUFFER_SIZE = 128;

// Structure to keep track of flight data for consumption calculation.
struct FlightData {
	time_t startTime;
	double startFuel;
	time_t lastTime;
	double lastFuel;
	bool firstData;
	FlightData() : startTime(0), startFuel(0.0), lastTime(0), lastFuel(0.0), firstData(true) {}
};

mutex coutMutex; // To synchronize console output

// Thread function to handle each client connection.
void handleClient(SOCKET clientSocket) {
	FlightData flight;
	char buffer[BUFFER_SIZE];
	int bytesReceived;
	string recvBuffer; // Buffer for accumulating partial data

	// First, expect a unique ID terminated by a newline.
	// We'll assume the unique ID is sent as a separate message ending with '\n'
	while (recvBuffer.find('\n') == string::npos) {
		bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
		if (bytesReceived <= 0) {
			closesocket(clientSocket);
			return;
		}
		buffer[bytesReceived] = '\0';
		recvBuffer += buffer;
	}
	// Extract the unique ID up to the newline
	size_t pos = recvBuffer.find('\n');
	string uniqueID = recvBuffer.substr(0, pos);
	// Remove the unique ID from the buffer
	recvBuffer.erase(0, pos + 1);

	{
		lock_guard<mutex> lock(coutMutex);
		cout << "Connected client, airplane ID: " << uniqueID << endl;
	}

	// Now process the rest of the telemetry data.
	while ((bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
		buffer[bytesReceived] = '\0';
		recvBuffer += buffer;

		// Process complete lines from recvBuffer.
		size_t lineEnd;
		while ((lineEnd = recvBuffer.find('\n')) != string::npos) {
			string line = recvBuffer.substr(0, lineEnd);
			recvBuffer.erase(0, lineEnd + 1);

			// Trim the line before checking if it's empty

			if (trim(line).empty())
				continue;

			TelemetryDataPoint dataPoint;
			if (!parseTelemetryData(line, dataPoint)) {
				lock_guard<mutex> lock(coutMutex);
				cout << "Failed to parse telemetry data: " << line << endl;
				continue;
			}

			// Convert the parsed timestamp (struct tm) to time_t.
			time_t currentTime = mktime(&dataPoint.timestamp);
			double currentFuel = dataPoint.fuelRemaining;

			// On first valid reading, store as the start of the flight.
			if (flight.firstData) {
				flight.startTime = currentTime;
				flight.startFuel = currentFuel;
				flight.firstData = false;
			}
			else {
				// Calculate current fuel consumption (fuel consumed per second).
				double fuelConsumed = flight.lastFuel - currentFuel;
				double timeDiff = difftime(currentTime, flight.lastTime);
				double consumptionRate = (timeDiff > 0) ? (fuelConsumed / timeDiff) : 0.0;

				lock_guard<mutex> lock(coutMutex);
				cout << "Airplane " << uniqueID
					<< " | " << asctime(&dataPoint.timestamp)
					<< " Fuel Remaining: " << currentFuel
					<< " | Current Consumption: " << consumptionRate << " fuel/sec" << endl;
			}

			// Update the last received reading.
			flight.lastTime = currentTime;
			flight.lastFuel = currentFuel;
		}
	}

	// When the connection ends, calculate the flight's average fuel consumption.
	double totalFuelConsumed = flight.startFuel - flight.lastFuel;
	double totalTime = difftime(flight.lastTime, flight.startTime);
	double averageConsumption = (totalTime > 0) ? (totalFuelConsumed / totalTime) : 0.0;

	{
		lock_guard<mutex> lock(coutMutex);
		cout << "Flight for airplane " << uniqueID
			<< " ended. Average Fuel Consumption: "
			<< averageConsumption << " fuel/sec" << endl;
	}

	closesocket(clientSocket);
}

int main() {
	// Initialize Winsock.
	WSADATA wsaData;
	int wsaErr = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (wsaErr != 0) {
		cerr << "WSAStartup failed: " << wsaErr << endl;
		return 1;
	}

	// Create the server socket.
	SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (serverSocket == INVALID_SOCKET) {
		cerr << "Failed to create socket. Error: " << WSAGetLastError() << endl;
		WSACleanup();
		return 1;
	}

	// Bind the socket.
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	serverAddr.sin_addr.s_addr = INADDR_ANY;

	if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
		cerr << "Bind failed with error: " << WSAGetLastError() << endl;
		closesocket(serverSocket);
		WSACleanup();
		return 1;
	}

	// Listen for incoming connections.
	if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
		cerr << "Listen failed with error: " << WSAGetLastError() << endl;
		closesocket(serverSocket);
		WSACleanup();
		return 1;
	}

	cout << "Server listening on port " << SERVER_PORT << endl;

	// Main loop: accept incoming connections and spawn a thread for each.
	while (true) {
		sockaddr_in clientAddr;
		int clientAddrLen = sizeof(clientAddr);
		SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientAddrLen);
		if (clientSocket == INVALID_SOCKET) {
			cerr << "Accept failed with error: " << WSAGetLastError() << endl;
			continue;
		}

		// Start a new thread for this client.
		thread clientThread(handleClient, clientSocket);
		clientThread.detach(); // Detach to allow independent processing.
	}

	// Cleanup (note: in this infinite loop, this code is not reached).
	closesocket(serverSocket);
	WSACleanup();
	return 0;
}
