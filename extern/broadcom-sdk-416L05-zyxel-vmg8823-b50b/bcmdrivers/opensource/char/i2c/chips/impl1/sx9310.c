
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>  /* kzalloc() */
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/bcm_log.h>
#include <boardparms.h>
#include "bcm_map_part.h"
#include <bcmnetlink.h>	/* kerSysSendtoMonitorTask()*/
#include <bcm_intr.h>
#include <linux/delay.h>
#include <board.h>

/* I2C client chip addresses */
#define SX9310_I2C_ADDR 0x28

// SX9310 register
// Interrupt & Status
#define REG_IRQSRC	0x00
	#define SHIFT_RESETIRQ		7
	#define MASK_RESETIRQ		(0x1 << SHIFT_RESETIRQ)
	#define SHIFT_CLOSEANYIRQ	6
	#define MASK_CLOSEANYIRQ	(0x1 << SHIFT_CLOSEANYIRQ)
	#define SHIFT_FARANYIRQ		5
	#define MASK_FARANYIRQ		(0x1 << SHIFT_FARANYIRQ)
	#define SHIFT_COMPONEIRQ	4
	#define MASK_COMPONEIRQ		(0x1 << SHIFT_COMPONEIRQ)
	#define SHIFT_CONVDONEIRQ	3
	#define MASK_CONVDONEIRQ	(0x1 << SHIFT_CONVDONEIRQ)
	#define SHIFT_CLOSEALLIRQ	2
	#define MASK_CLOSEALLIRQ	(0x1 << SHIFT_CLOSEALLIRQ)
	#define SHIFT_FARALLIRQ		1
	#define MASK_FARALLIRQ		(0x1 << SHIFT_FARALLIRQ)
	#define SHIFT_SMARTSARIRQ	0
	#define MASK_SMARTSARIRQ	(0x1 << SHIFT_SMARTSARIRQ)
#define REG_STAT1	0x02
	#define SHIFT_PROXSTAT12	7
	#define MASK_PROXSTAT12		(0x1 << SHIFT_PROXSTAT12)
	#define SHIFT_PROXSTATANY	6
	#define MASK_PROXSTATANY	(0x1 << SHIFT_PROXSTATANY)
	#define SHIFT_PROXSTATALL	5
	#define MASK_PROXSTATALL	(0x1 << SHIFT_PROXSTATALL)
#define REG_IRQMSK	0x03
	#define SHIFT_CLOSEANYIRQEN	6
	#define MASK_CLOSEANYIRQEN	(0x1 << SHIFT_CLOSEANYIRQEN)
	#define SHIFT_FARANYIRQEN	5
	#define MASK_FARANYIRQEN	(0x1 << SHIFT_FARANYIRQEN)
	#define SHIFT_COMPDONEIRQEN	4
	#define MASK_COMPDONEIRQEN	(0x1 << SHIFT_COMPDONEIRQEN)
	#define SHIFT_CONVDONEIRQEN	3
	#define MASK_CONVDONEIRQEN	(0x1 << SHIFT_CONVDONEIRQEN)
	#define SHIFT_CLOSEALLIRQEN	2
	#define MASK_CLOSEALLIRQEN	(0x1 << SHIFT_CLOSEALLIRQEN)
	#define SHIFT_FARALLIRQEN	1
	#define MASK_FARALLIRQEN	(0x1 << SHIFT_FARALLIRQEN)
	#define SHIFT_SMARTSARIRQEN	0
	#define MASK_SMARTSARIRQEN	(0x1 << SHIFT_SMARTSARIRQEN)
#define REG_IRQFUNC	0x04
	#define SHIFT_IRQPOLARITY	5
	#define MASK_IRQPOLARITY	(0x1 << SHIFT_IRQPOLARITY)
	#define SHIFT_IRQFUNCTION	1
	#define MASK_IRQFUNCTION	(0xf << SHIFT_IRQFUNCTION)
	#define SHIFT_IRQMODE		0
	#define MASK_IRQMODE		(0x1 << SHIFT_IRQMODE)
// Proximity Sensing Control
#define REG_PROXCTRL0	0x10
	#define SHIFT_SCANPERIOD	4
	#define MASK_SCANPERIOD		(0xf << SHIFT_SCANPERIOD)
	#define SHIFT_SENSOREN		0
	#define MASK_SENSOREN		(0xf << SHIFT_SENSOREN)
#define REG_PROXCTRL4	0x14
	#define SHIFT_FREQ			3
	#define MASK_FREQ			(0x1f << SHIFT_FREQ)
	#define SHIFT_RESOLUTION	0
	#define MASK_RESOLUTION		(0x7 << SHIFT_RESOLUTION)
#define REG_PROXCTRL4	0x14
	#define SHIFT_FREQ			3
	#define MASK_FREQ			(0x1f << SHIFT_FREQ)
