#include "test.h"
#include "main.h"
#include "OLED_1in5_rgb.h"
#include "ImageData.h"
#include <stdbool.h>
#include "cmsis_os2.h"
#include "string.h"
#include "version.h"

#include "global_types.h"

// Queue OLED
extern osMessageQueueId_t oled_msg_queue;
extern void WriteReg(uint8_t Reg);
extern uint8_t BlackImage[32768];

void oled_thread(void *argument);
void conv_head();
void conv_info(uint8_t screen_number, oled_t* oled_msg);
void adr_info(uint8_t screen_number, oled_t* oled_msg);

#define HOLDING_TIME 8000
#define INIT_TIME 24000

void conv_head() {
  Paint_ClearWindows(0, 0, OLED_1in5_RGB_WIDTH, OLED_1in5_RGB_HEIGHT, BLACK);
  Paint_DrawString_EN(0, 0, "Конвейер", &Courier12R, WHITE, BLACK);
  Paint_DrawString_EN(0, 13, "Пожаротушение", &Courier12R, WHITE, BLACK);
  Paint_DrawString_EN(0, 26, "Давление", &Courier12R, WHITE, BLACK);
  Paint_DrawString_EN(0, 39, "Камера", &Courier12R, WHITE, BLACK);
  Paint_DrawString_EN(0, 52, "Источник", &Courier12R, WHITE, BLACK);
  Paint_DrawString_EN(0, 65, "Температура", &Courier12R, WHITE, BLACK);
  Paint_DrawString_EN(0, 78, "Порог", &Courier12R, WHITE, BLACK);
}

