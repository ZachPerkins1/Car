#include <iostream>
#include <cstdio>
#include <queue>
#include <string>
#include <cstring>
#include <sstream>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <opencv2/opencv.hpp>
#include <vector>
#include <jsmn.h>
#include <unistd.h>
#include <algorithm>

const char* IP = "192.168.1.244";
const int CHUNK_SIZE = 4096;
const int IMAGE_QUALITY = 25;

class Status {
private:
    std::string level;
    std::string notif;

public:
    std::string getLevel();
    std::string getNotif();

    Status(std::string l = "norm", std::string s = "Operating Normally");
};

class Logger {
private:
    Status visionStatus;
    Status controlStatus;

    pthread_mutex_t readLock;

    void appendToJSON(std::stringstream&, std::string, Status);

public:
    void setVision(Status);
    void setNotif(Status);
    std::string* getJSON();
};


void communicationsMain(Logger*);
void visionMain(Logger*);
void controlsMain(Logger*);

int bindTCPSocket(sockaddr_in* address);
void makeAddress(struct sockaddr_in*, const char* ip, int port);
std::string* recv_safe(int sock, size_t buffer_size);
void sendMotorValues(int x, int y);
void* readAndEncodeFrame(void*);
int sendCameraFrame(std::vector<uchar>& frame, int cSock, int dSock, sockaddr*, socklen_t);

struct FrameData {
    cv::VideoCapture cap;
    std::vector<uchar> data;
};

int main() {
    Logger* log = new Logger();
    std::cout << "test" << std::endl;
    visionMain(log);
}

void communicationsMain(Logger* log) {
    sockaddr_in address;
    makeAddress(&address, IP, 3002);
    int sock = bindTCPSocket(&address);
    int remote = accept(sock, NULL, NULL);

    while (true) {
        char buffer[1024];
        recv(remote, (void*)buffer, 1024, 0);
        if (strncmp(buffer, "POLL", 4) == 0) {
            std::string* json = log->getJSON();
            send(remote, json->c_str(), json->size(), 0);
            delete json;
        }
    }
}

void visionMain(Logger* log) {
    cv::VideoCapture cap(0);

    sockaddr_in address;
    makeAddress(&address, IP, 3000);
    int c_sock = bindTCPSocket(&address);
    int remote = accept(c_sock, NULL, NULL);
   
    int d_sock = socket(AF_INET, SOCK_DGRAM, 0);
    bind(d_sock, (sockaddr*)&address, sizeof(address));

    std::cout << "TEST" << std::endl;
    
    char inBuffer[1024];
    sockaddr remoteAddress;
    socklen_t remoteAddressLen = sizeof(remoteAddress);
    recvfrom(d_sock, (void*)inBuffer, 1024, 0, &remoteAddress, &remoteAddressLen);
    std::cout << "TESst" << std::endl;

    while (true) {
	std::cout << "loop" << std::endl;
        struct FrameData fData;
        fData.cap = cap;
        fData.data = std::vector<uchar>();
        readAndEncodeFrame((void*)&fData);

        recv(remote, (void*)inBuffer, 1024, 0);
        if (strncmp(inBuffer, "OKAY", 4) == 0) {
	    std::cout << "test passed" << std::endl;
            sendCameraFrame(fData.data, remote, d_sock, &remoteAddress, remoteAddressLen);
        }
    }
}

void* readAndEncodeFrame(void* in) {
    FrameData* d = (FrameData*)in;
    cv::Mat raw;
    d->cap.read(raw);

    std::vector<int> attrib;
    attrib.push_back(cv::IMWRITE_JPEG_QUALITY);
    attrib.push_back(IMAGE_QUALITY);
    imencode(".jpeg", raw, d->data, attrib);
}

int sendCameraFrame(std::vector<uchar>& frame, int cSock, int dSock, sockaddr* remoteAddress, socklen_t remoteAddressLen) {
    char sendBuffer[1024];
    int dataLen = frame.size();
    int chunkCount = (dataLen + (CHUNK_SIZE - 1)) / CHUNK_SIZE;
    sprintf(sendBuffer, "%d", chunkCount);
    send(cSock, (void*)sendBuffer, 1, 0);
    
    for (int i = 0; i < chunkCount - 1; i++) {
        sendto(dSock, &(frame.data()[i*CHUNK_SIZE]), CHUNK_SIZE, 0, remoteAddress, remoteAddressLen);
    }

    int rem = dataLen % CHUNK_SIZE;
    sendto(dSock, &(frame.data()[dataLen - rem]), rem, 0, remoteAddress, remoteAddressLen);
}

void controlsMain(Logger* log) {
    jsmn_parser parser;

    sockaddr_in address;
    makeAddress(&address, IP, 3001);
    int sock = bindTCPSocket(&address);
    int remote = accept(sock, NULL, NULL);
    
    while (true) {
        jsmn_init(&parser);
        send(remote, "POLL", 4, 0);
        char buffer[1024];
        recv(remote, (void*)buffer, sizeof(buffer), 0);
        
        jsmntok_t tokens[32];
        int toklen = jsmn_parse(&parser, buffer, strlen(buffer), tokens, 32);
        int x = 0;
        int y = 0;

        for (int i = 1; i < toklen - 1; i+=2) {
            int tokStrLen = tokens[i].end - tokens[i].start;
            char* token = &(buffer[tokens[i].start]);
            int num = 0;
            sscanf(&(buffer[tokens[i + 1].start]), "%d", &num);


            if (strncmp(token, "y", tokStrLen) == 0) {
                y = num;
            } else if (strncmp(token, "x", tokStrLen) == 0) {
                x = num;
            }
        }

        // std::cout << buffer << std::endl;
        std::cout << x << ", " << y << std::endl;
        sendMotorValues(x, y);
        usleep(10000);
    }
}

void sendMotorValues(int steering, int motors) {

}

void makeAddress(struct sockaddr_in *addr, const char* ip, int port) {
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = inet_addr(ip);
    addr->sin_port = htons(port);
}

int bindTCPSocket(struct sockaddr_in *addr) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    bind(sock, (sockaddr*)addr, sizeof(*addr));
    listen(sock, 1);
    return sock;
}

std::string* recvSafe(int sock, size_t size) {
    char buffer[size];
    int err = recv(sock, (void*)buffer, sizeof(buffer), 0);
    if (err == -1)
        return NULL;
    else if (strcmp(buffer, "") == 0)
        return NULL;
    else
        return new std::string(buffer);
}

std::string Status::getLevel() { return level; }
std::string Status::getNotif() { return notif; }

Status::Status(std::string l, std::string n) : level(l), notif(n) {}

void Logger::appendToJSON(std::stringstream& ss, std::string system, Status status) {     
    ss << "\"" << system << "\":{" << "\"msg\":\"" << status.getNotif() << "\",\"ind\":\"" + status.getLevel() << "\"}";
}

void Logger::setVision(Status s) {
    pthread_mutex_lock(&readLock);
    visionStatus = s;
    pthread_mutex_unlock(&readLock);
}

void Logger::setNotif(Status s) { 
    pthread_mutex_lock(&readLock);
    controlStatus = s; 
    pthread_mutex_unlock(&readLock);
}

std::string* Logger::getJSON() {
    std::stringstream ss;
    ss << "{";
    pthread_mutex_lock(&readLock);
    appendToJSON(ss, "vision", visionStatus);
    ss << ",";
    appendToJSON(ss, "controls", controlStatus);
    pthread_mutex_unlock(&readLock);
    ss << "}";
    return new std::string(ss.str());
}


