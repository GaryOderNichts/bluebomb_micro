#ifndef _BLUEBOMB_MICRO_H_
#define _BLUEBOMB_MICRO_H_

#include <stdint.h>

enum {
    STAGE_INIT,
    STAGE_SERVICE_RESPONSE,
    STAGE_ATTRIB_RESPONSE,
    STAGE_ATTRIB_RESPONSE_DONE,
    STAGE_HAX_WAITING,
    STAGE_HAX,
    STAGE_UPLOAD_PAYLOAD,
    STAGE_JUMP_PAYLOAD,
};

int bluebomb_setup(uint32_t l2cb);

int bluebomb_get_stage(void);

#endif // _BLUEBOMB_MICRO_H_
