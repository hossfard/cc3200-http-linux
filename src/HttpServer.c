#include "HttpServer.h"
#include "Router.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <simplelink.h>
#include <osi.h>
#include <common.h>


// Application specific status/error codes
typedef enum{
    // Choosing -0x7D0 to avoid overlap w/ host-driver's error codes
    SOCKET_CREATE_ERROR = -0x7D0,
    BIND_ERROR = SOCKET_CREATE_ERROR - 1,
    LISTEN_ERROR = BIND_ERROR -1,
    SOCKET_OPT_ERROR = LISTEN_ERROR -1,
    CONNECT_ERROR = SOCKET_OPT_ERROR -1,
    ACCEPT_ERROR = CONNECT_ERROR - 1,
    SEND_ERROR = ACCEPT_ERROR -1,
    RECV_ERROR = SEND_ERROR -1,
    SOCKET_CLOSE_ERROR = RECV_ERROR -1,
    DEVICE_NOT_IN_STATION_MODE = SOCKET_CLOSE_ERROR - 1,
    STATUS_CODE_MAX = -0xBB8
}e_AppStatusCodes;


/*! Create a TCP socket and return its id
 *
 * \param port
 * \return TODO
 *
 * TODO: error handling
 */
int createTcpSocket(int port){
  SlSockAddrIn_t sLocalAddr;

  // filling the TCP server socket address
  sLocalAddr.sin_family = SL_AF_INET;
  sLocalAddr.sin_port = sl_Htons(port);
  sLocalAddr.sin_addr.s_addr = 0;

  // Create a TCP socket
  int socketId = sl_Socket(SL_AF_INET, SL_SOCK_STREAM, 0);
  if( socketId < 0 ){
    // error
    ASSERT_ON_ERROR(SOCKET_CREATE_ERROR);
  }

  // Bind the TCP socket to the TCP server address
  int iStatus = sl_Bind(socketId, (SlSockAddr_t*)&sLocalAddr, sizeof(SlSockAddrIn_t));
  if (iStatus < 0){
    sl_Close(socketId);
    ASSERT_ON_ERROR(BIND_ERROR);
  }

  // Put the socket for listening to the incoming TCP connection
  iStatus = sl_Listen(socketId, 0);
  if (iStatus < 0){
    sl_Close(socketId);
    ASSERT_ON_ERROR(LISTEN_ERROR);
  }

  // Make the socket non-blocking
  long lNonBlocking = 1;
  iStatus = sl_SetSockOpt(socketId, SL_SOL_SOCKET, SL_SO_NONBLOCKING,
                          &lNonBlocking, sizeof(lNonBlocking));
  if (iStatus < 0){
    sl_Close(socketId);
    ASSERT_ON_ERROR(SOCKET_OPT_ERROR);
  }
  return socketId;
}


long httpServer(unsigned short port){
  int iAddrSize = sizeof(SlSockAddrIn_t);
  int newSocketId = SL_EAGAIN;  // try again
  int socketId = createTcpSocket(port);
  int iStatus = 0;

  // putting the buffer on the stack can cause stack overflow
  /* TODO: maximum buffer length is hardcoded */
  int const BUFFER_LENGTH = 1000;
  char *buffer = malloc(sizeof(char)*BUFFER_LENGTH);

  // Listen for incoming connections
  while (true){
    /* Handle requests */
    newSocketId = SL_EAGAIN;

    SlSockAddrIn_t sAddr;
    // Waiting for an incoming TCP connection
    while (newSocketId < 0){
      // Accept a connection form a TCP client, if there is any
      // otherwise returns SL_EAGAIN (try again)
      newSocketId = sl_Accept(socketId, (struct SlSockAddr_t *)&sAddr,
                              (SlSocklen_t*)&iAddrSize);
      if (newSocketId == SL_EAGAIN ){
        osi_Sleep(1);
      }
      else if (newSocketId < 0){
        // error
        sl_Close(newSocketId);
        sl_Close(socketId);
        ASSERT_ON_ERROR(ACCEPT_ERROR);
      }
    }

    Report("Connected device\n");
    Report("addr: = %u\n", sAddr.sin_addr.s_addr);
    Report("port: = %u\n", sAddr.sin_port);

    memset(buffer, '\0', BUFFER_LENGTH);
    iStatus = sl_Recv(newSocketId, buffer, BUFFER_LENGTH, 0);
    int attemptNumber = 2;
    // May get 'Try Again' when socket is in non-blocking and using
    // scheduler
    while (iStatus == SL_EAGAIN){
      iStatus = sl_Recv(newSocketId, buffer, BUFFER_LENGTH, 0);
      ++attemptNumber;
      if (attemptNumber > 5){
        Report("More than 5 sl_Recv attempts. Giving up.\n");
        ASSERT_ON_ERROR(RECV_ERROR);
        break;
      }
    }

    Report("Received buffer = %s\n", buffer);

    /* error */
    if (iStatus <= 0){
      Report("Receive Error [%d]", iStatus);
      char packetData[] = "HTTP/1.1 500 OK\r\nContent-Length: 25\r\nContent-Type: "
        "text/plain\r\nConnection: Closed\r\n\r\n{\"error\":\"receive error\"}";
      iStatus = sl_Send(newSocketId, packetData, strlen(packetData), 0);
      if (iStatus < 0){
        // error
        ASSERT_ON_ERROR(SEND_ERROR);
      }
    }
    else{
      routerHandleRequest(buffer, newSocketId);
    }
    iStatus = sl_Close(newSocketId);
    ASSERT_ON_ERROR(iStatus);
  }

  free(buffer);

  // close the connected socket after receiving from connected TCP client
  Report("Closing sockets\n");

  ASSERT_ON_ERROR(iStatus);
  // close the listening socket
  iStatus = sl_Close(socketId);
  ASSERT_ON_ERROR(iStatus);

  Report("end server\n");
  return SUCCESS;
}
