#include <arch/mouse.h>
#include <arch/io.h>
#include <arch/spinlock.h>
#include <arch/interrupt.h>
#include <string.h>

static uint8_t mouse_cycle = 0;
static uint8_t mouse_packet[4];
static uint8_t mouse_device_id = 0;
static int mouse_has_wheel = 0;
static int mouse_has_5btn = 0;

static mouse_packet_t mouse_buffer[MOUSE_BUFFER_SIZE];
static uint32_t mouse_head = 0;
static uint32_t mouse_tail = 0;
static spinlock_t mouse_lock = SPINLOCK_INIT;

static mouse_callback_t mouse_callback = NULL;

static inline uint8_t mouse_read_data(void)
{
    return inb(MOUSE_DATA_PORT);
}

static inline void mouse_write_data(uint8_t val)
{
    outb(MOUSE_DATA_PORT, val);
}

static inline void mouse_write_cmd(uint8_t cmd)
{
    outb(MOUSE_CMD_PORT, cmd);
}

static inline uint8_t mouse_read_status(void)
{
    return inb(MOUSE_CMD_PORT);
}

static inline void mouse_wait_input(void)
{
    uint32_t timeout = 100000;
    while (timeout--) {
        if (!(mouse_read_status() & 0x02)) return;
        __asm__ volatile("pause");
    }
}

static inline void mouse_wait_output(void)
{
    uint32_t timeout = 100000;
    while (timeout--) {
        if (mouse_read_status() & 0x01) return;
        __asm__ volatile("pause");
    }
}

static int mouse_send_command(uint8_t cmd)
{
    mouse_wait_input();
    mouse_write_cmd(MOUSE_CMD_AUX_WRITE);

    mouse_wait_input();
    mouse_write_data(cmd);

    mouse_wait_output();

    if (!(mouse_read_status() & 0x01)) return -1;

    uint8_t resp = mouse_read_data();
    if (resp != MOUSE_RESP_ACK) return -1;

    return 0;
}

static int mouse_send_with_ack(uint8_t cmd)
{
    mouse_wait_input();
    mouse_write_cmd(MOUSE_CMD_AUX_WRITE);
    mouse_wait_input();
    mouse_write_data(cmd);

    mouse_wait_output();
    if (!(mouse_read_status() & 0x01)) return -1;

    uint8_t resp = mouse_read_data();
    if (resp != MOUSE_RESP_ACK) return -1;

    return 0;
}

static int mouse_buffer_put(const mouse_packet_t* pkt)
{
    uint32_t next = (mouse_head + 1) % MOUSE_BUFFER_SIZE;
    if (next == mouse_tail) return -1;
    mouse_buffer[mouse_head] = *pkt;
    mouse_head = next;
    return 0;
}

static void mouse_process_packet(void)
{
    mouse_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));

    if (mouse_device_id == 0x04 || mouse_device_id == 0x03) {
        if (mouse_cycle < 4) return;
    } else {
        if (mouse_cycle < 3) return;
    }

    uint8_t flags = mouse_packet[0];
    uint8_t x = mouse_packet[1];
    uint8_t y = mouse_packet[2];

    pkt.buttons = 0;
    if (flags & 0x01) pkt.buttons |= MOUSE_BTN_LEFT;
    if (flags & 0x02) pkt.buttons |= MOUSE_BTN_RIGHT;
    if (flags & 0x04) pkt.buttons |= MOUSE_BTN_MIDDLE;
    if (mouse_has_5btn) {
        if (flags & 0x08) pkt.buttons |= MOUSE_BTN_4;
        if (flags & 0x10) pkt.buttons |= MOUSE_BTN_5;
    }

    pkt.dx = (int32_t)x;
    pkt.dy = (int32_t)y;

    if (flags & 0x10) pkt.dx = pkt.dx | 0xFFFFFF00;
    if (flags & 0x20) pkt.dy = pkt.dy | 0xFFFFFF00;

    pkt.dy = -pkt.dy;

    pkt.wheel = 0;
    if (mouse_has_wheel && mouse_cycle >= 4) {
        pkt.wheel = (int8_t)mouse_packet[3];
    } else if (mouse_has_5btn && mouse_cycle >= 4) {
        pkt.wheel = (int8_t)mouse_packet[3];
    }

    if (pkt.dx > 0x80000000) pkt.dx -= 0x100000000;
    if (pkt.dy > 0x80000000) pkt.dy -= 0x100000000;

    spin_lock(&mouse_lock);
    mouse_buffer_put(&pkt);
    spin_unlock(&mouse_lock);

    if (mouse_callback) {
        mouse_callback(&pkt);
    }
}

