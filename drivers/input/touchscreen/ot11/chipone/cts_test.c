#define LOG_TAG         "Test"

#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_strerror.h"
#include "cts_test.h"
#include "cts_tcs.h"

#ifdef CTS_CONFIG_MKDIR_FOR_CTS_TEST
/* for ksys_mkdir/sys_mkdir */
#include <linux/syscalls.h>
#include <linux/namei.h>
#endif /* CTS_CONFIG_MKDIR_FOR_CTS_TEST */

const char *cts_test_item_str(int test_item)
{
#define case_test_item(item) \
    case CTS_TEST_ ## item: return #item "-TEST"

    switch (test_item) {
        case_test_item(RESET_PIN);
        case_test_item(INT_PIN);
        case_test_item(RAWDATA);
        case_test_item(NOISE);
        case_test_item(OPEN);
        case_test_item(SHORT);
        case_test_item(COMPENSATE_CAP);

    default:
        return "INVALID";
    }
#undef case_test_item
}
/*Tab A9 code for AX6739A-953 by suyurui at 20230609 start*/
/* ICNL9911/S/C */
#define CTS_FIRMWARE_WORK_MODE_NORMAL               (0x00)
#define CTS_FIRMWARE_WORK_MODE_FACTORY              (0x01)
#define CTS_FIRMWARE_WORK_MODE_CONFIG               (0x02)
#define CTS_FIRMWARE_WORK_MODE_TEST                 (0x03)

/* ICNL9916/9916C/9922 */
#define CTS_FIRMWARE_WORK_MODE_CFG                  (0x01)
#define CTS_FIRMWARE_WORK_MODE_OPEN_SHORT           (0x02)

#define CTS_TEST_SHORT                              (0x01)
#define CTS_TEST_OPEN                               (0x02)
/*Tab A9 code for AX6739A-953 by suyurui at 20230610 start*/
#define CTS_TEST_NORMAL_MODE                        (0x00)
/*Tab A9 code for AX6739A-953 by suyurui at 20230610 end*/
#define CTS_TEST_OPEN_SHORT_DET_MODE                (0x05)

#define CTS_SHORT_TEST_UNDEFINED                    (0x00)
#define CTS_SHORT_TEST_BETWEEN_COLS                 (0x01)
#define CTS_SHORT_TEST_BETWEEN_ROWS                 (0x02)
#define CTS_SHORT_TEST_BETWEEN_GND                  (0x03)

#define CTS_PWR_MODE_ACTIVE                         (0x00)
/*Tab A9 code for AX6739A-953 by suyurui at 20230609 end*/
/*Tab A9 code for OT11BSP-51 by suyurui at 20230620 start*/
#define SELF_TEST_RETRY_TIME                        30
#define SELF_TEST_DELAY_TIME                        150
#define RETRY_COUNT                                 3
/*Tab A9 code for OT11BSP-51 by suyurui at 20230620 end*/
/*Tab A9 code for AX6739A-1436 by suyurui at 20230705 start*/
#define OPEN_RETRY_COUNT                            5
#define CELL_NULL_COUNT                             100
#define CELL_NULL_COUNT_FIRST_FRAME                 96
#define CELL_NULL_COUNT_SECOND_FRAME                98
#define CELL_NULL_COUNT_ALL                         92
/*Tab A9 code for AX6739A-1436 by suyurui at 20230705 end*/

#define RAWDATA_BUFFER_SIZE(cts_dev) \
    (cts_dev->hwdata->num_row * cts_dev->hwdata->num_col * 2)

int disable_fw_esd_protection(struct cts_device *cts_dev)
{
    return cts_fw_reg_writeb(cts_dev, 0x8000 + 342, 1);
}

int disable_fw_monitor_mode(struct cts_device *cts_dev)
{
    int ret;
    u8 value;

    ret = cts_fw_reg_readb(cts_dev, 0x8000 + 344, &value);
    if (ret)
        return ret;

    if (value & BIT(0))
        return cts_fw_reg_writeb(cts_dev, 0x8000 + 344, value & (~BIT(0)));

    return 0;
}

int disable_fw_auto_compensate(struct cts_device *cts_dev)
{
    return cts_fw_reg_writeb(cts_dev, 0x8000 + 276, 1);
}

int set_fw_work_mode(const struct cts_device *cts_dev, u8 mode)
{
    int ret, retries;
    u8 pwr_mode;

    cts_info("Set firmware work mode to %u", mode);

    ret = cts_fw_reg_writeb(cts_dev, CTS_DEVICE_FW_REG_WORK_MODE, mode);
    if (ret) {
        cts_err("Write firmware work mode register failed %d", ret);
        return ret;
    }

    ret = cts_fw_reg_readb(cts_dev, 0x05, &pwr_mode);
    if (ret) {
        cts_err("Read firmware power mode register failed %d", ret);
        return ret;
    }

    if (pwr_mode == 1) {
        ret = cts_send_command(cts_dev, CTS_CMD_QUIT_GESTURE_MONITOR);
        if (ret) {
            cts_err("Send cmd QUIT_GESTURE_MONITOR failed %d", ret);
            return ret;
        }

        msleep(50);
    }

    retries = 0;
    do {
        u8 sys_busy, curr_mode;

        msleep(10);

        ret = cts_fw_reg_readb(cts_dev, 0x01, &sys_busy);
        if (ret) {
            cts_err("Read firmware system busy register failed %d", ret);
            return ret;
        }
        if (sys_busy)
            continue;

        ret = cts_fw_reg_readb(cts_dev, 0x3F, &curr_mode);
        if (ret) {
            cts_err("Read firmware current work mode failed %d", ret);
            return ret;
        }

        if (curr_mode == mode || curr_mode == 0xFF)
            break;
    } while (retries++ < 1000);

    return (retries >= 1000 ? -ETIMEDOUT : 0);
}

int set_display_state(const struct cts_device *cts_dev, bool active)
{
    int ret;
    u8 access_flag;

    cts_info("Set display state to %s", active ? "ACTIVE" : "SLEEP");

    ret = cts_hw_reg_readb_relaxed(cts_dev, CTS_DEV_HW_REG_DDI_REG_CTRL, &access_flag);
    if (ret) {
        cts_err("Read display access flag failed %d", ret);
        return ret;
    }

    ret = cts_hw_reg_writeb_relaxed(cts_dev, CTS_DEV_HW_REG_DDI_REG_CTRL, access_flag | 0x01);
    if (ret) {
        cts_err("Write display access flag %02x failed %d", access_flag, ret);
        return ret;
    }

    if (active) {
        ret = cts_hw_reg_writeb_relaxed(cts_dev, 0x3C044, 0x55);
        if (ret) {
            cts_err("Write DCS-CMD11 fail");
            return ret;
        }

        msleep(100);

        ret = cts_hw_reg_writeb_relaxed(cts_dev, 0x3C0A4, 0x55);
        if (ret) {
            cts_err("Write DCS-CMD29 fail");
            return ret;
        }

        msleep(100);
    } else {
        ret = cts_hw_reg_writeb_relaxed(cts_dev, 0x3C0A0, 0x55);
        if (ret) {
            cts_err("Write DCS-CMD28 fail");
            return ret;
        }

        msleep(100);

        ret = cts_hw_reg_writeb_relaxed(cts_dev, 0x3C040, 0x55);
        if (ret) {
            cts_err("Write DCS-CMD10 fail");
            return ret;
        }

        msleep(100);
    }

    ret = cts_hw_reg_writeb_relaxed(cts_dev, CTS_DEV_HW_REG_DDI_REG_CTRL, access_flag);
    if (ret) {
        cts_err("Restore display access flag %02x failed %d", access_flag, ret);
        return ret;
    }

    return 0;
}

struct cts_fw_short_test_param {
    u8 type;
    u32 col_pattern[2];
    u32 row_pattern[2];
};

bool set_short_test_type(struct cts_device *cts_dev, u8 type)
{
    static struct cts_fw_short_test_param param = {
        .type = CTS_SHORT_TEST_BETWEEN_COLS,
        .col_pattern = { 0, 0 },
        .row_pattern = { 0, 0 }
    };
    int i, ret;

    cts_info("Set short test type to %u", type);

    param.type = type;
    for (i = 0; i < 5; i++) {
        u8 type_readback;

        ret = cts_fw_reg_writesb(cts_dev, 0x5000, &param, sizeof(param));
        if (ret) {
            cts_err("Set short test type to %u failed %d", type, ret);
            continue;
        }
        ret = cts_fw_reg_readb(cts_dev, 0x5000, &type_readback);
        if (ret) {
            cts_err("Get short test type failed %d", ret);
            continue;
        }
        if (type == type_readback)
            return 0;
        else {
            cts_err("Set test type %u != readback %u", type, type_readback);
            continue;
        }
    }

    return ret;
}

int cts_write_file(struct file *filp, const void *data, size_t size)
{
#ifdef CFG_CTS_FOR_GKI
    cts_info("%s(): kernel_write is forbiddon with GKI Version!", __func__);
    return -EPERM;
#else
    loff_t pos;
    ssize_t ret;

    pos = filp->f_pos;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
    ret = kernel_write(filp, data, size, &pos);
#else
    ret = kernel_write(filp, data, size, pos);
#endif

    if (ret >= 0) {
        filp->f_pos += ret;
    }

    return ret;
#endif
}

#ifdef CTS_CONFIG_MKDIR_FOR_CTS_TEST
/* copied from fs/namei.c */
long do_mkdirat(int dfd, const char __user *pathname, umode_t mode)
{
    struct dentry *dentry;
    struct path path;
    int error;
    unsigned int lookup_flags = LOOKUP_DIRECTORY;

retry:
    dentry = user_path_create(dfd, pathname, &path, lookup_flags);
    if (IS_ERR(dentry))
          return PTR_ERR(dentry);

    if (!IS_POSIXACL(path.dentry->d_inode))
        mode &= ~current_umask();
    error = security_path_mkdir(&path, dentry, mode);
    if (!error)
        error = vfs_mkdir(path.dentry->d_inode, dentry, mode);
    done_path_create(&path, dentry);
    if (retry_estale(error, lookup_flags)) {
        lookup_flags |= LOOKUP_REVAL;
        goto retry;
    }
    return error;
}

/* Make directory for filepath
 * If filepath = "/A/B/C/D.file", it will make dir /A/B/C recursive
 * like userspace mkdir -p
 */
