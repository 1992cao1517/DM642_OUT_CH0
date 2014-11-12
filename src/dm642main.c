/********************************************************************/
/* Video tracking system via Three-frame difference method          */
/* Author: Ming Wen                                                 */
/********************************************************************/

/********************************************************************/
/*  Copyright 2006 by Vision Magic Ltd.								*/
/*  Restricted rights to use, duplicate or disclose this code are	*/
/*  granted through contract.									    */
/*  															    */
/********************************************************************/

#include <stdlib.h>
#include <csl.h>
#include <csl_cache.h>
#include <csl_emifa.h>
#include <csl_i2c.h>
#include <csl_gpio.h>
#include <csl_irq.h>
#include <csl_chip.h>
#include <csl_dat.h>
#include <csl_timer.h>

#include "iic.h"
#include "vportcap.h"
#include "vportdis.h"
#include "sa7121h.h"
#include "TVP51xx.h"
#include "vmd642.h"
#include "vmd642_uart.h"
#include "frame_operation.h"
#include "ctrl_operation.h"
#include "g_config.h"

interrupt void MovingCtrl(void);
void do_analysis();

/********************************************************************/

extern far void vectors();
extern volatile Uint32 capNewFrame;
extern volatile Uint32 disNewFrame;

/********************************************************************/

