#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/apps/mqtt.h"
#include "lwip/dns.h"

#include "smarthome.h"

/**
 * Home Assistant integration via MQTT.
 *
 * Publishes device state to MQTT topics and subscribes to command topics.
 * Supports Home Assistant MQTT auto-discovery for automatic entity creation.
 *
 * Topics:
 *   padproxy/pc/state        → "off" | "booting" | "on" | "sleeping"
 *   padproxy/bt/connected    → "true" | "false"
 *   padproxy/availability    → "online" (LWT: "offline")
 *   padproxy/command/power   ← "toggle" | "on" | "off" (subscribe)
 *   padproxy/command/ir      ← "send" (subscribe)
 *
 * Auto-discovery:
 *   homeassistant/switch/padproxy/pc_power/config
 *   homeassistant/sensor/padproxy/pc_state/config
 *   homeassistant/binary_sensor/padproxy/bt_connected/config
 */

#ifdef SMARTHOME_HASS

/* ── State ──────────────────────────────────────────────────────────────── */

static mqtt_client_t *s_client;
static smarthome_cmd_cb_t s_cmd_cb;
static const device_config_t *s_config;
static bool s_connected;
static bool s_discovery_sent;

/** Reconnection state. */
static uint32_t s_reconnect_at_ms;
static uint32_t s_backoff_ms;

#define MQTT_BACKOFF_INITIAL_MS  2000
#define MQTT_BACKOFF_MAX_MS     30000

/* ── MQTT Topic Constants ───────────────────────────────────────────────── */

#define TOPIC_PC_STATE       "padproxy/pc/state"
#define TOPIC_BT_CONNECTED   "padproxy/bt/connected"
#define TOPIC_AVAILABILITY   "padproxy/availability"
#define TOPIC_CMD_POWER      "padproxy/command/power"
#define TOPIC_CMD_IR         "padproxy/command/ir"

/* ── MQTT Callbacks ─────────────────────────────────────────────────────── */

static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len,
                                  u8_t flags)
{
    (void)arg; (void)flags;

    char payload[64];
    size_t copy_len = len < sizeof(payload) - 1 ? len : sizeof(payload) - 1;
    memcpy(payload, data, copy_len);
    payload[copy_len] = '\0';

    printf("[hass] MQTT data: %s\n", payload);

    if (!s_cmd_cb) return;

    if (strcmp(payload, "toggle") == 0) {
        s_cmd_cb(SMARTHOME_CMD_POWER_TOGGLE);
    } else if (strcmp(payload, "on") == 0) {
        s_cmd_cb(SMARTHOME_CMD_POWER_ON);
    } else if (strcmp(payload, "off") == 0) {
        s_cmd_cb(SMARTHOME_CMD_POWER_OFF);
    } else if (strcmp(payload, "send") == 0) {
        s_cmd_cb(SMARTHOME_CMD_IR_SEND);
    }
}

static void mqtt_incoming_publish_cb(void *arg, const char *topic,
                                     u32_t tot_len)
{
    (void)arg; (void)tot_len;
    printf("[hass] MQTT topic: %s\n", topic);
}

static void mqtt_connection_cb(mqtt_client_t *client, void *arg,
                                mqtt_connection_status_t status)
{
    (void)client; (void)arg;

    if (status == MQTT_CONNECT_ACCEPTED) {
        printf("[hass] MQTT connected\n");
        s_connected = true;
        s_backoff_ms = MQTT_BACKOFF_INITIAL_MS;
        s_discovery_sent = false;

        /* Subscribe to command topics */
        mqtt_subscribe(s_client, TOPIC_CMD_POWER, 1, NULL, NULL);
        mqtt_subscribe(s_client, TOPIC_CMD_IR, 1, NULL, NULL);

        /* Publish online availability */
        mqtt_publish(s_client, TOPIC_AVAILABILITY, "online", 6, 1, 1,
                     NULL, NULL);
    } else {
        printf("[hass] MQTT connection failed (status %d)\n", status);
        s_connected = false;
    }
}