#define REG_PROXCTRL7	0x17
	#define SHIFT_AVGDEB		6
	#define MASK_AVGDEB			(0x3 << SHIFT_AVGDEB)
#define REG_PROXCTRL10	0x1A
	#define SHIFT_CLOSEDEB		2
	#define MASK_CLOSEDEB		(0x3 << SHIFT_CLOSEDEB)
	#define SHIFT_FARDEB		0
	#define MASK_FARDEB			(0x3 << SHIFT_FARDEB)
// Miscellaneous
#define REG_I2CADDR		0x40
	#define SHIFT_I2CADDRESS	0
	#define MASK_I2CADDRESS		(0x7f << SHIFT_I2CADDRESS)
#define REG_PAUSE		0x41
	#define SHIFT_PAUSECTRL		0
	#define MASK_PAUSECTRL		(0x1 << SHIFT_PAUSECTRL)
		#define PAUSECTRL_SLEEP_MODE	0x0
		#define PAUSECTRL_RESUME_MODE	0x1
#define REG_WHOAMI		0x42
	#define DEFAULT_WHOAMI	0x01
#define REG_RESET		0x7f
	#define DEFAULT_SOFTRESET	0xde


#include <asm/uaccess.h> /*copy_from_user*/
#include <linux/ctype.h> /*isxdigit*/
#include <linux/proc_fs.h>
#define PROC_ENTRY_DIR "sx9310"
#define PROC_READ_NAME "read"
#define PROC_WRITE_NAME "write"
#define PROC_LAST_EVENT_NAME "last_event"
#define PROC_ENABLE_NAME "enable"
#define PROC_FAR_EVENT_DELAY_TIME_NAME "far_event_delay_time"
typedef enum{
  IDLE,
  CLOSE,
  FAR
}event_type;
char g_Last_Event = 0x0;
	#define EVENT_IDLE				0x0
	#define EVENT_OBJECT_APPROACH	0x1
	#define EVENT_OBJECT_LEAVE		0x2
	#define EVENT_RESET				0x3
const int gObjectLeaveTimeout = 30;	// 30 sec

struct proc_dir_entry *p0, *p1, *p2, *p3, *p4;

/* Addresses to scan */
static unsigned short normal_i2c[] = {SX9310_I2C_ADDR, I2C_CLIENT_END};

#if defined(CONFIG_I2C_GPIO)
#include <linux/platform_device.h>
#include <linux/i2c-gpio.h>
char *g_platform_info_ptr = NULL;
static void sx9310_setup_i2c_gpio(void);
#endif

#define RETURN_IF_NULL_CLIENT(C)    if(!client) return(-1)

static void send_event_work_handler(struct work_struct *w);
static struct workqueue_struct *sensor_wq = 0;
static DECLARE_DELAYED_WORK(send_event_work, send_event_work_handler);
unsigned long gCloseEventDelayTime = 0;	//Default didn`t delay
unsigned long gFarEventDelayTime = 0;
#define DEFAULT_FAR_EVENT_DELAY_TIME	30	// default delay 3 second

/* Each client has this additional data */
struct sx9310_i2c_data {
    struct i2c_client client;
	int event;
};

static struct sx9310_i2c_data *pclient_data;

static int sx9310_init(void);
static int sx9310_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int sx9310_i2c_remove(struct i2c_client *client);
static int sx9310_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
static unsigned short sensorEvent_irq = BP_NOT_DEFINED;
void sensorEvent_irq_init(void);
void sensorEvent_irq_deinit(void);
void sx9310_update_event(void);
extern int BpGetSensorEventExtIntr( unsigned short *pusValue );
static void sensorEvent_timer_handler(unsigned long arg);
static struct timer_list sensorEvent_timer;
extern bcmLogLevel_t bcmLog_getLogLevel(bcmLogId_t logId);
static int sx9310_create_proc(void);
int sx9310_exist;

static const struct i2c_device_id sx9310_i2c_id_table[] = {
    { "sx9310_i2c", 0 },
    { },
};
MODULE_DEVICE_TABLE(i2c, sx9310_i2c_id_table);

static struct i2c_driver sx9310_i2c_driver = {
    .class = ~0,
    .driver = {
        .name = "sx9310_i2c",
    },
    .probe  = sx9310_i2c_probe,
    .remove = sx9310_i2c_remove,
    .id_table = sx9310_i2c_id_table,
    .detect  = sx9310_i2c_detect,
    .address_list = normal_i2c

};

static int __init sx9310_i2c_init(void)
{
#if defined(CONFIG_I2C_GPIO)
    sx9310_setup_i2c_gpio();
#endif
    return i2c_add_driver(&sx9310_i2c_driver);
}

