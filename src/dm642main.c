/********************************************************************/
/* Output video of Three-frame difference method                    */
/* Author: Ming Wen                                                 */
/********************************************************************/

/********************************************************************/
/*  Copyright 2006 by Vision Magic Ltd.								*/
/*  Restricted rights to use, duplicate or disclose this code are	*/
/*  granted through contract.									    */
/*  															    */
/********************************************************************/

#include <csl.h>
#include <csl_cache.h>
#include <csl_emifa.h>
#include <csl_i2c.h>
#include <csl_gpio.h>
#include <csl_irq.h>
#include <csl_chip.h>
#include <csl_dat.h>
#include "iic.h"
#include "vportcap.h"
#include "vportdis.h"
#include "sa7121h.h"
#include "TVP51xx.h"
#include "frame_operation.h"

/********************************************************************/

/*VMD642��emifa�����ýṹ*/
EMIFA_Config VMDEMIFConfig ={
	   0x00052078,/*gblctl EMIFA(B)global control register value */
	   			  /*��CLK6��4��1ʹ�ܣ���MRMODE��1��ʹ��EK2EN,EK2RATE*/
	   0xffffffd3,/*cectl0 CE0 space control register value*/
	   			  /*��CE0�ռ���ΪSDRAM*/
	   0x73a28e01,/*cectl1 CE1 space control register value*/
	   			  /*Read hold: 1 clock;
	   			    MTYPE : 0000,ѡ��8λ���첽�ӿ�
	   			    Read strobe ��001110��14��clock���
	   			    TA��2 clock; Read setup 2 clock;
	   			    Write hold :2 clock; Write strobe: 14 clock
	   			    Write setup :7 clock
	   			    --					 ---------------
	   			  	  \		 14c		/1c
	   			 	   \----------------/ */
	   0x22a28a22, /*cectl2 CE2 space control register value*/
       0x22a28a42, /*cectl3 CE3 space control register value*/
	   0x57226000, /*sdctl SDRAM control register value*/
	   0x0000081b, /*sdtim SDRAM timing register value*/
	   0x001faf4d, /*sdext SDRAM extension register value*/
	   0x00000002, /*cesec0 CE0 space secondary control register value*/
	   0x00000002, /*cesec1 CE1 space secondary control register value*/
	   0x00000002, /*cesec2 CE2 space secondary control register value*/
	   0x00000073 /*cesec3 CE3 space secondary control register value*/
};

/*VMD642��IIC�����ýṹ*/
I2C_Config VMD642IIC_Config = {
    0,  /* master mode,  i2coar;������ģʽ   */
    0,  /* no interrupt, i2cimr;ֻд���������������жϷ�ʽ*/
    (20-5), /* scl low time, i2cclkl;  */
    (20-5), /* scl high time,i2cclkh;  */
    1,  /* configure later, i2ccnt;*/
    0,  /* configure later, i2csar;*/
    0x4ea0, /* master tx mode,     */
            /* i2c runs free,      */
            /* 8-bit data + NACK   */
            /* no repeat mode      */
    (75-1), /* 4MHz clock, i2cpsc  */
};

CHIP_Config VMD642percfg = {
	CHIP_VP2+\
	CHIP_VP1+\
	CHIP_VP0+\
	CHIP_I2C
};

I2C_Handle hVMD642I2C;
int portNumber;
extern SA7121H_ConfParams sa7121hPAL[45];
extern SA7121H_ConfParams sa7121hNTSC[45];
Uint8 vFromat = 0;
Uint8 misc_ctrl = 0x6D;
Uint8 output_format = 0x47;
// ��ַΪ0 for cvbs port1,ѡ�񸴺��ź���Ϊ����
Uint8 input_sel = 0x00;
/*��ַΪ0xf����Pin27���ó�ΪCAPEN����*/
Uint8 pin_cfg = 0x02;
/*��ַΪ1B*/
Uint8 chro_ctrl_2 = 0x14;
/*ͼ����������*/
VP_Handle vpHchannel0;
VP_Handle vpHchannel1;
VP_Handle vpHchannel2;

/********************************************************************/

/*ȷ��ͼ��Ĳ���*/
int numPixels = 720;//ÿ��720������
int numLines  = 576;//ÿ֡576�У�PAL��

/*��������ռ�*/
#pragma DATA_ALIGN(CACHE_A, CACHE_L2_LINESIZE)
#pragma DATA_ALIGN(CACHE_B, CACHE_L2_LINESIZE)
#pragma DATA_ALIGN(CACHE_S, CACHE_L2_LINESIZE)
#pragma DATA_SECTION(CACHE_A, ".cache")
#pragma DATA_SECTION(CACHE_B, ".cache")
#pragma DATA_SECTION(CACHE_S, ".cache")
Uint8 CACHE_A[720];
Uint8 CACHE_B[720];
Uint8 CACHE_S[720];

/*�����С�ļ��� Yͨ�� 720 * 588
Cb �� Cr ͨ�� 720 * 294 */

/*SDRAM ��ַ 0x80000000 - 0x81FFFFFF*/
/*ע������ĵ�ַ��cmd�ļ��������ĵ�ַһ��*/

/*��ǰͼ���׵�ַ���ռ����� vportcap.c �з���*/
Uint32 capYbuffer  = 0x80000000;
Uint32 capCbbuffer = 0x800675c0;
Uint32 capCrbuffer = 0x8009b0a0;

/*����ռ䣬��¼��һ֡ͼ���׵�ַ*/
#pragma DATA_SECTION(ChaAYSpace1, ".ChaAYSpace1")
Uint8 ChaAYSpace1[720*588];
#pragma DATA_SECTION(ChaACbSpace1, ".ChaACbSpace1")
Uint8 ChaACbSpace1[360*588];
#pragma DATA_SECTION(ChaACrSpace1, ".ChaACrSpace1")
Uint8 ChaACrSpace1[360*588];