/*�˳���ɽ���Ƶ�ɼ���1 CH1(�ڶ���ͨ��)�����ݾ���Video Port0�ͳ�*/
void main()
{
	Uint8 addrI2C;
	int i;

	/* The next position to store a frame */
	Uint8 nextFrame;
    Uint8 nextDiff;

	/* Current Buf addr */
	Uint32 YBuf, CbBuf, CrBuf;
    /* Buf addr been subtract */
    Uint32 YSubBuf, CbSubBuf, CrSubBuf;
    /* Buf addr to be add */
    Uint32 YAddBuf, CbAddBuf, CrAddBuf;
    /* Buf addr to store results */
    Uint32 YAnsBuf, CbAnsBuf, CrAnsBuf;

/*-------------------------------------------------------*/
/* perform all initializations                           */
/*-------------------------------------------------------*/
	/*Initialise CSL����ʼ��CSL��*/
	CSL_init();
	CHIP_config(&VMD642percfg);
/*----------------------------------------------------------*/
	/*EMIFA�ĳ�ʼ������CE0��ΪSDRAM�ռ䣬CE1��Ϊ�첽�ռ�
	 ע��DM642֧�ֵ���EMIFA������EMIF*/
	EMIFA_config(&VMDEMIFConfig);
    
/*----------------------------------------------------------*/
    /*TIMER��ʼ��������TIMER0*/
    hTimer = TIMER_open(TIMER_DEV0, 0);
    TimerEventId = TIMER_getEventId(hTimer);
    TIMER_config(hTimer, &timerConfig);
    
/*----------------------------------------------------------*/
	/*�ж�������ĳ�ʼ��*/
	//Point to the IRQ vector table
    IRQ_setVecs(vectors);
    IRQ_nmiEnable();
    IRQ_globalEnable();
    IRQ_map(IRQ_EVT_VINT1, 11);
    IRQ_map(IRQ_EVT_VINT0, 12);
    IRQ_map(TimerEventId, 14);
    IRQ_reset(IRQ_EVT_VINT1);
    IRQ_reset(IRQ_EVT_VINT0);
    IRQ_reset(TimerEventId);
    IRQ_enable(TimerEventId);   /* Enable timer interrupt */
    
    /*��һ�����ݿ���������ͨ·*/
    DAT_open(DAT_CHAANY, DAT_PRI_LOW, DAT_OPEN_2D);

/*----------------------------------------------------------*/
/*��RS485����ź�ͨ·*/
    /* Open UART */
    g_uartHandleA = VMD642_UART_open(VMD642_UARTB,
    									UARTHW_VMD642_BAUD_9600,
    									&g_uartConfig);
    
/*----------------------------------------------------------*/
	/*����IIC�ĳ�ʼ��*/
	hVMD642I2C = I2C_open(I2C_PORT0, I2C_OPEN_RESET);
	I2C_config(hVMD642I2C, &VMD642IIC_Config);

/*----------------------------------------------------------*/
	/*����TVP5150pbs�ĳ�ʼ��*/
	/*ѡ��TVP5150��������Ƶ�ɼ���һͨ·ch0, ��U12*/

	/*��GPIO0����ΪGPINTʹ��*/
	GPIO_RSET(GPGC, 0x0);

	/*��GPIO0��Ϊ���*/
	GPIO_RSET(GPDIR, 0x1);

	/*GPIO0���Ϊ�ߣ�ѡ��IIC0����*/
	GPIO_RSET(GPVAL, 0x0);

	/*ѡ��ڶ���5150��U12*/
	addrI2C = 0xBA >> 1;
    _IIC_write(hVMD642I2C, addrI2C, 0x00, input_sel);
    _IIC_write(hVMD642I2C, addrI2C, 0x03, misc_ctrl);
    _IIC_write(hVMD642I2C, addrI2C, 0x0D, output_format);
    _IIC_write(hVMD642I2C, addrI2C, 0x0F, pin_cfg);
    _IIC_write(hVMD642I2C, addrI2C, 0x1B, chro_ctrl_2);
    /*��ȡ��Ƶ��ʽ*/
    _IIC_read(hVMD642I2C, addrI2C, 0x8c, &vFromat);
    vFromat = vFromat & 0xff;
	switch (vFromat)
	{
		case TVP51XX_NTSCM:
		case TVP51XX_NTSC443:
			NTSCorPAL = 1;/*ϵͳΪNTSC��ģʽ*/
			break;
		case TVP51XX_PALBGHIN:
		case TVP51XX_PALM:
		case TVP5150_FORCED_PAL:
			NTSCorPAL = 0;/*ϵͳΪPAL��ģʽ*/
			break;
		default:
			NTSCorPAL = 2;/*ϵͳΪ��֧�ֵ�ģʽ*/
			break;
	}
	if(NTSCorPAL == 2)
	{
		/*ϵͳ��֧�ֵ�ģʽ����������*/
		for(;;)
		{}
	}

/*----------------------------------------------------------*/
	/*����SAA7121H�ĳ�ʼ��*/

	/*GPIO0���Ϊ�ͣ�ѡ��IIC0���ߣ�����ͼ�����*/
	GPIO_RSET(GPVAL, 0x0);
	/*ѡ���һ��5150����U10*/
	addrI2C = 0xB8 >> 1;
	/*��Video Port0����Ƶ����ڵ����ݿ���Ϊ����״̬��
	  ʹ��SCLK������27����Ϊ����*/
	_IIC_write(hVMD642I2C, addrI2C, 0x03, 0x1);

	/*����SAA7121H*/
	/*GPIO0���Ϊ�ͣ�ѡ��IIC1���ߣ�����ͼ�����*/
	GPIO_RSET(GPVAL, 0x1);

	/*��ʼ��Video Port0*/
	/*��Video Port0��Ϊencoder���*/
	portNumber = 0;
	vpHchannel0 = bt656_8bit_ncfd(portNumber);

	addrI2C = 0x88 >> 1;
	for(i=0; i<43; i++)
	{
		if(NTSCorPAL == 1)
		{
			_IIC_write(hVMD642I2C,
					   addrI2C,
					   (sa7121hNTSC[i].regsubaddr),
					   (sa7121hNTSC[i].regvule));
		}
		else
		{
			_IIC_write(hVMD642I2C,
					   addrI2C,
					   (sa7121hPAL[i].regsubaddr),
					   (sa7121hPAL[i].regvule));
		}
	}

/*----------------------------------------------------------*/
	/*��ʼ��Video Port1*/
	/*��Video Port1��Ϊ�ɼ�����*/
	portNumber = 1;
	vpHchannel1 = bt656_8bit_ncfc(portNumber);

/*----------------------------------------------------------*/
	/*�����ɼ�ģ��*/
	bt656_capture_start(vpHchannel1);
    
    /*������ʱ��*/
    TIMER_start(hTimer);

	/*��һ�����У��ɼ���֡����*/
	for (nextFrame = 1; nextFrame <= 3; nextFrame++)
	{
		/*�ȴ�һ֡���ݲɼ����*/
		while(capNewFrame == 0){}
		/*����ɼ���ɵı�־����ʼ��һ֡�ɼ�*/
		capNewFrame = 0;

		switch (nextFrame)
		{
			case 1:
				YBuf = Ybuffer1; CbBuf = Cbbuffer1; CrBuf = Crbuffer1;
				break;
			case 2:
				YBuf = Ybuffer2; CbBuf = Cbbuffer2; CrBuf = Crbuffer2;
				break;
			case 3:
				YBuf = Ybuffer3; CbBuf = Cbbuffer3; CrBuf = Crbuffer3;
				break;
		}
        send_frame_gray(numLines, numPixels, capYbuffer, YBuf);
	}
    nextFrame = 1;  /*������һ֡��λ��*/

    /*��������֡��ͼ��*/
    for (nextDiff = 1; nextDiff <= 2; nextDiff++)
    {
        switch (nextDiff)
        {
            case 1:
                YBuf = Ybuffer2;   YSubBuf = Ybuffer1;   YAnsBuf = YbufferDiff12;
                CbBuf = Cbbuffer2; CbSubBuf = Cbbuffer1; CbAnsBuf = CbbufferDiff12;
                CrBuf = Crbuffer2; CrSubBuf = Crbuffer1; CrAnsBuf = CrbufferDiff12;
                break;
            case 2:
                YBuf = Ybuffer3;   YSubBuf = Ybuffer2;   YAnsBuf = YbufferDiff23;
                CbBuf = Cbbuffer3; CbSubBuf = Cbbuffer2; CbAnsBuf = CbbufferDiff23;
                CrBuf = Crbuffer3; CrSubBuf = Crbuffer2; CrAnsBuf = CrbufferDiff23;
                break;
        }
        gen_diff_frame_gray(numLines, numPixels, YBuf, YSubBuf, YAnsBuf);
    }

    /*ƴ��֡��ͼ�񣬲���������ʾ��*/
    YBuf = YbufferDiff12;   YAddBuf = YbufferDiff23;
    CbBuf = CbbufferDiff12; CbAddBuf = CbbufferDiff23;
    CrBuf = CrbufferDiff12; CrAddBuf = CrbufferDiff23;
    merge_diff_frame_gray(numLines, numPixels, YBuf, CbBuf, CrBuf, YAddBuf, CbAddBuf, CrAddBuf,
        disYbuffer, disCbbuffer, disCrbuffer);
    
    /*�ɼ����������*/
    srand(TIMER_getCount(hTimer));
    /*��ʼ��Kalman�˲���*/
    init_kalman_filter();
    
    /*
    histograms(numLines, numPixels, YbufferPost);
    send_frame_gray(numLines, numPixels, YbufferPost, disYbuffer);
    */

	/*������ʾģ��*/
	bt656_display_start(vpHchannel0);
	/*����ʵʱ����ѭ��*/
	for(;;)
	{
		/*���ɼ����������Ѿ��ɼ��ã�����ʾ�������������ѿ�*/
		if((capNewFrame == 1)&&(disNewFrame == 1))
		{
			/*����ɼ���ɵı�־����ʾ������ʾ������ͼ��*/
			capNewFrame =0;  disNewFrame =0;

            /*����һ֡ͼ��*/
            switch (nextFrame)
            {
                case 1:
                    YBuf = Ybuffer1; CbBuf = Cbbuffer1; CrBuf = Crbuffer1;
                    break;
                case 2:
                    YBuf = Ybuffer2; CbBuf = Cbbuffer2; CrBuf = Crbuffer2;
                    break;
                case 3:
                    YBuf = Ybuffer3; CbBuf = Cbbuffer3; CrBuf = Crbuffer3;
                    break;
            }
            send_frame_gray(numLines, numPixels, capYbuffer, YBuf);

            /*�ռ�ָ��������¸�λ��*/
            if (nextFrame >= 3)
                nextFrame = 1;
            else
                nextFrame ++;

            /*����֡��ͼ��*/
            for (nextDiff = 1; nextDiff <= 2; nextDiff++)
            {
                switch (nextDiff)
                {
                    case 1:
                        YAnsBuf = YbufferDiff12; CbAnsBuf = CbbufferDiff12; CrAnsBuf = CrbufferDiff12;
                        switch (nextFrame)
                        {
                            case 1:
                                YBuf = Ybuffer2;   YSubBuf = Ybuffer1;
                                CbBuf = Cbbuffer2; CbSubBuf = Cbbuffer1;
                                CrBuf = Crbuffer2; CrSubBuf = Crbuffer1;
                                break;
                            case 2:
                                YBuf = Ybuffer3;   YSubBuf = Ybuffer2;
                                CbBuf = Cbbuffer3; CbSubBuf = Cbbuffer2;
                                CrBuf = Crbuffer3; CrSubBuf = Crbuffer2;
                                break;
                            case 3:
                                YBuf = Ybuffer1;   YSubBuf = Ybuffer3;
                                CbBuf = Cbbuffer1; CbSubBuf = Cbbuffer3;
                                CrBuf = Crbuffer1; CrSubBuf = Crbuffer3;
                                break;
                        }
                        break;
                    case 2:
                        YAnsBuf = YbufferDiff23; CbAnsBuf = CbbufferDiff23; CrAnsBuf = CrbufferDiff23;
                        switch (nextFrame)
                        {
                            case 1:
                                YBuf = Ybuffer3;   YSubBuf = Ybuffer2;
                                CbBuf = Cbbuffer3; CbSubBuf = Cbbuffer2;
                                CrBuf = Crbuffer3; CrSubBuf = Crbuffer2;
                                break;
                            case 2:
                                YBuf = Ybuffer1;   YSubBuf = Ybuffer3;
                                CbBuf = Cbbuffer1; CbSubBuf = Cbbuffer3;
                                CrBuf = Crbuffer1; CrSubBuf = Crbuffer3;
                                break;
                            case 3:
                                YBuf = Ybuffer2;   YSubBuf = Ybuffer1;
                                CbBuf = Cbbuffer2; CbSubBuf = Cbbuffer1;
                                CrBuf = Crbuffer2; CrSubBuf = Crbuffer1;
                        }
                        break;
                }
                gen_diff_frame_gray(numLines, numPixels, YBuf, YSubBuf, YAnsBuf);
            }

            /*ƴ��֡��ͼ�񣬲���������ʾ��*/
            YBuf = YbufferDiff12;   YAddBuf = YbufferDiff23;
            CbBuf = CbbufferDiff12; CbAddBuf = CbbufferDiff23;
            CrBuf = CrbufferDiff12; CrAddBuf = CrbufferDiff23;
            merge_diff_frame_gray(numLines, numPixels, YBuf, CbBuf, CrBuf, YAddBuf, CbAddBuf, CrAddBuf,
                disYbuffer, disCbbuffer, disCrbuffer);
            
            /*
            histograms(numLines, numPixels, YbufferPost);
            send_frame_gray(numLines, numPixels, YbufferPost, disYbuffer);
            */
		}
	}
}

