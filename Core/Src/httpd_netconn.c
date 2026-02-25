#include "httpd_netconn.h"
#include "main.h"
#include "cmsis_os2.h"
#include "lwip/apps/fs.h"
#include "lwip/api.h"
#include "string.h"
#include "jsmn.h"
#include "flash_if.h"
#include <stdbool.h>
#include "crc.h"
#include "version.h"
#include "socket.h"
#include "tcp.h"
#include "global_types.h"
#include "selftest.h"

// modbus master/slave rtu, tcp
#include "Modbus.h"

// for function mktime() & difftime()
//#include <time.h>

#define DELAY_MS(x) (osDelay(pdMS_TO_TICKS(x)))

static void http_server(struct netconn *conn);
uint8_t config(const char *json, jsmntok_t *t, uint16_t count, element_type_t tp, uint8_t sp);
static int fwupdate_state_machine(struct netconn* conn, char* buf, u16_t buflen);
static void fwupdate_send_success(struct netconn* conn, const char* str);
static int jsoneq(const char *json, jsmntok_t *tok, const char *s);
static void stage_set(struct netconn *conn, uint8_t config);

static const unsigned char PAGE_HEADER_200_OK[] = {
  //"HTTP/1.1 200 OK"
  0x48, 0x54, 0x54, 0x50, 0x2F, 0x31, 0x2E, 0x31, 0x20, 0x32, 0x30, 0x30, 0x20, 
  0x4F, 0x4B, 0x0D, 0x0A,
  0x00
};

static const unsigned char PAGE_HEADER_SERVER[] = {
  //"Server: lwIP"
  0x53, 0x65, 0x72, 0x76, 0x65, 0x72, 0x3A, 0x20, 0x6C, 0x77, 0x49, 0x50, 0x0D, 
  0x0A,
  0x00
};

static const unsigned char PAGE_HEADER_CONTENT_TEXT[] = {
  //"Content-type: text/html"
  0x43, 0x6F, 0x6E, 0x74, 0x65, 0x6E, 0x74, 0x2D, 0x74, 0x79, 0x70, 0x65, 0x3A, 
  0x20, 0x74, 0x65, 0x78, 0x74, 0x2F, 0x68, 0x74, 0x6D, 0x6C, 0x0D, 0x0A, 0x0D,
  0x0A,
  0x00
};

static const unsigned char PAGE_HEADER_CONTENT_JSON[] = {
  //"Content-type: application/json; charset=utf-8"
  0x43, 0x6F, 0x6E, 0x74, 0x65, 0x6E, 0x74, 0x2D, 0x74, 0x79, 0x70, 0x65, 0x3A, 
  0x20, 0x61, 0x70, 0x70, 0x6C, 0x69, 0x63, 0x61, 0x74, 0x69, 0x6F, 0x6E, 0x2F, 
  0x6A, 0x73, 0x6F, 0x6E, 0x3B, 0x20, 0x63, 0x68, 0x61, 0x72, 0x73, 0x65, 0x74, 
  0x3D, 0x75, 0x74, 0x66, 0x2D, 0x38, 0x0D, 0x0A, 0x0D, 0x0A,
  0x00
};

static const unsigned char PAGE_HEADER_REQUIRED[] = {
  //"HTTP/1.1 401 Authorization Required"
  0x48, 0x54, 0x54, 0x50, 0x2F, 0x31, 0x2E, 0x31, 0x20, 0x34, 0x30, 0x31, 0x20, 
  0x41, 0x75, 0x74, 0x68, 0x6F, 0x72, 0x69, 0x7A, 0x61, 0x74, 0x69, 0x6F, 0x6E, 
  0x20, 0x52, 0x65, 0x71, 0x75, 0x69, 0x72, 0x65, 0x64, 0x0D, 0x0A,
  0x00
};

static const unsigned char PAGE_HEADER_BASIC[] = {
  //"WWW-Authenticate: Basic realm="User Visible Realm""
  0x57, 0x57, 0x57, 0x2D, 0x41, 0x75, 0x74, 0x68, 0x65, 0x6E, 0x74, 0x69, 0x63, 
  0x61, 0x74, 0x65, 0x3A, 0x20, 0x42, 0x61, 0x73, 0x69, 0x63, 0x20, 0x72, 0x65, 
  0x61, 0x6C, 0x6D, 0x3D, 0x22, 0x55, 0x73, 0x65, 0x72, 0x20, 0x56, 0x69, 0x73, 
  0x69, 0x62, 0x6C, 0x65, 0x20, 0x52, 0x65, 0x61, 0x6C, 0x6D, 0x22, 0x0D, 0x0A,
  0x00
};
/*
static const unsigned char PAGE_HEADER_CONNECTION[] = {
  //"Connection: keep-alive"
  0x43, 0x6F, 0x6E, 0x6E, 0x65, 0x63, 0x74, 0x69, 0x6F, 0x6E, 0x3A, 0x20, 0x6B,
  0x65, 0x65, 0x70, 0x2D, 0x61, 0x6C, 0x69, 0x76, 0x65, 0x0D, 0x0A,
  0x00
};
*/
static const unsigned char PAGE_HEADER_CONNECTION_CLOSE[] = {
  //"Connection: close"
  0x43, 0x6F, 0x6E, 0x6E, 0x65, 0x63, 0x74, 0x69, 0x6F, 0x6E, 0x3A, 0x20, 0x63, 
  0x6C, 0x6F, 0x73, 0x65, 0x0D, 0x0A,
  0x00
};

#define CONTENT_LENGTH_TAG "Content-Length:"
#define OCTET_STREAM_TAG "application/octet-stream\r\n\r\n"
#define BOUNDARY_TAG "multipart/form-data; boundary="
#define EMPTY_LINE_TAG "\r\n\r\n"
#define AUTHORIZATION_TAG "Authorization: Basic"

#define STATUS_ERROR -1
#define STATUS_NONE 0
#define STATUS_INPROGRESS 1
#define STATUS_DONE 2
  
typedef enum {
  FWUPDATE_STATE_HEADER,
  FWUPDATE_STATE_OCTET_START,
  FWUPDATE_STATE_OCTET_STREAM,
} fwupdate_state_t;

typedef struct {
  fwupdate_state_t state;
  int content_length;
  int file_length; /*!< file length in byte*/
  int accum_length;
  //volatile uint32_t addr;
  uint8_t accum_buf[4];
  uint8_t accum_buf_len;
} fwupdate_t;

static fwupdate_t fwupdate;

typedef enum {
  POST_STATE_HEADER,
  POST_STATE_START,
  POST_STATE_BODY,
} post_state_t;

typedef struct {
  post_state_t state;
  uint32_t content_length;
  uint32_t accum_length;
  char content[3072];
  uint8_t url;
} post_t;

static post_t post_data @ ".ccmram";

#ifdef DEBUG_FW_UPDATE
char start_bin[16];
char stop_bin[16];

uint32_t status_w;
uint32_t status_e;
#endif

// 2304
#define HTML_LEN 3072
char html[3072] @ ".ccmram";

uint16_t length_html = 0;

uint16_t boundary_length;

static __IO uint32_t FlashWriteAddress;

uint32_t end_address;

uint32_t recv_count;

extern bool configuration_change;

osTimerId_t timerTimeout;