int cts_mkdir_for_file(const char *filepath, umode_t mode)
{
#ifdef CFG_CTS_FOR_GKI
    cts_info("%s(): some functions are forbiddon with GKI Version!", __func__);
    return -EPERM;
#else
    char *dirname = NULL;
    int dirname_len;
    char *s;
    int ret;
    mm_segment_t fs;

    if (filepath == NULL) {
        cts_err("Create dir for file with filepath = NULL");
        return -EINVAL;
    }

    if (filepath[0] == '\0' || filepath[0] != '/') {
        cts_err("Create dir for file with invalid filepath[0]: %c", filepath[0]);
        return -EINVAL;
    }

    dirname_len = strrchr(filepath, '/') - filepath;
    if (dirname_len == 0) {
        cts_warn("Create dir for file '%s' in root dir", filepath);
        return 0;
    }

    dirname = kstrndup(filepath, dirname_len, GFP_KERNEL);
    if (dirname == NULL) {
        cts_err("Create dir alloc mem for dirname failed");
        return -ENOMEM;
    }

    cts_info("Create dir '%s' for file '%s'", dirname, filepath);

    fs = get_fs();
    set_fs(KERNEL_DS);

    s = dirname + 1;    /* Skip leading '/' */
    while (1) {
        char c = '\0';

        /* Bypass leading non-'/'s and then subsequent '/'s */
        while (*s) {
            if (*s == '/') {
                do {
                    ++s;
                } while (*s == '/');
                c = *s;    /* Save current char */
                *s = '\0';    /* and replace it with nul */
                break;
            }
            ++s;
        }

        cts_dbg(" - Create dir '%s'", dirname);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 17, 0)
        ret = ksys_mkdir(dirname, 0777);
#else
        ret = sys_mkdir(dirname, 0777);
#endif
        if (ret < 0 && ret != -EEXIST) {
            cts_info("Create dir '%s' failed %d(%s)", dirname, ret,
                cts_strerror(ret));
            /* Remove any inserted nul from the path */
            *s = c;
            break;
        }
        /* Reset ret to 0 if return -EEXIST */
        ret = 0;

        if (c) {
            /* Remove any inserted nul from the path */
            *s = c;
        } else {
            break;
        }
    }

    set_fs(fs);

    if (dirname) {
        kfree(dirname);
    }

    return ret;
#endif
}
#endif /* CTS_CONFIG_MKDIR_FOR_CTS_TEST */

struct file *cts_test_data_filp;
int cts_start_dump_test_data_to_file(const char *filepath, bool append_to_file)
{
    int ret;

#define START_BANNER \
    ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n"

    cts_info("Start dump test data to file '%s'", filepath);

#ifdef CTS_CONFIG_MKDIR_FOR_CTS_TEST
    ret = cts_mkdir_for_file(filepath, 0777);
    if (ret) {
        cts_err("Create dir for test data file failed %d", ret);
        return ret;
    }
#endif /* CTS_CONFIG_MKDIR_FOR_CTS_TEST */

#ifdef CFG_CTS_FOR_GKI
    cts_info("%s(): filp_open is forbiddon with GKI Version!", __func__);
    ret = -EPERM;
    return ret;
#else
    cts_test_data_filp = filp_open(filepath,
        O_WRONLY | O_CREAT | (append_to_file ? O_APPEND : O_TRUNC),
        S_IRUGO | S_IWUGO);
    if (IS_ERR(cts_test_data_filp)) {
        ret = PTR_ERR(cts_test_data_filp);
        cts_test_data_filp = NULL;
        cts_err("Open file '%p' for test data failed %d", cts_test_data_filp, ret);
        return ret;
    }

    cts_write_file(cts_test_data_filp, START_BANNER, strlen(START_BANNER));

    return 0;
#endif
#undef START_BANNER
}

void cts_stop_dump_test_data_to_file(void)
{
#define END_BANNER \
    "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n"
#ifndef CFG_CTS_FOR_GKI
    int r;
#endif

    cts_info("Stop dump test data to file");

    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp, END_BANNER, strlen(END_BANNER));
#ifndef CFG_CTS_FOR_GKI
        r = filp_close(cts_test_data_filp, NULL);
        if (r) {
            cts_err("Close test data file failed %d", r);
        }
#endif
        cts_test_data_filp = NULL;
    } else {
        cts_warn("Stop dump tsdata to file with filp = NULL");
    }
#undef END_BANNER
}

static void cts_dump_tsdata(struct cts_device *cts_dev,
    const char *desc, u16 *data, bool to_console)
{
#define SPLIT_LINE_STR \
    "--------------------------------------------------------"\
    "--------------------------------------------------------"
#define ROW_NUM_FORMAT_STR  "%2d | "
#define COL_NUM_FORMAT_STR  "%-5u "
#define DATA_FORMAT_STR     "%-5u "

    int r, c;
    u32 max, min, sum, average;
    int max_r, max_c, min_r, min_c;
    char line_buf[256];
    int ret = 0;
    int count = 0;
    u8 rows = 0;
    u8 cols = 0;

#ifdef CTS_SWAP_X_Y
    rows = cts_dev->fwdata.cols;
    cols = cts_dev->fwdata.rows;

    if ((strstr(desc, "Noisy") != NULL) ||
        (strstr(desc, "Rawdata MAX") != NULL) ||
        (strstr(desc, "Rawdata MIN") != NULL)) {
        cts_info("No need swap again");
    } else {
        ret = cts_swap_x_y(cts_dev, data);
        if (ret) {
            cts_err("Allocate memory for tmpdata failed");
            //return -ENOMEM;
        }
    }
#else
    rows = cts_dev->fwdata.rows;
    cols = cts_dev->fwdata.cols;
#endif

    max = min = data[0];
    sum = 0;
    max_r = max_c = min_r = min_c = 0;
    for (r = 0; r < rows; r++) {
        for (c = 0; c < cols; c++) {
            u16 val = data[r * cols + c];

            sum += val;
            if (val > max) {
                max = val;
                max_r = r;
                max_c = c;
            } else if (val < min) {
                min = val;
                min_r = r;
                min_c = c;
            }
        }
    }
    average = sum / (rows * cols);

    count = 0;
    count += scnprintf(line_buf + count, sizeof(line_buf) - count,
        " %s test data MIN: [%u][%u]=%u, MAX: [%u][%u]=%u, AVG=%u",
        desc, min_r, min_c, min, max_r, max_c, max, average);
    if (to_console) {
        cts_info(SPLIT_LINE_STR);
        cts_info("%s", line_buf);
        cts_info(SPLIT_LINE_STR);
    }
    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
        cts_write_file(cts_test_data_filp, line_buf, count);
        cts_write_file(cts_test_data_filp, "\n", 1);
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
    }

    count = 0;
    count += scnprintf(line_buf + count, sizeof(line_buf) - count, "   |  ");
    for (c = 0; c < cols; c++) {
        count += scnprintf(line_buf + count, sizeof(line_buf) - count,
            COL_NUM_FORMAT_STR, c);
    }
    if (to_console) {
        cts_info("%s", line_buf);
        cts_info(SPLIT_LINE_STR);
    }
    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp, line_buf, count);
        cts_write_file(cts_test_data_filp, "\n", 1);
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
    }

    for (r = 0; r < rows; r++) {
        count = 0;
        count += scnprintf(line_buf + count, sizeof(line_buf) - count,
            ROW_NUM_FORMAT_STR, r);
        for (c = 0; c < cols; c++) {
            count += scnprintf(line_buf + count, sizeof(line_buf) - count,
                DATA_FORMAT_STR, data[r * cols + c]);
        }
        if (to_console) {
            cts_info("%s", line_buf);
        }
        if (cts_test_data_filp) {
            cts_write_file(cts_test_data_filp, line_buf, count);
            cts_write_file(cts_test_data_filp, "\n", 1);
        }
    }
    if (to_console) {
        cts_info(SPLIT_LINE_STR);
    }
    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
    }
#undef SPLIT_LINE_STR
#undef ROW_NUM_FORMAT_STR
#undef COL_NUM_FORMAT_STR
#undef DATA_FORMAT_STR
}

static bool is_invalid_node(u32 *invalid_nodes, u32 num_invalid_nodes,
    u16 row, u16 col)
{
    int i;

    for (i = 0; i < num_invalid_nodes; i++) {
        if (MAKE_INVALID_NODE(row, col) == invalid_nodes[i]) {
            return true;
        }
    }

    return false;
}

/* Return number of failed nodes */
static int validate_tsdata(struct cts_device *cts_dev, const char *desc, u16 *data,
    u32 *invalid_nodes, u32 num_invalid_nodes, bool per_node, int *min, int *max)
{
#define SPLIT_LINE_STR \
    "------------------------------"

    int r, c;
    /*Tab A9 code for AX6739A-2422 by suyurui at 20230722 start*/
    int failed_cnt = -1;
    bool flag = false;
    /*Tab A9 code for AX6739A-2422 by suyurui at 20230722 end*/
    u8 rows = 0;
    u8 cols = 0;

    cts_info("%s validate data: %s, num invalid node: %u, thresh[0]=[%d, %d]",
        desc, per_node ? "Per-Node" : "Uniform-Threshold",
        num_invalid_nodes, min ? min[0] : INT_MIN, max ? max[0] : INT_MAX);

#ifdef CTS_SWAP_X_Y
    rows = cts_dev->fwdata.cols;
    cols = cts_dev->fwdata.rows;
#else
    rows = cts_dev->fwdata.rows;
    cols = cts_dev->fwdata.cols;
#endif

    for (r = 0; r < rows; r++) {
        for (c = 0; c < cols; c++) {
            int offset = r * cols + c;
            /*Tab A9 code for AX6739A-2422 by suyurui at 20230722 start*/
            flag = true;
            /*Tab A9 code for AX6739A-2422 by suyurui at 20230722 end*/
            if (num_invalid_nodes &&
                is_invalid_node(invalid_nodes, num_invalid_nodes, r, c)) {
                continue;
            }

            if ((min != NULL && data[offset] < min[per_node ? offset : 0]) ||
                (max != NULL && data[offset] > max[per_node ? offset : 0])) {
                /*Tab A9 code for AX6739A-2422 by suyurui at 20230722 start*/
                if (failed_cnt == -1) {
                    cts_info(SPLIT_LINE_STR);
                    cts_info("%s failed nodes:", desc);
                    failed_cnt = 0;
                }
                /*Tab A9 code for AX6739A-2422 by suyurui at 20230722 end*/
                failed_cnt++;

                cts_info("  %3d: [%-2d][%-2d] = %u", failed_cnt, r, c, data[offset]);
            }
        }
    }
    /*Tab A9 code for AX6739A-2422 by suyurui at 20230722 start*/
    if (flag && (failed_cnt == -1)) {
        failed_cnt = 0;
    }
    /*Tab A9 code for AX6739A-2422 by suyurui at 20230722 end*/

    if (failed_cnt) {
        cts_info(SPLIT_LINE_STR);
        cts_info("%s test %d node total failed", desc, failed_cnt);
    }

    return failed_cnt;

#undef SPLIT_LINE_STR
}

