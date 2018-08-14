include <iostream>
#include <queue>
#include <string>
#include <pthread.h>
#include <sys/socket.h>
#include <string.h>
#include <sstream>

class Status {
private:
    std::string level;
    std::string notif;

public
    std::string getLevel();
    std::string getNotif();

    Status(std::string l, std::string n);
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
void makeAddress(struct sockaddr_in*, char* ip, int port);


class Status {
private:
    std::string level;
    std::string notif;
public:
    std::string getLevel() { return level; };
    std::string getNotif() { return notif; };

    Status(std::string l, std::string n) : level(l), notif(n) {};
};

void Logger::appendToJSON(std::stringstream& ss, std::string system, Status status) {     
    ss << "tset";//"\"" << system << "\":{" << "\"msg\":\"" << status.getNotif() << "\",\"ind\":\"" + status.getLevel() << "\"}";
}

    void Logger::setVision(Status s) {
        pthread_mutex_lock(&readLock);
        visionStatus = s;
        pthread_mutex_unlock(&readLock);
    }
    
    void setNotif(Status s) { 
        pthread_mutex_lock(&readLock);
        controlStatus = s; 
        pthread_mutex_unlock(&readLock);
    }

    std::string* getJSON() {
        std::stringstream ss;
        ss << "{";
        pthread_mutex_lock(&readLock);
        appendToJSON(ss, "vision", visionStatus);
        appendToJSON(ss, "controls", controlStatus);
        pthread_mutex_unlock(&readLock);
        ss << "}";
        return new std::string(ss.str());
    }
};

int main() {

}

void communicationsMain(Logger* log) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr address;
    makeAddress(address, "192.168.1.244", 3000);
    
    bind(sock, address, sizeof(address));
    listen(sock, 1);
    int remote;
    accept(&remote, NULL, NULL);

    while (true) {
        char buffer[1024];
        recv(remote, (void*)buffer, 1024, NULL);
        if (strcmp(buffer, "POLL") == 0) {
            
        }
    }
}


void visionMain(Logger* log) {
    
}

void controlsMain(Logger* log) {

}

void makeAddress(struct sockaddr_in *addr, char* ip, int port) {
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = inet_addr(ip);
    addr->sin_port = htons(port);
}


