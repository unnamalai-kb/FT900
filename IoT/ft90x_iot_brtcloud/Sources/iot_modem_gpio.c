#include <ft900.h>
#include "tinyprintf.h"

/* FreeRTOS Headers. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

/* netif Abstraction Header. */
#include "net.h"

/* IOT Headers. */
#include <iot_config.h>
#include "iot/iot.h"
#include "iot/iot_utils.h"

/* IoT Modem */
#include "iot_modem.h"
#include "json.h"


#include <string.h>
#include <stdlib.h>





///////////////////////////////////////////////////////////////////////////////////
#define DEBUG
#ifdef DEBUG
#define DEBUG_PRINTF(...) do {CRITICAL_SECTION_BEGIN;tfp_printf(__VA_ARGS__);CRITICAL_SECTION_END;} while (0)
#else
#define DEBUG_PRINTF(...)
#endif
///////////////////////////////////////////////////////////////////////////////////



extern TaskHandle_t g_iot_app_handle; // used by iot_modem_gpio_process()
extern iot_handle g_handle;           // used to publish packets
extern GPIO_PROPERTIES g_oGpioProperties[GPIO_COUNT];
extern uint8_t g_ucGpioEnabled[GPIO_COUNT];

static uint8_t g_aucGpioIndex[GPIO_COUNT] = { 0, 1, 2, 3 };
static TimerHandle_t g_oGpioTimer[GPIO_COUNT] = { NULL, NULL, NULL, NULL };
static void ISR_gpio();





static inline uint8_t GPIO_OUTPUT_PIN(int index)
{
    if (index < 3) {
        return GPIO_OUTPUT_PIN_0 + index;
    }
    return GPIO_OUTPUT_PIN_3;
}

void iot_modem_gpio_init(int voltage)
{
    // Initialize the GPIO Input pins
    for (int i=0; i<GPIO_COUNT; i++) {
        gpio_dir(GPIO_INPUT_PIN_0 + i, pad_dir_input);
    }

    // Initialize the GPIO Output pins
    for (int i=0; i<GPIO_COUNT; i++) {
        gpio_dir(GPIO_OUTPUT_ENABLE_PIN_0 + i, pad_dir_output);

        // disable output enable pins at initialization
        gpio_write(GPIO_OUTPUT_ENABLE_PIN_0 + i, 0);

        gpio_dir(GPIO_OUTPUT_PIN(i), pad_dir_output);
    }

    // Initialize the GPIO voltage pins
    gpio_dir(GPIO_VOLTAGE_PIN_0, pad_dir_output);
    gpio_dir(GPIO_VOLTAGE_PIN_1, pad_dir_output);
    iot_modem_gpio_set_voltage(voltage);
}

void iot_modem_gpio_enable_interrupt()
{
    interrupt_attach(interrupt_gpio, (uint8_t)interrupt_gpio, ISR_gpio);
}

void iot_modem_gpio_set_voltage(int voltage)
{
    // TODO: wait for correction from Sree
    if (!voltage) {
        // Set to 3.3 V
        gpio_write(GPIO_VOLTAGE_PIN_0, 0);
        gpio_write(GPIO_VOLTAGE_PIN_1, 1);
    }
    else {
        // Set to 5 V
        gpio_write(GPIO_VOLTAGE_PIN_0, 1);
        gpio_write(GPIO_VOLTAGE_PIN_1, 0);
    }
}

void iot_modem_gpio_process(int number, int activate)
{
    // This is called by the main task after receiving notification from timer
    char topic[MQTT_MAX_TOPIC_SIZE] = {0};
    char payload[16] = {0};

    tfp_snprintf( topic, sizeof(topic), TOPIC_GPIO, PREPEND_REPLY_TOPIC, iot_utils_getdeviceid(), number, MENOS_DEFAULT);
    tfp_snprintf( payload, sizeof(payload), PAYLOAD_TRIGGER_GPIO_NOTIFICATION, activate);
    iot_publish( g_handle, topic, payload, strlen(payload), 1 );
    //DEBUG_PRINTF("PUB %s (%d) %s (%d)\r\n\r\n", topic, strlen(topic), payload, strlen(payload));
}

// Need to copy the structure here in order to get the pvTimerID parameter
typedef struct tmrTimerControl /* The old naming convention is used to prevent breaking kernel aware debuggers. */
{
    const char                *pcTimerName;        /*<< Text name.  This is not used by the kernel, it is included simply to make debugging easier. */ /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
    ListItem_t                xTimerListItem;        /*<< Standard linked list item as used by all kernel features for event management. */
    TickType_t                xTimerPeriodInTicks;/*<< How quickly and often the timer expires. */
    void                     *pvTimerID;            /*<< An ID to identify the timer.  This allows the timer to be identified when the same callback is used for multiple timers. */
    TimerCallbackFunction_t    pxCallbackFunction;    /*<< The function that will be called when the timer expires. */
    #if( configUSE_TRACE_FACILITY == 1 )
        UBaseType_t            uxTimerNumber;        /*<< An ID assigned by trace tools such as FreeRTOS+Trace */
    #endif
    uint8_t                 ucStatus;            /*<< Holds bits to say if the timer was statically allocated or not, and if it is active or not. */
} xTIMER;

