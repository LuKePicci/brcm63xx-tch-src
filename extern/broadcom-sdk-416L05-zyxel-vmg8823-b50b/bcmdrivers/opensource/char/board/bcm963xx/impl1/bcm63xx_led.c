/*
    Copyright 2000-2011 Broadcom Corporation

    <:label-BRCM:2011:DUAL/GPL:standard
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License, version 2, as published by
    the Free Software Foundation (the "GPL").
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    
    A copy of the GPL is available at http://www.broadcom.com/licenses/GPLv2.php, or by
    writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
    
    :>
*/
/***************************************************************************
 * File Name  : bcm63xx_led.c
 *
 * Description: 
 *    This file contains bcm963xx board led control API functions. 
 *
 ***************************************************************************/

/* Includes. */
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/capability.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/bcm_assert_locks.h>
#include <asm/uaccess.h>

#include <bcm_map_part.h>
#include <board.h>
#include <boardparms.h>
#include <shared_utils.h>
#include <bcm_led.h>
#include <bcmtypes.h>

extern spinlock_t bcm_gpio_spinlock;

#ifdef ZYXEL_SW_INTERNET_LED_BLINK //__ZyXEL__, David, support SW Internet LED blinking.
extern int bcmxtmrt_dataforwarded(void);
UINT8 ledInternetOn = FALSE;
UINT8 ledInternetDataTimerOn = TRUE;
#endif

#define kFastBlinkCount     0          // 125ms
#define kSlowBlinkCount     1          // 250ms

#define kLedOff             0
#define kLedOn              1

#define kLedGreen           0
#define kLedRed             1

// uncomment // for debug led
// #define DEBUG_LED

typedef int (*BP_LED_FUNC) (unsigned short *);

typedef struct {
    BOARD_LED_NAME ledName;
    BP_LED_FUNC bpFunc;
    BP_LED_FUNC bpFuncFail;
} BP_LED_INFO, *PBP_LED_INFO;

typedef struct {
    short ledGreenGpio;             // GPIO # for LED
    short ledRedGpio;               // GPIO # for Fail LED
    BOARD_LED_STATE ledState;       // current led state
    short blinkCountDown;           // Count for blink states
} LED_CTRL, *PLED_CTRL;

static BP_LED_INFO bpLedInfo[] =
{
    {kLedAdsl, BpGetAdslLedGpio, BpGetAdslFailLedGpio},
    {kLedSecAdsl, BpGetSecAdslLedGpio, BpGetSecAdslFailLedGpio},
    {kLedWanData, BpGetWanDataLedGpio, BpGetWanErrorLedGpio},
    {kLedSes, BpGetWirelessSesLedGpio, NULL},
    {kLedVoip, BpGetVoipLedGpio, NULL},
    {kLedVoip1, BpGetVoip1LedGpio, BpGetVoip1FailLedGpio},
    {kLedVoip2, BpGetVoip2LedGpio, BpGetVoip2FailLedGpio},
    {kLedPots, BpGetPotsLedGpio, NULL},
    {kLedDect, BpGetDectLedGpio, NULL},
    /*ZyXEL Porting , yushiuan , 2015/01/12.B*/
    //Michael.20130618.001.B: Add to support VoIP MWI LED.
#if defined(ZYXEL) && (ZYXEL==1)
    {kLedPhone1MWI, BpGetPhone1MWILedGpio, NULL},
    {kLedPhone2MWI, BpGetPhone2MWILedGpio, NULL},
#endif //defined(ZYXEL) && (ZYXEL==1).
    //Michael.20130618.001.E: Add to support VoIP MWI LED.
    /*ZyXEL Porting , yushiuan , 2015/01/12.End*/
    {kLedGpon, BpGetGponLedGpio, BpGetGponFailLedGpio},
    {kLedMoCA, BpGetMoCALedGpio, BpGetMoCAFailLedGpio},
#if defined(CONFIG_BCM96838) || defined(CONFIG_BCM96848)
    {kLedOpticalLink,  NULL, BpGetOpticalLinkFailLedGpio},
    {kLedUSB, BpGetUSBLedGpio, NULL},
    {kLedSim, BpGetGpioLedSim, NULL},
    {kLedSimITMS, BpGetGpioLedSimITMS, NULL},
    {kLedEpon, BpGetEponLedGpio, BpGetEponFailLedGpio},
#endif
//20130822, TimLiu add for control ZyXEL LED
#if defined(ZYXEL) && (ZYXEL==1)
    {kLedSes2, BpGetWirelessSesLed2Gpio, NULL},
    {kLedVoip1Msg, BpGetVoip1MessageGpio, NULL},
    {kLedVoip2Msg, BpGetVoip2MessageGpio, NULL},
    //Michael.20151207.002.B: Add to support the Red Fail VoIP Phone LED.
    {kLedVoip1Fail, BpGetVoip1FailLedGpio, NULL},
    {kLedVoip2Fail, BpGetVoip2FailLedGpio, NULL},
    //Michael.20151207.002.E: Add.
    {kLedUSB0, BpGetUSB0LedGpio, BpGetUSB0FailLedGpio},
	{kLedUSB1, BpGetUSB1LedGpio, BpGetUSB1FailLedGpio},
	{kLedInternetData, BpGetInternetDataLedGpio, BpGetInternetFailLedGpio},
#if defined(ZYXEL_SUPPORT_3G_LED_GREEN_AND_RED)
	{kLedInternetFail, BpGetInternetFailLedGpio, NULL},
#endif
	{kLedVdsl, BpGetVdslLedGpio, BpGetVdslFailLedGpio},
	{kLedSecVdsl, BpGetSecVdslLedGpio, BpGetSecVdslFailLedGpio},
#if 1
	{kLedPowerOn, BpGetBootloaderPowerOnLedGpio, NULL},
	{kLedPostFail, BpGetBootloaderStopLedGpio, NULL},
#if defined(ZYXEL_FW_UPGRADE_POWER_LED_BLINK)
	{kLedFwUpgrade, BpGetBootloaderStopLedGpio, NULL},
#endif
#else
	{kLedPowerOn, BpGetBootloaderPowerOnLedGpio, BpGetBootloaderStopLedGpio},
#endif
#if defined(ZYPRJ_VMG8948)
	{kLedGuestG, BpGetGuestWirelessLedGpio, NULL},
	{kLedGuestO, BpGetGuestWPSLedGpio, NULL},
#endif
#if defined(ZYXEL_WPS_SPOTLIGHT)	
#if defined(ZYXEL_WPS_SPOTLIGHT_2GPIO)
	{kLedWpsSpotlight, BpGetWPSSpotlightGpio, BpGetWpsSpotlightRedGpio},
#else
	{kLedWpsSpotlight, BpGetWPSSpotlightGpio, NULL},
#endif	
#endif
#if 1 //power management
	{kLedAll, NULL, NULL},
	{kLedAllExceptPower, NULL, NULL},
#endif
#endif
    {kLedEnd, NULL, NULL}
};