/****************************************************************************/
/* Write Byte: Writes the val into LS Byte of Register                      */
/* Returns:                                                                 */
/*   0 on success, negative value on failure.                               */
/****************************************************************************/
int sx9310_i2c_write_byte(char offset, char val)
{

    char buf[2];
    struct i2c_client *client = &pclient_data->client;

    RETURN_IF_NULL_CLIENT(client);
    BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "\nEntering the function %s \n", __FUNCTION__);

    buf[0] = offset;
    buf[1] = val;

    if (i2c_master_send(client, buf, 2) == 2)
    {
        return 0;
    }

    return -1;

}
EXPORT_SYMBOL(sx9310_i2c_write_byte);

/****************************************************************************/
/* sx9310_i2c_read_byte: Reads Sensor register                                     */
/* Returns:                                                                 */
/*   Successfully read bytes(2) or 0 if no valid Sensor modules.               */
/* Note: the register values are put into *data as Host endianess           */
/****************************************************************************/
char sx9310_i2c_read_byte( char *reg)
{
    struct i2c_msg msg[2];
	char val;
	struct i2c_client *client = &pclient_data->client;

    RETURN_IF_NULL_CLIENT(client);
    BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "Entering the function %s \n", __FUNCTION__);

    /* First write the register  */
    msg[0].addr = msg[1].addr = client->addr;
    msg[0].flags = msg[1].flags = 0;
    msg[0].len = 1;
    msg[0].buf = reg;

    /* Now read the data */
    msg[1].flags = I2C_M_RD;
    msg[1].len = 1;
    msg[1].buf = &val;

    /* On I2C bus, we receive LS byte first. So swap bytes as necessary */
    if(i2c_transfer(client->adapter, msg, 2) == 2)
    {
        return val;
    }

    return -1;
}
EXPORT_SYMBOL(sx9310_i2c_read_byte);

static irqreturn_t sensorEvent_isr(int irq, void *dev_id)
{
	unsigned long currentJiffies = jiffies;

#if defined(CONFIG_BCM963138) || defined(CONFIG_BCM963148)
    BcmHalExternalIrqMask(sensorEvent_irq);
#else
    BcmHalInterruptDisable(sensorEvent_irq);
#endif
	if(bcmLog_getLogLevel(BCM_LOG_ID_I2C) > 1)
		mod_timer(&sensorEvent_timer, currentJiffies + msecs_to_jiffies(100));
	else
		mod_timer(&sensorEvent_timer, (currentJiffies + usecs_to_jiffies(300)));	/* 300 usec */

	return IRQ_HANDLED;
}

static int map_external_irq (int irq)
{
    int map_irq;
    irq &= ~BP_EXT_INTR_FLAGS_MASK;

    switch (irq) {
    case BP_EXT_INTR_0   :
        map_irq = INTERRUPT_ID_EXTERNAL_0;
        break ;
    case BP_EXT_INTR_1   :
        map_irq = INTERRUPT_ID_EXTERNAL_1;
        break ;
    case BP_EXT_INTR_2   :
        map_irq = INTERRUPT_ID_EXTERNAL_2;
        break ;
    case BP_EXT_INTR_3   :
        map_irq = INTERRUPT_ID_EXTERNAL_3;
        break ;
#if defined(CONFIG_BCM96838) || defined(CONFIG_BCM963138) || defined(CONFIG_BCM963148) || defined(CONFIG_BCM96848) || defined(CONFIG_BCM963381)
    case BP_EXT_INTR_4   :
        map_irq = INTERRUPT_ID_EXTERNAL_4;
        break ;
    case BP_EXT_INTR_5   :
        map_irq = INTERRUPT_ID_EXTERNAL_5;
        break ;
#endif
#if defined(CONFIG_BCM963381) || defined(CONFIG_BCM96848)
    case BP_EXT_INTR_6   :
        map_irq = INTERRUPT_ID_EXTERNAL_6;
        break ;
    case BP_EXT_INTR_7   :
        map_irq = INTERRUPT_ID_EXTERNAL_7;
        break ;
#endif
    default           :
        printk ("Invalid External Interrupt definition (%08x)\n", irq) ;
        map_irq = 0 ;
        break ;
    }

    return (map_irq) ;
}

void sensorEvent_irq_init()
{
	int ext_irq_idx;

    if( BpGetSensorEventExtIntr(&sensorEvent_irq) == BP_SUCCESS )
    {
        if( sensorEvent_irq != BP_EXT_INTR_NONE )
        {
            printk("SensorEvent: Interrupt 0x%x is enabled.\n", sensorEvent_irq);
        }
        else
        {
			printk("SensorEvent: Acquire Interrupt 0x%x is failed.\n", sensorEvent_irq);
        }
    }
    else
        return;

    if( sensorEvent_irq != BP_EXT_INTR_NONE )
    {
		ext_irq_idx = (sensorEvent_irq&~BP_EXT_INTR_FLAGS_MASK)-BP_EXT_INTR_0;
		//if (!IsExtIntrConflict(extIntrInfo[ext_irq_idx]))
		{
			sensorEvent_irq = map_external_irq (sensorEvent_irq);
			BcmHalMapInterrupt((FN_HANDLER)sensorEvent_isr, 0, sensorEvent_irq);
#if !defined(CONFIG_ARM)
			BcmHalInterruptEnable(sensorEvent_irq);
#endif
		}
    }
}