static void gpio_timer( TimerHandle_t xTimer )
{
    // Inform the main task to publish the notification packet
    uint8_t index = *((uint8_t*)xTimer->pvTimerID);

    DEBUG("GPIO timer %d\r\n", index);

    uint8_t status = gpio_read(GPIO_INPUT_PIN_0 + index);

    // If status is still high, send activation, otherwise send deactivation
    if (g_oGpioProperties[index].m_ucMode == GPIO_MODES_INPUT_HIGH_LEVEL ||
        g_oGpioProperties[index].m_ucMode == GPIO_MODES_INPUT_HIGH_EDGE) {
        if (status) {
            // Set bit for GPIO X and bit for activation
            xTaskNotify(
                g_iot_app_handle,
                TASK_NOTIFY_BIT(TASK_NOTIFY_BIT_GPIO0 + index) |
                TASK_NOTIFY_BIT(TASK_NOTIFY_BIT_ACTIVATION),
                eSetBits);
        }
        else {
            // Set bit for GPIO X
            xTaskNotify(g_iot_app_handle,
                TASK_NOTIFY_BIT(TASK_NOTIFY_BIT_GPIO0 + index),
                eSetBits);
        }
    }
    // If status is still low, send activation, otherwise send deactivation
    else if (g_oGpioProperties[index].m_ucMode == GPIO_MODES_INPUT_LOW_LEVEL ||
               g_oGpioProperties[index].m_ucMode == GPIO_MODES_INPUT_LOW_EDGE) {
        if (!status) {
            // Set bit for GPIO X and bit for activation
            xTaskNotify(
                g_iot_app_handle,
                TASK_NOTIFY_BIT(TASK_NOTIFY_BIT_GPIO0 + index) |
                TASK_NOTIFY_BIT(TASK_NOTIFY_BIT_ACTIVATION),
                eSetBits);
        }
        else {
            // Set bit for GPIO X
            xTaskNotify(g_iot_app_handle,
                TASK_NOTIFY_BIT(TASK_NOTIFY_BIT_GPIO0 + index),
                eSetBits);
        }
    }
}

static inline void gpio_create_timer(int index)
{
    // Create the timer given the alert type and alert period
    char acTimerName[8] = {0};
    tfp_snprintf( acTimerName, sizeof(acTimerName), "GPIO%d", index );

    g_oGpioTimer[index] = xTimerCreate(acTimerName,
        pdMS_TO_TICKS(g_oGpioProperties[index].m_ulAlertperiod),
        (BaseType_t)g_oGpioProperties[index].m_ucAlert,
        &g_aucGpioIndex[index],
        gpio_timer
        );
    if (!g_oGpioTimer[index]) {
        DEBUG_PRINTF( "xTimerCreate GPIO %d failed\r\n", index );
    }
}

static inline void gpio_delete_timer(int index)
{
    if (g_oGpioTimer[index] != NULL) {
        if ( xTimerDelete( g_oGpioTimer[index], pdMS_TO_TICKS(1000)) != pdTRUE ) {
            DEBUG_PRINTF( "xTimerDelete GPIO %d failed\r\n", index );
        }
        g_oGpioTimer[index] = NULL;
    }
}

static inline void gpio_create_timer_or_interrupt(int index, uint8_t pin, gpio_int_edge_t edge)
{
    // If already high, start the timer
    // Else, run an ISR to wait until it goes high
    // If already low, start the timer
    // Else, run an ISR to wait until it goes low
    if (gpio_read(pin) == edge) {
        gpio_create_timer(index);
    }
    else {
        gpio_interrupt_enable(pin, edge);
    }
}

static inline void gpio_output_set_level(uint8_t pin, uint8_t state)
{
	gpio_write(pin, state);
}

static inline void gpio_output_set_pulse(uint8_t pin, uint8_t state, uint32_t width)
{
    gpio_write(pin, state);
    delayms(width);
    gpio_write(pin, !state);
}

static inline void gpio_output_set_clock(uint8_t pin, uint8_t state, uint32_t mark, uint32_t space, uint32_t count)
{
    for (int i=0; i<count; i++) {
        gpio_write(pin, state);
        delayms(mark);
        gpio_write(pin, !state);
        delayms(space);
    }
}