static int wait_fw_to_normal_work(struct cts_device *cts_dev)
{
    int i = 0;
    int ret;

    cts_info("Wait fw to normal work");

    do {
        u8 work_mode;

        ret = cts_tcs_get_workmode(cts_dev, &work_mode);
        if (ret) {
            cts_err("Get fw curr work mode failed %d", work_mode);
            continue;
        } else {
            if (work_mode == CTS_FIRMWARE_WORK_MODE_NORMAL)
                return 0;
        }

        mdelay(10);
    } while (++i < 100);

    return ret ? ret : -ETIMEDOUT;
}
/*Tab A9 code for AX6739A-953 by suyurui at 20230610 start*/
static int prepare_test_raw(struct cts_device *cts_dev)
{
    int ret = 0;
    int i = 0;
    u8 workmode = -1;

    cts_info("Prepare test raw");

    cts_reset_device(cts_dev);

    ret = cts_tcs_set_esd_enable(cts_dev, 0x0);
    if (ret) {
        cts_err("Disable esd failed %d", ret);
        return ret;
    }

    ret = cts_tcs_set_mnt_enable(cts_dev, 0x0);
    if (ret) {
        cts_err("Disable mnt failed %d", ret);
        return ret;
    }

    ret = cts_tcs_get_krang_current_workmode(cts_dev, &workmode);
    if (ret) {
        cts_err("Get krang current workmode failed %d", ret);
    } else if (workmode == CTS_TEST_NORMAL_MODE) {
        goto workmode_ready;
    }

    ret = cts_tcs_set_krang_workmode(cts_dev, CTS_TEST_NORMAL_MODE);
    if (ret) {
        cts_err("Set krang workmode failed %d", ret);
        return ret;
    }

    mdelay(100);

    for (i = 0; i < 10; i++) {
        ret = cts_tcs_get_krang_current_workmode(cts_dev, &workmode);
        if (ret) {
            cts_err("Get krang current workmode failed %d", ret);
        } else {
            if (workmode == CTS_TEST_NORMAL_MODE)
                break;
            cts_err("Get krang current workmode: %d != %d", workmode, CTS_TEST_NORMAL_MODE);
        }
        mdelay(20);
    }

    if (ret || i >= 10) {
        return -EINVAL;
    }

workmode_ready:
    ret = cts_tcs_set_pwr_mode(cts_dev, CTS_PWR_MODE_ACTIVE);
    if (ret) {
        cts_err("Set pwr mode to %d(%s) failed %d", CTS_PWR_MODE_ACTIVE, "ACTIVE", ret);
        return ret;
    }

    cts_dev->rtdata.testing = true;

    return 0;
}
/*Tab A9 code for AX6739A-953 by suyurui at 20230610 end*/
/*Tab A9 code for AX6739A-953 by suyurui at 20230609 start*/
static int prepare_test(struct cts_device *cts_dev)
{
    int ret = 0;

    cts_info("Prepare test");

    cts_reset_device(cts_dev);

    ret = cts_tcs_set_esd_enable(cts_dev, 0x0);
    if (ret) {
        cts_err("Disable esd failed %d", ret);
        return ret;
    }

    ret = cts_tcs_set_cneg_enable(cts_dev, 0x0);
    if (ret) {
        cts_err("Disable cneg failed %d", ret);
        return ret;
    }

    ret = cts_tcs_set_mnt_enable(cts_dev, 0x0);
    if (ret) {
        cts_err("Disable mnt failed %d", ret);
        return ret;
    }

    cts_dev->rtdata.testing = true;

    return 0;
}
/*Tab A9 code for AX6739A-953 by suyurui at 20230609 end*/

#define SHORT_COLS_TEST_LOOP            3
/*Tab A9 code for AX6739A-953 by suyurui at 20230609 start*/
#define SHORT_ROWS_TEST_LOOP            3
/*Tab A9 code for AX6739A-953 by suyurui at 20230609 end*/

#ifdef CFG_CTS_HAS_RESET_PIN
int cts_test_reset_pin(struct cts_device *cts_dev, struct cts_test_param *param)
{
    ktime_t start_time, end_time, delta_time;
    int ret;

    if (cts_dev == NULL || param == NULL) {
        cts_err("Reset-pin test with invalid param: cts_dev: %p test param: %p",
            cts_dev, param);
        return -EINVAL;
    }

    cts_info("Reset-Pin test, flags: 0x%08x, drive log file: '%s' buf size: %d",
        param->flags, param->driver_log_filepath, param->driver_log_buf_size);

    start_time = ktime_get();

    ret = cts_stop_device(cts_dev);
    if (ret) {
        cts_err("Stop device failed %d", ret);
        goto show_test_result;
    }

    cts_lock_device(cts_dev);

    cts_plat_set_reset(cts_dev->pdata, 0);
    mdelay(50);

    if (cts_plat_is_normal_mode(cts_dev->pdata)) {
        ret = -EIO;
        cts_err("Device is alive while reset is low");
    }
    cts_plat_set_reset(cts_dev->pdata, 1);
    mdelay(50);

    {
        int r = wait_fw_to_normal_work(cts_dev);
        if (r) {
            cts_err("Wait fw to normal work failed %d", r);
        }
    }

    if (!cts_plat_is_normal_mode(cts_dev->pdata)) {
        ret = -EIO;
        cts_err("Device is offline while reset is high");
    }

#ifdef CONFIG_CTS_CHARGER_DETECT
    if (cts_is_charger_exist(cts_dev)) {
        int r = cts_set_dev_charger_attached(cts_dev, true);
        if (r) {
            cts_err("Set dev charger attached failed %d", r);
        }
    }
#endif /* CONFIG_CTS_CHARGER_DETECT */

#ifdef CONFIG_CTS_EARJACK_DETECT
    if (cts_is_earjack_exist(cts_dev)) {
        int r = cts_set_dev_earjack_attached(cts_dev, true);
        if (r)
            cts_err("Set dev earjack attached failed %d", r);
    }
#endif /* CONFIG_CTS_EARJACK_DETECT */

#ifdef CONFIG_CTS_GLOVE
    if (cts_is_glove_enabled(cts_dev)) {
        cts_enter_glove_mode(cts_dev);
    }
#endif

#ifdef CFG_CTS_FW_LOG_REDIRECT
    if (cts_is_fw_log_redirect(cts_dev)) {
        cts_enable_fw_log_redirect(cts_dev);
    }
#endif

    cts_unlock_device(cts_dev);

    {
        int r = cts_start_device(cts_dev);
        if (r) {
            cts_err("Start device failed %d", r);
        }
    }

    if (!cts_dev->rtdata.program_mode) {
        cts_set_normal_addr(cts_dev);
    }

show_test_result:
    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);
    if (ret) {
        cts_info("Reset-Pin test FAIL %d(%s), ELAPSED TIME: %lldms",
            ret, cts_strerror(ret), ktime_to_ms(delta_time));
    } else {
        cts_info("Reset-Pin test PASS, ELAPSED TIME: %lldms",
            ktime_to_ms(delta_time));
    }

    return ret;
}
#endif

int cts_test_int_pin(struct cts_device *cts_dev, struct cts_test_param *param)
{
    ktime_t start_time, end_time, delta_time;
    int ret;

    if (cts_dev == NULL || param == NULL) {
        cts_err("Int-pin test with invalid param: cts_dev: %p test param: %p",
            cts_dev, param);
        return -EINVAL;
    }

    cts_info("Int-Pin test, flags: 0x%08x, drive log file: '%s' buf size: %d",
        param->flags, param->driver_log_filepath, param->driver_log_buf_size);

    start_time = ktime_get();

    ret = cts_stop_device(cts_dev);
    if (ret) {
        cts_err("Stop device failed %d", ret);
        goto show_test_result;
    }

    cts_lock_device(cts_dev);
    ret = cts_tcs_set_int_test(cts_dev, 1);
    if (ret) {
        cts_err("Enable Int Test failed");
        goto unlock_device;
    }
    ret = cts_tcs_set_int_pin(cts_dev, 1);
    if (ret) {
        cts_err("Enable Int Test High failed");
        goto exit_int_test;
    }
    mdelay(10);
    if (cts_plat_get_int_pin(cts_dev->pdata) == 0) {
        cts_err("INT pin state != HIGH");
        ret = -EFAULT;
        goto exit_int_test;
    }

    ret = cts_tcs_set_int_pin(cts_dev, 0);
    if (ret) {
        cts_err("Enable Int Test LOW failed");
        goto exit_int_test;
    }
    mdelay(10);
    if (cts_plat_get_int_pin(cts_dev->pdata) != 0) {
        cts_err("INT pin state != LOW");
        ret = -EFAULT;
        goto exit_int_test;
    }

exit_int_test:
    {
        int r = cts_tcs_set_int_test(cts_dev, 0);
        if (r) {
            cts_err("Disable Int Test failed %d", r);
        }
    }
    mdelay(10);

unlock_device:
    cts_unlock_device(cts_dev);

    {
        int r = cts_start_device(cts_dev);
        if (r) {
            cts_err("Start device failed %d", r);
        }
    }

show_test_result:
    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);
    if (ret) {
        cts_info("Int-Pin test FAIL %d(%s), ELAPSED TIME: %lldms",
            ret, cts_strerror(ret), ktime_to_ms(delta_time));
    } else {
        cts_info("Int-Pin test PASS, ELAPSED TIME: %lldms",
            ktime_to_ms(delta_time));
    }

    return ret;
}

/*Tab A9 code for AX6739A-953 by suyurui at 20230610 start*/
extern int cts_test_polling_rawdata(struct cts_device *cts_dev,
    u8 *buf, size_t size);
