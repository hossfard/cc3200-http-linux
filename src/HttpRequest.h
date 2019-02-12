#ifndef HTTP_REQUEST_H_
#define HTTP_REQUEST_H_

typedef enum {
  HTTP_REQ_OTHER = 0,
  HTTP_REQ_GET = 1,
  HTTP_REQ_POST = 2,
  HTTP_REQ_PUT = 3,
  HTTP_REQ_DELETE = 4
} HttpRequestType;


typedef struct{
  HttpRequestType method;
  char *path;
  char *contentType;
  int contentLength;
  // offset from beginning of array to start of body
  int bodyStart;
} HttpHeader;


/* Parse Http header
 *
 * Notes:
 *   - Message content length is read off of 'Content-Length'
 *     property, not actual length of body
 *
 */
HttpHeader* parseHeader(char const* httpRequest);

void freeHttpHeader(HttpHeader *header);

char* genJsonResponse(char const* code, char const* json);


#endif /* HTTP_REQUEST_H_ */
