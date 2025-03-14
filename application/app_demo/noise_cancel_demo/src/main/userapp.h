#ifndef AUDIO_TEST_H
#define AUDIO_TEST_H
#include <msg_manager.h>

enum USERAPP_EVENT_TYPE {
    /* test player message */
	MSG_USERAPP_EVENT = MSG_APP_MESSAGE_START,
};

enum USERAPP_CMD {
	NC_START,
	NC_STOP,
};

void nc_start(char *save_url);
void nc_stop(void);

#endif