int cts_test_rawdata(struct cts_device *cts_dev, struct cts_test_param *param)
{
    struct cts_rawdata_test_priv_param *priv_param;
    bool driver_validate_data = false;
    bool validate_data_per_node = false;
    bool stop_test_if_validate_fail = false;
    bool dump_test_data_to_user = false;
    bool dump_test_data_to_console = false;
    bool dump_test_data_to_file = false;
    int num_nodes;
    int tsdata_frame_size;
    int frame;
    int  fail_frame = 0;
    u16 *rawdata = NULL;
    ktime_t start_time, end_time, delta_time;
    int i;
    int ret;

    if (cts_dev == NULL || param == NULL ||
        param->priv_param_size != sizeof(*priv_param) ||
        param->priv_param == NULL) {
        cts_err("Rawdata test with invalid param: priv param: %p size: %d",
            param->priv_param, param->priv_param_size);
        return -EINVAL;
    }

    priv_param = param->priv_param;
    if (priv_param->frames <= 0) {
        cts_info("Rawdata test with too little frame %u", priv_param->frames);
        return -EINVAL;
    }

    num_nodes = cts_dev->hwdata->num_row * cts_dev->hwdata->num_col;
    tsdata_frame_size = 2 * num_nodes;

    driver_validate_data =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_DATA);
    validate_data_per_node =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_PER_NODE);
    dump_test_data_to_user =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE);
    dump_test_data_to_console =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE);
    dump_test_data_to_file =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE);
    stop_test_if_validate_fail =
        !!(param->flags & CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED);

    cts_info("Rawdata test, flags: 0x%08x, frames: %d, num invalid node: %u, "
         "test data file: '%s' buf size: %d, drive log file: '%s' buf size: %d",
         param->flags, priv_param->frames, param->num_invalid_node,
         param->test_data_filepath, param->test_data_buf_size,
         param->driver_log_filepath, param->driver_log_buf_size);

    start_time = ktime_get();

    if (dump_test_data_to_user) {
        rawdata = (u16 *) param->test_data_buf;
    } else {
        rawdata = (u16 *) kmalloc(tsdata_frame_size, GFP_KERNEL);
        if (rawdata == NULL) {
            cts_err("Allocate memory for rawdata failed");
            ret = -ENOMEM;
            goto show_test_result;
        }
    }

    /* Stop device to avoid un-wanted interrrupt */
    ret = cts_stop_device(cts_dev);
    if (ret) {
        cts_err("Stop device failed %d", ret);
        goto free_mem;
    }

    if (dump_test_data_to_file) {
        int r = cts_start_dump_test_data_to_file(param->test_data_filepath,
            !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE_APPEND));
        if (r) {
            cts_err("Start dump test data to file failed %d", r);
        }
    }

    cts_lock_device(cts_dev);
    /*Tab A9 code for AX6739A-953 by suyurui at 20230609 start*/
    ret = prepare_test_raw(cts_dev);
    /*Tab A9 code for AX6739A-953 by suyurui at 20230609 end*/
    if (ret) {
        cts_err("Prepare test failed %d", ret);
        goto err_reset_device;
    }

    ret = cts_set_int_data_method(cts_dev, INT_DATA_METHOD_NONE);
    if (ret) {
        cts_err("Set int data method to %d(%s) failed %d", INT_DATA_METHOD_NONE, "NONE", ret);
        goto err_reset_device;
    }
    ret = cts_set_int_data_types(cts_dev, INT_DATA_TYPE_RAWDATA);
    if (ret) {
        cts_err("Set int data types to %d(%s) failed %d", INT_DATA_TYPE_RAWDATA, "RAWDATA", ret);
        goto err_reset_device;
    }
    ret = cts_set_int_data_method(cts_dev, INT_DATA_METHOD_POLLING);
    if (ret) {
        cts_err("Set int data method to %d(%s) failed %d", INT_DATA_METHOD_POLLING, "POLLING", ret);
        goto err_reset_device;
    }

    for (frame = 0; frame < priv_param->frames; frame++) {
        bool data_valid = false;

        for (i = 0; i < 3; i++) {
            ret = cts_test_polling_rawdata(cts_dev, (u8 *)rawdata,
                    RAWDATA_BUFFER_SIZE(cts_dev));
            if (ret < 0) {
                cts_err("Get raw data failed: %d", ret);
                mdelay(30);
            } else {
                data_valid = true;
                break;
            }
        }

        if (!data_valid) {
            ret = -EIO;
            break;
        }

        if (dump_test_data_to_user) {
            *param->test_data_wr_size += tsdata_frame_size;
        }

        if (dump_test_data_to_console || dump_test_data_to_file) {
            cts_dump_tsdata(cts_dev, "Rawdata", rawdata, dump_test_data_to_console);
        }

        if (driver_validate_data) {
            ret = validate_tsdata(cts_dev, "Rawdata", rawdata, param->invalid_nodes,
                param->num_invalid_node, validate_data_per_node, param->min, param->max);
            if (ret) {
                cts_err("Rawdata test failed %d", ret);
                fail_frame++;
                cts_err("Rawdata test has %d nodes failed", ret);
                if (stop_test_if_validate_fail) {
                    break;
                }
            }
        }

        if (dump_test_data_to_user) {
            rawdata += num_nodes;
        }
    }

    if (dump_test_data_to_file) {
        cts_stop_dump_test_data_to_file();
    }

err_reset_device:
    cts_reset_device(cts_dev);
    cts_set_int_data_method(cts_dev, INT_DATA_METHOD_NONE);
    cts_set_int_data_types(cts_dev, INT_DATA_TYPE_NONE);

    cts_dev->rtdata.testing = false;
    cts_unlock_device(cts_dev);
    {
        int r = cts_start_device(cts_dev);
        if (r) {
            cts_err("Start device failed %d", r);
        }
    }

free_mem:
    if (!dump_test_data_to_user && rawdata != NULL) {
        kfree(rawdata);
    }

show_test_result:
    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);

    if (ret) {
        cts_info("Rawdata test FAIL %d(%s), ELAPSED TIME: %lldms",
            ret, cts_strerror(ret), ktime_to_ms(delta_time));
    } else if (fail_frame > 0) {
        cts_info("Rawdata test has %d frame(s) FAIL, ELAPSED TIME: %lldms",
            fail_frame, ktime_to_ms(delta_time));
    } else if (fail_frame == 0) {
        cts_info("Rawdata test PASS, ELAPSED TIME: %lldms",
            ktime_to_ms(delta_time));
    }

    return (ret ? ret : (fail_frame ? fail_frame : 0));
}
/*Tab A9 code for AX6739A-953 by suyurui at 20230610 end*/
/*Tab A9 code for AX6739A-953 by suyurui at 20230610 start*/
int cts_test_noise(struct cts_device *cts_dev, struct cts_test_param *param)
{
    struct cts_noise_test_priv_param *priv_param;
    bool driver_validate_data = false;
    bool validate_data_per_node = false;
    bool dump_test_data_to_user = false;
    bool dump_test_data_to_console = false;
    bool dump_test_data_to_file = false;
    int num_nodes;
    int tsdata_frame_size;
    int frame;
    u16 *buffer = NULL;
    int buf_size = 0;
    u16 *curr_rawdata = NULL;
    u16 *max_rawdata = NULL;
    u16 *min_rawdata = NULL;
    u16 *noise = NULL;
    bool first_frame = true;
    bool data_valid = false;
    ktime_t start_time, end_time, delta_time;
    int i;
    int ret;

    if (cts_dev == NULL || param == NULL ||
        param->priv_param_size != sizeof(*priv_param) ||
        param->priv_param == NULL) {
        cts_err("Noise test with invalid param: priv param: %p size: %d",
            param->priv_param, param->priv_param_size);
        return -EINVAL;
    }

    priv_param = param->priv_param;
    if (priv_param->frames < 2) {
        cts_err("Noise test with too little frame %u", priv_param->frames);
        return -EINVAL;
    }

    num_nodes = cts_dev->hwdata->num_row * cts_dev->hwdata->num_col;
    tsdata_frame_size = 2 * num_nodes;

    driver_validate_data =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_DATA);
    validate_data_per_node =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_PER_NODE);
    dump_test_data_to_user =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE);
    dump_test_data_to_console =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE);
    dump_test_data_to_file =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE);

    cts_info("Noise test, flags: 0x%08x, frames: %d, num invalid node: %u, "
         "test data file: '%s' buf size: %d, drive log file: '%s' buf size: %d",
         param->flags, priv_param->frames, param->num_invalid_node,
         param->test_data_filepath, param->test_data_buf_size,
         param->driver_log_filepath, param->driver_log_buf_size);

    start_time = ktime_get();

    buf_size = (driver_validate_data ? 4 : 1) * tsdata_frame_size;
    buffer = (u16 *) kmalloc(buf_size, GFP_KERNEL);
    if (buffer == NULL) {
        cts_err("Alloc mem for touch data failed");
        ret = -ENOMEM;
        goto show_test_result;
    }

    curr_rawdata = buffer;
    if (driver_validate_data) {
        max_rawdata    = curr_rawdata + 1 * num_nodes;
        min_rawdata    = curr_rawdata + 2 * num_nodes;
        noise        = curr_rawdata + 3 * num_nodes;
    }

    /* Stop device to avoid un-wanted interrrupt */
    ret = cts_stop_device(cts_dev);
    if (ret) {
        cts_err("Stop device failed %d", ret);
        goto free_mem;
    }

    if (dump_test_data_to_file) {
        int r = cts_start_dump_test_data_to_file(param->test_data_filepath,
            !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE_APPEND));
        if (r) {
            cts_err("Start dump test data to file failed %d", r);
        }
    }

    cts_lock_device(cts_dev);
    /*Tab A9 code for AX6739A-953 by suyurui at 20230609 start*/
    ret = prepare_test_raw(cts_dev);
    /*Tab A9 code for AX6739A-953 by suyurui at 20230609 end*/
    if (ret) {
        cts_err("Prepare test failed %d", ret);
        goto disable_get_tsdata;
    }

    ret = cts_set_int_data_method(cts_dev, INT_DATA_METHOD_NONE);
    if (ret) {
        cts_err("Set int data method to %d(%s) failed %d", INT_DATA_METHOD_NONE, "NONE", ret);
        goto disable_get_tsdata;
    }
    ret = cts_set_int_data_types(cts_dev, INT_DATA_TYPE_RAWDATA);
    if (ret) {
        cts_err("Set int data types to %d(%s) failed %d", INT_DATA_TYPE_RAWDATA, "RAWDATA", ret);
        goto disable_get_tsdata;
    }
    ret = cts_set_int_data_method(cts_dev, INT_DATA_METHOD_POLLING);
    if (ret) {
        cts_err("Set int data method to %d(%s) failed %d", INT_DATA_METHOD_POLLING, "POLLING", ret);
        goto disable_get_tsdata;
    }

    for (frame = 0; frame < priv_param->frames; frame++) {
        for (i = 0; i < 3; i++) {
            ret = cts_test_polling_rawdata(cts_dev, (u8 *)curr_rawdata,
                RAWDATA_BUFFER_SIZE(cts_dev));
            if (ret < 0) {
                cts_err("Get raw data failed: %d", ret);
                mdelay(30);
            } else {
                data_valid = true;
                break;
            }
        }

        if (i >= 3) {
            cts_err("Read rawdata failed");
            ret = -EIO;
            goto disable_get_tsdata;
        }

        if (dump_test_data_to_console || dump_test_data_to_file) {
            cts_dump_tsdata(cts_dev, "Noise-rawdata", curr_rawdata,
                dump_test_data_to_console);
        }

        if (dump_test_data_to_user) {
            memcpy(param->test_data_buf + frame * tsdata_frame_size,
                curr_rawdata, tsdata_frame_size);
            *param->test_data_wr_size += tsdata_frame_size;
        }

        if (driver_validate_data) {
            if (unlikely(first_frame)) {
                memcpy(max_rawdata, curr_rawdata, tsdata_frame_size);
                memcpy(min_rawdata, curr_rawdata, tsdata_frame_size);
                first_frame = false;
            } else {
                for (i = 0; i < num_nodes; i++) {
                    if (curr_rawdata[i] > max_rawdata[i]) {
                        max_rawdata[i] = curr_rawdata[i];
                    } else if (curr_rawdata[i] < min_rawdata[i]) {
                        min_rawdata[i] = curr_rawdata[i];
                    }
                }
            }
        }
    }

    data_valid = true;

