#ifndef _hx3918_FACTORY_TEST_H_
#define _hx3918_FACTORY_TEST_H_

#include <stdint.h>
#include <stdbool.h>

#define FT_OPEN_G_LED_LTH     3000
#define FT_OPEN_R_LED_LTH     3000
#define FT_OPEN_IR_LED_LTH    3000

#define FT_CARD_G_LED_HTH     200000
#define FT_CARD_R_LED_HTH     400000
#define FT_CARD_IR_LED_HTH    400000

typedef enum {
    LEAK_LIGHT_TEST = 1,
    GRAY_CARD_TEST = 2,
    FT_INT_TEST = 3
} TEST_MODE_t;

typedef struct {
    int32_t green_data_final;
    int32_t red_data_final;
    int32_t ir_data_final;
} factory_result_t;


//void hx3918_factory_test_config(void);
void hx3918_factory_ft_leak_light_test_config(void);
void hx3918_factory_ft_card_test_config(void);
void hx3918_factory_ft_int_test_config(void);

bool hx3918_factory_test_read(int32_t *ps_data);
factory_result_t hx3918_factroy_test(uint32_t  test_mode);


#endif // _hx3918_FACTORY_TEST_H_




