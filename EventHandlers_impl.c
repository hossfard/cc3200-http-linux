#include <netapp.h>
#include "hw_ints.h"
#include "hw_types.h"
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "rom.h"
#include "rom_map.h"
#include "interrupt.h"
#include "prcm.h"
#include <uart_if.h>
#include <uart.h>

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
