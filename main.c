/* Basic HTTP server accepting basic GET/POST requests
 *
 * This is heavily based off the `CC3200SDK/example/tcp_server` example
 *
 */

/*
 *
 *! \addtogroup tcp_socket
 *! @{
 *
 */

// Standard includes
#include <stdlib.h>
#include <string.h>

// simplelink includes
#include <simplelink.h>
#include <wlan.h>

// driverlib includes
#include <hw_ints.h>
#include <hw_types.h>
#include <hw_memmap.h>
#include <hw_common_reg.h>
#include <rom.h>
#include <rom_map.h>
#include <interrupt.h>
#include <prcm.h>
#include <uart.h>
#include <utils.h>

// common interface includes
#include <udma_if.h>
#include <common.h>
#ifndef NOTERM
#include <uart_if.h>
#endif

#include <osi.h>

#include <gpio_if.h>
#include <stdbool.h>
#include "HttpRequest.h"
#include "Router.h"
#include "pinmux.h"
#include <jsmn.h>

#define BUF_SIZE            1400
#define TCP_PACKET_COUNT    1000

#define AP_SSID_LEN_MAX         (33)

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


/*
 * GLOBAL VARIABLES -- Start
 */
volatile unsigned long  g_ulStatus = 0;     //SimpleLink Status
unsigned long  g_ulGatewayIP = 0; //Network Gateway IP address
unsigned char  g_ucConnectionSSID[SSID_LEN_MAX+1]; //Connection SSID
unsigned char  g_ucConnectionBSSID[BSSID_LEN_MAX]; //Connection BSSID
volatile unsigned long  g_ulPacketCount = TCP_PACKET_COUNT;
unsigned char  g_ucConnectionStatus = 0;
unsigned char  g_ucSimplelinkstarted = 0;
unsigned long  g_ulIpAddr = 0;
char g_cBsdBuf[BUF_SIZE];

OsiTaskHandle g_deviceInitTaskHandle = NULL;
OsiTaskHandle g_apModeTaskHandle = NULL;

#if defined(ccs) || defined (gcc)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif

/*
 * GLOBAL VARIABLES -- End
 */

void boardInit(void);
void initializeAppVariables();
long wlanConnect(signed char* ssid, signed char* key);
long configureSimpleLinkToDefaultState();
long httpServer(unsigned short port);
long apMode(char *ssid);
void deviceInitTask();
void apModeTask();
void httpServerTask();


int main(){
  long lRetVal = -1;

  // Board Initialization
  boardInit();

  // Initialize the uDMA
  UDMAInit();

  // Configure the pinmux settings for the peripherals exercised
  PinMuxConfig();

  // Configuring UART
  InitTerm();

  initializeAppVariables();

  lRetVal = VStartSimpleLinkSpawnTask(SPAWN_TASK_PRIORITY);

  int const OSI_STACK_SIZE = 2048;

  osi_TaskCreate(deviceInitTask, (signed char*)"device-init",
                 OSI_STACK_SIZE, NULL, 99, &g_deviceInitTaskHandle);

  Report("\n> deviceHandle %d \n\r", g_deviceInitTaskHandle);
  osi_TaskCreate(apModeTask, (signed char*)"ap-mode",
                 OSI_STACK_SIZE, NULL, 1, &g_apModeTaskHandle);

  osi_TaskCreate(httpServerTask, (signed char*)"httpServerTask",
                 OSI_STACK_SIZE, NULL, 1, NULL);

  osi_start();

  // power of the Network processor
  lRetVal = sl_Stop(SL_STOP_TIMEOUT);

  return 0;
}


void deviceInitTask(){
  UART_PRINT("Device Init \n\r");

  /*
   * Following function configure the device to default state by cleaning
   * the persistent settings stored in NVMEM (viz. connection profiles &
   * policies, power policy etc)
   *
   * Applications may choose to skip this step if the developer is sure
   * that the device is in its default state at start of applicaton
   *
   * Note that all profiles and persistent settings that were done on the
   * device will be lost
   *
   */
  int lRetVal = configureSimpleLinkToDefaultState();

  if (lRetVal < 0){
    if (DEVICE_NOT_IN_STATION_MODE == lRetVal){
      UART_PRINT("Failed to configure the device in its default state \n\r");
    }
    LOOP_FOREVER();
  }
  UART_PRINT("Device is configured in default state \n\r");

  OsiTaskHandle ptr = g_deviceInitTaskHandle;
  g_deviceInitTaskHandle = NULL;
  osi_TaskDelete(&ptr);
}


