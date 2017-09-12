#include "Router.h"
#include <stdio.h>
#include <stdlib.h>
#include <socket.h>
#include <jsmn.h>
#include <gpio_if.h>
#include <uart_if.h>
#include "HttpRequest.h"

int ledPathHandler(char const* req, HttpHeader *header, int socketId);
int ledPostHandler(char const* req, HttpHeader *header, int socketId);
int ledGetHandler(char const* req, HttpHeader *header, int socketId);


int routerHandleRequest(char const* req, int socketId){
  // Parse Request
  HttpHeader* header = parseHeader(req);
  int ret = 0;

  if (header->path == NULL){
    freeHttpHeader(header);
    char *resp = genJsonResponse("400 Bad Request", "{\"error\":\"Invalid path\"}");
    free(resp);
    ret = sl_Send(socketId, resp, strlen(resp), 0);
  }
  else{
    // LED status
    if (strcmp(header->path, "/led") == 0){
      ret = ledPathHandler(req, header, socketId);
    }
    else{
      char *resp = genJsonResponse("404 Not Found", "{\"error\":\"Invalid path\"}");
      ret = sl_Send(socketId, resp, strlen(resp), 0);
      free(resp);
    }
  }

  freeHttpHeader(header);
  return ret;
}


/* Return value of key in JSON object
 *
 * JSON string must be first parsed with JSMN
 *
 * Expected JSON is in the following format:
 *   {"key1": "value", "key2", "value"}
 *
 * Caller is responsible for clean up of returned value
 *
 * \param key key to look up
 * \param jsonStr jsonString
 * \param tokens list of JSMN tokens
 * \param tokenCount size of JSMN tokens
 * \return value of json key if found, NULL otherwise
 *
 */
char* jsonValue(char const* key, char const* jsonStr, jsmntok_t *tokens, int tokenCount){
  if ((tokens[0].size == 0) || (tokens[0].type != JSMN_OBJECT)){
    return NULL;
  }

  char *ret = NULL;
  /* Skip base */
  for (int i=1; i<tokenCount; ++i){
    if (tokens[i].type != JSMN_STRING){
      i += tokens[i].size;
    }
    else{
      if (strncmp(jsonStr+tokens[i].start, key, tokens[i].end - tokens[i].start) == 0){
        int len = (tokens[i+1].end - tokens[i+1].start);
        char *ret = malloc(sizeof(char)*(len+1));
        strncpy(ret, jsonStr+tokens[i+1].start, len);
        ret[len] = '\0';
        return ret;
      }
    }
  }
  return ret;
}


/* Handler for path "/led"
 *
 * \param req HTTP request
 * \param parsed HTTP header
 * \param socketId socketId of the client
 * \return TODO
 *
 */
int ledPathHandler(char const* req, HttpHeader *header, int socketId){
  int ret = 0;

  /* Toggle LED lights */
  if (header->method == HTTP_REQ_POST){
  /* Content-length is required */
    if (header->contentLength < 1){
      char *resp = genJsonResponse("411 Length Required", "{}");
      ret = sl_Send(socketId, resp, strlen(resp), 0);
      free(resp);
    }
    else{
      ret = ledPostHandler(req, header, socketId);
    }
  }
  else if (header->method == HTTP_REQ_GET){
    ret = ledGetHandler(req, header, socketId);
  }
  else{
    // invalid request
    char *resp = genJsonResponse("405 Method Not Allowed", "{}");
    ret = sl_Send(socketId, resp, strlen(resp), 0);
    free(resp);
  }

  return ret;
};


/* POST handler on "/led"
 *
 * \param req HTTP request
 * \param parsed HTTP header
 * \param socketId socketId of the client
 * \return TODO
 */
int ledPostHandler(char const* req, HttpHeader *header, int socketId){
  int ret = 0;

  /* Data must be JSON */
  if (strcmp(header->contentType, "application/json") != 0){
    char *resp = genJsonResponse("415 Unsupported Media Type", "{\"error\": \"Expected JSON\"}");
    ret = sl_Send(socketId, resp, strlen(resp), 0);
    free(resp);
  }
  else{
    /* Maximum of three tokens allowed 3*2+1=7 (3 keys, 3 values, 1
     * json object)
     *
     * TODO: this will fail if the client sends more data
     */
    unsigned int const MAX_TOKENS = 7;
    jsmn_parser parser;
    jsmn_init(&parser);
    jsmntok_t tokens[MAX_TOKENS];
    char const* bodyPtr = req + header->bodyStart;;
    int bodyLength = strlen(req) - header->bodyStart;
    int tokenCount = jsmn_parse(&parser, bodyPtr, bodyLength, tokens, MAX_TOKENS);

    char* redVal = jsonValue("red", bodyPtr, tokens, tokenCount);
    char* greenVal = jsonValue("green", bodyPtr, tokens, tokenCount);
    char* orangeVal = jsonValue("orange", bodyPtr, tokens, tokenCount);
    /* TODO: clean up */
    if (redVal != NULL){
      if (strcmp(redVal, "on") == 0){
        GPIO_IF_LedOn(MCU_RED_LED_GPIO);
      }
      else if (strcmp(redVal, "off") == 0){
        GPIO_IF_LedOff(MCU_RED_LED_GPIO);
      }
      free(redVal);
    }
    if (greenVal != NULL){
      if (strcmp(greenVal, "on") == 0){
        GPIO_IF_LedOn(MCU_GREEN_LED_GPIO);
      }
      else if (strcmp(greenVal, "off") == 0){
        GPIO_IF_LedOff(MCU_GREEN_LED_GPIO);
      }
      free(greenVal);
    }
    if (orangeVal != NULL){
      if (strcmp(orangeVal, "on") == 0){
        GPIO_IF_LedOn(MCU_ORANGE_LED_GPIO);
      }
      else if (strcmp(orangeVal, "off") == 0){
        GPIO_IF_LedOff(MCU_ORANGE_LED_GPIO);
      }
      free(orangeVal);
    }
    char *resp = genJsonResponse("204 No Content", "");
    sl_Send(socketId, resp, strlen(resp), 0);
    free(resp);
  }

  return ret;
}


int ledGetHandler(char const* req, HttpHeader *header, int socketId){
  /* return status of led lights */
  char data[200];
  if (GPIO_IF_LedStatus(MCU_ORANGE_LED_GPIO) == 0){
    strcpy(data, "{\"orange\":\"off\",");
  }
  else{
    strcpy(data, "{\"orange\":\"on\",");
  }
  if (GPIO_IF_LedStatus(MCU_GREEN_LED_GPIO) == 0){
    strcat(data, "\"green\":\"off\",");
  }
  else{
    strcat(data, "\"green\":\"on\",");
  }
  if (GPIO_IF_LedStatus(MCU_RED_LED_GPIO) == 0){
    strcat(data, "\"red\":\"off\"}");
  }
  else{
    strcat(data, "\"red\":\"on\"}");
  }
  char* jsonResp = genJsonResponse("200 OK", data);
  int iStatus = sl_Send(socketId, jsonResp, strlen(jsonResp), 0);
  free(jsonResp);
  if (iStatus < 0){
    Report("Send Error");
  }
  return iStatus;
}