void sensorEvent_irq_deinit()
{
    if(sensorEvent_irq != BP_NOT_DEFINED )
    {
		BcmHalInterruptDisable(sensorEvent_irq);
    }
}

static void send_event_work_handler(struct work_struct *w)
{
	kerSysSendtoMonitorTask(MSG_NETLINK_ZYXEL_SENSOR_EVENT, &g_Last_Event, sizeof(char));
}

/* Called when NIRQ was triggered */
void sx9310_update_event(void)
{
	char reg, value;
	unsigned long delayTime = gCloseEventDelayTime;
	int ret=0;

#if 0
	{
		char msb1,lsb1,msb2,lsb2,msb3,lsb3;
		reg = 0x31;
		msb1 = sx9310_i2c_read_byte( &reg );
		reg = 0x32;
		lsb1 = sx9310_i2c_read_byte( &reg );
		reg = 0x33;
		msb2 = sx9310_i2c_read_byte( &reg );
		reg = 0x34;
		lsb2 = sx9310_i2c_read_byte( &reg );
		reg = 0x35;
		msb3 = sx9310_i2c_read_byte( &reg );
		reg = 0x36;
		lsb3 = sx9310_i2c_read_byte( &reg );
		printk("PROXUSEFUL =0x%02x%02x\n", msb1, lsb1);
		printk("PROXAVG    =0x%02x%02x\n", msb2, lsb2);
		printk("PROXDIFF   =0x%02x%02x\n", msb3, lsb3);
	}
#endif

	reg = REG_IRQSRC;
	value = sx9310_i2c_read_byte( &reg );

	if( value & MASK_RESETIRQ )
	{
		g_Last_Event = EVENT_RESET;
	}else if( value == 0 )
	{
		g_Last_Event = EVENT_IDLE;
	}else if( value & MASK_CLOSEANYIRQ )
	{
		g_Last_Event = EVENT_OBJECT_APPROACH;
	}else if( value & MASK_FARANYIRQ )
	{
		g_Last_Event = EVENT_OBJECT_LEAVE;
		delayTime = gFarEventDelayTime;
	}else
		printk("%s: Unknow event(%x)!\n", __FUNCTION__, (unsigned int)value);

	printk("%s: g_Last_Event =%x\n", __FUNCTION__, (unsigned int)g_Last_Event);
	{
		//send event to userspace.
		//kerSysSendtoMonitorTask(MSG_NETLINK_ZYXEL_SENSOR_EVENT, &g_Last_Event, sizeof(char));
		if(delayed_work_pending(&send_event_work))
		{
			ret = cancel_delayed_work(&send_event_work);
			//Returns %true if @dwork was pending and canceled; %false if wasn't pending.
			if(!ret)
				printk(KERN_EMERG "cancel_delayed_work ret=%d\n", ret);
		}
		ret = queue_delayed_work(sensor_wq, &send_event_work, delayTime);
		//Returns %false if @work was already on a queue, %true otherwise.
		if(!ret)
			printk(KERN_EMERG "queue_delayed_work delay %ld ret=%d\n", delayTime, ret);
	}

	return;
}