disable_get_tsdata:
    if (dump_test_data_to_file) {
        cts_stop_dump_test_data_to_file();
    }

    cts_reset_device(cts_dev);
    cts_set_int_data_method(cts_dev, INT_DATA_METHOD_NONE);
    cts_set_int_data_types(cts_dev, INT_DATA_TYPE_NONE);

    cts_dev->rtdata.testing = false;

    cts_unlock_device(cts_dev);
    {
        int r = cts_start_device(cts_dev);
        if (r) {
            cts_err("Start device failed %d", r);
        }
    }

    if (driver_validate_data && data_valid) {
        for (i = 0; i < num_nodes; i++) {
            noise[i] = max_rawdata[i] - min_rawdata[i];
        }

        if (dump_test_data_to_user) {
            memcpy(param->test_data_buf + (frame + 0) * tsdata_frame_size,
                noise, tsdata_frame_size);
            memcpy(param->test_data_buf + (frame + 1) * tsdata_frame_size,
                max_rawdata, tsdata_frame_size);
            memcpy(param->test_data_buf + (frame + 2) * tsdata_frame_size,
                min_rawdata, tsdata_frame_size);
        }

        if (dump_test_data_to_console || dump_test_data_to_file) {
            cts_dump_tsdata(cts_dev, "Noisy", noise,
                dump_test_data_to_console);
            cts_dump_tsdata(cts_dev, "Rawdata MAX", max_rawdata,
                dump_test_data_to_console);
            cts_dump_tsdata(cts_dev, "Rawdata MIN", min_rawdata,
                dump_test_data_to_console);
        }

        ret = validate_tsdata(cts_dev, "Noise test", noise, param->invalid_nodes,
            param->num_invalid_node, validate_data_per_node, param->min, param->max);
    }

free_mem:
    if (buffer) {
        kfree(buffer);
    }

show_test_result:
    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);
    if (ret > 0) {
        cts_info("Noise test has %d nodes FAIL, ELAPSED TIME: %lldms",
             ret, ktime_to_ms(delta_time));
    } else if (ret < 0) {
        cts_info("Noise test FAIL %d(%s), ELAPSED TIME: %lldms",
             ret, cts_strerror(ret), ktime_to_ms(delta_time));
    } else {
        cts_info("Noise test PASS, ELAPSED TIME: %lldms",
             ktime_to_ms(delta_time));
    }

    return ret;
}
/*Tab A9 code for AX6739A-953 by suyurui at 20230610 end*/

/* Return 0 success
    negative value while error occurs
    positive value means how many nodes fail */
/*Tab A9 code for AX6739A-953 by suyurui at 20230609 start*/
int cts_test_open(struct cts_device *cts_dev, struct cts_test_param *param)
{
    bool driver_validate_data = false;
    bool validate_data_per_node = false;
    bool dump_test_data_to_user = false;
    bool dump_test_data_to_console = false;
    bool dump_test_data_to_file = false;
    int num_nodes = 0;
    int tsdata_frame_size = 0;
    int ret = 0;
    /*Tab A9 code for AX6739A-1436 by suyurui at 20230705 start*/
    int count = OPEN_RETRY_COUNT;
    int cell_equal_0_count = 0;
    int cols = 0;
    int rows = 0;
    /*Tab A9 code for AX6739A-1436 by suyurui at 20230705 end*/
    u16 *test_result = NULL;
    u8 mode = 0;
    u8 workmode = -1;
    int i = 0;
    ktime_t start_time, end_time, delta_time;
    u8 old_int_data_method = cts_dev->fwdata.int_data_method;
    u16 old_int_data_types = cts_dev->fwdata.int_data_types;

    if (cts_dev == NULL || param == NULL) {
        cts_err("Open test with invalid param: cts_dev: %p test param: %p",
            cts_dev, param);
        return -EINVAL;
    }

    num_nodes = cts_dev->hwdata->num_row * cts_dev->hwdata->num_col;
    tsdata_frame_size = 2 * num_nodes;

    driver_validate_data =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_DATA);
    validate_data_per_node =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_PER_NODE);
    dump_test_data_to_user =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE);
    dump_test_data_to_console =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE);
    dump_test_data_to_file =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE);

    cts_info("Open test, flags: 0x%08x, num invalid node: %u, "
         "test data file: '%s' buf size: %d, drive log file: '%s' buf size: %d",
         param->flags, param->num_invalid_node,
         param->test_data_filepath, param->test_data_buf_size,
         param->driver_log_filepath, param->driver_log_buf_size);

    start_time = ktime_get();

    if (dump_test_data_to_user) {
        test_result = (u16 *) param->test_data_buf;
    } else {
        test_result = (u16 *) kmalloc(tsdata_frame_size, GFP_KERNEL);
        if (test_result == NULL) {
            cts_err("Allocate memory for test result faild");
            ret = -ENOMEM;
            goto show_test_result;
        }
    }

    ret = cts_stop_device(cts_dev);
    if (ret) {
        cts_err("Stop device failed %d", ret);
        goto free_mem;
    }

    cts_lock_device(cts_dev);
/*Tab A9 code for AX6739A-1436 by suyurui at 20230705 start*/
try_again:
/*Tab A9 code for AX6739A-1436 by suyurui at 20230705 end*/
    ret = prepare_test(cts_dev);
    if (ret) {
        cts_err("Prepare test failed %d", ret);
        goto err_free_test_result;
    }

    ret = cts_tcs_set_openshort_mode(cts_dev, CTS_TEST_OPEN);
    if (ret) {
        cts_err("Set test mode to OPEN_TEST failed %d", ret);
        goto err_free_test_result;
    }

    for (i = 0; i < 3; i++) {
        ret = cts_tcs_get_openshort_mode(cts_dev, &mode);
        if (ret) {
            cts_err("Get test mode failed %d", ret);
        } else {
            if (mode == CTS_TEST_OPEN)
                break;
            cts_err("Get test mode: %d != %d", mode, CTS_TEST_OPEN);
        }
        mdelay(5);
    }
    if (i >= 3 || ret) {
        ret = -EINVAL;
        goto err_free_test_result;
    }

    ret = cts_tcs_get_krang_current_workmode(cts_dev, &workmode);
    if (ret)
        cts_err("Get krang current workmode failed %d", ret);
    else if (workmode == CTS_TEST_OPEN_SHORT_DET_MODE)
        goto workmode_ready;

    ret = cts_tcs_set_krang_workmode(cts_dev, CTS_TEST_OPEN_SHORT_DET_MODE);
    if (ret) {
        cts_err("Set krang workmode failed %d", ret);
        goto err_free_test_result;
    }
    /*Tab A9 code for OT11BSP-51 by suyurui at 20230620 start*/
    mdelay(SELF_TEST_DELAY_TIME);

    for (i = 0; i < SELF_TEST_RETRY_TIME; i++) {
    /*Tab A9 code for OT11BSP-51 by suyurui at 20230620 end*/
        ret = cts_tcs_get_krang_current_workmode(cts_dev, &workmode);
        if (ret) {
            cts_err("Get krang current workmode failed %d", ret);
        } else {
            if (workmode == CTS_TEST_OPEN_SHORT_DET_MODE)
                break;
            cts_err("Get krang current workmode: %d != %d", workmode, CTS_TEST_OPEN_SHORT_DET_MODE);
        }
        mdelay(20);
    }
    if (i >= 10 || ret) {
        ret = -EINVAL;
        goto err_free_test_result;
    }