Uint32 Ybuffer1  = 0x80100000;
Uint32 Cbbuffer1 = 0x801675c0;
Uint32 Crbuffer1 = 0x8019b0a0;

/*����ռ䣬��¼�ڶ�֡ͼ���׵�ַ*/
#pragma DATA_SECTION(ChaAYSpace2, ".ChaAYSpace2")
Uint8 ChaAYSpace2[720*588];
#pragma DATA_SECTION(ChaACbSpace2, ".ChaACbSpace2")
Uint8 ChaACbSpace2[360*588];
#pragma DATA_SECTION(ChaACrSpace2, ".ChaACrSpace2")
Uint8 ChaACrSpace2[360*588];

Uint32 Ybuffer2  = 0x80200000;
Uint32 Cbbuffer2 = 0x802675c0;
Uint32 Crbuffer2 = 0x8029b0a0;

/*����ռ䣬��¼����֡ͼ���׵�ַ*/
#pragma DATA_SECTION(ChaAYSpace3, ".ChaAYSpace3")
Uint8 ChaAYSpace3[720*588];
#pragma DATA_SECTION(ChaACbSpace3, ".ChaACbSpace3")
Uint8 ChaACbSpace3[360*588];
#pragma DATA_SECTION(ChaACrSpace3, ".ChaACrSpace3")
Uint8 ChaACrSpace3[360*588];

Uint32 Ybuffer3  = 0x80300000;
Uint32 Cbbuffer3 = 0x803675c0;
Uint32 Crbuffer3 = 0x8039b0a0;

/*����ռ䣬��¼��һ��֡��ͼ���׵�ַ*/
#pragma DATA_SECTION(ChaAYSpaceDiff12, ".ChaAYSpaceDiff12")
Uint8 ChaAYSpaceDiff12[720*588];
#pragma DATA_SECTION(ChaACbSpaceDiff12, ".ChaACbSpaceDiff12")
Uint8 ChaACbSpaceDiff12[360*588];
#pragma DATA_SECTION(ChaACrSpaceDiff12, ".ChaACrSpaceDiff12")
Uint8 ChaACrSpaceDiff12[360*588];

Uint32 YbufferDiff12  = 0x80400000;
Uint32 CbbufferDiff12 = 0x804675c0;
Uint32 CrbufferDiff12 = 0x8049b0a0;

/*����ռ䣬��¼�ڶ���֡��ͼ���׵�ַ*/
#pragma DATA_SECTION(ChaAYSpaceDiff23, ".ChaAYSpaceDiff23")
Uint8 ChaAYSpaceDiff23[720*588];
#pragma DATA_SECTION(ChaACbSpaceDiff23, ".ChaACbSpaceDiff23")
Uint8 ChaACbSpaceDiff23[360*588];
#pragma DATA_SECTION(ChaACrSpaceDiff23, ".ChaACrSpaceDiff23")
Uint8 ChaACrSpaceDiff23[360*588];

Uint32 YbufferDiff23  = 0x80500000;
Uint32 CbbufferDiff23 = 0x805675c0;
Uint32 CrbufferDiff23 = 0x8059b0a0;

/*����ռ䣬����ͼ���׵�ַ*/
#pragma DATA_SECTION(ChaAYSpacePost, ".ChaAYSpacePost")
Uint8 ChaAYSpacePost[720*588];
#pragma DATA_SECTION(ChaACbSpacePost, ".ChaACbSpacePost")
Uint8 ChaACbSpacePost[360*588];
#pragma DATA_SECTION(ChaACrSpacePost, ".ChaACrSpacePost")
Uint8 ChaACrSpacePost[360*588];

Uint32 YbufferPost  = 0x80600000;
Uint32 CbbufferPost = 0x806675c0;
Uint32 CrbufferPost = 0x8069b0a0;

/*��ʾͼ���׵�ַ���ռ����� vportdis.c �з���*/
Uint32 disYbuffer  = 0x81000000;
Uint32 disCbbuffer = 0x810675c0;
Uint32 disCrbuffer = 0x8109b0a0;

/*ͼ���ʽ��־*/
Uint8 NTSCorPAL = 0;
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
    
    /* Position of centroid */
    int positionX, positionY;

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
	/*�ж�������ĳ�ʼ��*/
	//Point to the IRQ vector table
    IRQ_setVecs(vectors);
    IRQ_nmiEnable();
    IRQ_globalEnable();
    IRQ_map(IRQ_EVT_VINT1, 11);
    IRQ_map(IRQ_EVT_VINT0, 12);
    IRQ_reset(IRQ_EVT_VINT1);
    IRQ_reset(IRQ_EVT_VINT0);
    /*��һ�����ݿ���������ͨ·*/
    DAT_open(DAT_CHAANY, DAT_PRI_LOW, DAT_OPEN_2D);

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
        YbufferPost, disCbbuffer, disCrbuffer);
    centroid(numLines, numPixels, YbufferPost, &positionX, &positionY);
    draw_rectangle(numLines, numPixels, YbufferPost, positionX, positionY);
    send_frame_gray(numLines, numPixels, YbufferPost, disYbuffer);

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
                YbufferPost, disCbbuffer, disCrbuffer);
            centroid(numLines, numPixels, YbufferPost, &positionX, &positionY);
            draw_rectangle(numLines, numPixels, YbufferPost, positionX, positionY);
            send_frame_gray(numLines, numPixels, YbufferPost, disYbuffer);
		}
	}
}