osSemaphoreId_t httpdbufSemaphore;

osSemaphoreId_t httpdSemaphore;

extern modbusHandler_t ModbusTCPm;
extern modbusHandler_t ModbusRS2;

void send_response(struct netconn *conn) {
  char content_length_header[32] = {0};
  char response_headers[128] = {0};
  uint8_t headers_length;
  
  sprintf((char*)content_length_header,"Content-Length: %d\r\n", length_html);
  headers_length = sprintf((char*)response_headers,"%s%s%s%s%s", PAGE_HEADER_200_OK, PAGE_HEADER_SERVER, PAGE_HEADER_CONNECTION_CLOSE, content_length_header, PAGE_HEADER_CONTENT_JSON);
  netconn_write(conn, (const unsigned char*)response_headers, (size_t)headers_length, NETCONN_COPY | NETCONN_MORE);
  netconn_write(conn, (const unsigned char*)html, (size_t)length_html, NETCONN_COPY);
}

/**
 * @brief initialization
 *
 *
 * detailed description
 */
void stage_serial(struct netconn *conn) {
  if (osSemaphoreAcquire(httpdbufSemaphore, 50) == osOK) {
    memset(html, 0, HTML_LEN);
    length_html = 0;
    length_html = sprintf((char*)(html + length_html), "{\"serial\":\"%d\",", identification.serial_number);
    // int to string hex value
    length_html += sprintf((char*)(html + length_html), "\"mac0\":\"%X\",", identification.mac_0);
    // int to string hex value
    length_html += sprintf((char*)(html + length_html), "\"mac1\":\"%X\"}", identification.mac_1);
    send_response(conn);
    osSemaphoreRelease(httpdbufSemaphore);
  }
}

/**
 * @brief initialization
 *
 *
 * detailed description
 */
void stage_state(struct netconn *conn) {
  if (osSemaphoreAcquire(httpdbufSemaphore, 50) == osOK) {
    memset(html, 0, HTML_LEN);
    length_html = 0;
    length_html = sprintf((char*)(html + length_html), "{\"state\":[");
    for (uint8_t i = 0; i < 4; i++) {
      length_html += sprintf((char*)(html + length_html), "{\"e\":\"%d\",\"t\":\"%d\"},", thres[i].extinction, thres[i].transporter);
    }
    html[length_html-- - 1] = 0;
    length_html += sprintf((char*)(html + length_html), "]}");
    send_response(conn);
    osSemaphoreRelease(httpdbufSemaphore);
  }
}

#ifdef DEBUG_JSON
uint16_t m_buf_cam_len = 0;
uint16_t m_buf_sup_len = 0;
uint16_t m_buf_rel_len = 0;
uint16_t m_buf_sen_len = 0;
uint16_t m_length_html = 0;
#endif

err_t state_write;

/**
 * @brief initialization
 *
 *
 * detailed description
 */
