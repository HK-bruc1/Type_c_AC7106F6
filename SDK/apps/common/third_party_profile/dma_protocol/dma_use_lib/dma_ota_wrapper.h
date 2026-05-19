#ifndef __DMA_OTA_WRAPPER_H__
#define __DMA_OTA_WRAPPER_H__

#include <stddef.h>
#define TWS_OTA
#define DMA_OTA_DATA_BUFFER_SIZE_FOR_BURNING_DEFAULT_SIZE   (4096)
#define DMA_OTA_FLASH_SECTOR_SIZE                           (4096)


//#define DMA_OTA_DEBUG
#ifdef DMA_OTA_DEBUG
#include "stdio.h"
#define DMA_OTA_LOG_E(fmt, ...)         printf("[DMA_OTA] "fmt, ##__VA_ARGS__)
#define DMA_OTA_LOG_D(fmt, ...)         printf("[DMA_OTA] "fmt, ##__VA_ARGS__)
#define DMA_OTA_LOG_I(fmt, ...)         printf("[DMA_OTA] "fmt, ##__VA_ARGS__)
#define DMA_OTA_DUMP8(str, buf, cnt)    //DUMP8(str, buf, cnt)
#define DMA_OTA_ASSERT(cond, str, ...)  //ASSERT(cond, str, ##__VA_ARGS__)
#else
#define DMA_OTA_LOG_E(fmt, ...)
#define DMA_OTA_LOG_D(fmt, ...)
#define DMA_OTA_LOG_I(fmt, ...)
#define DMA_OTA_DUMP8(str, buf, cnt)
#define DMA_OTA_ASSERT(cond, str, ...)
#endif


typedef enum {
    DMA_OTA_TWS_UNKNOWN = 0,
    DMA_OTA_TWS_MASTER,
    DMA_OTA_TWS_SLAVE,
} DMA_OTA_TWS_ROLE_TYPE;

//typedef void (*transmit_data_cb)(uint16_t size, uint8_t *data);

typedef void (*dma_ota_timeout_cb)(void *data);