interrupt void MovingCtrl(void)
{
    extern Matrix21 X_post;
    extern Matrix21 z;
    Uint8 i;
    
    /* Calculate pre-configure parameters of filter */
    do_analysis();
    
    /* Iterate the filter process */
    kalman_filter();
    
    /* if no object found, stop the holder if it is running */
    if (nextMove == HOLDER_MOV_STAY && curMove != HOLDER_MOV_STAY) {
        for (i = 0; i < 7; i++) {
            VMD642_UART_putChar(g_uartHandleA, stay[i]);
        }
    }
    /* if we are tracking an object, decide the direction to move */
    if (nextMove == HOLDER_MOV_UNDEF) {
        if (z.array[0][0] < numPixels/2) {
            nextMove = HOLDER_MOV_LEFT;
        }
        else {
            nextMove = HOLDER_MOV_RIGHT;
        }
    }

    /* Set moving command if next != current */
    if (nextMove == HOLDER_MOV_LEFT && curMove != HOLDER_MOV_LEFT) {
        for (i = 0; i < 7; i++) {
            VMD642_UART_putChar(g_uartHandleA, turnLeft[i]);
        }
    }
    else if (nextMove == HOLDER_MOV_RIGHT && curMove != HOLDER_MOV_RIGHT) {
        for (i = 0; i < 7; i++) {
            VMD642_UART_putChar(g_uartHandleA, turnRight[i]);
        }
    }
    
    /* Finally update moving command */
    curMove = nextMove;
}

