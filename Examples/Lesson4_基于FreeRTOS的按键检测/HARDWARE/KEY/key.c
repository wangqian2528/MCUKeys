#include "key.h"
#include "sys.h"
#include "delay.h"
#include "timers.h"
#include "queue.h"

/**************************全局变量定义********************************/
t_keys_fifo keys_fifo;       // 按键缓冲FIFO
t_keys keys[KEY_ID_MAX];     // 按键数组
TimerHandle_t xTimerKeys;    // 定义定时器句柄
xQueueHandle xQueueKeys;     // 定义队列句柄


/******************************实现函数**********************************/

/***********************************************************************
* 函数名称： get_key1_state
* 函数功能： 获取按键的状态
* 输入参数：  无
* 返 回 值：   按键按下返回1 否则返回0
* 函数说明： 按键按下返回PRESS 否则返回LIFT
****************************************************************************/
static uint8_t get_key1_state(void)
{
    uint8_t res  = GPIO_ReadInputDataBit(KEY1_GPIO, KEY1_Pin);  //和平台移植相关
    if(res == 0)
    {
        return PRESS;   //按键按下
    }
    else
    {
        return LIFT;    //按键抬起
    }
}

/***********************************************************************
* 函数名称： key_in_fifo
* 函数功能： 将一个按键状态放入FIFO中
* 输入参数：  具体按键状态
* 返 回  值： 无
* 函数说明：无
****************************************************************************/
void key_in_fifo(e_keys_status key_state)
{
    xQueueSend(xQueueKeys, (void *)&key_state, 0);  //按键状态入队 不等待
//    keys_fifo.fifo_buffer[keys_fifo.write] = key_state;

    if (++keys_fifo.write  >= KEY_FIFO_SIZE)
    {
        keys_fifo.write = 0;
    }
}

/***********************************************************************
* 函数名称： key_out_fifo
* 函数功能： 从按键FIFO中取出一个按键事件
* 输入参数：  无
* 返 回  值：  当前FIFO中缓冲的按键事件
* 函数说明：无
****************************************************************************/
e_keys_status key_out_fifo(void)
{
    e_keys_status ret;

    if (keys_fifo.read == keys_fifo.write)
    {
        return KEY_NONE;
    }
    else
    {
//        ret = keys_fifo.fifo_buffer[keys_fifo.read];
        xQueueReceive(xQueueKeys, &ret, 0);     //从队列中获取一个按键状态
        if (++keys_fifo.read >= KEY_FIFO_SIZE)
        {
            keys_fifo.read = 0;
        }
        return ret;
    }
}


/***********************************************************************
* 函数名称： get_key_state
* 函数功能： 读取按键状态
* 输入参数：  按键ID
* 返 回  值：  按键状态 [ PRESS : 按键按下 LIFT : 按键抬起]
* 函数说明：   无
****************************************************************************/
uint8_t get_key_state(e_keys_id key_id)
{
    return keys[key_id].state;
}

/***********************************************************************
* 函数名称： set_keys_param
* 函数功能： 设置按键结构体中的参数
* 输入参数：  key_id[IN] : 按键ID
                     long_time[IN] :  长按时间*ms  0表示不支持长按
                     repeat_speed[IN] : 连发速度 ms 0表示不支持连发
* 返 回  值：  无
* 函数说明：   无
****************************************************************************/
void set_keys_param(e_keys_id key_id, uint16_t long_time, uint8_t  repeat_speed)
{
    keys[key_id].long_time = long_time;			/* 长按时间 0 表示不检测长按键事件 */
    keys[key_id].repeat_speed = repeat_speed;			/* 按键连发的速度，0表示不支持连发 */
    keys[key_id].repeat_count = 0;						/* 连发计数 器 清零 */
}


/***********************************************************************
* 函数名称： clear_keys_fifo
* 函数功能： 清除按键缓冲区
* 输入参数：  无
* 返 回  值：  无
* 函数说明：   无
****************************************************************************/
void clear_keys_fifo(void)
{
    keys_fifo.read = keys_fifo.write;
}