void conv_info(uint8_t screen_number, oled_t* oled_msg) {
  // line 1 x = 0
  Paint_ClearWindows(70, 0, 84, 13, BLACK);
  Paint_DrawNum(70, 0, screen_number, &Courier12R, 0, BLACK, WHITE);
  
  if (oled_msg->conv_IsOnline[screen_number-1]) {
    Paint_DrawRectangle(115, 3, 121, 9, GREEN, DOT_PIXEL_1X1, DRAW_FILL_FULL);
  } else {
    Paint_DrawRectangle(115, 3, 121, 9, RED, DOT_PIXEL_1X1, DRAW_FILL_FULL);
  }
  
  // line 2 x = 13
  if (oled_msg->water_IsOn[screen_number-1]) {
    Paint_ClearWindows(98, 13, 127, 26, BLACK);
    Paint_DrawString_EN(98, 13, "ВКЛ", &Courier12R, RED, BLACK);
  } else {
    Paint_ClearWindows(98, 13, 127, 26, BLACK);
    Paint_DrawString_EN(98, 13, "ВЫКЛ", &Courier12R, RED, BLACK);
  }
  
  // line 3 x = 26
  if (oled_msg->line2_status == 2) {
    Paint_DrawRectangle(115, 29, 121, 35, GREEN, DOT_PIXEL_1X1, DRAW_FILL_FULL);
  } else if (oled_msg->line2_status == 1) {
    Paint_DrawRectangle(115, 29, 121, 35, RED, DOT_PIXEL_1X1, DRAW_FILL_FULL);
  } else if (oled_msg->line2_status == 0) {
    Paint_DrawRectangle(115, 29, 121, 35, BLUE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
  }
  
  // line 4 x = 39
  if (oled_msg->camera_status[screen_number-1]) {
    Paint_DrawRectangle(115, 42, 121, 48, GREEN, DOT_PIXEL_1X1, DRAW_FILL_FULL);
  } else {
    Paint_DrawRectangle(115, 42, 121, 48, RED, DOT_PIXEL_1X1, DRAW_FILL_FULL);
  }
  
  // line 5 x = 52
  if (oled_msg->supply_status[screen_number-1]) {
    Paint_DrawRectangle(115, 55, 121, 61, GREEN, DOT_PIXEL_1X1, DRAW_FILL_FULL);
  } else {
    Paint_DrawRectangle(115, 55, 121, 61, RED, DOT_PIXEL_1X1, DRAW_FILL_FULL);
  }
  
  // line 6 x = 69
  Paint_ClearWindows(90, 65, 126, 78, BLACK);
  Paint_DrawNum(90, 65, (oled_msg->temp[screen_number-1]) / 10.0, &Courier12R, 1, BLACK, WHITE);
  
  // line 7 x = 78
  if (oled_msg->emergency_level[screen_number-1] == 0) {
    Paint_ClearWindows(90, 78, 126, 91, BLACK);
    Paint_DrawString_EN(90, 78, "<< 1", &Courier12R, RED, BLACK);
  } else if (oled_msg->emergency_level[screen_number-1] == 1) {
    Paint_ClearWindows(90, 78, 126, 91, BLACK);
    Paint_DrawString_EN(90, 78, ">> 1", &Courier12R, RED, BLACK);
  } else if (oled_msg->emergency_level[screen_number-1] == 2) {
    Paint_ClearWindows(90, 78, 126, 91, BLACK);
    Paint_DrawString_EN(90, 78, ">> 2", &Courier12R, RED, BLACK);
  } else if (oled_msg->emergency_level[screen_number-1] == 3) {
    Paint_ClearWindows(90, 78, 126, 91, BLACK);
    Paint_DrawString_EN(90, 78, ">> 3", &Courier12R, RED, BLACK);
  }
  
  // line 8 x = 91
  if (oled_msg->mode[screen_number-1] == 1) {
    Paint_ClearWindows(0, 91, OLED_1in5_RGB_WIDTH, 104, BLACK);
    Paint_DrawString_EN(0, 91, "Режим-Авария", &Courier12R, RED, BLACK);
  } else if (oled_msg->mode[screen_number-1] == 2) {
    Paint_ClearWindows(0, 91, OLED_1in5_RGB_WIDTH, 104, BLACK);
    Paint_DrawString_EN(0, 91, "Режим-Диспетчер", &Courier12R, RED, BLACK);
  } else if (oled_msg->water_IsOn[screen_number-1]) {
    Paint_ClearWindows(0, 91, OLED_1in5_RGB_WIDTH, 104, BLACK);
    Paint_DrawString_EN(0, 91, "Режим-Температура", &Courier12R, RED, BLACK);
  } else {
    Paint_ClearWindows(0, 91, OLED_1in5_RGB_WIDTH, 104, BLACK);
  }
  
  char buf[22] = {0};  
  sprintf(buf, "Версия %d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_BUILD);
  Paint_DrawString_EN(0, 104, buf, &Courier12R, RED, BLACK);
}

void adr_info(uint8_t screen_number, oled_t* oled_msg) {  
  Paint_ClearWindows(0, 13, OLED_1in5_RGB_WIDTH, OLED_1in5_RGB_HEIGHT, BLACK);
  
  Paint_ClearWindows(70, 0, 84, 13, BLACK);
  Paint_DrawNum(70, 0, screen_number, &Courier12R, 0, BLACK, WHITE);
  
  uint8_t indx_list = 1;
  
  for (uint8_t i = 1; i<11; i++) {
    if (((oled_msg->address_module[screen_number-1] >> i) & 1) && indx_list < 6) {
      Paint_DrawString_EN(0, 13*indx_list, "Адр", &Courier12R, WHITE, BLACK);
      Paint_DrawNum(35, 13*indx_list, i, &Courier12R, 0, BLACK, WHITE);
      indx_list++;
    } else if (((oled_msg->address_module[screen_number-1] >> i) & 1) && indx_list > 5) {
      Paint_DrawString_EN(64, 13*(indx_list-5), "Адр", &Courier12R, WHITE, BLACK);
      Paint_DrawNum(64+35, 13*(indx_list-5), i, &Courier12R, 0, BLACK, WHITE);
      indx_list++;
    }
  }
  
  if (oled_msg->address_module[screen_number - 1] & (1 << 21)) {
    Paint_DrawString_EN(0, 78, "КЗ на АДР линии!", &Courier12R, RED, BLACK);
  }            
  
}

void oled_thread(void *argument) {
  (void) argument;
  
  osStatus_t status;
  oled_t oled_msg = {0};
  uint32_t time_change_screen = 0;
  uint32_t time_init_display = 0;
  uint8_t screen_count = 0;
  uint8_t screen_number = 0;
  
  bool update = false;
  //
  char buf[22] = {0};
  
  OLED_1in5_rgb_Init();
  osDelay(500);
  OLED_1in5_rgb_Clear();
  osDelay(500);
  OLED_1in5_rgb_Init();
  osDelay(500);
  OLED_1in5_rgb_Clear();
  
  // create image
  Paint_NewImage(BlackImage, OLED_1in5_RGB_WIDTH, OLED_1in5_RGB_HEIGHT, 0, BLACK);
  // set scale
  Paint_SetScale(65);
  // Select Image
  Paint_SelectImage(BlackImage);
  // draw image on the screen
  OLED_1in5_rgb_Display(BlackImage);
  
  osDelay(3000);

  Paint_ClearWindows(0, 0, OLED_1in5_RGB_WIDTH, OLED_1in5_RGB_HEIGHT, BLACK);
  
  Paint_DrawString_EN(2, 2, "IP адрес", &Courier12R, WHITE, BLACK);
  memset(buf, 0, 22);
  sprintf(buf, "%s", ip4addr_ntoa(&settings.se.usk_ip_addr));
  Paint_DrawString_EN(2, 18, buf, &Courier12R, WHITE, BLACK);
  
  Paint_DrawString_EN(2, 36, "Маска", &Courier12R, WHITE, BLACK);
  memset(buf, 0, 22);
  sprintf(buf, "%s", ip4addr_ntoa(&settings.se.usk_mask_addr));
  Paint_DrawString_EN(2, 52, buf, &Courier12R, WHITE, BLACK);
  
  Paint_DrawString_EN(2, 70, "Шлюз", &Courier12R, WHITE, BLACK);
  memset(buf, 0, 22);
  sprintf(buf, "%s", ip4addr_ntoa(&settings.se.usk_gateway_addr));
  Paint_DrawString_EN(2, 86, buf, &Courier12R, WHITE, BLACK);
  
  memset(buf, 0, 22);
  sprintf(buf, "Версия %d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_BUILD);
  Paint_DrawString_EN(2, 104, buf, &Courier12R, RED, BLACK);

  OLED_1in5_rgb_Display(BlackImage);
  
  //osThreadId_t id = osThreadGetId();
  
  //osThreadSetPriority(id, osPriorityBelowNormal);
  
  osDelay(10000);
/*
  osEventFlagsWait(event_flags, 2, osFlagsWaitAny|osFlagsNoClear, osWaitForever);
*/

/*
  Paint_ClearWindows(0, 0, OLED_1in5_RGB_WIDTH, OLED_1in5_RGB_HEIGHT, BLACK);
  Paint_DrawString_EN(0, 0, "Конвейер", &Courier12R, WHITE, BLACK);
  Paint_DrawString_EN(0, 13, "Пожаротушение", &Courier12R, WHITE, BLACK);
  Paint_DrawString_EN(0, 26, "Давление", &Courier12R, WHITE, BLACK);
  Paint_DrawString_EN(0, 39, "Камера", &Courier12R, WHITE, BLACK);
  Paint_DrawString_EN(0, 52, "Источник", &Courier12R, WHITE, BLACK);
  Paint_DrawString_EN(0, 65, "Температура", &Courier12R, WHITE, BLACK);
  Paint_DrawString_EN(0, 78, "Порог", &Courier12R, WHITE, BLACK);
*/
  
/*
  memset(buf, 0, 22);
  sprintf(buf, "Версия %d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_BUILD);
  Paint_DrawString_EN(0, 104, buf, &Courier12R, RED, BLACK);
  // draw image on the screen
  OLED_1in5_rgb_Display(BlackImage);
*/
  uint32_t tick = osKernelGetTickCount();
  
  while (1) {
#ifdef DEBUG
    WM_OLED = uxTaskGetStackHighWaterMark(NULL);
#endif
    tick += pdMS_TO_TICKS(300);
    
    status = osMessageQueueGet(oled_msg_queue, &oled_msg, NULL, 0U);
    if (status == osOK) {
      update = true;

      if (oled_msg.conv_count == 1) {
        screen_count = 2;
      } else if (oled_msg.conv_count == 2) {
        screen_count = 4;
      }
    }
    
    if (time_change_screen < HAL_GetTick()) {
       // display off
      WriteReg(0xae);
      osDelay(1000);
      // display on
      WriteReg(0xaf);
      //
      time_change_screen = HAL_GetTick() + HOLDING_TIME;
      
      screen_number++;
      if (screen_number > screen_count) {
        screen_number = 1;
      }
      update = true;
    }
    
    if (update == true) {
      if (time_init_display < HAL_GetTick()) {
        OLED_1in5_rgb_Init();
        OLED_1in5_rgb_Clear();
        time_init_display = HAL_GetTick() + INIT_TIME;
      }
      
      if (oled_msg.conv_count == 1) {
        if (screen_number == 1) {
          conv_head();
          conv_info(1, &oled_msg);
        } else {
          if (oled_msg.line2_status == 0) {
            adr_info(1, &oled_msg);
          } else {
            conv_info(1, &oled_msg);
          }
        }
      } else if (oled_msg.conv_count == 2) {
        if (screen_number == 1) {
          conv_head();
          conv_info(1, &oled_msg);
        }
        if (screen_number == 2) {
          conv_info(2, &oled_msg);
        }
        if (screen_number == 3) {
          if (oled_msg.line2_status == 0) {
            adr_info(1, &oled_msg);
          } else {
            conv_info(1, &oled_msg);
          }
        }
        if (screen_number == 4) {
          if (oled_msg.line2_status == 0) {
            adr_info(2, &oled_msg);
          } else {
            conv_info(2, &oled_msg);
          }
        }
      }
      
      // draw image on the screen
      OLED_1in5_rgb_Display(BlackImage);
      update = false;
    }
    osDelayUntil(tick);
  }
}
