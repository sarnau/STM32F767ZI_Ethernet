#include "httpclient-netconn.h"
#include "JsonListener.h"
#include "JsonStreamingParser.h"
#include "httpclient.h"
#include <string.h>

#define WEBCLIENT_THREAD_PRIO (tskIDLE_PRIORITY + 4)

class ExampleListener : public JsonListener {
public:
  virtual void value(const char *key, const char *value);
};

void ExampleListener::value(const char *key, const char *value) {
  debug_printf("%s: \"%s\"\n", key, value);
}

static JsonStreamingParser parser;
static ExampleListener listener;

static void http_client_netconn_thread_returnpage(u8_t num, hc_errormsg err, char *data, u16_t len) {
  static int state = 0;
  //debug_printf("### %d %d %d %s\n", num, err, len, len == 0 ? "<END>" : data);
  if (len == 0) { // last event, connection was closed
    if (state == 2) {
      debug_printf("\n### DONE\n");
    } else {
      debug_printf("\n### ERROR\n");
    }
  } else {
    data[len] = 0;
    if (state == 0 && !strncmp(data, "HTTP/1.1 200 OK", 15)) {
      parser.setListener(&listener);
      state = 1;
    }
    if (state == 1) {
      // warning: if the header is just large enough, that these 4 bytes fall into the gap between two packages, we will miss it!
      const char *pos = strstr(data, "\r\n\r\n");
      if (pos) {
        state = 2;
        for (int i = pos - data + 4; i < len; ++i)
          parser.parse(data[i]);
      }
    } else if (state == 2) {
      for (int i = 0; i < len; ++i)
        parser.parse(data[i]);
    }
  }
}

extern "C" void http_client_netconn_thread(void *arg) {
  bool waitingDone = false;
  while (1) {
    if (waitingDone) {
      osDelay(1);
      continue;
    }
    ip_addr_t serverIp = IPADDR4_INIT_BYTES(192, 168, 178, 33);
    int num = hc_open(serverIp, "GET /api/RXm5yNUfXI5sp0Ta8hXz4oUxkBwZaWKAcyHikZ7k/lights/12 HTTP/1.0\r\n\r\n", http_client_netconn_thread_returnpage);
    debug_printf("### hc_open %d\n", num);
    if (num <= 0) {
      osDelay(1000);
    } else {
      waitingDone = true;
    }
  }
}

void http_client_netconn_init() {
  sys_thread_new("HTTPCLIENT", http_client_netconn_thread, NULL, DEFAULT_THREAD_STACKSIZE, WEBCLIENT_THREAD_PRIO);
}