// global variables:
static struct timer_list gLedTimer;
static PLED_CTRL gLedCtrl = NULL;
static int gTimerOn = FALSE;
static int gTimerOnRequests = 0;
static unsigned int gLedRunningCounter = 0;  // only used by WLAN
#if 1 //power management
static int gLedPowerMgmt = 1;
#endif

void (*ethsw_led_control)(unsigned long ledMask, int value) = NULL;
EXPORT_SYMBOL(ethsw_led_control);

/** This spinlock protects all access to gLedCtrl, gTimerOn
 *  gTimerOnRequests, gLedRunningCounter, and ledTimerStart function.
 *  Use spin_lock_irqsave to lock the spinlock because ledTimerStart
 *  may be called from interrupt handler (WLAN?)
 */
static spinlock_t brcm_ledlock;
static void ledTimerExpire(void);

#ifdef ZYXEL_SW_INTERNET_LED_BLINK //__ZyXEL__, David, support SW Internet LED blinking.
static struct timer_list gInternetLedTimer;
static void internetLedTimerExpire(void);
int (*isDataTrafficForwarded)(void);
int (*isEthDataTrafficForwarded)(void);
EXPORT_SYMBOL(isDataTrafficForwarded);
EXPORT_SYMBOL(isEthDataTrafficForwarded);
#endif

//**************************************************************************************
// LED operations
//**************************************************************************************
#if defined(ZYXEL) && defined(ZYPRJ_VMG8924_B10A)
void enableInternetLedPower(unsigned short led_state)
{
    //Only pull INET_LINK_LED gpio to high, whenyou need to turn on Internet LED.
    short bpGpio;
    if(BpGetInternetLedEnableGpio(&bpGpio) == BP_SUCCESS)
    {
        GPIO->GPIODir |= GPIO_NUM_TO_MASK(bpGpio);
        if (((bpGpio & BP_ACTIVE_LOW) && (led_state == kLedOn)) ||
            (!(bpGpio & BP_ACTIVE_LOW) && (led_state == kLedOff)))
            GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(bpGpio);
        else
            GPIO->GPIOio |= GPIO_NUM_TO_MASK(bpGpio);
    }
}
#endif

