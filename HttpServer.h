#ifndef HTTPSERVER_H_
#define HTTPSERVER_H_

#ifdef __cplusplus
extern "C" {
#endif

/*!  Start httpServer infinite listen loop
 *
 * \param port port to listen on
 * \return TODO
 *
 */
long httpServer(unsigned short port);

#ifdef __cplusplus
}
#endif

#endif /* HTTPSERVER_H_ */