workmode_ready:
    ret = cts_tcs_set_pwr_mode(cts_dev, CTS_PWR_MODE_ACTIVE);
    if (ret) {
        cts_err("Set pwr mode to %d(%s) failed %d", CTS_PWR_MODE_ACTIVE, "ACTIVE", ret);
        goto err_free_test_result;
    }

    ret = cts_set_int_data_method(cts_dev, INT_DATA_METHOD_NONE);
    if (ret) {
        cts_err("Set int data method to %d(%s) failed %d", INT_DATA_METHOD_NONE, "NONE", ret);
        goto err_free_test_result;
    }
    ret = cts_set_int_data_types(cts_dev, INT_DATA_TYPE_RAWDATA);
    if (ret) {
        cts_err("Set int data types to %d(%s) failed %d", INT_DATA_TYPE_RAWDATA, "RAWDATA", ret);
        goto err_free_test_result;
    }
    ret = cts_set_int_data_method(cts_dev, INT_DATA_METHOD_POLLING);
    if (ret) {
        cts_err("Set int data method to %d(%s) failed %d", INT_DATA_METHOD_POLLING, "POLLING", ret);
        goto err_free_test_result;
    }

    ret = cts_tcs_polling_test_data(cts_dev, (u8 *)test_result,
            RAWDATA_BUFFER_SIZE(cts_dev));
    if (ret) {
        cts_err("Read test result failed %d", ret);
        goto err_free_test_result;
    }

    if (dump_test_data_to_user) {
        *param->test_data_wr_size += tsdata_frame_size;
    }

    if (dump_test_data_to_file) {
        int r = cts_start_dump_test_data_to_file(param->test_data_filepath,
            !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE_APPEND));
        if (r) {
            cts_err("Start dump test data to file failed %d", r);
        }
    }

    if (dump_test_data_to_console || dump_test_data_to_file) {
        cts_dump_tsdata(cts_dev, "Open-circuit", test_result,
            dump_test_data_to_console);
    }

    if (dump_test_data_to_file) {
        cts_stop_dump_test_data_to_file();
    }

    if (driver_validate_data) {
        ret = validate_tsdata(cts_dev, "Open-circuit", test_result, param->invalid_nodes,
            param->num_invalid_node, validate_data_per_node, param->min, param->max);
    }

err_free_test_result:
    cts_reset_device(cts_dev);
    cts_set_int_data_method(cts_dev, old_int_data_method);
    cts_set_int_data_types(cts_dev, old_int_data_types);
    /*Tab A9 code for AX6739A-1436 by suyurui at 20230705 start*/
    if (ret > 0) {
        /*Tab A9 code for OT11BSP-60 by suyurui at 20230712 start*/
        if (dump_test_data_to_user) {
            *param->test_data_wr_size = 0;
            test_result = (u16 *) param->test_data_buf;
        }
        /*Tab A9 code for OT11BSP-60 by suyurui at 20230712 end*/
        cell_equal_0_count = 0;
        for (rows = 0; rows < cts_dev->fwdata.rows; rows++) {
            for (cols = 0; cols < cts_dev->fwdata.cols; cols++) {
                if (0 == test_result[rows * cts_dev->fwdata.cols + cols]) {
                    cell_equal_0_count++;
                }
            }
        }

        if (count-- && ((cell_equal_0_count % CELL_NULL_COUNT == 0) ||
            (cell_equal_0_count % CELL_NULL_COUNT == CELL_NULL_COUNT_ALL) ||
            (cell_equal_0_count % CELL_NULL_COUNT == CELL_NULL_COUNT_FIRST_FRAME) ||
            (cell_equal_0_count % CELL_NULL_COUNT == CELL_NULL_COUNT_SECOND_FRAME)))
        {
            cts_err("cell_equal_0_count:%d,retry count:%d", cell_equal_0_count, count);
            goto try_again;
        }
    }
    /*Tab A9 code for AX6739A-1436 by suyurui at 20230705 end*/

    cts_dev->rtdata.testing = false;

#ifdef CONFIG_CTS_CHARGER_DETECT
    if (cts_is_charger_exist(cts_dev)) {
        int r = cts_set_dev_charger_attached(cts_dev, true);
        if (r) {
            cts_err("Set dev charger attached failed %d", r);
        }
    }
#endif

#ifdef CONFIG_CTS_EARJACK_DETECT
    if (cts_is_earjack_exist(cts_dev)) {
        int r = cts_set_dev_earjack_attached(cts_dev, true);
        if (r)
            cts_err("Set dev earjack attached failed %d", r);
    }
#endif

#ifdef CONFIG_CTS_GLOVE
    if (cts_is_glove_enabled(cts_dev)) {
        cts_enter_glove_mode(cts_dev);
    }
#endif

#ifdef CFG_CTS_FW_LOG_REDIRECT
    if (cts_is_fw_log_redirect(cts_dev)) {
        cts_enable_fw_log_redirect(cts_dev);
    }
#endif

    cts_unlock_device(cts_dev);
    cts_start_device(cts_dev);

free_mem:
    if (!dump_test_data_to_user && test_result) {
        kfree(test_result);
    }

show_test_result:
    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);
    if (ret > 0) {
        cts_info("Open test has %d nodes FAIL, ELAPSED TIME: %lldms",
             ret, ktime_to_ms(delta_time));
    } else if (ret < 0) {
        cts_info("Open test FAIL %d(%s), ELAPSED TIME: %lldms",
             ret, cts_strerror(ret), ktime_to_ms(delta_time));
    } else {
        cts_info("Open test PASS, ELAPSED TIME: %lldms",
             ktime_to_ms(delta_time));
    }

    return ret;
}
/*Tab A9 code for AX6739A-953 by suyurui at 20230609 end*/

/* Return 0 success
    negative value while error occurs
    positive value means how many nodes fail */
/*Tab A9 code for AX6739A-953 by suyurui at 20230609 start*/
int cts_test_short(struct cts_device *cts_dev, struct cts_test_param *param)
{
    bool driver_validate_data = false;
    bool validate_data_per_node = false;
    bool stop_if_failed = false;
    bool dump_test_data_to_user = false;
    bool dump_test_data_to_console = false;
    bool dump_test_data_to_file = false;
    int num_nodes = 0;
    int tsdata_frame_size = 0;
    int loopcnt = 0;
    int retries = 0;
    /*Tab A9 code for OT11BSP-51 by suyurui at 20230620 start*/
    int count = RETRY_COUNT;
    /*Tab A9 code for OT11BSP-51 by suyurui at 20230620 end*/
    int ret = 0;
    u8 mode = 0;
    u8 workmode = -1;
    int i = 0;
    u16 *test_result = NULL;
    ktime_t start_time, end_time, delta_time;
    u8 old_int_data_method = cts_dev->fwdata.int_data_method;
    u16 old_int_data_types = cts_dev->fwdata.int_data_types;

    if (cts_dev == NULL || param == NULL) {
        cts_err("Short test with invalid param: cts_dev: %p test param: %p",
             cts_dev, param);
        return -EINVAL;
    }

    num_nodes = cts_dev->hwdata->num_row * cts_dev->hwdata->num_col;
    tsdata_frame_size = 2 * num_nodes;

    driver_validate_data =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_DATA);
    validate_data_per_node =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_PER_NODE);
    dump_test_data_to_user =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE);
    dump_test_data_to_console =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE);
    dump_test_data_to_file =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE);
    stop_if_failed =
        !!(param->flags & CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED);

    cts_info("Short test, flags: 0x%08x, num invalid node: %u, "
         "test data file: '%s' buf size: %d, drive log file: '%s' buf size: %d",
         param->flags, param->num_invalid_node,
         param->test_data_filepath, param->test_data_buf_size,
         param->driver_log_filepath, param->driver_log_buf_size);

    start_time = ktime_get();

    if (dump_test_data_to_user) {
        test_result = (u16 *) param->test_data_buf;
    } else {
        test_result = (u16 *) kmalloc(tsdata_frame_size, GFP_KERNEL);
        if (test_result == NULL) {
            cts_err("Allocate test result buffer failed");
            ret = -ENOMEM;
            goto show_test_result;
        }
    }

    ret = cts_stop_device(cts_dev);
    if (ret) {
        cts_err("Stop device failed %d", ret);
        goto err_free_test_result;
    }

    cts_lock_device(cts_dev);
/*Tab A9 code for OT11BSP-51 by suyurui at 20230620 start*/
try_again:
/*Tab A9 code for OT11BSP-51 by suyurui at 20230620 end*/
    ret = prepare_test(cts_dev);
    if (ret) {
        cts_err("Prepare test failed %d", ret);
        goto err_reset_device;
    }

    ret = cts_tcs_set_short_test_type(cts_dev, CTS_SHORT_TEST_UNDEFINED);
    if (ret) {
        cts_err("Set short test type to UNDEFINED failed %d", ret);
        goto err_reset_device;
    }
    ret = cts_tcs_set_openshort_mode(cts_dev, CTS_TEST_SHORT);
    if (ret) {
        cts_err("Set test mode to SHORT_TEST failed %d", ret);
        goto err_reset_device;
    }
    for (i = 0; i < 3; i++) {
        ret = cts_tcs_get_openshort_mode(cts_dev, &mode);
        if (ret) {
            cts_err("Get test mode failed %d", ret);
        } else {
            if (mode == CTS_TEST_SHORT)
                break;
            cts_err("Get test mode: %d != %d", mode, CTS_TEST_SHORT);
        }
        mdelay(5);
    }
    if (i >= 3 || ret) {
        ret = -EINVAL;
        goto err_reset_device;
    }

    ret = cts_tcs_get_krang_current_workmode(cts_dev, &workmode);
    if (ret) {
        cts_err("Get krang current workmode failed %d", ret);
    } else if (workmode == CTS_TEST_OPEN_SHORT_DET_MODE) {
        goto workmode_ready;
    }

    ret = cts_tcs_set_krang_workmode(cts_dev, CTS_TEST_OPEN_SHORT_DET_MODE);
    if (ret) {
        cts_err("Set krang workmode failed %d", ret);
        goto err_reset_device;
    }
    /*Tab A9 code for OT11BSP-51 by suyurui at 20230620 start*/
    mdelay(SELF_TEST_DELAY_TIME);

    for (i = 0; i < SELF_TEST_RETRY_TIME; i++) {
    /*Tab A9 code for OT11BSP-51 by suyurui at 20230620 end*/
        ret = cts_tcs_get_krang_current_workmode(cts_dev, &workmode);
        if (ret) {
            cts_err("Get krang current workmode failed %d", ret);
        } else {
            if (workmode == CTS_TEST_OPEN_SHORT_DET_MODE)
                break;
            cts_err("Get krang current workmode: %d != %d", workmode, CTS_TEST_OPEN_SHORT_DET_MODE);
        }
        mdelay(20);
    }
    if (i >= 10 || ret) {
        ret = -EINVAL;
        goto err_reset_device;
    }