static Bool sensorEvent_pressed(void)
{
	unsigned int intSts = 0;
	Bool pressed = 1;

#if defined(CONFIG_BCM96838)
	if ((sensorEvent_irq >= INTERRUPT_ID_EXTERNAL_0) && (sensorEvent_irq <= INTERRUPT_ID_EXTERNAL_5)) {
#else
	if ((sensorEvent_irq >= INTERRUPT_ID_EXTERNAL_0) && (sensorEvent_irq <= INTERRUPT_ID_EXTERNAL_3)) {
#endif

#if defined(CONFIG_BCM963138) || defined(CONFIG_BCM963148)
		intSts = kerSysGetGpioValue(MAP_EXT_IRQ_TO_GPIO( sensorEvent_irq - INTERRUPT_ID_EXTERNAL_0));
#elif defined(CONFIG_BCM963381) || defined(CONFIG_BCM96848)
		intSts = PERF->ExtIrqSts & (1 << (sensorEvent_irq - INTERRUPT_ID_EXTERNAL_0 + EI_STATUS_SHFT));
#else
		intSts = PERF->ExtIrqCfg & (1 << (sensorEvent_irq - INTERRUPT_ID_EXTERNAL_0 + EI_STATUS_SHFT));
#endif
	}
	else
		return 0;

	// assume the interrupt was active low
	if( intSts )
	{
		pressed = 0;
	}
	//printk("\n%s: intSts(%d)\n",__func__,intSts);

	return pressed;
}
static int isCheckEvent=0;
static void sensorEvent_timer_handler(unsigned long arg)
{
    unsigned long currentJiffies = jiffies;
    if ( sensorEvent_pressed() ) {
		if( isCheckEvent == 0 )
		{
			//printk("sensorEvent_timer_handler: NIRQ was triggered and check the event soruce.\n");
			sx9310_update_event();
			isCheckEvent = 1;
			if(bcmLog_getLogLevel(BCM_LOG_ID_I2C) > 1)
				mod_timer(&sensorEvent_timer, currentJiffies + msecs_to_jiffies(100));
			else
				mod_timer(&sensorEvent_timer, currentJiffies + usecs_to_jiffies(600));
		}else
		{
			//printk("sensorEvent_timer_handler: NIRQ still triggered and not be clean.\n");
			if(bcmLog_getLogLevel(BCM_LOG_ID_I2C) > 1)
				mod_timer(&sensorEvent_timer, currentJiffies + msecs_to_jiffies(1));
			else
				mod_timer(&sensorEvent_timer, currentJiffies + usecs_to_jiffies(300));
		}
    }
    else {
#if defined(CONFIG_BCM963138) || defined(CONFIG_BCM963148)
        BcmHalExternalIrqUnmask(sensorEvent_irq);
#else
        BcmHalInterruptEnable(sensorEvent_irq);
#endif
		//printk("sensorEvent_timer_handler: NIRQ was clean, re-enable the interrupt.\n");
		isCheckEvent = 0;
    }
}

static int sx9310_init(void)
{
    char reg,value;
    int ret = 0;

    sx9310_exist = 0;

	//clear NIRQ for power up
	reg = REG_IRQSRC;
	value = sx9310_i2c_read_byte( &reg );

	//reset sensor
	reg = REG_RESET; value=DEFAULT_SOFTRESET;
	if ( sx9310_i2c_write_byte( reg, value ) )
    {
		printk("%s:Reset fail.\n", __FUNCTION__);
        sx9310_exist = 0;
        return -1;
    }
	else
    {
		printk("%s:Reset succeed.\n", __FUNCTION__);

        /* Enhancement of auto-detect sensor */
        if (!sx9310_create_proc())
        {
            printk("%s:/proc/sx9310 created.\n", __FUNCTION__);
            sx9310_exist = 1;
        }
        else
        {
            sx9310_exist = 0;
            return -1;
        }
    }

    if (sx9310_exist)
    {
        sensor_wq = create_workqueue("sensor_wq");
        if (!sensor_wq)
        {
            printk(KERN_EMERG "no memory for work queue\n");
            return -ENOMEM;
        }

        //clear NIRQ for soft reset
        reg = REG_IRQSRC;
        value = sx9310_i2c_read_byte( &reg );
        printk("%s:clear NIRQ(%x).\n", __FUNCTION__, value);

        // set default config
        //reg = 0x10; value=0x24;
        //sx9310_i2c_write_byte( reg, value );
        reg = 0x11; value=0x00;
        sx9310_i2c_write_byte( reg, value );
        reg = 0x12; value=0x07;//0x87;
        sx9310_i2c_write_byte( reg, value );
        //reg = 0x13; value=0x02; // For Proximity board hardware version 1.00A 4A.
        reg = 0x13; value=0x03;   // For Proximity board hardware version 1.00C.
        sx9310_i2c_write_byte( reg, value );
        reg = 0x14; value=0x0f;
        sx9310_i2c_write_byte( reg, value );
        reg = 0x15; value=0x0a;//0x09;//0x01;
        sx9310_i2c_write_byte( reg, value );
        reg = 0x16; value=0x20;
        sx9310_i2c_write_byte( reg, value );
        reg = 0x17; value=0x8c;//0x0c;
        sx9310_i2c_write_byte( reg, value );
        reg = 0x18; value=0x26;//0x8d;
        sx9310_i2c_write_byte( reg, value );
        reg = 0x19; value=0x37;//0x27;//0x2b;
        sx9310_i2c_write_byte( reg, value );
        reg = 0x1a; value=0x0a;//0x05;
        sx9310_i2c_write_byte( reg, value );
        reg = 0x1b; value=0x00;
        sx9310_i2c_write_byte( reg, value );
        reg = 0x1c; value=0x00;
        sx9310_i2c_write_byte( reg, value );
        reg = 0x1d; value=0x00;
        sx9310_i2c_write_byte( reg, value );
        reg = 0x1e; value=0x00;
        sx9310_i2c_write_byte( reg, value );
        reg = 0x1f; value=0x00;
        sx9310_i2c_write_byte( reg, value );
        reg = 0x20; value=0x00;
        sx9310_i2c_write_byte( reg, value );
        reg = 0x21; value=0x04;//0x00;
        sx9310_i2c_write_byte( reg, value );
        reg = 0x22; value=0x00;
        sx9310_i2c_write_byte( reg, value );
        reg = 0x23; value=0x00;
        sx9310_i2c_write_byte( reg, value );
        reg = 0x30; value=0x02;
        sx9310_i2c_write_byte( reg, value );
        reg = 0x10; value=0x24;
        sx9310_i2c_write_byte( reg, value );
        //clear NIRQ
        if(bcmLog_getLogLevel(BCM_LOG_ID_I2C) > 1)
        {
            reg = REG_IRQSRC;
            value = sx9310_i2c_read_byte( &reg );
            printk("%s:clear NIRQ(%x).\n", __FUNCTION__, value);
        }
        // Default let sensor go into sleep mode
        // ESMD will let it go back to resume mode when boot ready
        reg = REG_PAUSE; value=(PAUSECTRL_SLEEP_MODE << SHIFT_PAUSECTRL);
        sx9310_i2c_write_byte( reg, value );
        printk("%s:Let sensor in sleep mode!\n", __FUNCTION__);

        gFarEventDelayTime = msecs_to_jiffies(DEFAULT_FAR_EVENT_DELAY_TIME*1000);	// Default delay 3 sec
    }

    return ret;
}

static int sx9310_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int err = 0;
    char reg, value;

    BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "\nEntering the function %s \n", __FUNCTION__);

    if (i2c_check_functionality(client->adapter, I2C_FUNC_I2C) != 0)
    {
        if (!(pclient_data = kzalloc(sizeof(struct sx9310_i2c_data), GFP_KERNEL)))
            err = -ENOMEM;
        else
        {
            /* Setup the i2c client data */
            pclient_data->client.addr = client->addr;
            pclient_data->client.adapter = client->adapter;
            pclient_data->client.driver = client->driver;
            pclient_data->client.flags = client->flags;

			pclient_data->event = 0;	//default set idel event
            i2c_set_clientdata(client, pclient_data);

            /* Enhancement of auto-detect sensor */
            reg = REG_IRQSRC;
            value = sx9310_i2c_read_byte( &reg ); //clear NIRQ for power up
            if (value == 0xff)
            {
                err = -ENXIO;
            }

        }
    }

	if ((err != -ENOMEM) || (err != -ENXIO))
    {
		err = sx9310_init();

        if (!err)
        {
            init_timer(&sensorEvent_timer);
            sensorEvent_timer.function = sensorEvent_timer_handler;
            sensorEvent_timer.expires  = jiffies + msecs_to_jiffies(100);
            sensorEvent_timer.data     = 0;

            sensorEvent_irq_init();
        }
    }

    return err;
}