// turn led on and set the ledState
static void setLed (PLED_CTRL pLed, unsigned short led_state, unsigned short led_type)
{
    short led_gpio;
    unsigned short gpio_state;
    unsigned long flags;
//20130822, TimLiu add for control ZyXEL LED
#if defined(ZYXEL) && defined(ZYPRJ_VMG5313)
    short WPS_LED;
#elif defined(ZYXEL) && defined(ZYPRJ_VMG5948)	// Hardware version : SR1
    short WPS_LED, WPS_5G_LED, DECT_LED;
#elif defined(ZYXEL) && defined(ZYPRJ_VMG8948)
	// do nothing
#endif

    if( gLedPowerMgmt == 0)
        return;

    if (led_type == kLedRed)
        led_gpio = pLed->ledRedGpio;
    else
        led_gpio = pLed->ledGreenGpio;

        dev_dbg(NULL,  "********************************************************\n");
        dev_dbg(NULL,  "setLed %d %x\n", led_gpio&0xff, led_gpio);
        dev_dbg(NULL,  "********************************************************\n");

    if (led_gpio == -1)
        return;

    if (((led_gpio & BP_ACTIVE_LOW) && (led_state == kLedOn)) || 
        (!(led_gpio & BP_ACTIVE_LOW) && (led_state == kLedOff)))
        gpio_state = 0;
    else
        gpio_state = 1;

    /* spinlock to protect access to GPIODir, GPIOio */
    spin_lock_irqsave(&bcm_gpio_spinlock, flags);

#if defined(CONFIG_BCM96328) || defined(CONFIG_BCM96318)
    /* Enable LED controller to drive this GPIO */
    if (!(led_gpio & BP_GPIO_SERIAL))
        GPIO->GPIOMode |= GPIO_NUM_TO_MASK(led_gpio);
#endif

#if defined(CONFIG_BCM96362) || defined(CONFIG_BCM963268) 
    /* Enable LED controller to drive this GPIO */
//20130822, TimLiu add for control ZyXEL LED
#if defined(ZYXEL) && defined(ZYPRJ_VMG5313)
    BpGetWirelessSesLedGpio(&WPS_LED);	//WPS LED using GPIO 44, cannot control by LED
    if(led_gpio == WPS_LED)
    {
    	GPIO->GPIOCtrl |= GPIO_NUM_TO_MASK(led_gpio - 32);	// this region control py wlan
        GPIO->GPIODir |= GPIO_NUM_TO_MASK(led_gpio);
    	if( gpio_state )	//low active
    		GPIO->GPIOio |= GPIO_NUM_TO_MASK(led_gpio);
    	else
    		GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(led_gpio);
    	goto finish;
    }
#elif defined(ZYXEL) && defined(ZYPRJ_VMG5948)	// Hardware version : SR1
    //These are all connected after GPIO 24.
    BpGetWirelessSesLedGpio(&WPS_LED);	//WPS LED using GPIO 44, cannot control by LED
    BpGetWirelessSesLed2Gpio(&WPS_5G_LED);
    BpGetDectLedGpio(&DECT_LED);
    if(led_gpio == WPS_LED || led_gpio == WPS_5G_LED || led_gpio == DECT_LED)
    {
    	//GPIO->GPIOCtrl |= GPIO_NUM_TO_MASK(led_gpio - 32);	// this region control by wlan
        //GPIO->GPIODir |= GPIO_NUM_TO_MASK(led_gpio);
    	if( gpio_state )	//low active
    		GPIO->GPIOio |= GPIO_NUM_TO_MASK(led_gpio);
    	else
    		GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(led_gpio);
    	goto finish;
    }
#elif defined(ZYXEL) && defined(ZYPRJ_VMG8948)
	// do nothing
#elif defined(ZYXEL) && defined(ZYPRJ_VMG8924_B10A)
    {
        //Only pull INET_LINK_LED gpio to high, when you need to turn on Internet LED.
        short inetGreenLed;
        BpGetInternetDataLedGpio(&inetGreenLed);
        if( led_gpio == inetGreenLed )
        {
            //printk("===debug isEthDataTrafficForwarded=%d, led_state=%d\n", (isEthDataTrafficForwarded!=NULL), led_state);
            enableInternetLedPower(led_state);
            #ifdef ZYXEL_SW_INTERNET_LED_BLINK
            // If ethWAN linkup, no need to software control Internet LED!
            // Let Internet LED controled by hardware signal when light up and blink.
            // Turn off Internet LED  by software.
            if ((isEthDataTrafficForwarded != NULL) && (led_state != kLedOff))
            {
                goto finish;
            }
            #endif
        }
    }
#endif
    if (!(led_gpio & BP_GPIO_SERIAL))
        GPIO->LEDCtrl |= GPIO_NUM_TO_MASK(led_gpio);
#endif

#if defined(CONFIG_BCM963138) || defined(CONFIG_BCM963148) || defined(CONFIG_BCM963381) || defined(CONFIG_BCM960333) || defined(CONFIG_BCM96848)
    bcm_led_driver_set(led_gpio, led_state);
#endif


#if defined(CONFIG_BCM96328) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM963268) || defined(CONFIG_BCM96318)
    LED->ledMode &= ~(LED_MODE_MASK << GPIO_NUM_TO_LED_MODE_SHIFT(led_gpio));
    if( gpio_state )
        LED->ledMode |= (LED_MODE_OFF << GPIO_NUM_TO_LED_MODE_SHIFT(led_gpio));
    else
        LED->ledMode |= (LED_MODE_ON << GPIO_NUM_TO_LED_MODE_SHIFT(led_gpio));

#elif defined(CONFIG_BCM96838)
    if ( (led_gpio&BP_LED_PIN) || (led_gpio&BP_GPIO_SERIAL) )
    {
        LED->ledMode &= ~(LED_MODE_MASK << GPIO_NUM_TO_LED_MODE_SHIFT(led_gpio));
        if( gpio_state )
            LED->ledMode |= (LED_MODE_OFF << GPIO_NUM_TO_LED_MODE_SHIFT(led_gpio));
        else
            LED->ledMode |= (LED_MODE_ON << GPIO_NUM_TO_LED_MODE_SHIFT(led_gpio));
    }
    else
    {
        led_gpio &= BP_GPIO_NUM_MASK;
        gpio_set_dir(led_gpio, 1);
        gpio_set_data(led_gpio, gpio_state);
    }

#elif !defined(CONFIG_BCM963138) && !defined(CONFIG_BCM963148) && !defined(CONFIG_BCM963381) && !defined(CONFIG_BCM960333) && !defined(CONFIG_BCM96848)
    if (led_gpio & BP_GPIO_SERIAL) {
        while (GPIO->SerialLedCtrl & SER_LED_BUSY);
        if( gpio_state )
            GPIO->SerialLed |= GPIO_NUM_TO_MASK(led_gpio);
        else
            GPIO->SerialLed &= ~GPIO_NUM_TO_MASK(led_gpio);
    }
    else {
        GPIO->GPIODir |= GPIO_NUM_TO_MASK(led_gpio);
        if( gpio_state )
            GPIO->GPIOio |= GPIO_NUM_TO_MASK(led_gpio);
        else
            GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(led_gpio);
    }
#endif

//20130822, TimLiu add for control ZyXEL LED
#if defined(ZYXEL) && (defined(ZYPRJ_VMG5313) || defined(ZYPRJ_VMG5948) || defined(ZYPRJ_VMG8924_B10A))
finish:
#endif
    spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);

}