int iot_modem_gpio_enable(GPIO_PROPERTIES* properties, int index, int enable)
{
    uint8_t pin = 0;

    if (properties->m_ucDirection == pad_dir_input) {
        // If INPUT pin, use GPIO_INPUT_PIN_0 as base address
        pin = GPIO_INPUT_PIN_0 + index;

        if (enable) {
            if (!properties->m_ulAlertperiod) {
                // invalid parameter
                return 0;
            }

            // HIGH LEVEL/EDGE
            if (properties->m_ucMode == GPIO_MODES_INPUT_HIGH_LEVEL ||
                properties->m_ucMode == GPIO_MODES_INPUT_HIGH_EDGE) {
                gpio_create_timer_or_interrupt(index, pin, gpio_int_edge_raising);
            }
            // LOW LEVEL/EDGE
            else if (properties->m_ucMode == GPIO_MODES_INPUT_LOW_LEVEL ||
                     properties->m_ucMode == GPIO_MODES_INPUT_LOW_EDGE) {
                gpio_create_timer_or_interrupt(index, pin, gpio_int_edge_falling);
            }
        }
        else {
            gpio_interrupt_disable(pin);
            gpio_delete_timer(index);
        }
    }
    else {

        // If OUTPUT pin, use GPIO_OUTPUT_PIN_0 as base address
        pin = GPIO_OUTPUT_PIN(index);

        if (enable) {
            // The data (level/pulse/clock) is written to the dedicated output pin while the configuration is enabled
            if (properties->m_ucMode == GPIO_MODES_OUTPUT_LEVEL) {
               	gpio_output_set_level(pin, properties->m_ucPolarity);
            }
            else if (properties->m_ucMode == GPIO_MODES_OUTPUT_PULSE) {
               	gpio_output_set_pulse(pin, properties->m_ucPolarity, properties->m_ulWidth);
            }
            else if (properties->m_ucMode == GPIO_MODES_OUTPUT_CLOCK) {
            	// TODO: this might need to be run in a task
               	gpio_output_set_clock(pin, properties->m_ucPolarity, properties->m_ulMark, properties->m_ulSpace, properties->m_ulCount);
            }
        }
        else {
            // The pin state is returned to inactive state when the configuration is disabled.
           	gpio_output_set_level(pin, !properties->m_ucPolarity);
        }
    }

    return 1;
}

void iot_modem_gpio_set_properties(int index, int direction, int polarity)
{
    // When a pin is configured as input, firmware will disable the output_enable
    // and remains disabled regardless of enable/disable configuration operation.
    // For a pin configured as output, the output_enable is enabled
    // and remains enabled throughout until the configuration is changed to input.
    // The output_enable is not altered during configuration enable/disable operations.

    // if input, disable the output enable pin
    // if output, enable the output enable pin
    gpio_write(GPIO_OUTPUT_ENABLE_PIN_0 + index, direction);

    // if output, set pin to inactive state (opposite of polarity)
    if (direction == pad_dir_output) {
        gpio_write(GPIO_OUTPUT_PIN(index), !polarity);
    }
}

void iot_modem_gpio_get_status(uint8_t* status, GPIO_PROPERTIES* properties, uint8_t* enabled)
{
    // With dedicated input pins, it is possible to read live status any time
    // regardless of whether configuration is enabled/disabled and whether a pin is input or output.
    for (int i=0; i<GPIO_COUNT; i++) {
        if (properties[i].m_ucDirection == pad_dir_input) {
            status[i] = gpio_read(GPIO_INPUT_PIN_0 + i);
        }
        else {
            status[i] = gpio_read(GPIO_OUTPUT_PIN(i));
        }
    }
}

static void ISR_gpio()
{
    uint8_t pin = 0;

    for (int i=0; i<GPIO_COUNT; i++) {
        if (g_ucGpioEnabled[i] && !g_oGpioTimer[i]) {
            // INPUT direction
            if (g_oGpioProperties[i].m_ucDirection == pad_dir_input) {

                pin = GPIO_INPUT_PIN_0 + i;

                // For HIGH-LEVEL
                if (g_oGpioProperties[i].m_ucMode == GPIO_MODES_INPUT_HIGH_LEVEL) {
                    if (gpio_read(pin) != 0) {
                        // debounce
                        delayms(5);
                        if (gpio_read(pin) != 0) {
                            gpio_interrupt_disable(pin);
                            gpio_create_timer(i);
                        }
                    }
                }
                // For LOW-LEVEL
                else if (g_oGpioProperties[i].m_ucMode == GPIO_MODES_INPUT_LOW_LEVEL) {
                    if (gpio_read(pin) == 0) {
                        // debounce
                        delayms(5);
                        if (gpio_read(pin) == 0) {
                            gpio_interrupt_disable(pin);
                            gpio_create_timer(i);
                        }
                    }
                }
                // For HIGH-EDGE and LOW-EDGE
                else if (g_oGpioProperties[i].m_ucMode >= GPIO_MODES_INPUT_HIGH_EDGE) {
                    if (gpio_is_interrupted(pin)) {
                        gpio_interrupt_disable(pin);
                        gpio_create_timer(i);
                    }
                }
            }
        }
    }
}


