#include "unity.h"
#include "HttpRequest.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void setUp(){
}


void tearDown(){
}


void parseHeaderTest_GET(void){
  char const req[] = "GET / HTTP/1.1\r\n\
Host: 192.168.1.65:5001\r\n\
User-Agent: curl/7.55.0\r\n\
Accept: */*\r\n\
Content-Type: application/x-www-form-urlencoded";

  HttpHeader *header = parseHeader(req);

  TEST_ASSERT_EQUAL(0, strcmp(header->path, "/"));
  TEST_ASSERT_EQUAL(0, header->contentLength);
  TEST_ASSERT_EQUAL(HTTP_REQ_GET, header->method);

  freeHttpHeader(header);
}


void parseHeaderTest_POST(void){
  char const req[] = "POST /led HTTP/1.1\r\n\
Host: 192.168.1.65:5001\r\n\
User-Agent: curl/7.55.0\r\n\
Accept: */*\r\n\
Content-Length: 26\r\n\
Content-Type: application/x-www-form-urlencoded\r\n\r\n\
red=on&green=on&orange=off";

  HttpHeader *header = parseHeader(req);

  TEST_ASSERT_EQUAL(0, strcmp(header->path, "/led"));
  TEST_ASSERT_EQUAL(26, header->contentLength);
  TEST_ASSERT_EQUAL(HTTP_REQ_POST, header->method);
  TEST_ASSERT_EQUAL(0, strcmp(header->contentType, "application/x-www-form-urlencoded"));
  // Must discount escape characters
  TEST_ASSERT_EQUAL(154, header->bodyStart);

  freeHttpHeader(header);
}



int main(void) {
  UNITY_BEGIN();
  RUN_TEST(parseHeaderTest_GET);
  RUN_TEST(parseHeaderTest_POST);
  return UNITY_END();
}