/***********************************************************************
* 函数名称： keys_hardware_init
* 函数功能： 初始化按键对应的IO口
* 输入参数：  无
* 返 回  值：  无
* 函数说明：   无
****************************************************************************/
static void keys_hardware_init(void)
{

    GPIO_InitTypeDef  GPIO_InitStructure;

    RCC_AHB1PeriphClockCmd(KEY1_RCC, ENABLE);//使能GPIO时钟

    GPIO_InitStructure.GPIO_Pin = KEY1_Pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;  //输入
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;//100MHz
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;//上拉
    GPIO_Init(KEY1_GPIO, &GPIO_InitStructure);//初始化GPIO

}

/***********************************************************************
* 函数名称： keys_value_init
* 函数功能： 初始化按键结构体中的相关变量
* 输入参数：  无
* 返 回  值：  无
* 函数说明：   无
****************************************************************************/
static void keys_value_init(void)
{
    uint8_t i;

    /* 对按键FIFO读写指针清零 */
    keys_fifo.read = 0;
    keys_fifo.write = 0;
    /* 给每个按键结构体成员变量赋一组缺省值 */
    for (i = 0; i < KEY_ID_MAX; i++)
    {
        keys[i].long_time = KEY_LONG_TIME;			/* 长按时间 0 表示不检测长按键事件 */
        keys[i].count = KEY_FILTER_TIME ;		/* 计数器设置为滤波时间 */
        keys[i].state = LIFT;							            /* 按键缺省状态，0为未按下 */
        keys[i].repeat_speed = KEY_REPEAT_TIME;						/* 按键连发的速度，0表示不支持连发 */
        keys[i].repeat_count = 0;						/* 连发计数器 */
        keys[i].double_count = 0;                      /* 双击计数器*/

        keys[i].short_key_down = NULL;          /* 按键按下回调函数*/
        keys[i].skd_arg = NULL;                     /* 按键按下回调函数参数*/
        keys[i].short_key_up = NULL;            /* 按键抬起回调函数*/
        keys[i].sku_arg = NULL;                     /* 按键抬起回调函数参数*/
        keys[i].long_key_down = NULL;         /* 按键长按回调函数*/
        keys[i].lkd_arg = NULL;                      /* 按键长按回调函数参数*/
        keys[i].double_key_down = NULL;
        keys[i].dkd_arg = NULL;

        keys[i].get_key_status = get_key1_state;    /* 获取按键状态函数绑定 这个和平台有关*/

        keys[i].report_flag = KEY_REPORT_DOWN | KEY_REPORT_UP | KEY_REPORT_LONG | KEY_REPORT_DOUBLE;

    }

}

