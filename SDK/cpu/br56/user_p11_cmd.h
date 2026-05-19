#ifndef __USER_P11_EVENT_IRQ_H__
#define __USER_P11_EVENT_IRQ_H__


struct p11_event_handler {
    int event_type;
    int (*handler)(int event_type, u16 len, u8 *data);
};


#define P11_IRQ_EVENT_HANDLER(type, fn) \
 	static const struct p11_event_handler __event_handler_##fn sec(.p11_irq_handler) __attribute__((used)) = { \
		.event_type = type, \
		.handler = fn, \
	}

extern struct p11_event_handler p11_event_handler_begin[];
extern struct p11_event_handler p11_event_handler_end[];

#define list_for_each_p11_event_handler(p) \
	for (p = p11_event_handler_begin; p < p11_event_handler_end; p++)

//数据最大数据长度在p11 系统配置，用户可以灵活配置
//当前配置512 字节
extern void user_main_post_to_p11_system(u8 cmd, u16 len, u8 *data, u8 wait);

#endif