// Use this ONLY to convert strings of bytes to strings of chars
// use functions from linux/kernel.h for everything else
static void str_to_num(char* in, char* out, int len)
{
    int i;
    memset(out, 0, len);

    for (i = 0; i < len * 2; i ++)
    {
        if ((*in >= '0') && (*in <= '9'))
            *out += (*in - '0');
        else if ((*in >= 'a') && (*in <= 'f'))
            *out += (*in - 'a') + 10;
        else if ((*in >= 'A') && (*in <= 'F'))
            *out += (*in - 'A') + 10;
        else
            *out += 0;

        if ((i % 2) == 0)
            *out *= 16;
        else
            out ++;

        in ++;
    }
    return;
}

static int proc_get_event(char *buf, char **start, off_t off, int cnt, int *eof, void *data)
{
	int len=0;

	if (off > 0)
		return 0;

	//printk("Current event is %x\n", g_Last_Event);
	len += sprintf(buf+len, "%x\n", g_Last_Event);

	return len;
}

static int proc_get_state(char *buf, char **start, off_t off, int cnt, int *eof, void *data)
{
	printk("Call proc_get_state\n");
	return 0;
}

static int proc_read_reg(struct file *f, const char *buf, unsigned long cnt, void *data)
{
    char input[32];
	char input_data[16];
	char reg;
    char value = 0;
	int num_of_octets;
	int i,r;

    if (cnt > 32)
        cnt = 32;

    if (copy_from_user(input, buf, cnt) != 0)
        return -EFAULT;

    r = cnt;

    for (i = 0; i < r; i ++)
    {
        if (!isxdigit(input[i]))
        {
            memmove(&input[i], &input[i + 1], r - i - 1);
            r --;
            i --;
        }
    }

    num_of_octets = r / 2;

    if (num_of_octets != 1)
        return -EFAULT;

	str_to_num(input, input_data, num_of_octets);
	reg = input_data[0];
	//printk("Get Register %x`s.\n", reg);
	value = sx9310_i2c_read_byte( &reg );
	printk("The value of register %x was %x\n", reg, value);

    return cnt;
}

