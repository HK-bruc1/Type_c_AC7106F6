#ifndef RESOURCE_MONITOR_H
#define RESOURCE_MONITOR_H

void resource_monitor_snapshot(const char *reason);
void resource_monitor_observe(void);
void resource_monitor_dump_key_tasks(const char *reason);

#endif /* RESOURCE_MONITOR_H */