/***********************************************************************
* 函数名称： detect_key
* 函数功能： 检测一个按键的状态并将状态值放入FIFO中
* 输入参数：  key_id[IN] : 按键ID
* 返 回  值：  无
* 函数说明：  应该放在一个1ms的周期函数中
****************************************************************************/
static void detect_key(e_keys_id key_id)
{
    t_keys *p_key;

    p_key = &keys[key_id];  //获取按键事件结构体
    if (p_key->get_key_status() == PRESS)  //如果按键已经被按下
    {
        if (p_key->count < KEY_FILTER_TIME)
        {
            p_key->count = KEY_FILTER_TIME;
        }
        else if(p_key->count < 2 * KEY_FILTER_TIME)
        {
            p_key->count+=KEY_TICKS;  //滤波 实际滤波时间为(KEY_FILTER_TIME+1)个周期
        }
        else
        {
            if (p_key->state == LIFT)
            {
                p_key->state = PRESS;

                if(p_key->report_flag&KEY_REPORT_DOWN)  //如果定义了按键按下上报功能
                {
                    /* 发送按钮按下的消息 */
                    key_in_fifo((e_keys_status)(KEY_STATUS * key_id + 1));   //存入按键按下事件
                }
                if(p_key->short_key_down)   //如果注册了回调函数 则执行
                {
                    p_key->short_key_down(p_key->skd_arg);
                }
            }

            if (p_key->long_time > 0)     //如果需要长按检测
            {
                if (p_key->long_count < p_key->long_time)
                {
                    /* 发送按钮持续按下的消息 */
                    if ((p_key->long_count+=KEY_TICKS) == p_key->long_time)
                    {
                        if(p_key->report_flag&KEY_REPORT_LONG)
                        {
                            /* 键值放入按键FIFO */
                            key_in_fifo((e_keys_status)(KEY_STATUS * key_id + 3));  //存入长按事件
                        }
                        if(p_key->long_key_down)        //如果定义了回调函数
                        {
                            p_key->long_key_down(p_key->lkd_arg);   //执行回调函数
                        }
                    }
                }
                else
                {
                    if (p_key->repeat_speed > 0)      //如果定义了连发事件
                    {
                        if ((p_key->repeat_count  += KEY_TICKS) >= p_key->repeat_speed)
                        {
                            p_key->repeat_count = 0;
                            /*长按按键后，每隔repeat_speed发送1个按键 */
                            key_in_fifo((e_keys_status)(KEY_STATUS * key_id + 1));
                        }
                    }
                }
            }
        }
    }
    else    //按键松开
    {
        if(p_key->count > KEY_FILTER_TIME)
        {
            p_key->count = KEY_FILTER_TIME;
        }
        else if(p_key->count != 0)
        {
            //按键松开滤波
            p_key->count-=KEY_TICKS;
        }
        else
        {
            //滤波结束
            if (p_key->state == PRESS)
            {
                p_key->state = LIFT;  //按键抬起

                if(p_key->report_flag&KEY_REPORT_UP)
                {
                    /* 发送按钮弹起的消息 */
                    key_in_fifo((e_keys_status)(KEY_STATUS * key_id + 2));
                }
                if(p_key->short_key_up) //如果定义了回调函数
                {
                    p_key->short_key_up(p_key->sku_arg);
                }
                //如果满足双击要求
                if((p_key->double_count > KEY_DOUBLE_MIN)&&(p_key->double_count < KEY_DOUBLE_MAX))
                {
                    p_key->double_count = 0;
                    if(p_key->report_flag&KEY_REPORT_DOUBLE)    //如果定义的上报双击标志
                    {
                        key_in_fifo((e_keys_status)(KEY_STATUS * key_id + 4));  //上报双击事件
                    }
                    if(p_key->double_key_down)  //如果定义了回调函数
                    {
                        p_key->double_key_down(p_key->dkd_arg); //执行回调函数
                    }
                }
                else
                {
                    //不满足双击要求 清零计数器
                    p_key->double_count = 0;
                }

            }
            p_key->double_count += KEY_TICKS ;  //双击事件计数

        }

        p_key->long_count = 0;  //长按计数清零
        p_key->repeat_count = 0;  //重复发送计数清零
    }
}

/***********************************************************************
* 函数名称： key_scan
* 函数功能： 按键扫描
* 输入参数： 无
* 返 回  值：  无
* 函数说明：  应该放在一个1ms的周期函数中
****************************************************************************/
void key_scan(void)
{
    uint8_t i;

    for (i = 0; i < KEY_ID_MAX; i++)
    {
        detect_key(i);
    }
}

/***********************************************************************
* 函数名称： vMCUKeysTimerCallback
* 函数功能： rtos定时器回调函数
* 输入参数：  xTimer[IN] : 当前定时器的句柄
* 返 回  值： 无
* 函数说明： 尽快处理不可阻塞
****************************************************************************/
void vMCUKeysTimerCallback( TimerHandle_t xTimer )
{
    key_scan();
}


/***********************************************************************
* 函数名称： keys_init
* 函数功能： 按键模块初始化
* 输入参数：  无
* 返 回  值： 创建成功返回0 否则返回一个负数
* 函数说明： 系统初始化时应当调用此函数
****************************************************************************/
int keys_init(void)
{
    keys_value_init();		        /* 初始化按键变量 */
    keys_hardware_init();		/* 初始化按键硬件 */
    
    //创建队列
    xQueueKeys = xQueueCreate(KEY_FIFO_SIZE, sizeof(e_keys_status));
    if(xQueueKeys == NULL)
    {
        //创建失败
        return -1;
    }
    // 申请定时器， 配置
    xTimerKeys = xTimerCreate
                   /*调试用， 系统不用*/
                   ("MCUKeys",
                   /*定时溢出周期， 单位是任务节拍数*/
                   KEY_TICKS,   
                   /*是否自动重载， 此处设置周期性执行*/
                   pdTRUE,
                   /*记录定时器ID号， 初始化零, 用户自己设置*/
                  ( void * ) 0,
                   /*回调函数*/
                  vMCUKeysTimerCallback);

     if( xTimerKeys != NULL ) {
        // 启动定时器， 0 表示不等待
        xTimerStart( xTimerKeys, 0 );
    }
     else
     {
        return -1;
     }
     
    return 0;
}