/* ── MQTT Publishing ────────────────────────────────────────────────────── */

static void publish_string(const char *topic, const char *value, bool retain)
{
    if (!s_connected || !s_client) return;
    mqtt_publish(s_client, topic, value, (uint16_t)strlen(value),
                 retain ? 1 : 0, retain ? 1 : 0, NULL, NULL);
}

/* ── Home Assistant Discovery ───────────────────────────────────────────── */

static void send_ha_discovery(void)
{
    if (!s_connected || s_discovery_sent) return;

    /* PC power switch */
    static const char switch_config[] =
        "{\"name\":\"PC Power\","
        "\"unique_id\":\"padproxy_pc_power\","
        "\"command_topic\":\"" TOPIC_CMD_POWER "\","
        "\"state_topic\":\"" TOPIC_PC_STATE "\","
        "\"payload_on\":\"on\","
        "\"payload_off\":\"off\","
        "\"state_on\":\"on\","
        "\"state_off\":\"off\","
        "\"availability_topic\":\"" TOPIC_AVAILABILITY "\","
        "\"device\":{\"identifiers\":[\"padproxy\"],"
        "\"name\":\"PadProxy\",\"manufacturer\":\"PadProxy\","
        "\"model\":\"Pico 2 W\"}}";

    mqtt_publish(s_client,
                 "homeassistant/switch/padproxy/pc_power/config",
                 switch_config, (uint16_t)strlen(switch_config),
                 0, 1, NULL, NULL);

    /* PC state sensor */
    static const char sensor_config[] =
        "{\"name\":\"PC State\","
        "\"unique_id\":\"padproxy_pc_state\","
        "\"state_topic\":\"" TOPIC_PC_STATE "\","
        "\"availability_topic\":\"" TOPIC_AVAILABILITY "\","
        "\"icon\":\"mdi:desktop-tower\","
        "\"device\":{\"identifiers\":[\"padproxy\"]}}";

    mqtt_publish(s_client,
                 "homeassistant/sensor/padproxy/pc_state/config",
                 sensor_config, (uint16_t)strlen(sensor_config),
                 0, 1, NULL, NULL);

    /* BT connected binary sensor */
    static const char bt_config[] =
        "{\"name\":\"Controller Connected\","
        "\"unique_id\":\"padproxy_bt_connected\","
        "\"state_topic\":\"" TOPIC_BT_CONNECTED "\","
        "\"payload_on\":\"true\","
        "\"payload_off\":\"false\","
        "\"availability_topic\":\"" TOPIC_AVAILABILITY "\","
        "\"icon\":\"mdi:controller\","
        "\"device\":{\"identifiers\":[\"padproxy\"]}}";

    mqtt_publish(s_client,
                 "homeassistant/binary_sensor/padproxy/bt_connected/config",
                 bt_config, (uint16_t)strlen(bt_config),
                 0, 1, NULL, NULL);

    /* IR send button */
    static const char ir_config[] =
        "{\"name\":\"TV Power (IR)\","
        "\"unique_id\":\"padproxy_ir_send\","
        "\"command_topic\":\"" TOPIC_CMD_IR "\","
        "\"payload_press\":\"send\","
        "\"availability_topic\":\"" TOPIC_AVAILABILITY "\","
        "\"icon\":\"mdi:remote\","
        "\"device\":{\"identifiers\":[\"padproxy\"]}}";

    mqtt_publish(s_client,
                 "homeassistant/button/padproxy/ir_send/config",
                 ir_config, (uint16_t)strlen(ir_config),
                 0, 1, NULL, NULL);

    s_discovery_sent = true;
    printf("[hass] HA discovery messages sent\n");
}

/* ── MQTT Connection ────────────────────────────────────────────────────── */