void stage_set(struct netconn *conn, uint8_t config) {
  
#define BUF1_LEN 81  
#define BUF2_LEN 812
  
  uint8_t el_count = 0;
  char buf_1[BUF1_LEN];
  char buf_2[BUF2_LEN];
  uint16_t len_2 = 0;
  
  uint32_t flags;
  uint8_t test = 0;
  
#ifdef DEBUG_JSON
  uint16_t buf_cam_len = 0;
  uint16_t buf_sup_len = 0;
  uint16_t buf_rel_len = 0;
  uint16_t buf_sen_len = 0;
#endif

  if (osSemaphoreAcquire(httpdbufSemaphore, 50) == osOK) {
    flags = osEventFlagsGet(event_flags);

    memset(html, 0, HTML_LEN);
    length_html = 0;

    if (config == 1) {
      length_html += sprintf((char*)(html + length_html), "{\"group\":[");
    } else {
      char ver[12] = {0};
      sprintf(ver, "%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_BUILD);
      length_html += sprintf((char*)(html + length_html), "{\"usk_ip\":\"%s\",", ip4addr_ntoa(&settings.se.usk_ip_addr));
      length_html += sprintf((char*)(html + length_html), "\"usk_mask\":\"%s\",", ip4addr_ntoa(&settings.se.usk_mask_addr));
      length_html += sprintf((char*)(html + length_html), "\"usk_gateway\":\"%s\",", ip4addr_ntoa(&settings.se.usk_gateway_addr));
      length_html += sprintf((char*)(html + length_html), "\"urm_id\":\"%d\",", settings.se.urm_id);
      length_html += sprintf((char*)(html + length_html), "\"umvh_id\":\"%d\",", settings.se.umvh_id);
      length_html += sprintf((char*)(html + length_html), "\"port_camera\":\"%d\",", settings.se.port_camera);
      length_html += sprintf((char*)(html + length_html), "\"rate\":\"%d\",", settings.se.scan_rate);
      length_html += sprintf((char*)(html + length_html), "\"timeout\":\"%d\",", settings.se.timeout);
      length_html += sprintf((char*)(html + length_html), "\"ver\":\"%s\",", ver);
      length_html += sprintf((char*)(html + length_html), "\"ser\":\"%d\",", identification.serial_number);
      // int to string hex value
      length_html += sprintf((char*)(html + length_html), "\"mac1\":\"%X\",", identification.mac_1);
      // int to string hex value
      length_html += sprintf((char*)(html + length_html), "\"mac0\":\"%X\",", identification.mac_0);
      length_html += sprintf((char*)(html + length_html), "\"uptm\":\"%d\",", HAL_GetTick()/1000);
      
      if ((flags & DATA_BIT) == DATA_BIT) {
        test = 1;
      } else {
        test = 0;
      }
      length_html += sprintf((char*)(html + length_html), "\"selftest\":\"%d\",", test);
      length_html += sprintf((char*)(html + length_html), "\"group\":[");
    }
    for (uint8_t g = 0; g < 10; g++) {
      length_html += sprintf((char*)(html + length_html), "{");

// camera
        memset(buf_2, 0, BUF2_LEN);
        len_2 = 0;
        for (uint8_t i = 0; i < 10; i++) {
          if (settings.el[i].id_group == g) {
            if ((strlen(settings.el[i].name) > 0) && settings.el[i].type == CAMERA) {
              memset(buf_1, 0, BUF1_LEN);
              if (config == 1) {
                sprintf(buf_1, "\"%s\",", settings.el[i].name);
              } else {
#ifdef DEBUG_JSON
                buf_cam_len = sprintf(buf_1, "{\"name\":\"%s\",\"ip\":\"%s\",\"id\":\"%d\",\"s\":\"%d\"},",
                                      settings.el[i].name,
                                      ip4addr_ntoa(&settings.el[i].ip_addr),
                                      settings.el[i].id_modbus, 
                                      equipment[i].camera);
#else
                sprintf(buf_1, "{\"name\":\"%s\",\"ip\":\"%s\",\"id\":\"%d\",\"s\":\"%d\"},",
                        settings.el[i].name,
                        ip4addr_ntoa(&settings.el[i].ip_addr),
                        settings.el[i].id_modbus, 
                        equipment[i].camera);
#endif
              }
              
#ifdef DEBUG_JSON
              if (m_buf_cam_len < buf_cam_len) {
                m_buf_cam_len = buf_cam_len;
              }
#endif
              len_2 += sprintf((char*)(buf_2 + len_2), "%s", buf_1);
              el_count++;
            }
          }
        }
        if (el_count > 0) {
          // remouve last ","
          buf_2[len_2 - 1] = 0;
          length_html += sprintf((char*)(html + length_html), "\"camera\":[%s],", buf_2);
        }
        el_count = 0;
// camera
      
// power_supply
        memset(buf_2, 0, BUF2_LEN);
        len_2 = 0;
        for (uint8_t i = 10; i < 20; i++) {
          if (settings.el[i].id_group == g) {
            if ((strlen(settings.el[i].name) > 0) && settings.el[i].type == SUPPLY) {
              memset(buf_1, 0, BUF1_LEN);
              if (config == 1) {
                sprintf(buf_1, "\"%s\",", settings.el[i].name);
              } else {
#ifdef DEBUG_JSON
                buf_sup_len = sprintf(buf_1, "{\"name\":\"%s\",\"ip\":\"%s\",\"id\":\"%d\",\"port\":\"%d\",\"s\":\"%d\"},",
                                      settings.el[i].name,
                                      ip4addr_ntoa(&settings.el[i].ip_addr),
                                      settings.el[i].id_modbus,
                                      settings.el[i].id_out,
                                      equipment[i-10].supply);
#else
                sprintf(buf_1, "{\"name\":\"%s\",\"ip\":\"%s\",\"id\":\"%d\",\"port\":\"%d\",\"s\":\"%d\"},",
                        settings.el[i].name,
                        ip4addr_ntoa(&settings.el[i].ip_addr),
                        settings.el[i].id_modbus,
                        settings.el[i].id_out,
                        equipment[i-10].supply);
#endif
              }

#ifdef DEBUG_JSON
              if (m_buf_sup_len < buf_sup_len) {
                m_buf_sup_len = buf_sup_len;
              } 
#endif
              len_2 += sprintf((char*)(buf_2 + len_2), "%s", buf_1);
              el_count++;
            }
          }
        }
        if (el_count > 0) {
          // remouve last ","
          buf_2[len_2 - 1] = 0;
          length_html += sprintf((char*)(html + length_html), "\"supply\":[%s],", buf_2);
        }
        el_count = 0;
// power_supply

// relay
        memset(buf_2, 0, BUF2_LEN);
        len_2 = 0;
        for (uint8_t i = 20; i < 30; i++) {
          if (settings.el[i].id_group == g) {
            if ((strlen(settings.el[i].name) > 0) && settings.el[i].type == RELAY) {
              memset(buf_1, 0, BUF1_LEN);
              if (config == 1) {
                sprintf(buf_1, "\"%s\",", settings.el[i].name);
              } else {
#ifdef DEBUG_JSON
                buf_rel_len = sprintf(buf_1, "{\"name\":\"%s\",\"id\":\"%d\",\"s\":\"%d\"},",
                                      settings.el[i].name,
                                      settings.el[i].id_out,
                                      equipment[i-20].valve);
#else
                sprintf(buf_1, "{\"name\":\"%s\",\"id\":\"%d\",\"s\":\"%d\"},",
                        settings.el[i].name,
                        settings.el[i].id_out,
                        equipment[i-20].valve);
#endif
              }
#ifdef DEBUG_JSON
              if (m_buf_rel_len < buf_rel_len) {
                m_buf_rel_len = buf_rel_len;
              } 
#endif
              len_2 += sprintf((char*)(buf_2 + len_2), "%s", buf_1);
              el_count++;
            }
          }
        }
        if (el_count > 0) {
          // remouve last ","
          buf_2[len_2 - 1] = 0;
          length_html += sprintf((char*)(html + length_html), "\"relay\":[%s],", buf_2);
        }
        el_count = 0;
// relay

// fire_sensor
        memset(buf_2, 0, BUF2_LEN);
        len_2 = 0;
        for (uint8_t i = 30; i < 40; i++) {
          if (settings.el[i].id_group == g) {
            if ((strlen(settings.el[i].name) > 0) && settings.el[i].type == SENSOR) {
              memset(buf_1, 0, BUF1_LEN);
              if (config == 1) {
                sprintf(buf_1, "\"%s\",", settings.el[i].name);
              } else {
#ifdef DEBUG_JSON
                buf_sen_len = sprintf(buf_1, "{\"name\":\"%s\",\"id\":\"%d\",\"s\":\"%d\"},",
                                      settings.el[i].name,
                                      settings.el[i].id_out,
                                      equipment[i-30].sensor);
#else
                sprintf(buf_1, "{\"name\":\"%s\",\"id\":\"%d\",\"s\":\"%d\"},",
                        settings.el[i].name,
                        settings.el[i].id_out,
                        equipment[i-30].sensor);
#endif
              }
#ifdef DEBUG_JSON
              if (m_buf_sen_len < buf_sen_len) {
                m_buf_sen_len = buf_sen_len;
              }
#endif
              len_2 += sprintf((char*)(buf_2 + len_2), "%s", buf_1);
              el_count++;
            }
          }
        }
        if (el_count > 0) {
          // remouve last ","
          buf_2[len_2 - 1] = 0;
          length_html += sprintf((char*)(html + length_html), "\"sensor\":[%s],", buf_2);
          if (config == 1) {
            html[length_html-- - 1] = 0;
          }
        }
        el_count = 0;
// fire_sensor

      if (config == 0) {
        memset(buf_2, 0, BUF2_LEN);
        len_2 = 0;
        if (config == 0 && settings.te[g].type == TEMP) {
          for (uint8_t i = 0; i < 4; i++) {
            len_2 += sprintf((char*)(buf_2 + len_2), "\"%d\",", settings.te[g].temp[i]);
          }
          // remouve last ","
          buf_2[len_2 - 1] = 0;
          length_html += sprintf((char*)(html + length_html), "\"temp\":[%s]", buf_2);
        }
      }
      // remove unused "{"
      if (html[length_html - 1] == 0x7B) {
        html[length_html-- - 1] = 0;
      } else {
        length_html += sprintf((char*)(html + length_html), "},");
      }
    }
    
#ifdef DEBUG_JSON
    m_length_html = length_html;
#endif
    
    // remouve last ","
    html[length_html-- - 1] = 0;

    if (config != 1 && strncmp(&html[length_html - 8], "\"group\"", 7) == 0) {
      length_html -= 9;
      length_html += sprintf((char*)(html + length_html), "}");
    } else {
      length_html += sprintf((char*)(html + length_html), "]}");
    }
    
    send_response(conn);

    osSemaphoreRelease(httpdbufSemaphore);
  }
}

/**
 * @brief initialization
 *
 *
 * detailed description
 */
static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
  if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
      strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
    return 0;
  }
  return -1;
}

/**
 * @brief initialization
 *
 *
 * detailed description
 */
static void fwupdate_send_success(struct netconn* conn, const char* str) {
  if (osSemaphoreAcquire(httpdbufSemaphore, 50) == osOK) {
    memset(html, 0, HTML_LEN);
    length_html = 0;
    length_html = sprintf((char*)(html + length_html), "%s", str);
    send_response(conn);
    osSemaphoreRelease(httpdbufSemaphore);
  }
}