/*! Start AP mode and broadcast SSID
 *
 * This task ends itself upon execution
 */
void apModeTask(){
  // Wait for the device to be intialized
  while (g_deviceInitTaskHandle != NULL){
    osi_Sleep(1000);
  }

  int lRetVal = sl_Start(0, 0, 0);

  if (lRetVal < 0){
    UART_PRINT("Failed to start the device \n\r");
    LOOP_FOREVER();
  }

  // Start AP mode
  apMode("LPSystems");

  // Remove task from scheduler
  OsiTaskHandle ptr = g_apModeTaskHandle;
  g_apModeTaskHandle = NULL;
  osi_TaskDelete(&ptr);
}


/*! Start HTTP server
 *
 */
void httpServerTask(){
  // Wait for AP mode to start
  while (g_apModeTaskHandle != NULL){
    osi_Sleep(1000);
  }

  // Start httpServer infinite listen loop
  httpServer(5001);
}


/*
 * \brief The Function Handles WLAN Events
 *
 * \param[in]  pWlanEvent - Pointer to WLAN Event Info
 *
 * \return None
 *
 */
void SimpleLinkWlanEventHandler(SlWlanEvent_t *pWlanEvent){
  Report("WLANEventHandler \n");

  switch(pWlanEvent->Event){
  case SL_WLAN_CONNECT_EVENT:{
      SET_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);

      //
      // Information about the connected AP (like name, MAC etc) will be
      // available in 'slWlanConnectAsyncResponse_t'-Applications
      // can use it if required
      //
      //  slWlanConnectAsyncResponse_t *pEventData = NULL;
      // pEventData = &pWlanEvent->EventData.STAandP2PModeWlanConnected;
      //
      //
      break;
    }
  case SL_WLAN_DISCONNECT_EVENT:
    {
      slWlanConnectAsyncResponse_t*  pEventData = NULL;

      CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);
      CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_AQUIRED);

      pEventData = &pWlanEvent->EventData.STAandP2PModeDisconnected;

      // If the user has initiated 'Disconnect' request,
      //'reason_code' is SL_WLAN_DISCONNECT_USER_INITIATED_DISCONNECTION
      if(SL_WLAN_DISCONNECT_USER_INITIATED_DISCONNECTION == pEventData->reason_code){
        Report("Device disconnected from the AP on application's "
                   "request \n\r");
      }
      else{
        Report("Device disconnected from the AP on an ERROR..!! \n\r");
      }
      break;
    }
  case SL_WLAN_STA_CONNECTED_EVENT:{
      // when device is in AP mode and any client connects to device cc3xxx
      SET_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);
      Report("CONNECTED EVENT \n\r");

      //
      // Information about the connected client (like SSID, MAC etc) will be
      // available in 'slPeerInfoAsyncResponse_t' - Applications
      // can use it if required
      //
      // slPeerInfoAsyncResponse_t *pEventData = NULL;
      // pEventData = &pSlWlanEvent->EventData.APModeStaConnected;
      //
      break;
    }
  case SL_WLAN_STA_DISCONNECTED_EVENT:{
    // when client disconnects from device (AP)
    CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);
    CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_LEASED);

    //
    // Information about the connected client (like SSID, MAC etc) will
    // be available in 'slPeerInfoAsyncResponse_t' - Applications
    // can use it if required
    //
    // slPeerInfoAsyncResponse_t *pEventData = NULL;
    // pEventData = &pSlWlanEvent->EventData.APModestaDisconnected;
    //
    break;
    }
  default:{
      Report("[WLAN EVENT] Unexpected event \n\r");
      break;
    }
  }
}


/*! This function handles network events such as IP acquisition, IP
 *           leased, IP released etc.
 *
 * \param[in]  pNetAppEvent - Pointer to NetApp Event Info
 *
 * \return None
 *
 */