// toggle the LED
static void ledToggle(PLED_CTRL pLed)
{
    short led_gpio;
    short green_led_gpio , red_led_gpio;
//20130822, TimLiu add for control ZyXEL LED
#if defined(ZYXEL) && defined(ZYPRJ_VMG5313)
    short WPS_LED;
#elif defined(ZYXEL) && defined(ZYPRJ_VMG5948)	// Hardware version : SR1
    short WPS_LED, WPS_5G_LED, DECT_LED;
#elif defined(ZYXEL) && defined(ZYPRJ_VMG8948)
	// do nothing
#endif

   green_led_gpio = pLed->ledGreenGpio ;
   red_led_gpio = pLed->ledRedGpio ;

    if( gLedPowerMgmt == 0)
        return;

    if ((-1== green_led_gpio) && (-1== red_led_gpio))
        return;

    /* Currently all the chips don't support blinking of RED LED */
    if (-1== green_led_gpio)
        return;
  
   led_gpio = green_led_gpio ;

#if defined(CONFIG_BCM96328) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM963268) || defined(CONFIG_BCM96318)
//20130822, TimLiu add for control ZyXEL LED
#if defined(ZYXEL) && defined(ZYPRJ_VMG5313)
    BpGetWirelessSesLedGpio(&WPS_LED);	//WPS LED using GPIO 44, cannot control by LED
    if(led_gpio == WPS_LED)
    {
        GPIO->GPIODir |= GPIO_NUM_TO_MASK(led_gpio);
    	GPIO->GPIOio ^= GPIO_NUM_TO_MASK(led_gpio);
    }else
#elif defined(ZYXEL) && defined(ZYPRJ_VMG5948)	// Hardware version : SR1
    //These are all connected after GPIO 24.
    BpGetWirelessSesLedGpio(&WPS_LED);	//WPS LED using GPIO 44, cannot control by LED
    BpGetWirelessSesLed2Gpio(&WPS_5G_LED);
    BpGetDectLedGpio(&DECT_LED);
    if(led_gpio == WPS_LED || led_gpio == WPS_5G_LED || led_gpio == DECT_LED)
    {
    	//GPIO->GPIOCtrl |= GPIO_NUM_TO_MASK(led_gpio - 32);	// this region control by wlan
        //GPIO->GPIODir |= GPIO_NUM_TO_MASK(led_gpio);
    	GPIO->GPIOio ^= GPIO_NUM_TO_MASK(led_gpio);
    }else
#elif defined(ZYXEL) && defined(ZYPRJ_VMG8948)
	// do nothing
#endif
    LED->ledMode ^= (LED_MODE_MASK << GPIO_NUM_TO_LED_MODE_SHIFT(led_gpio));

#elif defined(CONFIG_BCM963138) || defined(CONFIG_BCM963148) || defined(CONFIG_BCM963381) || defined(CONFIG_BCM96848) 
    bcm_led_driver_toggle(led_gpio);

#elif defined(CONFIG_BCM96838)
    if ( (led_gpio&BP_LED_PIN) || (led_gpio&BP_GPIO_SERIAL) )
        LED->ledMode ^= (LED_MODE_MASK << GPIO_NUM_TO_LED_MODE_SHIFT(led_gpio));
    else
    {
        unsigned long flags;
		led_gpio &= BP_GPIO_NUM_MASK;
        spin_lock_irqsave(&bcm_gpio_spinlock, flags);
        gpio_set_data(led_gpio, 1^gpio_get_data(led_gpio));
        spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);
    }
	
#elif defined(CONFIG_BCM960333)
    {
        unsigned long flags;
        led_gpio &= BP_GPIO_NUM_MASK;
        spin_lock_irqsave(&bcm_gpio_spinlock, flags);
        bcm_led_driver_toggle(led_gpio);
        spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);
    }
#else
#if !defined(CONFIG_BCM963138) && !defined(CONFIG_BCM963148) && !defined(CONFIG_BCM963381) && !defined(CONFIG_BCM96848)
    if (led_gpio & BP_GPIO_SERIAL) {
        while (GPIO->SerialLedCtrl & SER_LED_BUSY);
        GPIO->SerialLed ^= GPIO_NUM_TO_MASK(led_gpio);
    }
    else {
        unsigned long flags;

        spin_lock_irqsave(&bcm_gpio_spinlock, flags);
        GPIO->GPIODir |= GPIO_NUM_TO_MASK(led_gpio);
        GPIO->GPIOio ^= GPIO_NUM_TO_MASK(led_gpio);
        spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);
    }

#endif /* !defined(CONFIG_BCM963138) && !defined(CONFIG_BCM960333) && !defined(CONFIG_BCM963148) */
#endif
}   

#ifdef ZYXEL_SW_INTERNET_LED_BLINK //__ZyXEL__, David, support SW Internet LED blinking.
/** Start the Internet LED timer
 *
 * Must be called with brcm_ledlock held
 */
static void internetLedTimerStart(void)
{
#if defined(DEBUG_LED)
    printk("led: add_timer\n");
#endif

    BCM_ASSERT_HAS_SPINLOCK_C(&brcm_ledlock);

    init_timer(&gInternetLedTimer);
    gInternetLedTimer.function = (void*)internetLedTimerExpire;
    gInternetLedTimer.expires = jiffies + (HZ/4); /*flash frequency 2Hz*/
    add_timer (&gInternetLedTimer);
}

static void internetLedTimerExpire(void)
{
    unsigned long flags;
    int ret = 0;
    int ret1 = 0;

#if 0//1 //power management
	if(ledInternetDataTimerOn && gLedPowerMgmt)
#else    
	if (ledInternetDataTimerOn)
#endif
	{
        if (ledInternetOn == FALSE)
        {
            internetLedTimerStart();
            return;
        }
        BCM_ASSERT_NOT_HAS_SPINLOCK_V(&brcm_ledlock);
        if (isDataTrafficForwarded != NULL) {
            ret = isDataTrafficForwarded();
        }
        if (isEthDataTrafficForwarded != NULL) {
#ifdef ZYPRJ_VMG8924_B10A
            //When ethWan linkup, no need to software control internet LED.
            //The Internet LED will blink by hardware signal!
            internetLedTimerStart();
            return;
#endif
            ret1 = isEthDataTrafficForwarded();
        }

        spin_lock_irqsave(&brcm_ledlock, flags);
        if ((ret != 0) || (ret1 != 0))
        {
            ledToggle(&gLedCtrl[kLedInternetData]);
        }
        else
        {
            setLed(&gLedCtrl[kLedInternetData], kLedOn, kLedGreen);
        }
        internetLedTimerStart();
        spin_unlock_irqrestore(&brcm_ledlock, flags);
    }
}
#endif


/** Start the LED timer
 *
 * Must be called with brcm_ledlock held
 */
static void ledTimerStart(void)
{
#if defined(DEBUG_LED)
    printk("led: add_timer\n");
#endif

    BCM_ASSERT_HAS_SPINLOCK_C(&brcm_ledlock);

    init_timer(&gLedTimer);
    gLedTimer.function = (void*)ledTimerExpire;
    gLedTimer.expires = jiffies + msecs_to_jiffies(125);        // timer expires in ~125ms
    add_timer (&gLedTimer);
} 