/**
 * @brief initialization
 *
 *
 * detailed description
 */
static int post_state_machine(struct netconn* conn, char* buf, u16_t buflen) {
  char* buf_end = buf + buflen;
  int ret = STATUS_NONE;
  while (buf && buf < buf_end) {
    switch (post_data.state) {
    case POST_STATE_HEADER: {
      if ((buflen >= 19) && (strncmp(buf, "POST /configuration", 19) == 0)) {
        post_data.url = 1;
      } else if ((buflen >= 14) && (strncmp(buf, "POST /settings", 14) == 0)) {
        post_data.url = 2;        
/*
        int keep_alive = 1;
        int keep_idle = 1;
        int keep_interval = 1;
        int keep_count = 60;
        
        setsockopt(conn->socket, SOL_SOCKET, SO_KEEPALIVE, &keep_alive, sizeof(keep_alive));
        setsockopt(conn->socket, IPPROTO_TCP, TCP_KEEPIDLE, &keep_idle, sizeof(keep_idle));
        setsockopt(conn->socket, IPPROTO_TCP, TCP_KEEPINTVL, &keep_interval, sizeof(keep_interval));
        setsockopt(conn->socket, IPPROTO_TCP, TCP_KEEPCNT, &keep_count, sizeof(keep_count));
*/        
      } else if ((buflen >= 12) && (strncmp(buf, "POST /serial", 12) == 0)) {
        post_data.url = 3;
      } else if ((buflen >= 15) && (strncmp(buf, "POST /threshold", 15) == 0)) {
        post_data.url = 4;
      }

      if (post_data.url > 0) {
        ret = STATUS_ERROR;
        buf = strstr(buf, CONTENT_LENGTH_TAG);
        if (buf) {
          buf += strlen(CONTENT_LENGTH_TAG);
          post_data.content_length = atoi(buf);
          if (post_data.content_length > 0) {
            buf = strstr(buf, EMPTY_LINE_TAG);
            if (buf) {
              buf += strlen(EMPTY_LINE_TAG);
              post_data.state = POST_STATE_START;
              ret = STATUS_INPROGRESS;
            }
          } else {
            buf = 0;
          }
        }
      } else {
        buf = 0;
      }
      break;
    }
    case POST_STATE_START: {
      post_data.state = POST_STATE_HEADER;
      ret = STATUS_ERROR;
      if (buf) {
        post_data.state = POST_STATE_BODY;
        post_data.accum_length = 0;
        ret = STATUS_INPROGRESS;
      }
      break;
    }
    case POST_STATE_BODY: {
      uint32_t len = buf_end - buf;
      ret = STATUS_INPROGRESS;
      memcpy(post_data.content + post_data.accum_length, buf, len);
      post_data.accum_length += len;
      if (ret == STATUS_INPROGRESS) {
        if (post_data.accum_length >= post_data.content_length) {
          if (osSemaphoreAcquire(httpdbufSemaphore, 50) == osOK) {
/*
            memset(html, 0, HTML_LEN);
            length_html = sprintf((char*)(html), "%s", "{\"post\":\"1\"}");
            send_response(conn);
*/
/*
            memset(html, 0, HTML_LEN);
            length_html = sprintf(html,"%s%s%s%s", PAGE_HEADER_200_OK, PAGE_HEADER_SERVER, PAGE_HEADER_CONNECTION_CLOSE, PAGE_HEADER_CONTENT_TEXT);
            length_html += sprintf((char*)(html + length_html), "%s", send_status);
            netconn_write(conn, (const unsigned char*)html, length_html, NETCONN_COPY);
*/

            char content_length_header[32] = {0};
            memset(html, 0, HTML_LEN);
            length_html = 0;
            sprintf((char*)content_length_header,"Content-Length: %d\r\n", length_html);
            length_html = sprintf((char*)html, "%s%s%s%s%s", PAGE_HEADER_200_OK, PAGE_HEADER_SERVER, PAGE_HEADER_CONNECTION_CLOSE, content_length_header, PAGE_HEADER_CONTENT_TEXT);
            netconn_write(conn, (const unsigned char*)html, length_html, NETCONN_COPY);

            osSemaphoreRelease(httpdbufSemaphore);
          }
          ret = STATUS_DONE;
          post_data.state = POST_STATE_HEADER;
        }
      }
      buf = 0;
      break;
    }
    }
  } // while (buf && buf < buf_end)
  return ret;
}


/**
 * @brief initialization
 *
 *
 * detailed description
 */