void SimpleLinkNetAppEventHandler(SlNetAppEvent_t *pNetAppEvent){
  Report("simpleLinkNetAppEventHandler \n");

  if(!pNetAppEvent){
    return;
  }

  switch(pNetAppEvent->Event){
    case SL_NETAPP_IPV4_IPACQUIRED_EVENT:{
      SlIpV4AcquiredAsync_t *pEventData = NULL;
      Report("slNetAppEventHandler: ip acquaired \n");
      SET_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_AQUIRED);

      //Ip Acquired Event Data
      pEventData = &pNetAppEvent->EventData.ipAcquiredV4;
      g_ulIpAddr = pEventData->ip;

      //Gateway IP address
      g_ulGatewayIP = pEventData->gateway;

      Report("[NETAPP EVENT] IP Acquired: IP=%d.%d.%d.%d , "
                 "Gateway=%d.%d.%d.%d\n\r",
                 SL_IPV4_BYTE(g_ulIpAddr,3),
                 SL_IPV4_BYTE(g_ulIpAddr,2),
                 SL_IPV4_BYTE(g_ulIpAddr,1),
                 SL_IPV4_BYTE(g_ulIpAddr,0),
                 SL_IPV4_BYTE(g_ulGatewayIP,3),
                 SL_IPV4_BYTE(g_ulGatewayIP,2),
                 SL_IPV4_BYTE(g_ulGatewayIP,1),
                 SL_IPV4_BYTE(g_ulGatewayIP,0));
      break;
    }
    case SL_NETAPP_IP_LEASED_EVENT:{
      SET_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_LEASED);
      break;
    }
    case SL_NETAPP_IPV6_IPACQUIRED_EVENT:{
      SET_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_AQUIRED);
      break;
    }
    default:{
      Report("[NETAPP EVENT] Unexpected event [0x%x] \n\r",
                 pNetAppEvent->Event);
      break;
    }
  }
}


/*! This function handles General Events
 *
 * \param[in]     pDevEvent - Pointer to General Event Info
 *
 * \return None
 */
void SimpleLinkGeneralEventHandler(SlDeviceEvent_t *pDevEvent){
  Report("SimpleLinkGeneralEventHandler\r\n");
  if(!pDevEvent){
    return;
  }

  //
  // Most of the general errors are not FATAL are are to be handled
  // appropriately by the application
  //
  Report("[GENERAL EVENT] - ID=[%d] Sender=[%d]\n\n",
             pDevEvent->EventData.deviceEvent.status,
             pDevEvent->EventData.deviceEvent.sender);
}


//*****************************************************************************
// SimpleLink Asynchronous Event Handlers -- End
//*****************************************************************************



/* Initialize application variables
 *
 * \param[in]  None
 *
 * \return None
 *
 */
void initializeAppVariables(){
  g_ulStatus = 0;
  g_ulGatewayIP = 0;
  memset(g_ucConnectionSSID,0,sizeof(g_ucConnectionSSID));
  memset(g_ucConnectionBSSID,0,sizeof(g_ucConnectionBSSID));
  g_ulPacketCount = TCP_PACKET_COUNT;
}


/* Put the device in its default state
 *
 *   - Set the mode to STATION
 *   - Configures connection policy to Auto and AutoSmartConfig
 *   - Deletes all the stored profiles
 *   - Enables DHCP
 *   - Disables Scan policy
 *   - Sets Tx power to maximum
 *   - Sets power policy to normal
 *   - Unregister mDNS services
 *   - Remove all filters
 *
 * \param   none
 * \return  On success, zero is returned. On error, negative is returned
 */
