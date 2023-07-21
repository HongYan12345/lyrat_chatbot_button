

#ifndef _GOOGLE_TRANSLATE_H_
#define _GOOGLE_TRANSLATE_H_

#include "esp_err.h"
#include "audio_event_iface.h"

#ifdef __cplusplus
extern "C" {
#endif


//void http_get_token();
char *send_problema(const char *text, int *pos);
char *send_text(int tem_or_hum, int time, int range);
char *send_text_num(int tem_or_hum, int time, int range);
char *send_text2(int tem_or_hum, int time, int range);
char *send_error();
#ifdef __cplusplus
}
#endif

#endif