static int fwupdate_state_machine(struct netconn* conn, char* buf, u16_t buflen) {
  char* buf_start = buf;
  char* buf_end = buf + buflen;
  int ret = STATUS_NONE;
  while (buf && buf < buf_end) {
    if ((buflen >= 12) && (strncmp(buf, "POST /upload", 12) == 0)) {
/*
      int keep_alive = 1;
      int keep_idle = 1;
      int keep_interval = 1;
      int keep_count = 60;
      
      setsockopt(conn->socket, SOL_SOCKET, SO_KEEPALIVE, &keep_alive, sizeof(keep_alive));
      setsockopt(conn->socket, IPPROTO_TCP, TCP_KEEPIDLE, &keep_idle, sizeof(keep_idle));
      setsockopt(conn->socket, IPPROTO_TCP, TCP_KEEPINTVL, &keep_interval, sizeof(keep_interval));
      setsockopt(conn->socket, IPPROTO_TCP, TCP_KEEPCNT, &keep_count, sizeof(keep_count));
*/      
      memset(&fwupdate, 0, sizeof(fwupdate_t));
      fwupdate.state = FWUPDATE_STATE_HEADER;
    }
    switch (fwupdate.state) {
      case FWUPDATE_STATE_HEADER: {
        if ((buflen >= 12) && (strncmp(buf, "POST /upload", 12) == 0)) {
          ret = STATUS_ERROR;
          char* tmp = buf;
          buf = strstr(buf, CONTENT_LENGTH_TAG);
          if (buf) {
            buf += strlen(CONTENT_LENGTH_TAG);
            fwupdate.content_length = atoi(buf);
            if (fwupdate.content_length > 0) {
              buf = strstr(tmp, BOUNDARY_TAG);
              buf += strlen(BOUNDARY_TAG);
              char* boundary_end;              
              boundary_end = strstr(buf, "\r\n");              
              boundary_length = boundary_end - buf;
              buf = strstr(buf, EMPTY_LINE_TAG);
              if (buf) {
                buf += strlen(EMPTY_LINE_TAG);
                buf_start = buf;
                fwupdate.state = FWUPDATE_STATE_OCTET_START;
                ret = STATUS_INPROGRESS;
              }
            } else {
              buf = 0;
            }
          }
        } else {
          buf = 0;
        }
        break;
      } // case FWUPDATE_STATE_HEADER
      case FWUPDATE_STATE_OCTET_START: {
        fwupdate.state = FWUPDATE_STATE_HEADER;
        ret = STATUS_ERROR;
        
        buf = strstr(buf, OCTET_STREAM_TAG);
        if (buf) {
          buf += strlen(OCTET_STREAM_TAG);
          uint16_t multipart_length = buf - buf_start;
          // [\r\n][--][BOUNDARY][--][\r\n] => boundary_length + 8 byte
          fwupdate.file_length = fwupdate.content_length - multipart_length - boundary_length - 8;
          if (fwupdate.file_length == PARTITION_SIZE + 4) {
            fwupdate.state = FWUPDATE_STATE_OCTET_STREAM;
            fwupdate.accum_length = 0;
            fwupdate.accum_buf_len = 0;
            ret = STATUS_INPROGRESS;          
            // init flash
            HAL_FLASH_Unlock();
            FlashWriteAddress = PARTITION_2_ADDR;
            
#ifdef DEBUG_FW_UPDATE
            status_e = FLASH_If_Erase(FlashWriteAddress, 7);
#else
            FLASH_If_Erase(FlashWriteAddress, 7);
#endif
            //end_address = fwupdate.addr + length_partition;
            end_address = FlashWriteAddress + PARTITION_SIZE;
          }
        }
        break;
      } // case FWUPDATE_STATE_OCTET_START
      case FWUPDATE_STATE_OCTET_STREAM: {
        int len = buf_end - buf;
        ret = STATUS_INPROGRESS;
#ifdef DEBUG_FW_UPDATE
        if (fwupdate.accum_length == 0) {
          memcpy(start_bin, buf, 16);
        }
#endif
        if (fwupdate.accum_length + len > fwupdate.file_length) {
          len = fwupdate.file_length - fwupdate.accum_length;
        }
        // 
        if (fwupdate.accum_buf_len > 0) {
          uint32_t j = 0;
          while (fwupdate.accum_buf_len <= 3) {
            if (len > (j + 1)) {
              fwupdate.accum_buf[fwupdate.accum_buf_len++] = *(buf + j);
            } else {
              fwupdate.accum_buf[fwupdate.accum_buf_len++] = 0xFF;
            }
            j++;
          }
          
#ifdef DEBUG_FW_UPDATE
          //status_w = FLASH_If_Write(&fwupdate.addr, end_address, (uint32_t*)(fwupdate.accum_buf), 1);
          status_w = FLASH_If_Write(&FlashWriteAddress, end_address, (uint32_t*)(fwupdate.accum_buf), 1);
#else
          FLASH_If_Write(&FlashWriteAddress, end_address, (uint32_t*)(fwupdate.accum_buf), 1);
#endif
          
          fwupdate.accum_length += 4;
          fwupdate.accum_buf_len = 0;
          
#ifdef DEBUG_FW_UPDATE
          memset((void *)fwupdate.accum_buf, 0, 4);
#endif
          // update data pointer
          buf = (char*)(buf + j);
          len = len - j;
        }      
        uint32_t count = len / 4;
        uint32_t i = len % 4;
        if (i > 0) {        
          // store bytes in accum_buf
          fwupdate.accum_buf_len = 0;
#ifdef DEBUG_FW_UPDATE
          memset((void *)fwupdate.accum_buf, 0, 4);
#endif
          for(; i > 0; i--) {
            fwupdate.accum_buf[fwupdate.accum_buf_len++] = *(char*)(buf + len - i);
          }
        }
        
#ifdef DEBUG_FW_UPDATE
        //status_w = FLASH_If_Write(&fwupdate.addr, end_address, (uint32_t*)buf, count);
        status_w = FLASH_If_Write(&FlashWriteAddress, end_address, (uint32_t*)buf, count);
#else
        FLASH_If_Write(&FlashWriteAddress, end_address, (uint32_t*)buf, count);
#endif
        
        fwupdate.accum_length += count * 4;
        
        if (ret == STATUS_INPROGRESS) {
          if (fwupdate.accum_length == fwupdate.file_length) {
#ifdef DEBUG_FW_UPDATE
            memcpy(stop_bin, buf + len - 16, 16);
#endif
            uint32_t sum = hw_crc32((unsigned char*)PARTITION_2_ADDR, PARTITION_SIZE);
            uint32_t crc = (*(__IO uint32_t *)PARTITION_2_CRC_ADDR);
            
            char send_status[12] = {0};
            
            if(sum == crc) {
              uint32_t part = 1;
              taskENTER_CRITICAL();
              FlashWORD(PARTITION_SELECT_ADDR, (uint32_t *)&part, sizeof(part));
              taskEXIT_CRITICAL();
              sprintf((char*)(send_status), "{\"crc\":\"%d\"}", 1);
              ret = STATUS_DONE;
            } else {
              sprintf((char*)(send_status), "{\"crc\":\"%d\"}", 0);
              ret = STATUS_ERROR;
            }
            fwupdate_send_success(conn, send_status);
            HAL_FLASH_Lock();
            fwupdate.state = FWUPDATE_STATE_HEADER;
          }
        }
        buf = 0;
        break;
      } // case FWUPDATE_STATE_OCTET_STREAM
    } // switch (fwupdate.state)
  } // while (buf && buf < buf_end)
  return ret;
}

/**
 * @brief initialization
 *
 *
 * detailed description
 */
uint8_t config(const char *json_str, jsmntok_t *t, uint16_t count, element_type_t tp, uint8_t sp) {
  char el[7] = {0};
  uint16_t num = 0;
  switch (tp)
  {
    case NELEMENT: {
      break;
    }
    case CAMERA: {
      memcpy(el, "camera", 6);
      num = 0;
      break;
    }
    case SUPPLY: {
      memcpy(el, "supply", 6);
      num = 10;
      break;
    }
    case RELAY: {
      memcpy(el, "relay", 5);
      num = 20;
      break;
    }
    case SENSOR: {
      memcpy(el, "sensor", 6);
      num = 30;
      break;
    }
    case TEMP: {
      memcpy(el, "temp", 4);
      num = 0;
      break;
    }
  }
  uint8_t group = 0;
  
  for (uint16_t i = 0; i < count; i++) {
    if (t[i].type == JSMN_STRING && (int)strlen(el) == t[i].end - t[i].start && 
        strncmp(json_str + t[i].start, el, t[i].end - t[i].start) == 0) 
    {
      if (t[i + 1].type == JSMN_ARRAY) {
        
        uint16_t count_arr = t[i + 1].size;
        uint16_t count_obj = t[i + 2].size;
        
        if (sp == 0 || tp == TEMP) {
          uint16_t index = i + 2;
          count_obj = 0;
          do {
            if (sp == 0 ) {
              settings.el[num].type = tp;
              memset(settings.el[num].name, 0, 9);
              if (t[index].end - t[index].start < 9) {
                memcpy(settings.el[num].name, (void*)(json_str + t[index].start), t[index].end - t[index].start);
              } else {
                memcpy(settings.el[num].name, (void*)(json_str + t[index].start), 9);
              }
              settings.el[num].id_group = group;
              num++;
            } else if (tp == TEMP) {
              char value[16] = { 0 };
              memcpy((void*)value, (void*)(json_str + t[index].start), t[index].end - t[index].start);
              settings.te[group].temp[count_obj++] = atoi(value);
            }            
            index++;
            count_arr--;
          } while (count_arr > 0);          
        } else {
          i += 3;
          do {
            uint16_t index = 0;
            for (uint8_t j = 0; j < count_obj; j++) {
              char name[16] = { 0 };
              char value[16] = { 0 };
              index = i + j * 2;
              memcpy((void*)name, (void*)(json_str + t[index].start), t[index].end - t[index].start);
              index++;
              memcpy((void*)value, (void*)(json_str + t[index].start), t[index].end - t[index].start);              
              
              if (tp == CAMERA || tp == SUPPLY){
                if (strncmp(name, "ip", 2) == 0) {
                  settings.el[num].ip_addr.addr = ipaddr_addr(value);
                } else if (strncmp(name, "id", 2) == 0) {
                  settings.el[num].id_modbus = atoi(value);
                } else if (strncmp(name, "po", 2) == 0) {
                  settings.el[num].id_out = atoi(value);
                }
              } else if (tp == RELAY || tp == SENSOR ) {
                if (strncmp(name, "id", 2) == 0) {
                  settings.el[num].id_out = atoi(value);
                }
              }
            }
            num++;
            count_arr--;
            i += count_obj * 2 + 1;
          } while (count_arr > 0);
        }
        group++;
      }
    }
  }
  return group;
}