long configureSimpleLinkToDefaultState(){
  SlVersionFull   ver = {0};
  _WlanRxFilterOperationCommandBuff_t  RxFilterIdMask = {0};

  unsigned char ucVal = 1;
  unsigned char ucConfigOpt = 0;
  unsigned char ucConfigLen = 0;
  unsigned char ucPower = 0;

  long lRetVal = -1;
  long lMode = -1;

  lMode = sl_Start(0, 0, 0);
  ASSERT_ON_ERROR(lMode);

  // If the device is not in station-mode, try configuring it in station-mode
  if (ROLE_STA != lMode)
    {
      Report("ROLE_STA != lmode \r\n");
      if (ROLE_AP == lMode)
        {
          Report("ROLE_AP == lmode \r\n");
          // If the device is in AP mode, we need to wait for this event
          // before doing anything
          while(!IS_IP_ACQUIRED(g_ulStatus))
            {
#ifndef SL_PLATFORM_MULTI_THREADED
              _SlNonOsMainLoopTask();
#endif
            }
        }

      // Switch to STA role and restart
      lRetVal = sl_WlanSetMode(ROLE_STA);
      ASSERT_ON_ERROR(lRetVal);

      lRetVal = sl_Stop(0xFF);
      ASSERT_ON_ERROR(lRetVal);

      lRetVal = sl_Start(0, 0, 0);
      ASSERT_ON_ERROR(lRetVal);

      // Check if the device is in station again
      if (ROLE_STA != lRetVal)
        {
          // We don't want to proceed if the device is not coming up in STA-mode
          return DEVICE_NOT_IN_STATION_MODE;
        }
    }

  // Get the device's version-information
  ucConfigOpt = SL_DEVICE_GENERAL_VERSION;
  ucConfigLen = sizeof(ver);
  lRetVal = sl_DevGet(SL_DEVICE_GENERAL_CONFIGURATION, &ucConfigOpt,
                      &ucConfigLen, (unsigned char *)(&ver));
  ASSERT_ON_ERROR(lRetVal);

  Report("Host Driver Version: %s\n\r",SL_DRIVER_VERSION);
  Report("Build Version %d.%d.%d.%d.31.%d.%d.%d.%d.%d.%d.%d.%d\n\r",
         ver.NwpVersion[0],ver.NwpVersion[1],ver.NwpVersion[2],ver.NwpVersion[3],
         ver.ChipFwAndPhyVersion.FwVersion[0],ver.ChipFwAndPhyVersion.FwVersion[1],
         ver.ChipFwAndPhyVersion.FwVersion[2],ver.ChipFwAndPhyVersion.FwVersion[3],
         ver.ChipFwAndPhyVersion.PhyVersion[0],ver.ChipFwAndPhyVersion.PhyVersion[1],
         ver.ChipFwAndPhyVersion.PhyVersion[2],ver.ChipFwAndPhyVersion.PhyVersion[3]);

  // Set connection policy to Auto + SmartConfig
  //      (Device's default connection policy)
  lRetVal = sl_WlanPolicySet(SL_POLICY_CONNECTION,
                             SL_CONNECTION_POLICY(1, 0, 0, 0, 1), NULL, 0);
  ASSERT_ON_ERROR(lRetVal);

  // Remove all profiles
  lRetVal = sl_WlanProfileDel(0xFF);
  ASSERT_ON_ERROR(lRetVal);

  //
  // Device in station-mode. Disconnect previous connection if any
  // The function returns 0 if 'Disconnected done', negative number if already
  // disconnected Wait for 'disconnection' event if 0 is returned, Ignore
  // other return-codes
  //
  lRetVal = sl_WlanDisconnect();
  if(0 == lRetVal)
    {
      // Wait
      while(IS_CONNECTED(g_ulStatus))
        {
#ifndef SL_PLATFORM_MULTI_THREADED
          _SlNonOsMainLoopTask();
#endif
        }
    }

  // Enable DHCP client
  lRetVal = sl_NetCfgSet(SL_IPV4_STA_P2P_CL_DHCP_ENABLE,1,1,&ucVal);
  ASSERT_ON_ERROR(lRetVal);

  // Disable scan
  ucConfigOpt = SL_SCAN_POLICY(0);
  lRetVal = sl_WlanPolicySet(SL_POLICY_SCAN , ucConfigOpt, NULL, 0);
  ASSERT_ON_ERROR(lRetVal);

  // Set Tx power level for station mode
  // Number between 0-15, as dB offset from max power - 0 will set max power
  ucPower = 0;
  lRetVal = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID,
                       WLAN_GENERAL_PARAM_OPT_STA_TX_POWER, 1, (unsigned char *)&ucPower);
  ASSERT_ON_ERROR(lRetVal);

  // Set PM policy to normal
  lRetVal = sl_WlanPolicySet(SL_POLICY_PM , SL_NORMAL_POLICY, NULL, 0);
  ASSERT_ON_ERROR(lRetVal);

  // Unregister mDNS services
  lRetVal = sl_NetAppMDNSUnRegisterService(0, 0);
  ASSERT_ON_ERROR(lRetVal);

  // Remove  all 64 filters (8*8)
  memset(RxFilterIdMask.FilterIdMask, 0xFF, 8);
  lRetVal = sl_WlanRxFilterSet(SL_REMOVE_RX_FILTER, (_u8 *)&RxFilterIdMask,
                               sizeof(_WlanRxFilterOperationCommandBuff_t));
  ASSERT_ON_ERROR(lRetVal);

  lRetVal = sl_Stop(SL_STOP_TIMEOUT);
  ASSERT_ON_ERROR(lRetVal);

  initializeAppVariables();

  return lRetVal; // Success
}

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

    /* TODO: maximum buffer length is hardcoded */
    memset(buffer, '\0', BUFFER_LENGTH);
    iStatus = sl_Recv(newSocketId, buffer, BUFFER_LENGTH, 0);
    Report("Received buffer = %s\n", buffer);

    /* error */
    if (iStatus <= 0){
      Report("Receive Error");
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


/*! Connect to AP
 *
 * \param ssid SSID of the access point
 * \param key security key of AP. Leave empty if open
 * \return status value
 * \warning If the WLAN connection fails or we don't aquire an IP
 *          address, It will be stuck in this function forever.
 */
long wlanConnect(signed char* ssid, signed char* key){
  Report("Connecting to AP: %s ...\r\n", ssid);

  SlSecParams_t secParams = {0};
  long lRetVal = 0;

  secParams.Key = key;
  secParams.KeyLen = strlen((char*)key);
  if (secParams.KeyLen > 0){
    secParams.Type = SL_SEC_TYPE_WPA;
  }
  else{
    secParams.Type = SL_SEC_TYPE_OPEN;
  }

  lRetVal = sl_WlanConnect(ssid, strlen((char*)ssid), 0, &secParams, 0);
  ASSERT_ON_ERROR(lRetVal);

  // Wait for connection
  while ((!IS_CONNECTED(g_ulStatus)) || (!IS_IP_ACQUIRED(g_ulStatus))){
    // Wait for WLAN Event
#ifndef SL_PLATFORM_MULTI_THREADED
    _SlNonOsMainLoopTask();
#endif
  }

  if (lRetVal < 0){
    Report("Connection to AP failed \n\r");
  }
  else{
    Report("Connected to AP: %s \n\r", ssid);
    // IPV4
    Report("Device IP: %d.%d.%d.%d\n\r\n\r",
           SL_IPV4_BYTE(g_ulIpAddr, 3),
           SL_IPV4_BYTE(g_ulIpAddr, 2),
           SL_IPV4_BYTE(g_ulIpAddr, 1),
           SL_IPV4_BYTE(g_ulIpAddr, 0));
  }

  return SUCCESS;
}


/*! Board Initialization & Configuration
 *
 *\param  None
 *
 *\return None
 */
void boardInit(void){
  /* In case of TI-RTOS vector table is initialize by OS itself */
#ifndef USE_TIRTOS
  //
  // Set vector table base
  //
#if defined(ccs) || defined (gcc)
  MAP_IntVTableBaseSet((unsigned long)&g_pfnVectors[0]);
#endif
#if defined(ewarm)
  MAP_IntVTableBaseSet((unsigned long)&__vector_table);
#endif
#endif
  //
  // Enable Processor
  //
  MAP_IntMasterEnable();
  MAP_IntEnable(FAULT_SYSTICK);

  PRCMCC3200MCUInit();
}


long startAP(char * ssid){
  long lRetVal = sl_WlanSetMode(ROLE_AP);
  ASSERT_ON_ERROR(lRetVal);

  lRetVal = sl_WlanSet(SL_WLAN_CFG_AP_ID, WLAN_AP_OPT_SSID, strlen(ssid),
                       (unsigned char*)ssid);
  ASSERT_ON_ERROR(lRetVal);

  UART_PRINT("Device is configured in AP mode\n\r");
  /* Restart Network processor */
  lRetVal = sl_Stop(SL_STOP_TIMEOUT);

  // reset status bits
  CLR_STATUS_BIT_ALL(g_ulStatus);

  return sl_Start(NULL,NULL,NULL);
}


long apMode(char *ssid){
  long lRetVal = -1;

  //default is STA mode
  unsigned int g_uiDeviceModeConfig = ROLE_AP;
  unsigned int const MAX_SSID_LENGTH = 32;
  char ucAPSSID[MAX_SSID_LENGTH];
  unsigned short len, config_opt;

  if(lRetVal != ROLE_AP && g_uiDeviceModeConfig == ROLE_AP ){
    lRetVal = startAP(ssid);
    if (lRetVal == ROLE_AP){
      memset(ucAPSSID, '\0', AP_SSID_LEN_MAX);
      len = AP_SSID_LEN_MAX;
      config_opt = WLAN_AP_OPT_SSID;
      int ret = sl_WlanGet(SL_WLAN_CFG_AP_ID, &config_opt , &len,
                           (unsigned char*) ucAPSSID);
      ASSERT_ON_ERROR(ret);
      Report("\n\rDevice is in AP Mode [%s] \n\r", ucAPSSID);
    }
    else{
      ASSERT_ON_ERROR(lRetVal);
    }
  }

  //No Mode Change Required
  if (lRetVal == ROLE_AP){
    // waiting for the AP to acquire IP address from Internal DHCP Server

    ASSERT_ON_ERROR(lRetVal);

    unsigned char str2[] = "lpsystem.net";
    unsigned char len2 = strlen((const char *)str2);
    sl_NetAppSet(SL_NET_APP_DEVICE_CONFIG_ID, NETAPP_SET_GET_DEV_CONF_OPT_DOMAIN_NAME,
                 len2, (unsigned char*)str2);

    while (!IS_IP_ACQUIRED(g_ulStatus)){
#ifndef SL_PLATFORM_MULTI_THREADED
      _SlNonOsMainLoopTask();
#endif
    }

    // Stop Internal HTTP Server
    lRetVal = sl_NetAppStop(SL_NET_APP_HTTP_SERVER_ID);
    ASSERT_ON_ERROR( lRetVal);

    // Start Internal HTTP Server
    lRetVal = sl_NetAppStart(SL_NET_APP_HTTP_SERVER_ID);
    ASSERT_ON_ERROR( lRetVal);

    // Cycle the LED lighs 3 times to Indicate AP Mode
    int leds[] = {MCU_RED_LED_GPIO, MCU_ORANGE_LED_GPIO, MCU_GREEN_LED_GPIO};
    for (int iCount=0; iCount<3; iCount++){
      // Toggle direction of the lights
      int const offset = (iCount % 2) * 2;
      int const sgn = (offset > 0) ? -1 : 1;
      // Turn the LEDs on
      for (int i=0; i<3; ++i){
        GPIO_IF_LedOn(leds[offset + sgn*i]);
        MAP_UtilsDelay(1000000);
      }
      // Toggle the LEDs off
      for (int i=0; i<3; ++i){
        GPIO_IF_LedOff(leds[offset + sgn*i]);
        MAP_UtilsDelay(1000000);
      }
      MAP_UtilsDelay(1000000);
    }

    Report("\n\rConnect a device \n\r");
    while(!IS_IP_LEASED(g_ulStatus)){
      //wating for the client to connect
#ifndef SL_PLATFORM_MULTI_THREADED
      _SlNonOsMainLoopTask();
#endif
    }

  }

  return lRetVal;
}


void vApplicationStackOverflowHook(OsiTaskHandle *pxTask,
                                   signed char *pcTaskName){
  //Handle FreeRTOS Stack Overflow
  while(1){
  }
}

void vApplicationMallocFailedHook(){
  //Handle Memory Allocation Errors
  while(1){
  }
}


void vApplicationIdleHook(){
  //Handle Memory Allocation Errors
  while(1){
  }
}


/*
 *
 * Close the Doxygen group.
 *! @}
 *
 */
