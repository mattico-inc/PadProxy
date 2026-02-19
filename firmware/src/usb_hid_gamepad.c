#include "usb_hid_gamepad.h"
#include "usb_hid_report.h"

#include <string.h>
#include "tinyusb.h"
#include "class/hid/hid_device.h"
#include "esp_log.h"

static const char *TAG = "usb_hid";

/* ── State ───────────────────────────────────────────────────────────── */

static usb_hid_state_cb_t s_state_cb;
static usb_hid_state_t    s_state = USB_HID_NOT_MOUNTED;

/* ── USB Descriptors ─────────────────────────────────────────────────── */

/*
 * VID 0x1209 is the pid.codes shared VID for open-source hardware.
 * PID 0x0001 is a placeholder; register at https://pid.codes for production.
 */
#define USB_VID   0x1209
#define USB_PID   0x0001

enum {
    ITF_NUM_HID,
    ITF_NUM_TOTAL,
};

static const tusb_desc_device_t desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 1,
    .iProduct           = 2,
    .iSerialNumber      = 3,
    .bNumConfigurations = 1,
};

#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)
#define EPNUM_HID         0x81

static const uint8_t desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 0, HID_ITF_PROTOCOL_NONE,
                       sizeof(usb_hid_report_descriptor), EPNUM_HID,
                       CFG_TUD_HID_EP_BUFSIZE, 1),
};

static const char *desc_strings[] = {
    [0] = "",                   /* Language (handled by TinyUSB) */
    [1] = "PadProxy",          /* Manufacturer */
    [2] = "PadProxy Gamepad",  /* Product */
    [3] = "000001",            /* Serial */
};

/* ── TinyUSB descriptor callbacks ────────────────────────────────────── */

uint8_t const *tud_descriptor_device_cb(void)
{
    return (uint8_t const *)&desc_device;
}

uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return desc_configuration;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;

    /*
     * TinyUSB expects a UTF-16 string buffer returned from this callback.
     * We convert our ASCII strings on the fly.
     */
    static uint16_t buf[32];
    uint8_t len;

    if (index == 0) {
        buf[1] = 0x0409; /* English (US) */
        len = 1;
    } else {
        if (index >= sizeof(desc_strings) / sizeof(desc_strings[0]))
            return NULL;

        const char *str = desc_strings[index];
        len = (uint8_t)strlen(str);
        if (len > 31) len = 31;

        for (uint8_t i = 0; i < len; i++) {
            buf[1 + i] = str[i];
        }
    }

    /* First element: length + type */
    buf[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * len + 2));
    return buf;
}

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void)instance;
    return usb_hid_report_descriptor;
}

/* ── TinyUSB HID callbacks ───────────────────────────────────────────── */

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                                hid_report_type_t report_type,
                                uint8_t *buffer, uint16_t reqlen)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                            hid_report_type_t report_type,
                            uint8_t const *buffer, uint16_t bufsize)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
}

/* ── TinyUSB device callbacks ────────────────────────────────────────── */

void tud_mount_cb(void)
{
    ESP_LOGI(TAG, "USB mounted");
    s_state = USB_HID_MOUNTED;
    if (s_state_cb) {
        s_state_cb(USB_HID_MOUNTED);
    }
}

void tud_umount_cb(void)
{
    ESP_LOGI(TAG, "USB unmounted");
    s_state = USB_HID_NOT_MOUNTED;
    if (s_state_cb) {
        s_state_cb(USB_HID_NOT_MOUNTED);
    }
}

void tud_suspend_cb(bool remote_wakeup_en)
{
    (void)remote_wakeup_en;
    ESP_LOGI(TAG, "USB suspended");
    s_state = USB_HID_SUSPENDED;
    if (s_state_cb) {
        s_state_cb(USB_HID_SUSPENDED);
    }
}

void tud_resume_cb(void)
{
    ESP_LOGI(TAG, "USB resumed");
    s_state = USB_HID_MOUNTED;
    if (s_state_cb) {
        s_state_cb(USB_HID_MOUNTED);
    }
}

/* ── Public API ──────────────────────────────────────────────────────── */

void usb_hid_gamepad_init(usb_hid_state_cb_t state_cb)
{
    s_state_cb = state_cb;
    s_state = USB_HID_NOT_MOUNTED;

    const tinyusb_config_t tusb_cfg = {
        .device_descriptor        = &desc_device,
        .configuration_descriptor = desc_configuration,
        .string_descriptor        = desc_strings,
        .string_descriptor_count  =
            sizeof(desc_strings) / sizeof(desc_strings[0]),
        .external_phy             = false,
    };

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    ESP_LOGI(TAG, "USB HID gamepad initialized");
}

void usb_hid_gamepad_task(void)
{
    tud_task();
}

bool usb_hid_gamepad_send_report(const gamepad_report_t *report)
{
    if (!tud_mounted() || !tud_hid_ready())
        return false;

    usb_gamepad_report_t usb_report;
    usb_hid_report_from_gamepad(report, &usb_report);

    return tud_hid_report(0, &usb_report, sizeof(usb_report));
}

usb_hid_state_t usb_hid_gamepad_get_state(void)
{
    return s_state;
}