/**
 * @brief Timeout and reboot
 *
 */
void timeoutCallback(void *argument) {
  (void) argument;
  NVIC_SystemReset();
}

/**
 * @brief 
 *
 */
void close_conn(struct netconn *conn) {
  if (conn != NULL) {
    netconn_close(conn);
  }
  netconn_delete(conn);
  conn = NULL;
}

/**
 * @brief 
 *
 */
void authorization_process(struct netconn *conn, const char *buf, const char *url) {
  struct fs_file file;
  if (strstr(buf, AUTHORIZATION_TAG) > 0 && strstr(buf, "YWRtaW46YWRtaW4=") > 0) {
    fs_open(&file, url);
    netconn_write(conn, (const unsigned char*)(file.data), (size_t)file.len, NETCONN_NOCOPY);
    fs_close(&file);
  } else {
    if (osSemaphoreAcquire(httpdbufSemaphore, 50) == osOK) {
      char content_length_header[32] = {0};
      memset(html, 0, HTML_LEN);
      length_html = 0;
      sprintf((char*)content_length_header,"Content-Length: %d\r\n", length_html);
      length_html = sprintf((char*)html, "%s%s%s%s%s", PAGE_HEADER_REQUIRED, PAGE_HEADER_BASIC, PAGE_HEADER_CONNECTION_CLOSE, content_length_header, PAGE_HEADER_CONTENT_TEXT);
      netconn_write(conn, (const unsigned char*)html, length_html, NETCONN_COPY);
      osSemaphoreRelease(httpdbufSemaphore);
    }
  }
}

#define TOK_LENGTH 500
jsmntok_t token[TOK_LENGTH]  @ ".ccmram";

#ifdef DEBUG_JSON
uint16_t m_token = 0;
#endif

/**
 * @brief Routing http data
 *
 */
