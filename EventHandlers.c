#include <netapp.h>
#include <hw_ints.h>
#include <hw_types.h>
#include <hw_memmap.h>
#include <hw_common_reg.h>
#include <rom.h>
#include <rom_map.h>
#include <interrupt.h>
#include <prcm.h>
#include <uart_if.h>
#include <uart.h>
#include <common.h>


volatile unsigned long  g_ulStatus = 0;
unsigned long  g_ulGatewayIP = 0; // Network Gateway IP address
unsigned long  g_ulIpAddr = 0;    // CC3200 IP


unsigned long simpleLinkStatus(){
  return g_ulStatus;
}

void resetSimpleLinkStatus(){
  CLR_STATUS_BIT_ALL(g_ulStatus);
}

unsigned long simpleLinkGatewayIpAddress(){
  return g_ulGatewayIP;
}

unsigned long simpleLinkIpAddress(){
  return g_ulIpAddr;
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



/*
 * \brief This function handles HTTP server events
 *
 * \param[in]  pServerEvent - Contains the relevant event information
 * \param[in]    pServerResponse - Should be filled by the user with the
 *                                      relevant response information
 *
 * \return None
 *
 */
void SimpleLinkHttpServerCallback(SlHttpServerEvent_t *pHttpEvent,
                                  SlHttpServerResponse_t *pHttpResponse){
  Report("HttpServerCallback\r\n");

  if(!pHttpEvent || !pHttpResponse){
    return;
  }
  // Unused in this application
}


/*
 *
 * This function handles socket events indication
 *
 * \param[in]      pSock - Pointer to Socket Event Info
 *
 * \return None
 *
 */
void SimpleLinkSockEventHandler(SlSockEvent_t *pSock){
  Report("SockHandler\r\n");
  if(!pSock)
    {
      return;
    }

  //
  // This application doesn't work w/ socket - Events are not expected
  //
  switch( pSock->Event )
    {
    case SL_SOCKET_TX_FAILED_EVENT:
      switch( pSock->socketAsyncEvent.SockTxFailData.status)
        {
        case SL_ECLOSE:
          Report("[SOCK ERROR] - close socket (%d) operation "
                     "failed to transmit all queued packets\n\n",
                     pSock->socketAsyncEvent.SockTxFailData.sd);
          break;
        default:
          Report("[SOCK ERROR] - TX FAILED  :  socket %d , reason "
                 "(%d) \n\n",
                 pSock->socketAsyncEvent.SockTxFailData.sd, pSock->socketAsyncEvent.SockTxFailData.status);
          break;
        }
      break;

    default:
      Report("[SOCK EVENT] - Unexpected Event [%x0x]\n\n",pSock->Event);
      break;
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
