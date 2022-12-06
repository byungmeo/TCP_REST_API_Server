/* 목표
*
[소켓 관련 처리] : 동시 다발적인 요청을 처리하기 위해서는 Queue가 필요하다.
1. FD_SET을 통해 HTTP Client들을 Map에 넣고 Queue를 통해 작업들을 생성한다.
2. REST API Request를 처리하는 스레드를 여러개 만들고 Queue에 있는 작업들을 아래와 같이 처리한다.

[Request 수신 순서] : Request Packet이 쪼개져서 올 수도 있으므로, HTTP 형식을 이용하여 Packet를 수신해야 한다.
1. CRLF(\r\n)을 연속 2번 받을 때까지 offset을 이용하여 recv만 받는다
    CRLF가 나올 때마다 헤더 종류를 체크한 후 다음 헤더 값들을 잘 저장한다. (저장한 후 buffer에 덮어씌워도 된다)
        Request 종류 (이건 무조건 첫 줄)
        Content-Type
        Content-Length
3. Content-Length만큼 offset을 이용하여 recv만 받는다

[Body 파싱 후 처리 순서]
1. 만약 Content-Type이 application/json이라면, Body를 JSON으로 Parsing한다.
2. command와 userName을 알아내고 나머지 인수들을 받는다. (login command 없이 로그인 과정을 거치도록 한다)
3. command별로 기존과 동일하게 처리한다.
4. 다음과 같이 Response를 작성하고 send 한다.
    A. 만약 Request 종류나 Content-Type이 예상과 다른 경우
        에러 코드를 잘 포장해서 전송
    B. 잘 받은 경우
        포멧 텍스트 : "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: application/json\r\n\r\n%s"
        arg1 : 최종 json 문자열의 length
        arg2 : 최종 json 문자열
*/

#include <iostream>

#include "rapidjson/document.h"

#include <WinSock2.h>
#include <WS2tcpip.h>

using namespace std;

// ws2_32.lib 를 링크한다.
#pragma comment(lib, "Ws2_32.lib")


static const unsigned short REST_SERVER_PORT = 27016;
static const char* REST_SERVER_ADDRESS = "127.0.0.1";


SOCKET createPassiveSocketREST() {
    // REST API 통신용 TCP socket 을 만든다.
    SOCKET passiveSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (passiveSock == INVALID_SOCKET) {
        cerr << "socket failed with error " << WSAGetLastError() << endl;
        return 1;
    }

    // socket 을 특정 주소, 포트에 바인딩 한다.
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(REST_SERVER_PORT);
    inet_pton(AF_INET, REST_SERVER_ADDRESS, &serverAddr.sin_addr.s_addr);

    int r = bind(passiveSock, (sockaddr*)&serverAddr, sizeof(serverAddr));
    if (r == SOCKET_ERROR) {
        cerr << "bind failed with error " << WSAGetLastError() << endl;
        return 1;
    }

    // passive socket을 생성하고 반환한다.
    r = listen(passiveSock, 10);
    if (r == SOCKET_ERROR) {
        cerr << "listen faijled with error " << WSAGetLastError() << endl;
        return 1;
    }

    return passiveSock;
}

int main()
{
    char buffer[1024] = { 0, };
    // 고정된 response 패킷 (Content-Length도 고정)
    string response_packet = "HTTP/1.1 200 OK\r\nContent-Length: 8\r\nContent-Type: text/plain\r\n\r\nResponse";

    int r = 0;

    // Winsock 을 초기화한다.
    WSADATA wsaData;
    r = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (r != NO_ERROR) {
        cerr << "WSAStartup failed with error " << r << endl;
        return 1;
    }

    // passive socket 을 만들어준다.
    SOCKET passiveSock = createPassiveSocketREST();

    // 우선은 단일 request 요청만 처리해본다.
    struct sockaddr_in client_addr;
    int size_len = sizeof(client_addr);
    SOCKET client = accept(passiveSock, (sockaddr*)&client_addr, &size_len);

    while (true) {
        r = recv(client, buffer, sizeof(buffer), 0); // request를 받을 때 까지 blocking
        cout << "Received " << r << " Bytes" << endl;
        if (r > 0) {
            cout << buffer << endl;
        }

        // 우선은 모든 request에 대해서 정해진 response를 보낸다.
        r = send(client, response_packet.c_str(), response_packet.length(), 0);
        cout << "Sent " << r << " Bytes" << endl;
    }

    // 연결을 기다리는 passive socket 을 닫는다.
    r = closesocket(passiveSock);
    if (r == SOCKET_ERROR) {
        cerr << "closesocket(passive) failed with error " << WSAGetLastError() << endl;
        return 1;
    }

    // Winsock 을 정리한다.
    WSACleanup();
    return 0;
}