static void http_server(struct netconn *conn) {
  struct netbuf *inbuf;
  char* buf;
  uint16_t buflen;
  struct fs_file file;
  char tmp[16];
  err_t err;
  
  if (conn == NULL) {
    return;
  }
 
  //int keep_alive = 0;
  //int keep_idle = 2;
  //int keep_interval = 2;
  //int keep_count = 2;
  
  //setsockopt(conn->socket, SOL_SOCKET, SO_KEEPALIVE, &keep_alive, sizeof(keep_alive));
  //setsockopt(conn->socket, IPPROTO_TCP, TCP_KEEPIDLE, &keep_idle, sizeof(keep_idle));
  //setsockopt(conn->socket, IPPROTO_TCP, TCP_KEEPINTVL, &keep_interval, sizeof(keep_interval));
  //setsockopt(conn->socket, IPPROTO_TCP, TCP_KEEPCNT, &keep_count, sizeof(keep_count));
      
  netconn_set_recvtimeout(conn, 10000);
  
  // Read the data from the port, blocking if nothing yet there. 
  // We assume the request (the part we care about) is in one netbuf
  err = netconn_recv(conn, &inbuf);

  do {
    if (err == ERR_OK) {
      err = netconn_err(conn);
      if (err == ERR_OK) {
        recv_count++;
        netbuf_first(inbuf);
        do {
          if (netbuf_data(inbuf, (void**)&buf, &buflen) != ERR_OK) {
            break;
          }
          // Is this an HTTP GET command?
          if ((buflen >= 5) && (strncmp(buf, "GET /", 5) == 0)) {
            if ((strncmp(buf, "GET / ", 6) == 0) || (strncmp(buf, "GET /index.html", 15) == 0)) {
              fs_open(&file, "/index.html");
              netconn_write(conn, (const unsigned char*)(file.data), (size_t)file.len, NETCONN_NOCOPY);
              fs_close(&file);
            }
            else if((strncmp(buf, "GET /index.js", 13) == 0)) {
              fs_open(&file, "/index.js");
              netconn_write(conn, (const unsigned char*)(file.data), (size_t)file.len, NETCONN_NOCOPY);
              fs_close(&file);
            }
            else if (strncmp(buf, "GET /builder.html", 17) == 0) {
              authorization_process(conn, buf, "/builder.html");
            }
            else if (strncmp(buf, "GET /serial.html", 16) == 0) {
              authorization_process(conn, buf, "/serial.html");
            }
            else if (strncmp(buf, "GET /state.html", 15) == 0) {
              authorization_process(conn, buf, "/state.html");
            }
            else if (strncmp(buf, "GET /test.html", 14) == 0) {
              authorization_process(conn, buf, "/test.html");
            }          
            else if((strncmp(buf, "GET /dragula.min.css", 20) == 0)) {
              fs_open(&file, "/dragula.min.css");
              netconn_write(conn, (const unsigned char*)(file.data), (size_t)file.len, NETCONN_NOCOPY);
              fs_close(&file);
            }
            else if((strncmp(buf, "GET /builder.css", 16) == 0)) {
              fs_open(&file, "/builder.css");
              netconn_write(conn, (const unsigned char*)(file.data), (size_t)file.len, NETCONN_NOCOPY);
              fs_close(&file);
            }
            else if((strncmp(buf, "GET /dragula.min.js", 19) == 0)) {
              fs_open(&file, "/dragula.min.js");
              netconn_write(conn, (const unsigned char*)(file.data), (size_t)file.len, NETCONN_NOCOPY);
              fs_close(&file);
            }
            else if (strncmp(buf, "GET /stageset.json", 18) == 0) {
              stage_set(conn, 0);
            } 
            else if (strncmp(buf, "GET /stageload.json", 19) == 0) {
              stage_set(conn, 0);
            }
            else if (strncmp(buf, "GET /serial.json", 16) == 0) {
              stage_serial(conn);
            }
            else if (strncmp(buf, "GET /state.json", 15) == 0) {
              stage_state(conn);
            }
            else if (strncmp(buf, "GET /ota.html", 13) == 0) {
              fs_open(&file, "/ota.html");
              netconn_write(conn, (const unsigned char*)(file.data), (size_t)file.len, NETCONN_NOCOPY);
              fs_close(&file);
            }
            else if (strncmp(buf, "GET /selftest", 13) == 0) {
              if (osSemaphoreAcquire(httpdbufSemaphore, 50) == osOK) {
/*
                memset(html, 0, HTML_LEN);
                //length_html = sprintf((char*)html,"%s%s%s%s", PAGE_HEADER_200_OK, PAGE_HEADER_SERVER, PAGE_HEADER_CONNECTION_CLOSE, PAGE_HEADER_CONTENT_TEXT);
                length_html = sprintf((char*)html,"%s", PAGE_HEADER_200_OK);
                netconn_write(conn, (const unsigned char*)html, (size_t)length_html, NETCONN_COPY);
*/
                char content_length_header[32] = {0};
                memset(html, 0, HTML_LEN);
                length_html = 0;
                sprintf((char*)content_length_header,"Content-Length: %d\r\n", length_html);
                length_html = sprintf((char*)html, "%s%s%s%s%s", PAGE_HEADER_200_OK, PAGE_HEADER_SERVER, PAGE_HEADER_CONNECTION_CLOSE, content_length_header, PAGE_HEADER_CONTENT_TEXT);
                netconn_write(conn, (const unsigned char*)html, length_html, NETCONN_COPY);

                osSemaphoreRelease(httpdbufSemaphore);
              }
              uint32_t flags = osEventFlagsGet(event_flags);
              
              if ((flags & DATA_BIT) == 0) {
                set_before_selftest();
                osEventFlagsSet(event_flags, TEST_BIT);
              }
            }
            else {
              // Load Error page
              fs_open(&file, "/404.html");
              netconn_write(conn, (const unsigned char*)(file.data), (size_t)file.len, NETCONN_COPY);
              fs_close(&file);
            }
          }
          // Is this an HTTP POST command?
          else {
            int ret = fwupdate_state_machine(conn, buf, buflen);
            if (ret == STATUS_NONE) {
              // ignore
            } else if (ret == STATUS_INPROGRESS) {
              // Don't close the connection!
              osThreadSuspend(ModbusTCPm.myTaskModbusAHandle);
              osThreadSuspend(ModbusRS2.myTaskModbusAHandle);
            } else {
              // Some result, we should close the connection now
              if (ret == STATUS_DONE) {
                // reboot after we close the connection              
                osTimerStart(timerTimeout, 5000);
              }
            }
            
            if (fwupdate.state != FWUPDATE_STATE_OCTET_STREAM) {
              ret = post_state_machine(conn, buf, buflen);
              if (ret == STATUS_NONE) {
                // ignore
              } else if (ret == STATUS_INPROGRESS) {
                // Don't close the connection!
              } else {
                if (ret == STATUS_DONE) {
                  // POST /configuration
                  if (post_data.url == 1) {
                    jsmn_parser p;
                    memset(token, 0, sizeof(jsmntok_t)*TOK_LENGTH);
                    jsmn_init(&p);
                    int count = jsmn_parse(&p, post_data.content, strlen(post_data.content), token, TOK_LENGTH);
#ifdef DEBUG_JSON
                    if (m_token < count) {
                      m_token = count;
                    } 
#endif
                    if (count > 0) {
                      // clear configuration headers
                      uint16_t i = 0;
                      for (i = 0; i < 10; i++) {
                        settings.el[i].type = NELEMENT;
                        settings.te[i].type = NELEMENT;
                      }
                      for (i = 10; i < 40; i++) {
                        settings.el[i].type = NELEMENT;
                      }
                      uint8_t g = config(post_data.content, (jsmntok_t*)token, count, CAMERA, 0);
                      config(post_data.content, (jsmntok_t*)token, count, SUPPLY, 0);
                      config(post_data.content, (jsmntok_t*)token, count, RELAY, 0);
                      config(post_data.content, (jsmntok_t*)token, count, SENSOR, 0);
                      for (i = 0; i < g; i++) {
                        settings.te[i].type = TEMP;
                        settings.te[i].id_group = i;
                      }
                    }
                  } // if (post_data.url == 1)
                  // POST /settings
                  if (post_data.url == 2) {
                    jsmn_parser p;
                    memset(token, 0, sizeof(jsmntok_t)*TOK_LENGTH);
                    jsmn_init(&p);
                    // parsing response in format json
                    int count = jsmn_parse(&p, post_data.content, strlen(post_data.content), token, TOK_LENGTH);
#ifdef DEBUG_JSON
                    if (m_token < count) {
                      m_token = count;
                    }
#endif                    
#ifdef DEBUG
                    //WM_HTTPD = uxTaskGetStackHighWaterMark(NULL);
#endif
                    if (count > 0) {
                      config(post_data.content, (jsmntok_t*)token, count, CAMERA, 1);
                      config(post_data.content, (jsmntok_t*)token, count, SUPPLY, 1);
                      config(post_data.content, (jsmntok_t*)token, count, RELAY, 1);
                      config(post_data.content, (jsmntok_t*)token, count, SENSOR, 1);
                      config(post_data.content, (jsmntok_t*)token, count, TEMP, 1);
                      for (uint16_t i = 1; i < count; i++) {
                        if (jsoneq(post_data.content, &token[i], "usk_ip") == 0) {
                          memset((void *)tmp, 0, 16);
                          memcpy((void *)tmp, (void *)(post_data.content + token[i + 1].start), token[i + 1].end - token[i + 1].start);
                          settings.se.usk_ip_addr.addr = ipaddr_addr(tmp);
                        } else if (jsoneq(post_data.content, &token[i], "usk_mask") == 0) {
                          memset((void *)tmp, 0, 16);
                          memcpy((void *)tmp, (void *)(post_data.content + token[i + 1].start), token[i + 1].end - token[i + 1].start);
                          settings.se.usk_mask_addr.addr = ipaddr_addr(tmp);
                        } else if (jsoneq(post_data.content, &token[i], "usk_gateway") == 0) {
                          memset((void *)tmp, 0, 16);
                          memcpy((void *)tmp, (void *)(post_data.content + token[i + 1].start), token[i + 1].end - token[i + 1].start);
                          settings.se.usk_gateway_addr.addr = ipaddr_addr(tmp);
                        } else if (jsoneq(post_data.content, &token[i], "urm_id") == 0) {
                          memset((void *)tmp, 0, 16);
                          memcpy((void *)tmp, (void *)(post_data.content + token[i + 1].start), token[i + 1].end - token[i + 1].start);
                          settings.se.urm_id = atoi(tmp);
                        } else if (jsoneq(post_data.content, &token[i], "umvh_id") == 0) {
                          memset((void *)tmp, 0, 16);
                          memcpy((void *)tmp, (void *)(post_data.content + token[i + 1].start), token[i + 1].end - token[i + 1].start);
                          settings.se.umvh_id = atoi(tmp);
                        } else if (jsoneq(post_data.content, &token[i], "port_camera") == 0) {
                          memset((void *)tmp, 0, 16);
                          memcpy((void *)tmp, (void *)(post_data.content + token[i + 1].start), token[i + 1].end - token[i + 1].start);
                          settings.se.port_camera = atoi(tmp);
                        } else if (jsoneq(post_data.content, &token[i], "rate") == 0) {
                          memset((void *)tmp, 0, 16);
                          memcpy((void *)tmp, (void *)(post_data.content + token[i + 1].start), token[i + 1].end - token[i + 1].start);
                          settings.se.scan_rate = atoi(tmp);
                        } else if (jsoneq(post_data.content, &token[i], "timeout") == 0) {
                          memset((void *)tmp, 0, 16);
                          memcpy((void *)tmp, (void *)(post_data.content + token[i + 1].start), token[i + 1].end - token[i + 1].start);
                          settings.se.timeout = atoi(tmp);
                        }
                      } // for (uint8_t i = 1; i < count; i++)
                      
#ifdef DEBUG_FW_UPDATE
                      // sector 14
                      status_w = FlashWORD(CONFIGURATION_ADDR, (uint32_t *)&settings, sizeof(settings_t));
#else
                      FlashWORD(CONFIGURATION_ADDR, (uint32_t *)&settings, sizeof(settings_t));
#endif

                      configuration_change = true;
                      osTimerStart(timerTimeout, 5000);
                    }
                  } // if (post_data.url == 2)
                  // POST /serial
                  if (post_data.url == 3) {
                    jsmn_parser p;
                    memset(token, 0, sizeof(jsmntok_t)*TOK_LENGTH);
                    jsmn_init(&p);
                    int count = jsmn_parse(&p, post_data.content, strlen(post_data.content), token, TOK_LENGTH);
#ifdef DEBUG_JSON
                    if (m_token < count) {
                      m_token = count;
                    }
#endif                    
                    if (count > 0) {
                      for (uint16_t i = 1; i < count; i++) {
                        if (jsoneq(post_data.content, &token[i], "serial") == 0) {
                          memset((void *)tmp, 0, 16);
                          memcpy((void *)tmp, (void *)(post_data.content + token[i + 1].start), token[i + 1].end - token[i + 1].start);
                          identification.serial_number = atoi(tmp);                        
                        } else if (jsoneq(post_data.content, &token[i], "mac0") == 0) {
                          memset((void *)tmp, 0, 16);
                          memcpy((void *)tmp, (void *)(post_data.content + token[i + 1].start), token[i + 1].end - token[i + 1].start);
                          // string hex value to int
                          identification.mac_0 = strtol(tmp, NULL, 16);
                        } else if (jsoneq(post_data.content, &token[i], "mac1") == 0) {
                          memset((void *)tmp, 0, 16);
                          memcpy((void *)tmp, (void *)(post_data.content + token[i + 1].start), token[i + 1].end - token[i + 1].start);
                          // string hex value to int
                          identification.mac_1 = strtol(tmp, NULL, 16);
                        }
                      } // for (uint8_t i = 1; i < count; i++) {
                      
#ifdef DEBUG_FW_UPDATE
                      // sector 12
                      status_w = FlashWORD(IDENTIFICATION_ADDR, (uint32_t *)&identification, sizeof(identification_t));  
#else
                      FlashWORD(IDENTIFICATION_ADDR, (uint32_t *)&identification, sizeof(identification_t));                   
#endif
                      osTimerStart(timerTimeout, 5000);
                    }
                  } // if (post_data.url == 3)
                  // POST /threshold
                  if (post_data.url == 4) {
                    jsmn_parser p;
                    memset(token, 0, sizeof(jsmntok_t)*TOK_LENGTH);
                    jsmn_init(&p);
                    int count = jsmn_parse(&p, post_data.content, strlen(post_data.content), token, TOK_LENGTH);
#ifdef DEBUG_JSON
                    if (m_token < count) {
                      m_token = count;
                    }
#endif                    
                    if (count > 0) {
                      for (uint16_t i = 0; i < count; i++) {
                        const char* el = "state";
                        uint8_t num = 0;
                        if (token[i].type == JSMN_STRING && (int)strlen(el) == token[i].end - token[i].start &&
                            strncmp((const char*)(post_data.content + token[i].start), el, token[i].end - token[i].start) == 0) 
                        {
                          if (token[i + 1].type == JSMN_ARRAY) {
                            uint16_t count_arr = token[i + 1].size;
                            uint16_t count_obj = token[i + 2].size;
                            i += 3;
                            do {
                              uint16_t index = 0;
                              for (uint8_t j = 0; j < count_obj; j++) {
                                char name[16] = { 0 };
                                char value[16] = { 0 };
                                index = i + j * 2;
                                memcpy((void*)name, (void*)(post_data.content + token[index].start), token[index].end - token[index].start);
                                index++;
                                memcpy((void*)value, (void*)(post_data.content + token[index].start), token[index].end - token[index].start);
                                if (strncmp(name, "e", 1) == 0) {
                                  thres[num].extinction = atoi(value);
                                }
                                if (strncmp(name, "t", 1) == 0) {
                                  thres[num].transporter = atoi(value);
                                }
                              }
                              num++;
                              count_arr--;
                              i += count_obj * 2 + 1;
                            } while (count_arr > 0);
                          }
                        }                      
                      } // for (uint8_t i = 0; i < count; i++)
                      
#ifdef DEBUG_FW_UPDATE
                      status_w = FlashWORD(STATE_ADDR, (uint32_t *)&thres, sizeof(threshold_t)*4);
#else
                      FlashWORD(STATE_ADDR, (uint32_t *)&thres, sizeof(threshold_t)*4);          
#endif
                      
                    }                  
                  } // if (post_data.url == 4)
                  
                  // POST
                  
                } // (ret == POST_STATUS_DONE)
              }
            }
          }
#ifdef DEBUG
          //WM_HTTPD = uxTaskGetStackHighWaterMark(NULL);
#endif
        } while ((netbuf_next(inbuf) >= 0) && (netconn_err(conn) == ERR_OK));
      } else {
        break;
      }

      netbuf_delete(inbuf);
      
      err = netconn_recv(conn, &inbuf);
    }
  } while (err == ERR_OK);
  
  netbuf_delete(inbuf);
}

