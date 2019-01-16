#include <stdio.h>
#include "esp_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/timer_group_struct.h"
#include "driver/periph_ctrl.h" 
#include "driver/timer.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <driver/adc.h>
#include "driver/i2c.h"
#define HASASSERT
#include "opentx.h"

#define I2C_MASTER_FREQ_HZ 400000
#define MCP23017_ADDR_KEYS 0x20
#define MCP23017_ADDR_SW 0x21

#define GPIO_INTR_PIN GPIO_NUM_21
#define ESP_INTR_FLAG_DEFAULT 0

static const char *TAG = "keys_driver.cpp";
static xQueueHandle gpio_evt_queue = NULL;
portMUX_TYPE i2cMux= portMUX_INITIALIZER_UNLOCKED;

uint8_t IRAM_ATTR readI2CGPIO(uint8_t addr, uint8_t port);

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint8_t gpio_num = readI2CGPIO(MCP23017_ADDR_SW,0x11); //INTCAPB
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

void initKeys(){

    i2c_config_t conf;
    memset(&conf, 0, sizeof(conf));

    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = GPIO_NUM_23;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = GPIO_NUM_22;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_param_config(I2C_NUM_0, &conf);
    esp_err_t ret = i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
    if(ESP_OK==ret){
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (MCP23017_ADDR_KEYS << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(cmd,0x0A,true); //IOCON
        i2c_master_write_byte(cmd,BIT(5),true);//IOCON.SEQOP
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (MCP23017_ADDR_KEYS << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(cmd,0x0C,true); 
        i2c_master_write_byte(cmd,0xFF,true); //GPPUA
        i2c_master_write_byte(cmd,0xFF,true); //GPPUB
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (MCP23017_ADDR_KEYS << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(cmd,0x02,true); 
        i2c_master_write_byte(cmd,0xFF,true); //IOPOLA
        i2c_master_write_byte(cmd,0xFF,true); //IOPOLB
        i2c_master_stop(cmd);
        ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 2);
        if(ESP_OK!=ret){
            ESP_LOGE(TAG,"i2c write error.");
        }
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (MCP23017_ADDR_SW << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(cmd,0x0A,true); //IOCON
        i2c_master_write_byte(cmd,BIT(5),true);//IOCON.SEQOP
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (MCP23017_ADDR_SW << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(cmd,0x0C,true); 
        i2c_master_write_byte(cmd,0xFF,true); //GPPUA
        i2c_master_write_byte(cmd,0xFF,true); //GPPUB
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (MCP23017_ADDR_SW << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(cmd,0x02,true); 
        i2c_master_write_byte(cmd,0xFF,true); //IOPOLA
        i2c_master_write_byte(cmd,0xFF,true); //IOPOLB
        i2c_master_write_byte(cmd,0x00,true); //GPINTENA
        i2c_master_write_byte(cmd,0x0F,true); //GPINTENB
        i2c_master_write_byte(cmd,0x00,true); //DEFVALA
        i2c_master_write_byte(cmd,0x0F,true); //DEFVALB
        i2c_master_write_byte(cmd,0x00,true); //INTCONA
        i2c_master_write_byte(cmd,0x0F,true); //INTCONB
        i2c_master_stop(cmd);
        ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 2);
        if(ESP_OK!=ret){
            ESP_LOGE(TAG,"i2c write error.");
        }
        i2c_cmd_link_delete(cmd);

        //Configure interrupt
        gpio_config_t io_conf;
        io_conf.intr_type = (gpio_int_type_t)GPIO_PIN_INTR_NEGEDGE;
        io_conf.pin_bit_mask = 1ULL<<GPIO_INTR_PIN;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_up_en = (gpio_pullup_t)1;
        gpio_config(&io_conf);
        gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
        gpio_evt_queue = xQueueCreate(10, sizeof(uint8_t));
        gpio_isr_handler_add(GPIO_INTR_PIN, gpio_isr_handler, (void*) GPIO_INTR_PIN);
    } else {
        ESP_LOGE(TAG,"i2c driver install error.");
    }
    
}

uint16_t IRAM_ATTR readI2CGPIO(uint8_t addr){
    uint8_t dataa;
    uint8_t datab;
    return 0;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true);
    i2c_master_write_byte(cmd, 0x12, true);
    i2c_master_read(cmd, &dataa, 1, I2C_MASTER_ACK);
    i2c_master_read(cmd, &datab, 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    vTaskEnterCritical(&i2cMux);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 2);
    vTaskExitCritical(&i2cMux);
    if(ESP_OK!=ret){
        ESP_LOGE(TAG,"i2c read error.");
    }
    i2c_cmd_link_delete(cmd);
    return dataa & ((int16_t)datab)<<8;
}

uint8_t IRAM_ATTR readI2CGPIO(uint8_t addr, uint8_t port){
    uint8_t data;
    return 0;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true);
    i2c_master_write_byte(cmd, port, true);
    i2c_master_read(cmd, &data, 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    vTaskEnterCritical(&i2cMux);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 2);
    vTaskExitCritical(&i2cMux);
    if(ESP_OK!=ret){
        ESP_LOGE(TAG,"i2c read error.\n");
    }
    i2c_cmd_link_delete(cmd);
    return data;
}


void IRAM_ATTR readKeysAndTrims(){
    uint16_t keys_input = readI2CGPIO(MCP23017_ADDR_KEYS) ;
    uint32_t i;

#if ROTARY_ENCODERS > 0
    keys[BTN_REa].input(keys_input | BIT(6));
#endif

    uint8_t index = 0;
    for (i = 0; i < 6; i++) {
        keys[index].input(keys_input & (0x0100 << i));
        ++index;
    }

    for (i = 1; i < 256; i <<= 1) {
        keys[index].input(keys_input & i);
        ++index;
    }

    if ((keys_input & ~(uint16_t)(BIT(6)|BIT(7))) && (g_eeGeneral.backlightMode & e_backlight_mode_keys)) {
        // on keypress turn the light on
        backlightOn();
    }
}

bool keyDown()
{
    return readI2CGPIO(MCP23017_ADDR_KEYS) & ~(uint16_t)(BIT(7));
}

uint8_t switchState(uint8_t index)
{
    uint8_t result = 0;
    uint8_t port_input = readI2CGPIO(MCP23017_ADDR_SW, 0x12) ;
    switch (index) {
    case SW_ELE:
        result = !(port_input & (1<<INP_ElevDR));
        break;

    case SW_AIL:
        result = !(port_input & (1<<INP_AileDR));
        break;

    case SW_RUD:
        result = !(port_input & (1<<INP_RuddDR));
        break;

        //       INP_C_ID1  INP_C_ID2
        // ID0      0          1
        // ID1      1          1
        // ID2      1          0
    case SW_ID0:
        result = !(port_input & (1<<INP_ID1));
        break;

    case SW_ID1:
        result = (port_input & (1<<INP_ID1))&& (port_input & (1<<INP_ID2));
        break;

    case SW_ID2:
        result = !(port_input & (1<<INP_ID2));
        break;

    case SW_GEA:
        result = !(port_input & (1<<INP_Gear));
        break;

    case SW_THR:
        result = !(port_input & (1<<INP_ThrCt));
        break;

    case SW_TRN:
        result = !(port_input & (1<<INP_Trainer));
        break;

    default:
        break;
    }

    return result;
}


uint8_t trimDown(uint8_t idx){
    uint8_t port_input = readI2CGPIO(MCP23017_ADDR_KEYS, 0x12);
    return port_input & (1 << idx);  
}
