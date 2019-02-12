/* Simple wrapper over SimpleLink status
 *
 *
 */

#ifndef EVENTHANDLERS_H_
#define EVENTHANDLERS_H_

#ifdef __cplusplus
extern "C" {
#endif

/*! Return network gateway IP if assigned, 0 otherwise */
unsigned long simpleLinkGatewayIpAddress();

/*! Return network IP address if assigned, 0 otherwise */
unsigned long simpleLinkIpAddress();


/*! Return SimpleLink status bits
 *
 * \sa $CC3200SDK/example/common/common.h
 *
 */
unsigned long simpleLinkStatus();

/*! Reset SimpleLink status flags */
void resetSimpleLinkStatus();


#ifdef __cplusplus
}
#endif


#endif /* EVENTHANDLERS_H_ */
