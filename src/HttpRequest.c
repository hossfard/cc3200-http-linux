#include "HttpRequest.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


int digitCount(int x){
  int n = 1;
  if (x > 100000000){
    n += 4;
    x /= 100000000;
  }
  if (x > 10000){
    n += 4;
    x /= 10000;
  }
  if (x > 100){
    n += 2;
    x /= 100;
  }
  if (x > 10){
    n+= 1;
  }
  return n;
}


int httpContentLength(char const* post){
  char searchString[] = "Content-Length: ";
  char *ptr = strstr(post, searchString);
  int contentLength = 0;
  if (ptr != NULL){
    char *ptr0 = ptr + strlen(searchString);
    ptr = ptr0;
    while (*ptr != '\r'){
      ++ptr;
    }
    int N = (ptr-ptr0);
    char *lengthStr = (char*)malloc(N+1);
    memcpy(lengthStr, ptr0, N);
    lengthStr[N] = '\0';
    contentLength = atoi(lengthStr);
    free(lengthStr);
  }
  return contentLength;
}


char* httpPath(char const* req){
  char *firstSlashPtr = strstr(req, "/");
  char *httpPtr = strstr(req, "HTTP/");
  char *newlinePtr = strstr(req, "\r");

  if ((firstSlashPtr < httpPtr) && (httpPtr < newlinePtr)){
    int length = (httpPtr-firstSlashPtr)-1;
    if (length > 0){
      char *str = malloc(sizeof(char*)*(length+1));
      strncpy(str, firstSlashPtr, length);
      str[length] = '\0';
      return str;
    }
  }
  return NULL;
}


HttpRequestType requestType(char const* request){
  if (strncmp(request, "GET", 3) == 0){
    return HTTP_REQ_GET;
  }
  else if (strncmp(request, "POST", 4) == 0){
    return HTTP_REQ_POST;
  }
  else if (strncmp(request, "PUT", 3) == 0){
    return HTTP_REQ_POST;
  }
  else if (strncmp(request, "DELETE", 6) == 0){
    return HTTP_REQ_POST;
  }
  else{
    return HTTP_REQ_OTHER;
  }
}


char* contentType(char const* request){
  char const key[] = "Content-Type: ";
  char *contentTypePtr = strstr(request, key);
  if (contentTypePtr){
    contentTypePtr += strlen(key);
    char *end = strstr(contentTypePtr, "\r\n");
    int length = end - contentTypePtr;
    if (end){
      char *ret = malloc(sizeof(char)*(length+1));
      strncpy(ret, contentTypePtr, length);
      ret[length] = '\0';
      return ret;
    }
  }

  return NULL;
}


HttpHeader* parseHeader(char const* httpRequest){
  HttpHeader *ret = malloc(sizeof(HttpHeader));
  ret->method = requestType(httpRequest);
  ret->contentLength = httpContentLength(httpRequest);
  ret->path = httpPath(httpRequest);
  ret->contentType = contentType(httpRequest);

  char *bodyStartPtr = strstr(httpRequest, "\r\n\r\n") + 4;
  unsigned int bodyStartOffset = bodyStartPtr - httpRequest;
  if (bodyStartOffset < strlen(httpRequest)){
    ret->bodyStart = bodyStartOffset;
  }
  else{
    ret->bodyStart = -1;
  }

  return ret;
}


char *genJsonResponse(char const* code, char const* json){
  int jsonLength = strlen(json);
  char version[] = "HTTP/1.0 ";

  if (strcmp(json, "") == 0){
    int respLen = strlen(version) + strlen(code) + 2;
    char *ret = malloc(sizeof(char)*(respLen+1));
    snprintf(ret, respLen, "%s%s%s", version, code, "\r\n");
    ret[respLen] = '\0';
    return ret;
  }
  else{
    char ct[] = "Content-Type: application/json\r\n";
    char cl[] = "Content-Length: ";
    int respLen = strlen(version) + strlen(code) + 2 /* +2 for new line */
      + strlen(ct)                /* content-type length*/
      + strlen(cl) + 2            /* content-length length */
      + digitCount(jsonLength)    /* length of json */
      + 4                         /* end of header */
      + strlen(json);

    char *ret = malloc(sizeof(char)*(respLen+1));

    snprintf(ret, respLen, "%s%s%s%s%s%d%s%s",
             version, code, "\r\n",
             ct,
             cl, jsonLength, "\r\n\r\n",
             json);
    ret[respLen] = '\0';
    return ret;
  }
}


void freeHttpHeader(HttpHeader *header){
  free(header->path);
  free(header->contentType);
}