typedef struct {
    /**
    * @brief DMA OTA ms毫秒之后调用回调函数
    *
    * @param[in] cb      回调函数
    * @param[in] data    调用回调函数时传给回调函数的参数
    * @param[in] ms      多少毫秒后调用此函数
    *
    * @return 返回说明
    *        -true 创建timer成功
    *        -false 创建timer失败
    * @note ms毫秒之后调用cb回调函数，参数传data
    */
    bool (*callback_by_timeout)(dma_ota_timeout_cb cb,
                                void *data, uint32_t ms);

    /**
    * @brief 获取DMA设备版本号
    *
    * @param[out] fw_rev_0      4位固件版本号第一位
    * @param[out] fw_rev_1      4位固件版本号第二位
    * @param[out] fw_rev_2      4位固件版本号第三位
    * @param[out] fw_rev_3      4位固件版本号第四位
    *
    * @return 返回说明
    *        -false 获取版本号失败
    *        -true 获取版本号成功
    * @note 使用小度APP的DMA及OTA功能要求设备版本号必须4位，格式为xx.xx.xx.xx \n
    * 每位取值范围为0-99,同一产品禁止不同固件使用相同版本号
    */
    bool (*get_firmeware_version)(uint8_t *fw_rev_0, uint8_t *fw_rev_1,
                                  uint8_t *fw_rev_2, uint8_t *fw_rev_3);

    /**
    * @brief DMA协议栈内存申请
    *
    * @param[in] size    内存申请大小
    *
    * @return 返回说明
    *        -NULL 表示内存申请失败
    *        -非NULL 表示内存申请成功
    * @note 该内存申请用于DMA协议栈，内存申请峰值不超过4KB \n
    * 该接口应保证DMA设备在任何使用模式下均可申请内存，线程安全
    */
    void *(*dma_ota_heap_malloc)(uint32_t size);

    /**
    * @brief DMA协议栈内存释放
    *
    * @param[in] ptr    释放内存地址
    *
    * @note 该接口应保证DMA设备在任何使用模式下均可释放内存，线程安全
    */
    void (*dma_ota_heap_free)(void *ptr);

    /**
    * @brief 获取DMA设备与手机协商的MTU
    *
    * @param[out] mtu   DMA设备上行音频的MTU
    *
    * @return 返回说明
    *        -false 获取DMA设备与手机协商的MTU失败
    *        -true 获取DMA设备与手机协商的MTU成功
    *
    * @note TWS类设备Master设备调用该接口获取和手机之间的MTU \n
    * Slave设备不调用该接口
    *
    */
    bool (*get_mobile_mtu)(uint32_t *mtu);

    /**
    * @brief 获取TWS设备之间的MTU
    *
    * @param[out] mtu   DMA设备上行音频的MTU
    *
    * @return 返回说明
    *        -false 获取TWS设备之间的MTU失败
    *        -true 获取TWS设备之间的MTU成功
    *
    * @note TWS耳机设备两侧调用该接口须有相同的返回值
    *           非TWS耳机设备，只升级单个设备可以不注册此回调函数
    */
    bool (*get_peer_mtu)(uint32_t *mtu);

    /**
    * @brief 获取TWS设备的角色
    *
    * @return 返回说明
    *        -DMA_OTA_TWS_UNKNOWN 未知类型
    *        -DMA_OTA_TWS_MASTER   主耳Master
    *        -DMA_OTA_TWS_SLAVE    从耳Slave
    *
    * @note OTA过程中会调用该接口，须获取到非UNKNOWN的角色类型
    *           非TWS耳机设备，只升级单个设备可以不注册此回调函数
    */
    DMA_OTA_TWS_ROLE_TYPE(*get_tws_role)(void);

    /**
    * @brief 获取TWS左右耳两只耳机是否连接
    *
    * @return 返回说明
    *        -false 左右耳未连接
    *        -true 左右耳已连接
    *
    * @note OTA过程中会调用该接口
    *           非TWS耳机设备，只升级单个设备可以不注册此回调函数
    */
    bool (*is_tws_link_connected)(void);

    /**
    * @brief 发送数据给手机
    *
    * @param[in] size    发送数据的大小
    * @param[in] data    发送的数据
    *
    * @note OTA SDK与手机之间传输数据，只有主耳机调用 \n
    */
    void (*send_data_to_mobile)(uint16_t size, uint8_t *data);

    /**
    * @brief 发送数据给另一只耳机
    *
    * @param[in] size    发送数据的大小
    * @param[in] data    发送的数据
    *
    * @note OTA SDK两耳机之间传输数据，两耳机都会调用
    *           非TWS耳机设备，只升级单个设备可以不注册此回调函数
    */
    void (*send_data_to_tws_peer)(uint16_t size, uint8_t *data);

    /**
    * @brief 存储OTA相关的数据到flash，必须同步写入
    *
    * @param[in] size    写入数据的大小
    * @param[in] data    写入的数据
    *
    * @note OTA过程中存储OTA相关的数据
    *           现在数据大小是24Bytes，需要预留8Bytes
    */
    void (*save_ota_info)(uint16_t size, const uint8_t *data);

    /**
    * @brief 从flash读取OTA SDK存储的数据
    *
    * @param[in] size    要读取数据的大小，等于写入数据的大小
    * @param[in] buffer  存放数据的buffer
    *
    * @note 读取OTA过程中存储的OTA相关数据
    */
    void (*read_ota_info)(uint16_t size, uint8_t *buffer);

    /**
    * @brief 获取缓存Image数据的的buffer
    *
    * @param[out] buffer      buffer的地址
    * @param[in] buffer       需要的buffer大小(会传入于flash sector大小4096Bytes)
    *
    * @return 返回说明
    *        实际buffer大小(最好等于flash sector大小)
    *
    * @note 用于缓存收到的image数据，buffer满后调用write_image_data_to_flash回调一次写入到flash
    */
    uint32_t (*get_flash_data_buffer)(uint8_t **buffer, uint32_t size);

    /**
    * @brief 写入OTA Image数据到flash
    *
    * @param[in] size           写入数据的大小
    * @param[in] data           写入的数据
    * @param[in] image_offset   从此image offset处开始写入
    * @param[in] sync           数据是否同步写入flash
    *
    * @note OTA Image数据写入到flash
    *       get_flash_data_buffer 的buffer满了会写入
    *       image_offset不一定是累加的，可能会从较小的image_offset重新写入数据
    *       此接口内的代码实现，写数据前要先擦除Flash
    *       若get_flash_data_buffer是flash sector 4KB大小
    *           此接口的image_offset会是4KB对齐
    *           每次写入大小会是4KB
    */
    void (*write_image_data_to_flash)(uint16_t size, uint8_t *data, uint32_t image_offset, bool sync);

    /**
    * @brief 从flash内读取OTA Image数据
    *
    * @param[in] size           要读取数据的大小
    * @param[in] buffer         存放数据的buffer
    * @param[in] image_offset   从此image offset处开始读取
    *
    * @note 从flash内读取OTA Image数据
    */
    void (*read_image_data_from_flash)(uint16_t size, uint8_t *buffer, uint32_t image_offset);

    /**
    * @brief 从flash内擦除OTA Image数据
    *
    * @param[in] size           要擦除数据的大小
    * @param[in] image_offset   从此image offset处开始擦除
    *
    * @note 从flash内擦除OTA Image数据
    *           擦除是4KB对齐的，DMA_OTA_FLASH_SECTOR_SIZE 的倍数
    *           size是 DMA_OTA_FLASH_SECTOR_SIZE 的倍数(image的最后一部分不是)
    *           image_offset 是 DMA_OTA_FLASH_SECTOR_SIZE 的倍数
    */
    void (*erase_image_data_from_flash)(uint16_t size, uint32_t image_offset);

    /**
    * @brief 进入OTA模式
    *
    * @note 开始OTA后会回调此函数
    */
    void (*enter_ota_state)(void);

    /**
    * @brief 退出OTA模式
    *
    * @param[in] error      是否异常退出
    *           -true   异常退出
    *           -false  正常退出
    *
    * @note OTA异常退出或者正常结束后会回调此函数
    */
    void (*exit_ota_state)(bool error);
    /**
     * @brief 开始OTA之后，写Flash之前set Image大小
     *
     * @param[in] total_image_size      Image总大小
     *
     * @note #DMA_OTA_OPER.enter_ota_state之后
     *       #DMA_OTA_OPER.write_image_data_to_flash之前
     *       会调用此函数，通知设备新固件大小
     *       设备可以用做计算分区偏移等
     */
    void (*set_new_image_size)(uint32_t total_image_size);
    /**
    * @brief OTA数据传输完成
    *
    * @param[in] total_image_size      Image总大小
    *
    * @return 返回说明
    *           -true Image固件没有问题
    *           -false Image固件有问题
    *
    * @note OTA数据传输完成后回调此函数，可以在此函数内做固件的校验
    *           SDK内部会对整个image数据读flash做crc32校验，
    *           crc32校验的时间加上此函数返回的时间不可超过10s，
    *           OTA协议返回数据包给APP的超时时间是10s，超过10s会OTA失败。
    */
    bool (*data_transmit_complete_and_check)(uint32_t total_image_size);

    /**
    * @brief OTA数据check完成后的回调
    *
    * @param[in] check_pass      耳机固件校验成功与否
    *               - true 固件校验没问题（若为TWS耳机，两边都没问题）
    *               - true 固件校验有问题（若为TWS耳机，至少一边有问题）
    *
    * @note OTA数据传输并校验完成后回调此函数
    */
    void (*image_check_complete)(bool check_pass);

    /**
    * @brief 应用新的固件，系统重启，使用新的固件启动
    *
    * @return 返回说明
    *           -true   应用新的固件成功
    *           -false  应用新的固件失败
    *
    * @note 应用新的固件，系统重启，使用新的固件启动。
    *       只有固件校验通过，才会走到这个调用。
    *       此接口需要返回，SDK需要在返回后发response给APP
    */
    bool (*image_apply)(void);

    // TODO: about role switch
} DMA_OTA_OPER;


