/* Basic HTTP server accepting basic GET/POST requests
 *
 * This is heavily based off the `CC3200SDK/example/tcp_server` example
 *
 */

/*
 *
 *! \addtogroup http-server
 *! @{
 *
 */

// Standard includes
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// simplelink includes
#include <simplelink.h>
#include <wlan.h>

// driverlib includes
#include <hw_ints.h>
#include <hw_types.h>

//#include <uart.h>
//#include <gpio_if.h>
#include <udma_if.h>
#include <utils.h>
#include <prcm.h>
#include <interrupt.h>
#include <rom_map.h>

// common interface includes
#include <common.h>
#ifndef NOTERM
#include <uart_if.h>
#endif

#include <osi.h>

#include <gpio_if.h>
#include "HttpRequest.h"
#include "HttpServer.h"
#include "EventHandlers.h"
#include "pinmux.h"

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
long wlanConnect(signed char* ssid, signed char* key);
long configureSimpleLinkToDefaultState();
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

  lRetVal = VStartSimpleLinkSpawnTask(SPAWN_TASK_PRIORITY);

  int const OSI_STACK_SIZE = 2048;

  osi_TaskCreate(deviceInitTask, (signed char*)"device-init",
                 OSI_STACK_SIZE, NULL, 99, &g_deviceInitTaskHandle);

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
          while(!IS_IP_ACQUIRED(simpleLinkStatus()))
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
      while(IS_CONNECTED(simpleLinkStatus()))
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

  return lRetVal; // Success
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
  while ((!IS_CONNECTED(simpleLinkStatus())) || (!IS_IP_ACQUIRED(simpleLinkStatus()))){
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
           SL_IPV4_BYTE(simpleLinkIpAddress(), 3),
           SL_IPV4_BYTE(simpleLinkIpAddress(), 2),
           SL_IPV4_BYTE(simpleLinkIpAddress(), 1),
           SL_IPV4_BYTE(simpleLinkIpAddress(), 0));
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
  resetSimpleLinkStatus();

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

    while (!IS_IP_ACQUIRED(simpleLinkStatus())){
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
    while(!IS_IP_LEASED(simpleLinkStatus())){
      //wating for the client to connect
#ifndef SL_PLATFORM_MULTI_THREADED
      _SlNonOsMainLoopTask();
#endif
    }

  }

  return lRetVal;
}


/*
 *
 * Close the Doxygen group.
 *! @}
 *
 */