// led timer expire kicks in about ~100ms and perform the led operation according to the ledState and
// restart the timer according to ledState
static void ledTimerExpire(void)
{
    int i;
    PLED_CTRL pCurLed;
    unsigned long flags;
#if defined(ZYXEL_FW_UPGRADE_POWER_LED_BLINK) //fw upgrade led blinking when power management mode on
	int tmpgLedPowerMgmt=0;
#endif

#if 0//1 //power management
	if(gLedPowerMgmt)
	{
#endif
    BCM_ASSERT_NOT_HAS_SPINLOCK_V(&brcm_ledlock);

    for (i = 0, pCurLed = gLedCtrl; i < kLedEnd; i++, pCurLed++)
    {
#if defined(DEBUG_LED)
        printk("led[%d]: Mask=0x%04x, State = %d, blcd=%d\n", i, pCurLed->ledGreenGpio, pCurLed->ledState, pCurLed->blinkCountDown);
#endif
#if defined(ZYXEL_FW_UPGRADE_POWER_LED_BLINK) //fw upgrade led blinking when power management mode on
		if( (i==kLedFwUpgrade)||(i==kLedPowerOn)||(i==kLedPostFail) ){
			tmpgLedPowerMgmt = gLedPowerMgmt;
			gLedPowerMgmt = 1;
		}
#endif
        spin_lock_irqsave(&brcm_ledlock, flags);        // LEDs can be changed from ISR
        switch (pCurLed->ledState)
        {
        case kLedStateOn:
        case kLedStateOff:
        case kLedStateFail:
            pCurLed->blinkCountDown = 0;            // reset the blink count down
            spin_unlock_irqrestore(&brcm_ledlock, flags);
            break;

        case kLedStateSlowBlinkContinues:
            if (pCurLed->blinkCountDown-- == 0)
            {
                pCurLed->blinkCountDown = kSlowBlinkCount;
                ledToggle(pCurLed);
            }
            gTimerOnRequests++;
            spin_unlock_irqrestore(&brcm_ledlock, flags);
            break;

        case kLedStateFastBlinkContinues:
            if (pCurLed->blinkCountDown-- == 0)
            {
                pCurLed->blinkCountDown = kFastBlinkCount;
                ledToggle(pCurLed);
            }
            gTimerOnRequests++;
            spin_unlock_irqrestore(&brcm_ledlock, flags);
            break;

        case kLedStateUserWpsInProgress:          /* 200ms on, 100ms off */
            if (pCurLed->blinkCountDown-- == 0)
            {
                pCurLed->blinkCountDown = (((gLedRunningCounter++)&1)? kFastBlinkCount: kSlowBlinkCount);
                ledToggle(pCurLed);
            }
            gTimerOnRequests++;
            spin_unlock_irqrestore(&brcm_ledlock, flags);
            break;             

        case kLedStateUserWpsError:               /* 100ms on, 100ms off */
            if (pCurLed->blinkCountDown-- == 0)
            {
                pCurLed->blinkCountDown = kFastBlinkCount;
                ledToggle(pCurLed);
            }
            gTimerOnRequests++;
            spin_unlock_irqrestore(&brcm_ledlock, flags);
            break;        

        case kLedStateUserWpsSessionOverLap:      /* 100ms on, 100ms off, 5 times, off for 500ms */        
            if (pCurLed->blinkCountDown-- == 0)
            {
                if(((++gLedRunningCounter)%10) == 0) {
                    pCurLed->blinkCountDown = 4;
                    setLed(pCurLed, kLedOff, kLedGreen);
                    pCurLed->ledState = kLedStateUserWpsSessionOverLap;
                }
                else
                {
                    pCurLed->blinkCountDown = kFastBlinkCount;
                    ledToggle(pCurLed);
                }
            }
            gTimerOnRequests++;
            spin_unlock_irqrestore(&brcm_ledlock, flags);
            break;        

        default:
            spin_unlock_irqrestore(&brcm_ledlock, flags);
            printk("Invalid state = %d\n", pCurLed->ledState);
        }
#if defined(ZYXEL_FW_UPGRADE_POWER_LED_BLINK) //fw upgrade led blinking when power management mode on
		if( (i==kLedFwUpgrade)||(i==kLedPowerOn)||(i==kLedPostFail) )
			gLedPowerMgmt = tmpgLedPowerMgmt;
#endif
    }

    // Restart the timer if any of our previous LED operations required
    // us to restart the timer, or if any other threads requested a timer
    // restart.  Note that throughout this function, gTimerOn == TRUE, so
    // any other thread which want to restart the timer would only
    // increment the gTimerOnRequests and not call ledTimerStart.
    spin_lock_irqsave(&brcm_ledlock, flags);
    if (gTimerOnRequests > 0)
    {
        ledTimerStart();
        gTimerOnRequests = 0;
    }
    else
    {
        gTimerOn = FALSE;
    }
    spin_unlock_irqrestore(&brcm_ledlock, flags);
#if 0//1 //power management
	}
#endif	

}