void do_analysis(void)
{
    extern Uint32 disYbuffer;
    extern int numPixels, numLines;
    extern int positionX, positionY, rangeX, rangeY;
    
    extern Matrix21 X_pre, X_post, X_measure, B, v;
    extern Matrix22 P_pre, P_post;
    extern double u, sigma_u, sigma_z;
    
    histograms(numLines, numPixels, disYbuffer);
    
    hist_analysis(numLines, numPixels, &positionX, &positionY, &rangeX, &rangeY);
    
    /* No object is found, don't change measurement position, but update input value */
    /* Stop movement of the camera */
    if ((rangeX == 0 || rangeY == 0) && (curMove != HOLDER_MOV_UNDEF)) {
        u = curMove * angular_speed;
        nextMove = HOLDER_MOV_STAY;
    }
    /* One object is found, update variables */
    /* Move camera to track the object */
    else {
        X_measure.array[0][0] = positionX;
        X_measure.array[1][0] = positionY;
        u = curMove * angular_speed;  /* input value */
        sigma_u = rangeX / numPixels;   /* process error */
        sigma_z = 0.01;             /* measurement error, estimated as constant */
        nextMove = HOLDER_MOV_UNDEF;
    }
    /* Do iteration for state vector and covariance */
    X_pre = X_post;
    P_pre = P_post;
}