static bool attempt_mqtt_connect(void)
{
    if (!s_config || s_config->mqtt_broker[0] == '\0')
        return false;

    printf("[hass] Connecting to MQTT broker %s:%d...\n",
           s_config->mqtt_broker, s_config->mqtt_port);

    if (!s_client) {
        s_client = mqtt_client_new();
        if (!s_client) {
            printf("[hass] Failed to create MQTT client\n");
            return false;
        }
        mqtt_set_inpub_callback(s_client, mqtt_incoming_publish_cb,
                                mqtt_incoming_data_cb, NULL);
    }

    /* Resolve broker hostname */
    ip_addr_t broker_addr;
    err_t err = dns_gethostbyname(s_config->mqtt_broker, &broker_addr, NULL,
                                  NULL);
    if (err == ERR_INPROGRESS) {
        /* DNS lookup pending — try again next task cycle */
        return false;
    }
    if (err != ERR_OK) {
        /* Try as IP address */
        if (!ip4addr_aton(s_config->mqtt_broker, ip_2_ip4(&broker_addr))) {
            printf("[hass] Cannot resolve MQTT broker: %s\n",
                   s_config->mqtt_broker);
            return false;
        }
    }

    struct mqtt_connect_client_info_t ci = {0};
    ci.client_id   = "padproxy";
    ci.client_user = s_config->mqtt_username[0] ? s_config->mqtt_username
                                                : NULL;
    ci.client_pass = s_config->mqtt_password[0] ? s_config->mqtt_password
                                                : NULL;
    ci.keep_alive  = 60;
    ci.will_topic  = TOPIC_AVAILABILITY;
    ci.will_msg    = "offline";
    ci.will_qos    = 1;
    ci.will_retain = 1;

    cyw43_arch_lwip_begin();
    err = mqtt_client_connect(s_client, &broker_addr, s_config->mqtt_port,
                              mqtt_connection_cb, NULL, &ci);
    cyw43_arch_lwip_end();

    if (err != ERR_OK) {
        printf("[hass] MQTT connect error: %d\n", err);
        return false;
    }

    return true;
}

/* ── Platform Interface ─────────────────────────────────────────────────── */

static bool hass_init(const device_config_t *config, smarthome_cmd_cb_t cmd_cb)
{
    s_config = config;
    s_cmd_cb = cmd_cb;
    s_backoff_ms = MQTT_BACKOFF_INITIAL_MS;
    s_reconnect_at_ms = 0;

    return attempt_mqtt_connect();
}

static void hass_task(uint32_t now_ms)
{
    /* Send HA discovery after connecting */
    if (s_connected && !s_discovery_sent) {
        send_ha_discovery();
    }

    /* Check connection and reconnect if needed */
    if (!s_connected && s_config && s_config->mqtt_broker[0] != '\0') {
        if (now_ms >= s_reconnect_at_ms) {
            if (!attempt_mqtt_connect()) {
                s_backoff_ms *= 2;
                if (s_backoff_ms > MQTT_BACKOFF_MAX_MS)
                    s_backoff_ms = MQTT_BACKOFF_MAX_MS;
                s_reconnect_at_ms = now_ms + s_backoff_ms;
            }
        }
    }
}

static void hass_deinit(void)
{
    if (s_client && s_connected) {
        publish_string(TOPIC_AVAILABILITY, "offline", true);
        mqtt_disconnect(s_client);
    }
    s_connected = false;
}

static void hass_publish_pc_state(pc_power_state_t state)
{
    publish_string(TOPIC_PC_STATE, pc_power_state_name(state), true);
}

static void hass_publish_bt_status(bool connected)
{
    publish_string(TOPIC_BT_CONNECTED, connected ? "true" : "false", true);
}

const smarthome_platform_t smarthome_hass = {
    .init              = hass_init,
    .task              = hass_task,
    .deinit            = hass_deinit,
    .publish_pc_state  = hass_publish_pc_state,
    .publish_bt_status = hass_publish_bt_status,
};

#endif /* SMARTHOME_HASS */