static void boardLedStateChange(BOARD_LED_NAME ledName, BOARD_LED_STATE ledState)
{
	PLED_CTRL pLed;
	pLed = &gLedCtrl[ledName];
	
	switch (ledState)
	{
		case kLedStateOn:
		    // Enable SAR to control INET LED
#if defined(CONFIG_BCM96328) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM963268) || defined(CONFIG_BCM96318) || defined(CONFIG_BCM96838)
		    if (ledName == kLedWanData)
		        LED->ledHWDis &= ~GPIO_NUM_TO_MASK(pLed->ledGreenGpio);
#endif
#ifdef ZYXEL_SW_INTERNET_LED_BLINK //__ZyXEL__, David, support SW Internet LED blinking.
		    if (ledName == kLedInternetData) {
		        ledInternetOn = TRUE;
		    }
#endif
#if defined(ZYXEL_WPS_SPOTLIGHT) && defined(ZYXEL_WPS_SPOTLIGHT_2GPIO)
			//For WPS Spotlight also turn on Red and Green to transform to orange 
			if (ledName == kLedWpsSpotlight) { 
		        setLed (pLed, kLedOn, kLedRed);
		    }
#endif
		    setLed (pLed, kLedOn, kLedGreen);
		    break;

		case kLedStateOff:
#ifdef ZYXEL_SW_INTERNET_LED_BLINK //__ZyXEL__, David, support SW Internet LED blinking.
		    if (ledName == kLedInternetData) {
		        ledInternetOn = FALSE;
		    }
#endif
		    break;

		case kLedStateFail:
		    setLed (pLed, kLedOn, kLedRed);
		    break;

		case kLedStateSlowBlinkContinues:
		    pLed->blinkCountDown = kSlowBlinkCount;
		    gTimerOnRequests++;
		    break;

		case kLedStateFastBlinkContinues:
		    pLed->blinkCountDown = kFastBlinkCount;
		    gTimerOnRequests++;
		    break;

		case kLedStateUserWpsInProgress:          /* 200ms on, 100ms off */
		    pLed->blinkCountDown = kFastBlinkCount;
		    gLedRunningCounter = 0;
		    gTimerOnRequests++;
		    break;             

		case kLedStateUserWpsError:               /* 100ms on, 100ms off */
		    pLed->blinkCountDown = kFastBlinkCount;
		    gLedRunningCounter = 0;
		    gTimerOnRequests++;
		    break;        

		case kLedStateUserWpsSessionOverLap:      /* 100ms on, 100ms off, 5 times, off for 500ms */
		    pLed->blinkCountDown = kFastBlinkCount;
		    gTimerOnRequests++;
		    break;        

		default:
		    printk("Invalid led state\n");
	}
}

#if (ZYXEL==1) && (defined(CONFIG_BCM963138) || defined(CONFIG_BCM963148) || defined(CONFIG_BCM963381))
void boardLedCtrl(BOARD_LED_NAME ledName, BOARD_LED_STATE ledState);
#endif
#if (ZYXEL==1)
static void backup_hardware_led_config(void);
#endif

void __init boardLedInit(void)
{
    PBP_LED_INFO pInfo;
    unsigned short i;
    short gpio;

    spin_lock_init(&brcm_ledlock);

#if !(defined(CONFIG_BCM96328) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM963268) || defined(CONFIG_BCM96318) || defined(CONFIG_BCM96838) || defined(CONFIG_BCM963138) || defined(CONFIG_BCM960333) || defined(CONFIG_BCM963381) || defined(CONFIG_BCM963148) || defined(CONFIG_BCM96848))
    /* Set blink rate for hardware LEDs. */
    GPIO->LEDCtrl &= ~LED_INTERVAL_SET_MASK;
    GPIO->LEDCtrl |= LED_INTERVAL_SET_80MS;
#endif
#if (ZYXEL==1)	// for power manager
	backup_hardware_led_config();
#endif

    gLedCtrl = (PLED_CTRL) kmalloc((kLedEnd * sizeof(LED_CTRL)), GFP_KERNEL);

    if( gLedCtrl == NULL ) {
        printk( "LED memory allocation error.\n" );
        return;
    }

    /* Initialize LED control */
    for (i = 0; i < kLedEnd; i++) {
        gLedCtrl[i].ledGreenGpio = -1;
        gLedCtrl[i].ledRedGpio = -1;
        gLedCtrl[i].ledState = kLedStateOff;
        gLedCtrl[i].blinkCountDown = 0;            // reset the blink count down
    }

    for( pInfo = bpLedInfo; pInfo->ledName != kLedEnd; pInfo++ ) {
        if( pInfo->bpFunc && (*pInfo->bpFunc) (&gpio) == BP_SUCCESS )
        {
            gLedCtrl[pInfo->ledName].ledGreenGpio = gpio;
        }
        if( pInfo->bpFuncFail && (*pInfo->bpFuncFail)(&gpio)==BP_SUCCESS )
        {
            gLedCtrl[pInfo->ledName].ledRedGpio = gpio;
        }
#if defined(ZYXEL) && (ZYXEL==1)
        //20131017, TimLiu let power on LED keep blinking when booting.
        if(pInfo->ledName != kLedPowerOn) {
#endif
        setLed(&gLedCtrl[pInfo->ledName], kLedOff, kLedGreen);
        setLed(&gLedCtrl[pInfo->ledName], kLedOff, kLedRed);
#if defined(ZYXEL) && (ZYXEL==1)
        }
#if defined(CONFIG_BCM963138) || defined(CONFIG_BCM963148) || defined(CONFIG_BCM963381)
		else {
			//20140827, TimLiu, BCM963138 cannot flash LED by hardware,
			//			We need to control the led after CFE booting to kernel.
			boardLedCtrl(pInfo->ledName, kLedStateSlowBlinkContinues);
		}
#endif
#endif
    }
#if defined(ZYXEL) && (ZYXEL==1)
	//20131017, TimLiu in order to match the Product SPEC
	//			keep blink the power led when boot up.
    /*20130317, TimLiu keep the power on LED still on.*/
    //setLed(&gLedCtrl[kLedPowerOn], kLedOn, kLedGreen);
#endif

#ifdef ZYXEL_SW_INTERNET_LED_BLINK //__ZyXEL__, David, support SW Internet LED blinking.
    isDataTrafficForwarded = NULL;
    isEthDataTrafficForwarded = NULL;
    internetLedTimerStart();
#endif

#if defined(DEBUG_LED)
    for (i=0; i < kLedEnd; i++)
        printk("initLed: led[%d]: Gpio=0x%04x, FailGpio=0x%04x\n", i, gLedCtrl[i].ledGreenGpio, gLedCtrl[i].ledRedGpio);
#endif
}

static unsigned long target_board_ledsrc = 0x0;