void mouse_init(void)
{
    mouse_cycle = 0;
    mouse_has_wheel = 0;
    mouse_has_5btn = 0;
    mouse_head = 0;
    mouse_tail = 0;
    mouse_callback = NULL;
    spin_init(&mouse_lock);
    memset(mouse_packet, 0, sizeof(mouse_packet));
    memset(mouse_buffer, 0, sizeof(mouse_buffer));

    mouse_wait_output();
    (void)mouse_read_data();

    mouse_wait_input();
    mouse_write_cmd(MOUSE_CMD_AUX_ENABLE);

    for (volatile int i = 0; i < 1000; i++);

    mouse_wait_input();
    mouse_write_cmd(MOUSE_CMD_AUX_WRITE);
    mouse_wait_input();
    mouse_write_data(MOUSE_CMD_RESET);

    mouse_wait_output();
    (void)mouse_read_data();

    uint32_t timeout = 100000;
    while (timeout--) {
        if (mouse_read_status() & 0x01) break;
        __asm__ volatile("pause");
    }
    uint8_t bat = (mouse_read_status() & 0x01) ? mouse_read_data() : 0;
    (void)bat;

    timeout = 100000;
    while (timeout--) {
        if (mouse_read_status() & 0x01) break;
        __asm__ volatile("pause");
    }
    uint8_t dev_id = (mouse_read_status() & 0x01) ? mouse_read_data() : MOUSE_RESP_ID_PS2;
    mouse_device_id = dev_id;

    mouse_send_with_ack(MOUSE_CMD_SET_SCALE_1);
    mouse_send_with_ack(MOUSE_CMD_ENABLE);

    mouse_send_with_ack(MOUSE_CMD_SET_RATE);
    mouse_send_with_ack(MOUSE_SAMPLE_RATE_200);

    mouse_send_with_ack(MOUSE_CMD_SET_STREAM);

    mouse_send_with_ack(MOUSE_CMD_GET_DEVICE_ID);

    timeout = 100000;
    while (timeout--) {
        if (mouse_read_status() & 0x01) break;
        __asm__ volatile("pause");
    }
    if (mouse_read_status() & 0x01) {
        mouse_device_id = mouse_read_data();
    }

    if (mouse_device_id == 0x03 || mouse_device_id == 0x04) {
        mouse_has_wheel = 1;
        if (mouse_device_id == 0x04) {
            mouse_has_5btn = 1;
        }
        mouse_send_with_ack(MOUSE_CMD_SET_RATE);
        mouse_send_with_ack(MOUSE_SAMPLE_RATE_100);
        mouse_send_with_ack(MOUSE_CMD_SET_RATE);
        mouse_send_with_ack(MOUSE_SAMPLE_RATE_80);
        mouse_send_with_ack(MOUSE_CMD_SET_RATE);
        mouse_send_with_ack(MOUSE_SAMPLE_RATE_200);
        mouse_send_with_ack(MOUSE_CMD_GET_DEVICE_ID);

        timeout = 100000;
        while (timeout--) {
            if (mouse_read_status() & 0x01) break;
            __asm__ volatile("pause");
        }
        if (mouse_read_status() & 0x01) {
            mouse_device_id = mouse_read_data();
        }
    }

    mouse_cycle = 0;
    interrupt_register(MOUSE_IRQ, (void*)mouse_irq_handler);
}

void mouse_irq_handler(void)
{
    uint8_t status = mouse_read_status();

    if (!(status & 0x01)) return;
    if (status & 0x20) {
        mouse_wait_input();
        mouse_write_cmd(0xFE);
        return;
    }

    if (!(status & 0x01)) return;

    uint8_t data = mouse_read_data();

    mouse_packet[mouse_cycle] = data;
    mouse_cycle++;

    uint8_t expected = 3;
    if (mouse_has_wheel || mouse_has_5btn) {
        expected = 4;
    }

    if (mouse_cycle >= expected) {
        uint8_t flags = mouse_packet[0];
        if ((flags & 0xC8) == 0x08) {
            mouse_cycle = 0;
            return;
        }
        if (mouse_has_wheel || mouse_has_5btn) {
            if (mouse_packet[3] == 0x00 && (flags & 0x08) == 0 && (flags & 0xC0) == 0xC0) {
                mouse_cycle = 0;
                return;
            }
        }
        mouse_process_packet();
        mouse_cycle = 0;
    }
}

int mouse_poll(mouse_packet_t* packet)
{
    if (!packet) return 0;

    spin_lock(&mouse_lock);
    if (mouse_head == mouse_tail) {
        spin_unlock(&mouse_lock);
        return 0;
    }
    *packet = mouse_buffer[mouse_tail];
    mouse_tail = (mouse_tail + 1) % MOUSE_BUFFER_SIZE;
    spin_unlock(&mouse_lock);
    return 1;
}

void mouse_set_callback(mouse_callback_t cb)
{
    mouse_callback = cb;
}

void mouse_set_sample_rate(uint8_t rate)
{
    mouse_send_with_ack(MOUSE_CMD_SET_RATE);
    mouse_send_with_ack(rate);
}

int mouse_get_device_id(uint8_t* id)
{
    if (!id) return -1;
    *id = mouse_device_id;
    return 0;
}

void mouse_set_resolution(uint8_t resolution)
{
    if (resolution > 3) resolution = 3;
    mouse_send_with_ack(MOUSE_CMD_SET_RESOLUTION);
    mouse_send_with_ack(resolution);
}
