#ifndef ROUTER_H_
#define ROUTER_H_

#ifdef __cplusplus
extern "C" {
#endif


/* HTTP request handler on CC3200 chip
 *
 * Currently supports GET and POST requests to '/led'
 *
 * POST request must:
 *   - have Content-Type of 'application/json'
 *   - specify Content-Length
 *
 * Example: CC3200 has an IP address of 192.168.1.1 and running server
 * on 5001. Then Generate a POST request to `led` using curl
 *
 * `curl -H "Content-Type application/json" -X post -d "{\"orange\": \"on\"}" 192.168.1.1:5001/led`
 *
 * which will generate the request
 *   ```
 *      POST /led HTTP/1.1
 *      Host: 192.168.1.1:5001
 *      User-Agent: curl/7.55.0
 *      Accept: *\/\*
 *      Content-Type: application/json
 *      Content-Length: 15
 *
 *      {"green": "on"}
 *   ```
 *
 * \param req HTTP request including header and body
 * \param socketId socketId of client
 * \return TODO
 *
 */
int routerHandleRequest(char const* req, int socketId);


#ifdef __cplusplus
}
#endif

#endif /* ROUTER_H_ */