static void backup_hardware_led_config(void)
{
#if defined(CONFIG_BCM96328) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM963268) || defined(CONFIG_BCM96318) || defined(CONFIG_BCM96838)
	target_board_ledsrc = LED->ledHWDis;
#elif defined(CONFIG_BCM963138) || defined(CONFIG_BCM963148) || defined(CONFIG_BCM963381)
	target_board_ledsrc = LED->hWLedEn;
#else
	#error todo for new platform
#endif
	printk("%s:target_board_ledsrc=%lu\n", __FUNCTION__, target_board_ledsrc);
}

static void enable_hardware_led(int enable)
{
	if(enable){
#if defined(CONFIG_BCM96328) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM963268) || defined(CONFIG_BCM96318) || defined(CONFIG_BCM96838)
		LED->ledHWDis &= target_board_ledsrc;
#elif defined(CONFIG_BCM963138) || defined(CONFIG_BCM963148) || defined(CONFIG_BCM963381)
		LED->hWLedEn |= target_board_ledsrc;
#else
	#error todo for new platform
#endif

	}else{
#if defined(CONFIG_BCM96328) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM963268) || defined(CONFIG_BCM96318) || defined(CONFIG_BCM96838)
		LED->ledHWDis |= 0xFFFFFFFF;
#elif defined(CONFIG_BCM963138) || defined(CONFIG_BCM963148) || defined(CONFIG_BCM963381)
		LED->hWLedEn &= 0x00000000;
#else
	#error todo for new platform
#endif
	}
}

// led ctrl.  Maps the ledName to the corresponding ledInfoPtr and perform the led operation
void boardLedCtrl(BOARD_LED_NAME ledName, BOARD_LED_STATE ledState)
{
	unsigned short i;
    unsigned long flags;
    PLED_CTRL pLed;
#if defined(ZYXEL_FW_UPGRADE_POWER_LED_BLINK) //fw upgrade led blinking when power management mode on
	int tmpgLedPowerMgmt=0;
#endif

    BCM_ASSERT_NOT_HAS_SPINLOCK_V(&brcm_ledlock);

    spin_lock_irqsave(&brcm_ledlock, flags);     // LED can be changed from ISR
#if 1 //power management
	if( ( ((int) ledName == kLedAll) || ((int) ledName == kLedAllExceptPower) ) && (ledState == kLedStateOn) )
	{
		gLedPowerMgmt = 1;
					
		for (i = 0; i < kLedEnd; i++) 
		{
			if( (i != kLedAll) && (i != kLedAllExceptPower) ){
				boardLedStateChange(i, gLedCtrl[i].ledState);
				//printk("boardLedCtrl: led[%d]: Gpio=0x%04x, FailGpio=0x%04x, State = %d, blcd=%d\n", i, gLedCtrl[i].ledGreenGpio, gLedCtrl[i].ledRedGpio, gLedCtrl[i].ledState, gLedCtrl[i].blinkCountDown);
			}
		}
		enable_hardware_led(1);
	}
	else if( ( ((int) ledName == kLedAll) || ((int) ledName == kLedAllExceptPower) ) && (ledState == kLedStateOff) )
	{
		gLedPowerMgmt = 1;	//make sure to enable led control to disable LED
					
		for (i = 0; i < kLedEnd; i++) 
		{
			if( (i != kLedAll) && (i != kLedAllExceptPower) &&
			    ( ((int) ledName == kLedAllExceptPower) &&
#if defined(ZYXEL_FW_UPGRADE_POWER_LED_BLINK)
			      (i != kLedFwUpgrade) &&
#endif
			      (i != kLedPowerOn) &&
			      (i != kLedPostFail)
			    ) )
			{
				setLed (&gLedCtrl[i], kLedOff, kLedGreen);
				setLed (&gLedCtrl[i], kLedOff, kLedRed);
				//printk("boardLedCtrl: led[%d]: Gpio=0x%04x, FailGpio=0x%04x, State = %d, blcd=%d\n", i, gLedCtrl[i].ledGreenGpio, gLedCtrl[i].ledRedGpio, gLedCtrl[i].ledState, gLedCtrl[i].blinkCountDown);
			}
		}
		gLedPowerMgmt = 0;
		enable_hardware_led(0);
	}	
	else 
#endif
    if( (int) ledName < kLedEnd ) {

        pLed = &gLedCtrl[ledName];

        // If the state is kLedStateFail and there is not a failure LED defined
        // in the board parameters, change the state to kLedStateSlowBlinkContinues.
        if( ledState == kLedStateFail && pLed->ledRedGpio == -1 )
            ledState = kLedStateSlowBlinkContinues;

        // Save current LED state
        pLed->ledState = ledState;
#if defined(ZYXEL_FW_UPGRADE_POWER_LED_BLINK) //fw upgrade led blinking when power management mode on
	if( (ledName==kLedFwUpgrade)||(ledName==kLedPowerOn)||(ledName==kLedPostFail) ){
		tmpgLedPowerMgmt = gLedPowerMgmt;
		gLedPowerMgmt = 1;
	}
#endif
        // Start from LED Off and turn it on later as needed
        setLed (pLed, kLedOff, kLedGreen);
        setLed (pLed, kLedOff, kLedRed);

        // Disable HW control for WAN Data LED. 
        // It will be enabled only if LED state is On
#if defined(CONFIG_BCM96328) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM963268) || defined(CONFIG_BCM96318) || defined(CONFIG_BCM96838)
        if (ledName == kLedWanData)
            LED->ledHWDis |= GPIO_NUM_TO_MASK(pLed->ledGreenGpio);
#endif
#if 0 //power management
		if(gLedPowerMgmt)
#endif
			boardLedStateChange(ledName, ledState);
#if defined(ZYXEL_FW_UPGRADE_POWER_LED_BLINK) //fw upgrade led blinking when power management mode on
		if( (ledName==kLedFwUpgrade)||(ledName==kLedPowerOn)||(ledName==kLedPostFail) )
			gLedPowerMgmt = tmpgLedPowerMgmt;
#endif
    }

    // If gTimerOn is false, that means ledTimerExpire has already finished
    // running and has not restarted the timer.  Then we can start it here.
    // Otherwise, we only leave the gTimerOnRequests > 0 which will be
    // noticed at the end of the ledTimerExpire function.
    if (!gTimerOn && gTimerOnRequests > 0)
    {
        ledTimerStart();
        gTimerOn = TRUE;
        gTimerOnRequests = 0;
    }
    spin_unlock_irqrestore(&brcm_ledlock, flags);
}