workmode_ready:
    ret = cts_tcs_set_pwr_mode(cts_dev, CTS_PWR_MODE_ACTIVE);
    if (ret) {
        cts_err("Set pwr mode to %d(%s) failed %d", CTS_PWR_MODE_ACTIVE, "ACTIVE", ret);
        goto err_reset_device;
    }

    ret = cts_set_int_data_method(cts_dev, INT_DATA_METHOD_NONE);
    if (ret) {
        cts_err("Set int data method to %d(%s) failed %d", INT_DATA_METHOD_NONE, "NONE", ret);
        goto err_reset_device;
    }
    ret = cts_set_int_data_types(cts_dev, INT_DATA_TYPE_RAWDATA);
    if (ret) {
        cts_err("Set int data types to %d(%s) failed %d", INT_DATA_TYPE_RAWDATA, "RAWDATA", ret);
        goto err_reset_device;
    }
    ret = cts_set_int_data_method(cts_dev, INT_DATA_METHOD_POLLING);
    if (ret) {
        cts_err("Set int data method to %d(%s) failed %d", INT_DATA_METHOD_POLLING, "POLLING", ret);
        goto err_reset_device;
    }

    /* Short between colums */
    cts_info("Test short between columns");
    ret = cts_tcs_set_short_test_type(cts_dev, CTS_SHORT_TEST_BETWEEN_COLS);
    if (ret) {
        cts_err("Set short test type to BETWEEN_COLS failed %d", ret);
        goto err_reset_device;
    }
    ret = cts_tcs_polling_test_data(cts_dev, (u8 *)test_result,
            RAWDATA_BUFFER_SIZE(cts_dev));
    if (ret) {
        cts_err("Read test result failed %d", ret);
        goto err_reset_device;
    }

    if (dump_test_data_to_user) {
        *param->test_data_wr_size += tsdata_frame_size;
    }

    if (dump_test_data_to_file) {
        int r = cts_start_dump_test_data_to_file(param->test_data_filepath,
            !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE_APPEND));
        if (r) {
            cts_err("Start dump test data to file failed %d", r);
        }
    }

    if (dump_test_data_to_console || dump_test_data_to_file) {
        cts_dump_tsdata(cts_dev, "Col-short", test_result,
            dump_test_data_to_console);
    }

    if (driver_validate_data) {
        ret = validate_tsdata(cts_dev, "Col-short", test_result, param->invalid_nodes,
            param->num_invalid_node, validate_data_per_node, param->min, param->max);
        if (ret) {
            cts_err("Short between columns test failed %d", ret);
            if (stop_if_failed) {
                goto stop_dump_test_data_to_file;
            }
        }
    }
    if (dump_test_data_to_user) {
        test_result += num_nodes;
    }

    /* Short between rows */
    cts_info("Test short between rows");
    for (retries = 0; retries < 10; retries++) {
        ret = cts_tcs_set_short_test_type(cts_dev, CTS_SHORT_TEST_BETWEEN_ROWS);
        if (!ret)
            break;
        mdelay(10);
    }
    if (ret) {
        cts_err("Set short test type to BETWEEN_ROWS failed %d", ret);
        goto stop_dump_test_data_to_file;
    }
    for (loopcnt = 0; loopcnt < SHORT_ROWS_TEST_LOOP; loopcnt++) {
        ret = cts_tcs_polling_test_data(cts_dev, (u8 *)test_result,
                RAWDATA_BUFFER_SIZE(cts_dev));
        if (ret) {
            cts_err("Read test result failed %d", ret);
            goto stop_dump_test_data_to_file;
        }
        if (dump_test_data_to_user) {
            *param->test_data_wr_size += tsdata_frame_size;
        }
        if (dump_test_data_to_console || dump_test_data_to_file) {
            cts_dump_tsdata(cts_dev, "Row-short", test_result,
                dump_test_data_to_console);
        }
        if (driver_validate_data) {
            ret = validate_tsdata(cts_dev, "Row-short", test_result, param->invalid_nodes,
                param->num_invalid_node, validate_data_per_node, param->min, param->max);
            if (ret) {
                cts_err("Short between rows test failed %d", ret);
                if (stop_if_failed) {
                    goto stop_dump_test_data_to_file;
                }
            }
        }
        if (dump_test_data_to_user) {
            test_result += num_nodes;
        }
    }

    /* Short to GND */
    cts_info("Test short to GND");
    ret = cts_tcs_set_short_test_type(cts_dev, CTS_SHORT_TEST_BETWEEN_GND);
    if (ret) {
        cts_err("Set short test type to SHORT_TO_GND failed %d", ret);
        goto stop_dump_test_data_to_file;
    }

    ret = cts_tcs_polling_test_data(cts_dev, (u8 *)test_result,
        RAWDATA_BUFFER_SIZE(cts_dev));
    if (ret) {
        cts_err("Read test result failed %d", ret);
        goto stop_dump_test_data_to_file;
    }
    if (dump_test_data_to_user) {
        *param->test_data_wr_size += tsdata_frame_size;
    }
    if (dump_test_data_to_console || dump_test_data_to_file) {
        cts_dump_tsdata(cts_dev, "GND-short", test_result,
            dump_test_data_to_console);
    }
    if (driver_validate_data) {
        ret = validate_tsdata(cts_dev, "GND-short", test_result, param->invalid_nodes,
            param->num_invalid_node, validate_data_per_node, param->min, param->max);
        if (ret) {
            cts_err("Short to GND test failed %d", ret);
            if (stop_if_failed) {
                goto stop_dump_test_data_to_file;
            }
        }
    }
    if (dump_test_data_to_user) {
        test_result += num_nodes;
    }

stop_dump_test_data_to_file:
    if (dump_test_data_to_file) {
        cts_stop_dump_test_data_to_file();
    }

err_reset_device:
    cts_reset_device(cts_dev);
    cts_set_int_data_method(cts_dev, old_int_data_method);
    cts_set_int_data_types(cts_dev, old_int_data_types);

    cts_dev->rtdata.testing = false;
    /*Tab A9 code for OT11BSP-51 by suyurui at 20230620 start*/
    if (ret < 0 && count--) {
        /*Tab A9 code for OT11BSP-60 by suyurui at 20230712 start*/
        if (dump_test_data_to_user) {
            *param->test_data_wr_size = 0;
            test_result = (u16 *) param->test_data_buf;
        }
        /*Tab A9 code for OT11BSP-60 by suyurui at 20230712 end*/
        goto try_again;
    }
    /*Tab A9 code for OT11BSP-51 by suyurui at 20230620 end*/

#ifdef CONFIG_CTS_CHARGER_DETECT
    if (cts_is_charger_exist(cts_dev)) {
        int r = cts_set_dev_charger_attached(cts_dev, true);
        if (r) {
            cts_err("Set dev charger attached failed %d", r);
        }
    }
#endif

#ifdef CONFIG_CTS_EARJACK_DETECT
    if (cts_is_earjack_exist(cts_dev)) {
        int r = cts_set_dev_earjack_attached(cts_dev, true);
        if (r)
            cts_err("Set dev earjack attached failed %d", r);
    }
#endif

#ifdef CONFIG_CTS_GLOVE
    if (cts_is_glove_enabled(cts_dev)) {
        cts_enter_glove_mode(cts_dev);
    }
#endif

#ifdef CFG_CTS_FW_LOG_REDIRECT
    if (cts_is_fw_log_redirect(cts_dev)) {
        cts_enable_fw_log_redirect(cts_dev);
    }
#endif

    cts_unlock_device(cts_dev);
    cts_start_device(cts_dev);

err_free_test_result:
    if (!dump_test_data_to_user && test_result) {
        kfree(test_result);
    }

show_test_result:
    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);
    if (ret > 0) {
        cts_info("Short test has %d nodes FAIL, ELAPSED TIME: %lldms",
             ret, ktime_to_ms(delta_time));
    } else if (ret < 0) {
        cts_info("Short test FAIL %d(%s), ELAPSED TIME: %lldms",
             ret, cts_strerror(ret), ktime_to_ms(delta_time));
    } else {
        cts_info("Short test PASS, ELAPSED TIME: %lldms",
             ktime_to_ms(delta_time));
    }

    return ret;
}
/*Tab A9 code for AX6739A-953 by suyurui at 20230609 end*/

static int validate_comp_cap(struct cts_device *cts_dev, const char *desc, u8 *cap,
    u32 *invalid_nodes, u32 num_invalid_nodes, bool per_node, int *min, int *max)
{
#define SPLIT_LINE_STR \
    "------------------------------"

    int r, c;
    /*Tab A9 code for AX6739A-2422 by suyurui at 20230722 start*/
    int failed_cnt = -1;
    bool flag = false;
    /*Tab A9 code for AX6739A-2422 by suyurui at 20230722 end*/
    u8 rows = 0;
    u8 cols = 0;

    cts_info("Validate %s data: %s, num invalid node: %u, thresh[0]=[%d, %d]",
            desc, per_node ? "Per-Node" : "Uniform-Threshold",
            num_invalid_nodes, min ? min[0] : INT_MIN, max ? max[0] : INT_MAX);

#ifdef CTS_SWAP_X_Y
    rows = cts_dev->fwdata.cols;
    cols = cts_dev->fwdata.rows;
#else
    rows = cts_dev->fwdata.rows;
    cols = cts_dev->fwdata.cols;
#endif

    for (r = 0; r < rows; r++) {
        for (c = 0; c < cols; c++) {
            int offset = r * cols + c;
            /*Tab A9 code for AX6739A-2422 by suyurui at 20230722 start*/
            flag = true;
            /*Tab A9 code for AX6739A-2422 by suyurui at 20230722 end*/
            if (num_invalid_nodes &&
                is_invalid_node(invalid_nodes, num_invalid_nodes, r, c)) {
                continue;
            }

            if ((min != NULL && cap[offset] < min[per_node ? offset : 0]) ||
                (max != NULL && cap[offset] > max[per_node ? offset : 0])) {
                /*Tab A9 code for AX6739A-2422 by suyurui at 20230722 start*/
                if (failed_cnt == -1) {
                    cts_info(SPLIT_LINE_STR);
                    cts_info("%s failed nodes:", desc);
                    failed_cnt = 0;
                }
                /*Tab A9 code for AX6739A-2422 by suyurui at 20230722 end*/
                failed_cnt++;

                cts_info("  %3d: [%-2d][%-2d] = %u", failed_cnt, r, c, cap[offset]);
            }
        }
    }
    /*Tab A9 code for AX6739A-2422 by suyurui at 20230722 start*/
    if (flag && (failed_cnt == -1)) {
        failed_cnt = 0;
    }
    /*Tab A9 code for AX6739A-2422 by suyurui at 20230722 end*/

    if (failed_cnt) {
        cts_info(SPLIT_LINE_STR);
        cts_info("%s test %d node total failed", desc, failed_cnt);
    }

    return failed_cnt;

#undef SPLIT_LINE_STR
}

