/* main.c - Application main entry point */

#include <zephyr.h>
#include <sys/printk.h>
#include <sys/byteorder.h>
#include <sys/time_units.h>
#include <power/reboot.h>

#include <bluetooth/bluetooth.h>

#include <display/mb_display.h>

#define RSSI_THRESHOLD  (-90)

static const struct mb_image radar[] = {
    MB_IMAGE(
        { 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0 },
        { 0, 0, 1, 0, 0 },
        { 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0 }),
    MB_IMAGE(
        { 0, 0, 0, 0, 0 },
        { 0, 1, 1, 1, 0 },
        { 0, 1, 1, 1, 0 },
        { 0, 1, 1, 1, 0 },
        { 0, 0, 0, 0, 0 }),
    MB_IMAGE(
        { 1, 1, 1, 1, 1 },
        { 1, 1, 1, 1, 1 },
        { 1, 1, 0, 1, 1 },
        { 1, 1, 1, 1, 1 },
        { 1, 1, 1, 1, 1 }),
    MB_IMAGE(
        { 1, 1, 1, 1, 1 },
        { 1, 0, 0, 0, 1 },
        { 1, 0, 0, 0, 1 },
        { 1, 0, 0, 0, 1 },
        { 1, 1, 1, 1, 1 }),
    MB_IMAGE(
        { 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0 }),
};

static const struct mb_image ng[] = {
    MB_IMAGE(
        { 1, 0, 0, 0, 1 },
        { 0, 1, 0, 1, 0 },
        { 0, 0, 1, 0, 0 },
        { 0, 1, 0, 1, 0 },
        { 1, 0, 0, 0, 1 }),
    MB_IMAGE(
        { 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0 }),
};

struct disp_led {
    const int rowindex;
    const int colbit;
    int on;
};

static struct disp_led dis1[] = {
    {1, BIT(1), 0},
    {1, BIT(2), 0},
    {1, BIT(3), 0},
    {2, BIT(1), 0},
    {2, BIT(3), 0},
    {3, BIT(1), 0},
    {3, BIT(2), 0},
    {3, BIT(3), 0},
};
static const int dis1_size = ARRAY_SIZE(dis1);

static struct disp_led dis2[] = {
    {0, BIT(0), 0},
    {0, BIT(1), 0},
    {0, BIT(2), 0},
    {0, BIT(3), 0},
    {0, BIT(4), 0},
    {1, BIT(0), 0},
    {1, BIT(4), 0},
    {2, BIT(0), 0},
    {2, BIT(4), 0},
    {3, BIT(0), 0},
    {3, BIT(4), 0},
    {4, BIT(0), 0},
    {4, BIT(1), 0},
    {4, BIT(2), 0},
    {4, BIT(3), 0},
    {4, BIT(4), 0},
};
static const int dis2_size = ARRAY_SIZE(dis2);

static void disp_animate(int32_t duration, const struct mb_image *img, uint8_t img_count, uint8_t loop)
{
    struct mb_display *disp = mb_display_get();
    for(uint8_t j=0;j<loop;j++){
        for(uint8_t i=0;i<img_count;i++)
        {
            mb_display_image(disp, MB_DISPLAY_MODE_SINGLE, SYS_FOREVER_MS,
                &img[i], 1);
            k_sleep(K_MSEC(duration));
        }
    }
    mb_display_stop(disp);
}

static void clear_dis()
{
    for(int i=0;i<dis1_size;i++){
        dis1[i].on = 0;
    }
    for(int i=0;i<dis2_size;i++){
        dis2[i].on = 0;
    }
}

static void set_dis(const bt_addr_le_t *addr, int8_t rssi )
{
    if (rssi>=RSSI_THRESHOLD){
        int h = (addr->a.val[0]) % dis1_size;
        for(int i=0;i<dis1_size;i++){
            int idx = (i+h)%dis1_size;
            if (dis1[idx].on){
                continue;
            }
            dis1[idx].on = 1;
            return;
        }
    }
    int h = (addr->a.val[0]) % dis2_size;
    for(int i=0;i<dis2_size;i++){
        int idx = (i+h)%dis2_size;
        if (dis2[idx].on){
            continue;
        }
        dis2[idx].on = 1;
        return;
    }
}

static void disp_dis()
{
    struct mb_image hits[2]={{},{}};
    for(int i=0;i<dis1_size;i++){
        if (dis1[i].on){
            hits[0].row[dis1[i].rowindex] |= dis1[i].colbit;
        }
    }
    for(int i=0;i<dis2_size;i++){
        if (dis2[i].on){
            hits[0].row[dis2[i].rowindex] |= dis2[i].colbit;
        }
    }
    disp_animate(250, hits, ARRAY_SIZE(hits), 10);
}

static bool adv_data_found(struct bt_data *data, void *user_data)
{
    int *hit = (int *)user_data;
    if(data->type == BT_DATA_UUID16_ALL) {
       if (sys_get_le16(data->data) == 0xFD6F) {
           // Complete 16-bit Service UUID Section — The UUID is 0xFD6F
           *hit=1;
       }
       return false;
    }
    return true;
}

int count;

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
             struct net_buf_simple *ad)
{
    char addr_str[BT_ADDR_LE_STR_LEN];
    int hit;
    hit=0;
    bt_data_parse(ad, adv_data_found, &hit);
    if (hit) {
        count++;
        bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
        printk("[%d] COCOA found: %s (RSSI %d)\n", count, addr_str, rssi);
        set_dis(addr, rssi);
    }
}

void main(void)
{
    int err;

    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return;
    }
    printk("Bluetooth initialized\n");

    clear_dis();

    count = 0;
    err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, device_found);
    if (err) {
        printk("Scanning failed to start (err %d)\n", err);
        disp_animate(500, ng, ARRAY_SIZE(ng), 5);
    } else {
        printk("Scanning successfully started\n");
        disp_animate(200, radar, ARRAY_SIZE(radar), 10);

        bt_le_scan_stop();      
        printk("Stop Scanning: %d\n", count);

        disp_dis();
    }
    sys_reboot(SYS_REBOOT_WARM);
}