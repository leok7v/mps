#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

enum {
    NANOSECONDS_IN_MICROSECOND = 1000,
    NANOSECONDS_IN_MILLISECOND = NANOSECONDS_IN_MICROSECOND * 1000,
    NANOSECONDS_IN_SECOND = NANOSECONDS_IN_MILLISECOND * 1000
};

#define countof(a) (sizeof((a))/sizeof(*(a)))
typedef unsigned char byte;
#define null NULL

static uint64_t time_in_nanoseconds() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * NANOSECONDS_IN_SECOND + (uint64_t)ts.tv_nsec; 
}

static struct sockaddr_in localhost() {
    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_port = htons(5555);
    if (inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) <= 0) {	
        perror("inet_pton() failed");
        exit(1);
    }
    return address;    
}

static byte receive_byte(int s) {
    for (;;) {
        byte b = 0;
        int r = recv(s, &b, 1, 0);
        if (r < 0) { perror("recv() failed"); exit(1); }
        if (r == 1) { return b; }
    }
}

static void send_byte(int s, byte b) {
    for (;;) {
        int r = send(s, &b, 1, MSG_DONTROUTE);
        if (r < 0) { perror("send() failed"); exit(1); }
        if (r == 1) { return; }
    }
}

static void report_mps(uint64_t* last_mps_time, int* mps) {
    (*mps)++;
    uint64_t time = time_in_nanoseconds();     
    if (time > *last_mps_time + 1LL * NANOSECONDS_IN_SECOND) {
        printf("roundtrips per second=%.1f\n", *mps / 1.0);
        *mps = 0;
        *last_mps_time = time;
    }
}

static const int ON = 1;

static void server() {
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    int r = setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (char*)&ON, sizeof(ON));
    if (r != 0){ perror("setsockopt(SO_REUSEADDR) failed"); exit(1); }        
    struct sockaddr_in address = localhost();
    r = bind(listener, (const struct sockaddr*)&address, sizeof(struct sockaddr_in));
    if (r != 0) { perror("bind() failed"); exit(1); }
    r = listen(listener, 1);
    if (r != 0) { perror("listen() failed"); exit(1); }
    int s = accept(listener, null, 0);    
    if (s <= 0) { perror("accept() failed"); exit(1); }
    r = setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char*)&ON, sizeof(ON));
    if (r != 0) { perror("setsockopt(TCP_NODELAY) failed"); exit(1); }
    uint64_t last_mps_time = time_in_nanoseconds();     
    int mps = 0; // messages per second (round trip actually)    
    byte b = 0;
    for (;;) {
        send_byte(s, b);
//      printf("server sent: 0x%02X\n", b);
        b = receive_byte(s);
//      printf("server recv: 0x%02X\n", b);
        report_mps(&last_mps_time, &mps);
    }
}

static void client() {
    int s = socket(AF_INET, SOCK_STREAM, 0);;
    struct sockaddr_in address = localhost();
    int r = connect(s, (const struct sockaddr*)&address, sizeof(address));
    if (r != 0) { perror("connect() failed"); exit(1); }
    r = setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char*)&ON, sizeof(ON));
    if (r != 0) { perror("setsockopt(TCP_NODELAY) failed"); exit(1); }        
    uint64_t last_mps_time = time_in_nanoseconds();     
    int mps = 0; // messages per second (round trip actually)    
    for (;;) {
        byte b = receive_byte(s);
//      printf("client recv: 0x%02X\n", b);
        b++;
//      printf("client sent: 0x%02X\n", b);
        send_byte(s, b);
        report_mps(&last_mps_time, &mps);
    }
}

int main(int argc, char** argv) {
    if (argc > 1 && strcmp(argv[1], "server") == 0) { server(); }
    else if (argc > 1 && strcmp(argv[1], "client") == 0) { client(); }
    else { fprintf(stderr, "%s server|client\n", argv[0]); exit(1); }
    return 0;
}    