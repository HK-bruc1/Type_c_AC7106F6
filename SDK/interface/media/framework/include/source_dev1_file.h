#ifndef SOURCE_DEV1_FILE_H
#define SOURCE_DEV1_FILE_H

#include "jlstream.h"

struct source_dev1_provider {
    void *priv;
    struct stream_fmt format;
    u16 frame_bytes;
    int (*read)(void *priv, u8 *data, u16 len);
};

int source_dev1_provider_register(
    const struct source_dev1_provider *provider);
void source_dev1_provider_unregister(
    const struct source_dev1_provider *provider);
void source_dev1_data_notify(void);

#endif