void cts_dump_comp_cap(struct cts_device *cts_dev, u8 *cap, bool to_console)
{
#define SPLIT_LINE_STR \
    "-----------------------------------------------------------------------------"
#define ROW_NUM_FORMAT_STR  "%2d | "
#define COL_NUM_FORMAT_STR  "%3u "
#define DATA_FORMAT_STR     "%4d"

    int r, c;
    u32 max, min, sum, average;
    int max_r, max_c, min_r, min_c;
    char line_buf[256];
    int count = 0;
    int ret = 0;
    u8 rows = 0;
    u8 cols = 0;

#ifdef CTS_SWAP_X_Y
    rows = cts_dev->fwdata.cols;
    cols = cts_dev->fwdata.rows;

    ret = cts_swap_cap_x_y(cts_dev, cap);
    if (ret) {
        cts_err("Allocate memory for tmpdata failed");
    }
#else
    rows = cts_dev->fwdata.rows;
    cols = cts_dev->fwdata.cols;
#endif

    max = min = cap[0];
    sum = 0;
    max_r = max_c = min_r = min_c = 0;
    for (r = 0; r < rows; r++) {
        for (c = 0; c < cols; c++) {
            u16 val = cap[r * cols + c];

            sum += val;
            if (val > max) {
                max = val;
                max_r = r;
                max_c = c;
            } else if (val < min) {
                min = val;
                min_r = r;
                min_c = c;
            }
        }
    }
    average = sum / (rows * cols);

    count = 0;
    count += scnprintf(line_buf + count, sizeof(line_buf) - count,
        " Compensate Cap MIN: [%u][%u]=%u, MAX: [%u][%u]=%u, AVG=%u",
        min_r, min_c, min, max_r, max_c, max, average);
    if (to_console) {
        cts_info(SPLIT_LINE_STR);
        cts_info("%s", line_buf);
        cts_info(SPLIT_LINE_STR);
    }
    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
        cts_write_file(cts_test_data_filp, line_buf, count);
        cts_write_file(cts_test_data_filp, "\n", 1);
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
    }

    count = 0;
    count += scnprintf(line_buf + count, sizeof(line_buf) - count, "      ");
    for (c = 0; c < cols; c++) {
        count += scnprintf(line_buf + count, sizeof(line_buf) - count,
            COL_NUM_FORMAT_STR, c);
    }
    if (to_console) {
        cts_info("%s", line_buf);
        cts_info(SPLIT_LINE_STR);
    }
    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp, line_buf, count);
        cts_write_file(cts_test_data_filp, "\n", 1);
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
    }

    for (r = 0; r < rows; r++) {
        count = 0;
        count += scnprintf(line_buf + count, sizeof(line_buf) - count,
            ROW_NUM_FORMAT_STR, r);
        for (c = 0; c < cols; c++) {
            count += scnprintf(line_buf + count, sizeof(line_buf) - count,
                DATA_FORMAT_STR, cap[r * cols + c]);
        }
        if (to_console) {
            cts_info("%s", line_buf);
        }
        if (cts_test_data_filp) {
            cts_write_file(cts_test_data_filp, line_buf, count);
            cts_write_file(cts_test_data_filp, "\n", 1);
        }
    }

    if (to_console) {
        cts_info(SPLIT_LINE_STR);
    }
    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
    }
#undef SPLIT_LINE_STR
#undef ROW_NUM_FORMAT_STR
#undef COL_NUM_FORMAT_STR
#undef DATA_FORMAT_STR
}
/*Tab A9 code for AX6739A-953 by suyurui at 20230610 start*/
int cts_test_compensate_cap(struct cts_device *cts_dev,
    struct cts_test_param *param)
{
    bool driver_validate_data = false;
    bool validate_data_per_node = false;
    bool dump_test_data_to_user = false;
    bool dump_test_data_to_console = false;
    bool dump_test_data_to_file = false;
    int num_nodes;
    u8 *cap = NULL;
    int ret = 0;
    ktime_t start_time, end_time, delta_time;
    u8 old_int_data_method = cts_dev->fwdata.int_data_method;
    u16 old_int_data_types = cts_dev->fwdata.int_data_types;

    if (cts_dev == NULL || param == NULL) {
        cts_err("Compensate cap test with invalid param: cts_dev: %p test param: %p",
            cts_dev, param);
        return -EINVAL;
    }

    num_nodes = cts_dev->hwdata->num_row * cts_dev->hwdata->num_col;

    driver_validate_data =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_DATA);
    if (driver_validate_data) {
        validate_data_per_node =
            !!(param->flags & CTS_TEST_FLAG_VALIDATE_PER_NODE);
    }
    dump_test_data_to_user =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE);
    dump_test_data_to_console =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE);
    dump_test_data_to_file =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE);

    cts_info("Compensate cap test, flags: 0x%08x num invalid node: %u, "
         "test data file: '%s' buf size: %d, drive log file: '%s' buf size: %d",
         param->flags, param->num_invalid_node,
         param->test_data_filepath, param->test_data_buf_size,
         param->driver_log_filepath, param->driver_log_buf_size);

    start_time = ktime_get();

    if (dump_test_data_to_user) {
        cap = (u8 *) param->test_data_buf;
    } else {
        cap = (u8 *) kzalloc(num_nodes, GFP_KERNEL);
        if (cap == NULL) {
            cts_err("Alloc mem for compensate cap failed");
            ret = -ENOMEM;
            goto show_test_result;
        }
    }

    /* Stop device to avoid un-wanted interrrupt */
    ret = cts_stop_device(cts_dev);
    if (ret) {
        cts_err("Stop device failed %d", ret);
        goto free_mem;
    }

    cts_lock_device(cts_dev);
    ret = prepare_test_raw(cts_dev);
    if (ret) {
        cts_err("Prepare test failed %d", ret);
        goto unlock_device;
    }
    ret = cts_set_int_data_method(cts_dev, INT_DATA_METHOD_NONE);
    if (ret) {
        cts_err("Set int data method to %d(%s) failed %d", INT_DATA_METHOD_NONE, "NONE", ret);
        goto unlock_device;
    }
    ret = cts_set_int_data_types(cts_dev, INT_DATA_TYPE_CNEGDATA);
    if (ret) {
        cts_err("Set int data types to %d(%s) failed %d", INT_DATA_TYPE_CNEGDATA, "CNEGDATA", ret);
        goto unlock_device;
    }
    ret = cts_set_int_data_method(cts_dev, INT_DATA_METHOD_POLLING);
    if (ret) {
        cts_err("Set int data method to %d(%s) failed %d", INT_DATA_METHOD_POLLING, "POLLING", ret);
        goto unlock_device;
    }

    ret = cts_tcs_polling_test_data(cts_dev, cap, num_nodes);

unlock_device:
    cts_reset_device(cts_dev);
    cts_set_int_data_method(cts_dev, old_int_data_method);
    cts_set_int_data_types(cts_dev, old_int_data_types);

    cts_dev->rtdata.testing = false;
#ifdef CONFIG_CTS_CHARGER_DETECT
    if (cts_is_charger_exist(cts_dev)) {
        int r = cts_set_dev_charger_attached(cts_dev, true);
        if (r) {
            cts_err("Set dev charger attached failed %d", r);
        }
    }
#endif //CONFIG_CTS_CHARGER_DETECT

#ifdef CONFIG_CTS_EARJACK_DETECT
    if (cts_is_earjack_exist(cts_dev)) {
        int r = cts_set_dev_earjack_attached(cts_dev, true);
        if (r)
            cts_err("Set dev earjack attached failed %d", r);
    }
#endif //CONFIG_CTS_EARJACK_DETECT

#ifdef CONFIG_CTS_GLOVE
    if (cts_is_glove_enabled(cts_dev)) {
        cts_enter_glove_mode(cts_dev);
    }
#endif //CONFIG_CTS_GLOVE

#ifdef CFG_CTS_FW_LOG_REDIRECT
    if (cts_is_fw_log_redirect(cts_dev)) {
        cts_enable_fw_log_redirect(cts_dev);
    }
#endif //CFG_CTS_FW_LOG_REDIRECT

    cts_unlock_device(cts_dev);

    if (ret) {
        cts_err("Get compensate cap failed %d", ret);
        goto start_device;
    }

    if (dump_test_data_to_user) {
        *param->test_data_wr_size = num_nodes;
    }

    if (dump_test_data_to_file) {
        int r = cts_start_dump_test_data_to_file(param->test_data_filepath,
            !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE_APPEND));
        if (r) {
            cts_err("Start dump test data to file failed %d", r);
        }
    }

    if (dump_test_data_to_console || dump_test_data_to_file) {
        cts_dump_comp_cap(cts_dev, cap, dump_test_data_to_console);
    }

    if (dump_test_data_to_file) {
        cts_stop_dump_test_data_to_file();
    }

    if (driver_validate_data) {
        ret = validate_comp_cap(cts_dev, "Compensate-Cap", cap, param->invalid_nodes,
            param->num_invalid_node, validate_data_per_node, param->min, param->max);
    }

start_device:
    {
        int r = cts_start_device(cts_dev);
        if (r) {
            cts_err("Start device failed %d", r);
        }
    }

free_mem:
    if (!dump_test_data_to_user && cap) {
        kfree(cap);
    }

show_test_result:
    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);
    if (ret > 0) {
        cts_info("Compensate-Cap test has %d nodes FAIL, ELAPSED TIME: %lldms",
             ret, ktime_to_ms(delta_time));
    } else if (ret < 0) {
        cts_info("Compensate-Cap test FAIL %d(%s), ELAPSED TIME: %lldms",
             ret, cts_strerror(ret), ktime_to_ms(delta_time));
    } else {
        cts_info("Compensate-Cap test PASS, ELAPSED TIME: %lldms",
             ktime_to_ms(delta_time));
    }

    return ret;
}
/*Tab A9 code for AX6739A-953 by suyurui at 20230610 end*/

MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
MODULE_IMPORT_NS(ANDROID_GKI_VFS_EXPORT_ONLY);