#ifdef __cplusplus
extern "C" {
#endif


/**
* @brief 停止OTA，调用后，APP将显示升级失败
*
* @note 停止OTA，以下情况需要调用此接口
*       跟APP的连接断开，主耳机调用
*       跟另一边耳机断开，主从耳机都调用
*       准备进行主从切换，主从切换前主耳机调用
*       其他需要停止OTA的情况，也可以调用此接口，需要主耳机调用
*/
void dma_ota_stop_ota(void);

/**
* @brief 接收手机发送的数据
*
* @param[in] size           数据的大小
* @param[in] data           数据
* @param[in] isViaBle       数据是否通过BLE传输
* @param[in] isRelayToSlave 数据是否需要主耳机转发给从耳机
*
* @note
*      -# OTA SDK接收手机的数据，若从耳机能通过此接口直接收到手机的数据则 isRelayToSlave = false \n
*           当isRelayToSlave设置为true的时候，SDK会通过 #DMA_OTA_OPER.send_data_to_tws_peer 回调函数 \n
*           发送OTA数据给另一只耳机，另一只耳机通过 #dma_ota_recv_tws_data 函数接收数据 \n
*      -# 非TWS耳机设备，isRelayToSlave参数传false
*/
void dma_ota_recv_mobile_data(uint16_t size, uint8_t *data, bool isViaBle, bool isRelayToSlave);

/**
* @brief 接收另一只耳机发送的数据
*
* @param[in] size           数据的大小
* @param[in] data           数据
*
* @note OTA SDK接收另一只耳机发送的数据
*       非TWS耳机设备，不用关心此接口
*/
void dma_ota_recv_tws_data(uint16_t size, uint8_t *data);

/**
* @brief GET OTA库的版本号
*
* @note 可随时调用此接口，获取版本号字符串，格式类似 "1.0.1.0"
*/
const char *dma_ota_get_lib_version_string(void);

/**
* @brief 注册回掉函数
*
* @param[in] operation      包含所有回调函数的结构体，内部只引用此结构体的指针
*
* @note 向OTA SDK内注册所有的回调函数，内部只引用此结构体的指针，调用此函数后结构体内存不要释放
*       必须先调用此接口注册函数之后，再调用 #dma_ota_init 函数
*/
void dma_ota_register_operation(DMA_OTA_OPER *operation);

/**
* @brief 初始化OTA SDK
*
* @param[in] max_image_size 最大image大小，单位Byte，ota文件不可超过此大小
*
* @note 初始化OTA SDK，在开机的时候调用
*         必须在调用此函数之前调用 #dma_ota_register_operation 注册回调函数，
*         此函数内逻辑会调用几个回调函数用来初始化buffer和获取版本号
*/
void dma_ota_init(uint32_t max_image_size);


#ifdef __cplusplus
}
#endif

#endif // __DMA_OTA_WRAPPER_H__