#ifdef DEBUG_NETCONN
uint16_t error_netconn_accept;
#endif

/**
 * @brief TCP connection handling function on port 80
 *
 */
void http_server_thread(void *argument) {
  (void) argument;
  // reset after timer expires
  timerTimeout = osTimerNew(timeoutCallback, osTimerOnce, (void *)0, NULL);
  //
  httpdbufSemaphore = osSemaphoreNew(1, 1, NULL);
  // set a limit on the number of connections no more than 1
  httpdSemaphore = osSemaphoreNew(1, 1, NULL);
  struct netconn *conn;
  struct netconn *newconn;
  err_t err;
  
#ifdef DEBUG_NETCONN  
  error_netconn_accept = 0;
#endif
  
  while (1) {
    conn = netconn_new(NETCONN_TCP);
    if (conn != NULL) {
      if (netconn_bind(conn, NULL, 80) == ERR_OK) {
        if (netconn_listen(conn) == ERR_OK) {
          netconn_set_recvtimeout(conn, 100);
          recv_count = 0;
#ifdef DEBUG
          //WM_HTTPD = uxTaskGetStackHighWaterMark(NULL);
#endif
          do {
            if (osSemaphoreAcquire(httpdSemaphore, 50) == osOK) {
              err = netconn_accept(conn, &newconn);              
              if (err == ERR_OK) {
                http_server(newconn);
                close_conn(newconn);                
              }
              osSemaphoreRelease(httpdSemaphore);
            } else {
              DELAY_MS(1);
            }
          } while (err != ERR_CLSD);
#ifdef DEBUG_NETCONN
          error_netconn_accept++;
#endif
        }
      }     
    }
    close_conn(conn);
    DELAY_MS(1);
  }
}
