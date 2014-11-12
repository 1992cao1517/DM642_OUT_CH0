/*****************************************************************************
 * Author: bitmingw
 * Global configurations, used by main function.
 * THIS HEADER FILE SHOULD ONLY BE INCLUDED BY "dm642main.c"
 *****************************************************************************/

#ifndef _H_GLOBAL_CONFIG
#define _H_GLOBAL_CONFIG

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

/* Config VMD642 UART Communication */
VMD642_UART_Config g_uartConfig ={
	   0x00,/*�Ĵ���IER,�������ж�*/
	   0x57,/*�Ĵ���FCR,���г�ʼ��*/
	   0x03,/*�Ĵ���LCR,�ֳ�=8bit*/
	   0x01,/*�Ĵ���MCR,����RTS���*/
};

/* Config Timer0 */
TIMER_Config timerConfig = {
    0x00000280, /* interal clock, reset counter and hold */
    0x00E4E1C0, /* interrupt every 0.2s */
    0x00000000  /* start from 0 */
};

/* Config I2C handler and address */
I2C_Handle hVMD642I2C;
int portNumber;
extern SA7121H_ConfParams sa7121hPAL[45];
extern SA7121H_ConfParams sa7121hNTSC[45];
Uint8 vFromat = 0;
Uint8 misc_ctrl = 0x6D;
Uint8 output_format = 0x47;
/*��ַΪ0 for cvbs port1,ѡ�񸴺��ź���Ϊ����*/
Uint8 input_sel = 0x00;
/*��ַΪ0xf����Pin27���ó�ΪCAPEN����*/
Uint8 pin_cfg = 0x02;
/*��ַΪ1B*/
Uint8 chro_ctrl_2 = 0x14;

/*ͼ����������*/
VP_Handle vpHchannel0;
VP_Handle vpHchannel1;
VP_Handle vpHchannel2;

/* Config UART handler */
Uint8 g_ioBuf;
Uint16 g_uartBuf;
VMD642_UART_Handle g_uartHandleA;

/* Config Timer handler */
TIMER_Handle hTimer;
Uint32 TimerEventId;

/********************************************************************/

/* GLOBAL VARIABLES - CONTROL PARAMETERS */
/* NOTE: holder turn left == camera turn right, here we use camera as reference */
Uint8 turnRight[7] = {0xFF, 0x01, 0x00, 0x04, 0x3F, 0x00, 0x44};
Uint8 turnLeft[7]  = {0xFF, 0x01, 0x00, 0x02, 0x3F, 0x00, 0x42};
Uint8 stay[7]      = {0xFF, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01};

//int moveCmd[3] = {HOLDER_MOV_STAY, HOLDER_MOV_LEFT, HOLDER_MOV_RIGHT};

/* Indicates the move commands */
int curMove  = HOLDER_MOV_STAY;
int nextMove = HOLDER_MOV_STAY;

/* Rotate angular speed: pix/s */
static double angular_speed = 43.64;    /* Set the value by experiment */

/********************************************************************/

/*ȷ��ͼ��Ĳ���*/
int numPixels = 720;//ÿ��720������
int numLines  = 576;//ÿ֡576�У�PAL��

/*�˶�Ŀ��λ��*/
int positionX;
int positionY;
int rangeX;
int rangeY;

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

/*����ֱ��ͼ�洢�ռ�*/
#pragma DATA_ALIGN(HIST_X, CACHE_L2_LINESIZE)
#pragma DATA_ALIGN(HIST_Y, CACHE_L2_LINESIZE)
#pragma DATA_SECTION(HIST_X, ".bss")
#pragma DATA_SECTION(HIST_Y, ".bss")
Uint32 HIST_X[720];
Uint32 HIST_Y[588];

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

#endif