#if defined(CONFIG_NEW_LEDS)
#include <linux/leds.h>
#define LED_AUTONAME_MAX_SIZE    25
#define MAX_LEDS 64


struct sysfsled {
    struct led_classdev cdev;
    int bcm_led_pin; /* This also means pins connected on a shift
    register controlled by the LED controller (see BP_GPIO_SERIAL).*/
    char name[LED_AUTONAME_MAX_SIZE];
};

struct sysfsled_ctx {
    int    num_leds;
    struct sysfsled leds[MAX_LEDS];
};

static struct sysfsled_ctx bcmctx = {0, {}};

static void brightness_set(struct led_classdev *cdev, enum led_brightness value)
{
    struct sysfsled *led = container_of(cdev,
                    struct sysfsled, cdev);

    /* On Broadcom chips, LEDs connected to any kind of pin can be
     controlled via setLed. setLed only needs the pin ("GPIO") from
     the LED_CTRL argument. Fake a LED_CTRL to pass the pin to setLed.
     The color obviously doesn't matter here. */
    LED_CTRL ledctrl = {.ledGreenGpio=led->bcm_led_pin};
    setLed(&ledctrl, (value == LED_FULL)?kLedOn:kLedOff, kLedGreen);
}

static void __exit bcmsysfsleds_unregister(void *opaque)
{
    struct sysfsled_ctx *ctx = opaque;
    int i;

    for (i = 0; i < MAX_LEDS; i++) {
        if (ctx->leds[i].cdev.brightness_set) {
            led_classdev_unregister(&ctx->leds[i].cdev);
            ctx->leds[i].cdev.brightness_set = NULL;
        }
    }
}

static int __init bcmsysfsleds_register(int bcm_led_pin, char *led_name)
{
    int i;

    if (bcmctx.num_leds >= MAX_LEDS) {
        printk(KERN_ERR "Too many BCM LEDs registered.\n");
        return -ENOMEM;
    }

    if(led_name != NULL) {
        i = bcmctx.num_leds;
        snprintf(bcmctx.leds[i].name, LED_AUTONAME_MAX_SIZE-1,
            "%s_%d%c", led_name, bcm_led_pin&BP_GPIO_NUM_MASK, (bcm_led_pin&BP_GPIO_SERIAL)?'s':'\0');
        bcmctx.leds[i].cdev.name = bcmctx.leds[i].name;
        bcmctx.leds[i].cdev.brightness  = LED_OFF;
        bcmctx.leds[i].cdev.brightness_set = brightness_set;
        bcmctx.leds[i].bcm_led_pin = bcm_led_pin;

        if (led_classdev_register(NULL, &bcmctx.leds[i].cdev)) {
            printk(KERN_ERR "BCM LEDs registration failed. %d\n", bcm_led_pin&BP_GPIO_NUM_MASK);
            bcmctx.leds[i].cdev.brightness_set = NULL;
            return -1;
        }
        bcmctx.num_leds++;
    }

    i = bcmctx.num_leds;
    snprintf(bcmctx.leds[i].name, LED_AUTONAME_MAX_SIZE-1,
        "%d%c", bcm_led_pin&BP_GPIO_NUM_MASK, (bcm_led_pin&BP_GPIO_SERIAL)?'s':'\0');
    bcmctx.leds[i].cdev.name = bcmctx.leds[i].name;
    bcmctx.leds[i].cdev.brightness  = LED_OFF;
    bcmctx.leds[i].cdev.brightness_set = brightness_set;
    bcmctx.leds[i].bcm_led_pin = bcm_led_pin;
    if (led_classdev_register(NULL, &bcmctx.leds[i].cdev)) {
        printk(KERN_ERR "BCM LEDs registration failed. %d\n", bcm_led_pin&BP_GPIO_NUM_MASK);
        bcmctx.leds[i].cdev.brightness_set = NULL;
        return -1;
    }

    bcmctx.num_leds++;

    return 0;
}
#endif

int __init bcmsysfsleds_init(void)
{
#if defined(CONFIG_NEW_LEDS)
        unsigned short bcmledgpioid;
	int index=0, token=0, rc;
        char *ledName=NULL;
        for(;;) {
                ledName=NULL;
                rc = BpGetLedName(index, &token,  &bcmledgpioid, &ledName);
                if (rc == BP_MAX_ITEM_EXCEEDED) {
                    break;
                }
#if defined(ZYPRJ_VMG1312) || defined(ZYPRJ_VMG5313B) || defined(ZYPRJ_VMG8924_B10A) || defined(ZYPRJ_VMG3927_B50B) || defined(ZYPRJ_VMG8825_B50K) || defined(ZYPRJ_XMG8825_B50A)
				//printk("====%d, %s\n", rc, (ledName!=NULL)?ledName:"NULL");
                if ( (ledName!=NULL) && !strncmp(ledName, "LedVdsl", 7 ) ) {
					//printk("====No need to setup LedVdsl LED device.\n");
					//VMG1312 using the same LED for ADSL and VDSL.
					//Prevent to create the same LED device(the same gpio).
                    continue;
                }
#endif
                if(rc == BP_SUCCESS) {
                        bcmsysfsleds_register(bcmledgpioid, ledName);
                }
                else {
                        index++;
                        token=0;
                }
        }
#endif
        return 0;
}

void __exit bcmsysfsleds_exit(void)
{
#if defined(CONFIG_NEW_LEDS)
    int i;

    for (i = 0; i < MAX_LEDS; i++) {
        if (bcmctx.leds[i].cdev.brightness_set) {
            led_classdev_unregister(&bcmctx.leds[i].cdev);
            bcmctx.leds[i].cdev.brightness_set = NULL;
        }
    }
#endif
}

module_init(bcmsysfsleds_init);