static int proc_write_reg(struct file *f, const char *buf, unsigned long cnt, void *data)
{
    char input[32];
	char input_data[16];
	char reg;
    char value = 0;
	int num_of_octets;
	int i,r;

    if (cnt > 32)
        cnt = 32;

    if (copy_from_user(input, buf, cnt) != 0)
        return -EFAULT;

    r = cnt;

    for (i = 0; i < r; i ++)
    {
        if (!isxdigit(input[i]))
        {
            memmove(&input[i], &input[i + 1], r - i - 1);
            r --;
            i --;
        }
    }

    num_of_octets = r / 2;

    if (num_of_octets != 2)
        return -EFAULT;

	str_to_num(input, input_data, num_of_octets);
	reg = input_data[0];
	value = input_data[1];
	printk("Write %x to Register %x.\n", value, reg);
	if( sx9310_i2c_write_byte( reg, value ) )
		printk("Write failed!\n");
	//else
	//	printk("Write succeed!\n");

    return cnt;
}

static int proc_read_enable(char *buf, char **start, off_t off, int cnt, int *eof, void *data)
{
	int len=0;
	char reg = REG_PAUSE;
	int IsSensorWorking=0;
	char value = 0x0;

	if (off > 0)
		return 0;

	value = sx9310_i2c_read_byte(&reg);
	if( (value && MASK_PAUSECTRL) == PAUSECTRL_RESUME_MODE)
		IsSensorWorking = 1;
	else
		IsSensorWorking = 0;
	len += sprintf(buf+len, "%x\n", IsSensorWorking);

    return len;
}

static int proc_write_enable(struct file *f, const char *buf, unsigned long cnt, void *data)
{
    char input[2];
    char value = 0x0;
    char reg = REG_PAUSE;
    int ret=0;

    if (cnt != 2)
        return -EFAULT;
    if (copy_from_user(input, buf, cnt) != 0)
        return -EFAULT;
    if( strncmp (input,"1",1) == 0)
        value |= (PAUSECTRL_RESUME_MODE << SHIFT_PAUSECTRL);
    else if( strncmp (input,"0",1) == 0) {
        if(delayed_work_pending(&send_event_work))
        {
            ret = cancel_delayed_work(&send_event_work);
            //Returns %true if @dwork was pending and canceled; %false if wasn't pending.
            if(!ret)
                printk(KERN_EMERG "cancel_delayed_work ret=%d\n", ret);
        }
        value |= (PAUSECTRL_SLEEP_MODE << SHIFT_PAUSECTRL);
    }else
        return -EFAULT;
    //printk("Write %x to Register %x.\n", value, reg);
    if( sx9310_i2c_write_byte( reg, value ) )
        printk("Write failed!\n");
    //else
    //printk("Write succeed!\n");

    return cnt;
}

static int proc_read_far_event_delay_time(char *buf, char **start, off_t off, int cnt, int *eof, void *data)
{
	int len = 0;

	if (off > 0)
		return 0;

	//printk("Current Far event time is %u.\n", jiffies_to_msecs(gFarEventDelayTime)/1000);
	len += sprintf(buf+len, "%u\n", jiffies_to_msecs(gFarEventDelayTime)/1000);

    return len;
}

static int proc_write_far_event_delay_time(struct file *f, const char *buf, unsigned long cnt, void *data)
{
    char input[32];
	unsigned long ul;

    if (copy_from_user(input, buf, cnt) != 0)
        return -EFAULT;

	ul = simple_strtoul (input, NULL, 10);

	//printk("Far event time set to %lu.\n", ul);
	gFarEventDelayTime = msecs_to_jiffies(ul*1000);

    return cnt;
}

static int sx9310_create_proc(void)
{
    p0 = proc_mkdir(PROC_ENTRY_DIR, NULL);
    if (p0 == NULL)
    {
        printk("%s: failed to create proc files!\n", __FUNCTION__);
        return ENOMEM;
    }
    p1 = create_proc_entry(PROC_READ_NAME, 0644, p0);
    if (p1 == NULL)
        return -1;
    p1->read_proc  = proc_get_state;
    p1->write_proc = proc_read_reg;
    #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
    //New linux no longer requires proc_dir_entry->owner field.
    #else
    p1->owner       = THIS_MODULE;
    #endif
    p2 = create_proc_entry(PROC_WRITE_NAME, 0644, p0);
    if (p2 == NULL)
        return -1;
    p2->read_proc  = proc_get_state;
    p2->write_proc = proc_write_reg;
    #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
    //New linux no longer requires proc_dir_entry->owner field.
    #else
    p2->owner       = THIS_MODULE;
    #endif
    p2 = create_proc_entry(PROC_LAST_EVENT_NAME, 0644, p0);
    if (p2 == NULL)
        return -1;
    p2->read_proc  = proc_get_event;
    #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
    //New linux no longer requires proc_dir_entry->owner field.
    #else
    p2->owner       = THIS_MODULE;
    #endif
    p3 = create_proc_entry(PROC_ENABLE_NAME, 0644, p0);
    if (p3 == NULL)
        return -1;
    p3->read_proc  = proc_read_enable;
    p3->write_proc = proc_write_enable;
    #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
    //New linux no longer requires proc_dir_entry->owner field.
    #else
    p3->owner       = THIS_MODULE;
    #endif
    p4 = create_proc_entry(PROC_FAR_EVENT_DELAY_TIME_NAME, 0644, p0);
    if (p3 == NULL)
        return -1;
    p4->read_proc  = proc_read_far_event_delay_time;
    p4->write_proc = proc_write_far_event_delay_time;
    #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
    //New linux no longer requires proc_dir_entry->owner field.
    #else
    p4->owner       = THIS_MODULE;
    #endif

    return 0;
}

static int sx9310_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{ //here earlier than probe

    BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "\nEntering the function %s \n", __FUNCTION__);
    strcpy(info->type, "sx9310_i2c");
    info->flags = 0;

    return 0;
}

static int sx9310_i2c_remove(struct i2c_client *client)
{
    int err = 0;

    BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "\nEntering the function %s \n", __FUNCTION__);

    if (sx9310_exist)
    {
        remove_proc_entry(PROC_FAR_EVENT_DELAY_TIME_NAME, p0);
        remove_proc_entry(PROC_ENABLE_NAME, p0);
        remove_proc_entry(PROC_LAST_EVENT_NAME, p0);
        remove_proc_entry(PROC_WRITE_NAME, p0);
        remove_proc_entry(PROC_READ_NAME, p0);
        remove_proc_entry(PROC_ENTRY_DIR, NULL);

        sensorEvent_irq_deinit();
    }

    kfree(i2c_get_clientdata(client));

    if (sensor_wq)
    {
        cancel_delayed_work(&send_event_work);
        flush_workqueue(sensor_wq);
        destroy_workqueue(sensor_wq);
    }


    return err;
}

static void __exit sx9310_i2c_exit(void)
{
#if defined(CONFIG_I2C_GPIO)
    if( g_platform_info_ptr )
        kfree(g_platform_info_ptr);
#endif
    i2c_del_driver(&sx9310_i2c_driver);
}

#if defined(CONFIG_I2C_GPIO)
/****************************************************************************
 * If CONFIG_I2C_GPIO is defined, the kernel I2C gpio driver,
 * kernel/linux/drivers/i2c/busses/i2c-gpio.c, is compiled into the image.
 * Register a device with it. Function calls to i2c_transfer and i2c_master_send
 * go to the i2c-gpio driver.
 *
 * If CONFIG_I2C_GPIO is not defined, another I2C driver such as the Broadcom
 * I2C driver, bcmdrivers/broadcom/char/i2c/busses/impl1/i2c_bcm6xxx.c, is
 * compiled into the image.  Function calls to i2c_transfer and i2c_master_send
 * go to that I2C driver.
 ****************************************************************************/
static void sx9310_setup_i2c_gpio(void)
{
    if( g_platform_info_ptr == NULL )
    {
        g_platform_info_ptr = kzalloc(sizeof(struct platform_device) +
            sizeof(struct i2c_gpio_platform_data), GFP_KERNEL);

        if( g_platform_info_ptr )
        {
            //unsigned short bpGpio_scl, bpGpio_sda;
            struct platform_device *pd =
                (struct platform_device *) g_platform_info_ptr;
            struct i2c_gpio_platform_data *igpd =
                (struct i2c_gpio_platform_data *) (pd + 1);

            pd->name = "i2c-gpio";
            pd->dev.platform_data = (void *)igpd;

            //if( BpGetI2cGpios(&bpGpio_scl, &bpGpio_sda) == BP_SUCCESS )
            {
                igpd->sda_pin = (unsigned short)BP_GPIO_24_AH/*bpGpio_sda*/;
                igpd->scl_pin = (unsigned short)BP_GPIO_25_AH/*bpGpio_scl*/;
            }
            //else
            //    printk("sx9310_Dev: error getting SCL and SDA GPIOs\n");

            /* Set udelay and timeout to 0 to take driver default values. */
            igpd->udelay = igpd->timeout = 0;

            if( platform_device_register(pd) < 0 )
                printk("sx9310_Dev: error registering platform device\n");
        }
        else
            printk("sx9310_Dev: error allocating platform_device info\n");
    }
    else /* should not happen */
        printk("sx9310_Dev: platform_device info already allocated\n");
}
#endif

module_init(sx9310_i2c_init);
module_exit(sx9310_i2c_exit);

MODULE_AUTHOR("Tim Liu");
MODULE_DESCRIPTION("SX9310 I2C driver");
MODULE_LICENSE("GPL");
