/*************************************************
                                           
 ZEM 200                                          
                                                    
 sensor.c
                                                      
 Copyright (C) 2003-2006, ZKSoftware Inc.      		
                                                      
*************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "arca.h"
#include "options.h"
#include "sensor.h"
#include "zkfp.h"
#include "zkfp10.h"
#include "lcm.h"
#include "exfun.h"
#include "utils.h"
#include "main.h"
#include "cim.h"
#include "hv7131.h"

HANDLE fhdl=0;

int ImageWidth=CMOS_WIDTH; 	//当前采集区域的宽度
int ImageHeight=CMOS_HEIGHT; 	//当前采集区域的高度

#define CORRECT_NONE    	0
#define CORRECT_REVERSE 	1
#define CORRECT_ROTATION 	2
#define MARGIN 			8	

//unsigned int img_bpp = 8; /* 8/16/32 */
#if 0
static int hasFingerThreshold=320*36;
static int noFingerThreshold=280*36;
#else
//修改动态支持有膜无膜指纹头
int hasFingerThreshold=7000; //有指纹的阀值
int noFingerThreshold=6500;  //无指纹的阀值
#endif



int fd_sensor=1;

void InitSensor(int LeftLine, int TopLine, int Width, int Height, int FPReaderOpt)
{      
        ImageWidth=Width;
        ImageHeight=Height;
        cim_init(Width,Height);
        if( sensor_init(LeftLine, TopLine, Width, Height, FPReaderOpt)!=0 )
	{
		GPIO_HY7131_Power(FALSE);
		fd_sensor = 0;
		printf("Initialize sensor failed !!!\n");
	}
	else
	{
		fd_sensor = 1;
		abs(-1);
		printf("%d\n",abs(-1));
        	printf("Finished sensor init.\n");
	}
}

void FreeSensor(void)
{     
        sensor_close();
}

void FlushSensorBuffer(void)
{
	//nothing to do
}

int CaptureSensor(char *Buffer, BOOL Sign, PSensorBufInfo SensorBufInfo)
{
	//U32 framesize=ImageWidth*ImageHeight;
	U32 framesize=gOptions.OImageWidth*gOptions.OImageHeight;
	U32 len;
//	static int i=0;
//	char filename[128];
//printf(" fhdl=%x , fd_sensor=%d \n", fhdl, fd_sensor);

	if (!fhdl || !fd_sensor)	return FALSE;
	len=cim_read((void *)Buffer, framesize);
//	DBPRINTF("READING IMAGE Len:%d, framesize:%d\n",len,framesize);
	//if(len>0)
	if(len==framesize)
	{
		FilterRGB(Buffer, gOptions.OImageWidth, gOptions.OImageHeight);
		//FilterRGB(Buffer, ImageWidth, ImageHeight);
		/*if(gOptions.NoFilm)
			ReverseImage(Buffer,gOptions.OImageWidth,gOptions.OImageHeight);*/
		if(SensorBufInfo)
		{
			//image buffer info
			SensorBufInfo->SensorNo=255;
			SensorBufInfo->RawImgLen=len;
			SensorBufInfo->RawImgPtr=(void *)Buffer;
			SensorBufInfo->DewarpedImgLen=len;
			SensorBufInfo->DewarpedImgPtr=(void *)Buffer;
		}
		return TRUE;
	}
	else
		return FALSE;
}

int CalcThreshold(int NewT)
{
	if(NewT<10) return NewT<0?0:NewT*3;
	else return NewT>50? 70: NewT+20;
}

int CalcNewThreshold(int Thr)
{
	if(Thr<30) return Thr<0?0:Thr/3;
	else return Thr>70? 50: Thr-20;
}

#define P_O_WIDTH(s) s[0]
#define P_O_HEIGHT(s) s[1]
#define P_CP0_X(s) ((s[2]))
#define P_CP0_Y(s) ((s[3]))
#define P_CP1_X(s) ((s[4]))
#define P_CP1_Y(s) ((s[5]))
#define P_CP2_X(s) ((s[6]))
#define P_CP2_Y(s) ((s[7]))
#define P_CP3_X(s) ((s[8]))
#define P_CP3_Y(s) ((s[9]))
#define P_TP0_X(s) s[10]
#define P_TP0_Y(s) s[11]
#define P_TP1_X(s) s[12]
#define P_TP1_Y(s) s[13]
#define P_TP2_X(s) s[14]
#define P_TP2_Y(s) s[15]
#define P_TP3_X(s) s[16]
#define P_TP3_Y(s) s[17]
#define P_TP4_X(s) s[18]
#define P_TP4_Y(s) s[19]
#define FP_WIDTH(s) s[20]
#define FP_HEIGHT(s) s[21]

short sizes[22];

int FPBaseInit(char *FingerCacheBuf)
{
	//short sizes[22];
	BYTE params[8];
	
	P_O_WIDTH(sizes)=gOptions.OImageWidth;
	P_O_HEIGHT(sizes)=gOptions.OImageHeight;
	FP_WIDTH(sizes)=gOptions.ZF_WIDTH;
	FP_HEIGHT(sizes)=gOptions.ZF_HEIGHT;
	P_CP2_X(sizes)=(short)gOptions.CPX[0];
	P_CP0_Y(sizes)=(short)gOptions.CPY[0];
	P_CP3_X(sizes)=(short)gOptions.CPX[1];
	P_CP1_Y(sizes)=(short)gOptions.CPY[1];
	P_CP0_X(sizes)=(short)gOptions.CPX[2];
	P_CP2_Y(sizes)=(short)gOptions.CPY[2];
	P_CP1_X(sizes)=(short)gOptions.CPX[3];
	P_CP3_Y(sizes)=(short)gOptions.CPY[3];
	params[0]=1;
	params[1]=50;
	params[2]=65;
	params[3]=110;
	params[4]=0;
	params[5]=255;
#ifdef TESTIMAGE
	P_TP0_X(sizes)=8;
	P_TP0_Y(sizes)=8;
#else
	P_TP0_X(sizes)=180;
	P_TP0_Y(sizes)=138;
#endif	
	if (gOptions.ZKFPVersion != ZKFPV10)
	{
		if((FingerCacheBuf==NULL) && fhdl) 
			FingerCacheBuf=(char*)fhdl; 
		else if(FingerCacheBuf==NULL) 
			FingerCacheBuf=(char*)malloc(1024*1024*2);
	}
	fhdl=0;
	if (gOptions.ZKFPVersion == ZKFPV10)
	{
		BIOKEY_SET_PARAMETER_10(fd_sensor, ZKFP_PARAM_CODE_DBMM, ZKFP_DBMM_EXTERNAL);
		fhdl=BIOKEY_INIT_10(fd_sensor, (WORD*)sizes, params, (BYTE*)FingerCacheBuf, gOptions.NewFPReader);
	}
	else
	{
		fhdl=BIOKEY_INIT(fd_sensor, (WORD*)sizes, params, (BYTE*)FingerCacheBuf, gOptions.NewFPReader);
	}
	if (fhdl)
	{
		if (gOptions.ZKFPVersion == ZKFPV10)
		{
			BIOKEY_MATCHINGPARAM_10(fhdl, IDENTIFYSPEED, gOptions.MThreshold);
			BIOKEY_SETNOISETHRESHOLD_10(fhdl, gOptions.MaxNoiseThr, gOptions.MinMinutiae, gOptions.MaxTempLen, 500);	
		}
		else
		{
			BIOKEY_MATCHINGPARAM(fhdl, IDENTIFYSPEED, gOptions.MThreshold);
			BIOKEY_SETNOISETHRESHOLD(fhdl, gOptions.MaxNoiseThr, gOptions.MinMinutiae, gOptions.MaxTempLen, 500);	
		}
	}
	else
	{
		//fp reader error
		printf("Initialize BIOKEY error!!!\n");
	}
	return (int)fhdl;
}

#define gOHeight gOptions.OImageHeight
#define gOWidth gOptions.OImageWidth
#define gOSize gOptions.OImageWidth*gOptions.OImageHeight
#define NewThr
#ifdef NewThr
#define MIN_THR 30
#define TOP_THR 80
#define INC_THR 20
#else
#define MIN_THR 50
#define TOP_THR 80
#define INC_THR 0
#endif
int LastIndex=0;
int ShowVar=1;
int LastUserID=0;

U32 FingerCount=0;
int GainAutoAdjust(int m1)
{
	if(gOptions.NewFPReader & CORRECT_REVERSE)
	{
		//2008.06.05
		//if(m1<150)	//AGC-Auto Gain Control
		//	IncCMOSGain();
		//else if(m1>230)
		//	DecCMOSGain();
		//else
			return TRUE;
	}
	else
	{
		if(m1>230)	//AGC-Auto Gain Control
			DecCMOSGain();
		else if(m1<150)
			IncCMOSGain();
		else
			return TRUE;
	}
	//printf("GainAutoAdjust m1=%d\n",m1);
	return FALSE;
}

extern TSensorBufInfo SensorBufInfo;
int CalcVar(BYTE *img, int width, int height, int *var, int *mean, int *whiteSum, int FrameWidth);

int AutoGainImage(int i)
{
	int m1,v1,iTemp;
	
	while(i--)
	{
		if(CaptureSensor(gImageBuffer,ONLY_LOCAL,&SensorBufInfo))
		{
			CalcVar((BYTE*)gImageBuffer, gOWidth, gOHeight, &v1, &m1, &iTemp,32);
			if(m1==0) return FALSE;
			if(GainAutoAdjust(m1))
				return TRUE;
		}

	}
	return FALSE;
}

#if 0
static void ReverseImage(BYTE *image,int w, int h)
{
	static int i;
	BYTE *p=image;
	for(i=0; i<w*h; i++)
	{
		*p=255-*p;
		p++;
	}
}

static void RegionDivideAdaptive(BYTE* lpDIBBits, int lmageWidth, int lmageHeight, int adjustValue, int *AvgPixel)
{
        static int i;  //循环变量
        static int j;  //循环变量
        // 指向源图像的指针
        static unsigned char*  lpSrc;
        //像素值
        static unsigned char pixeltemp;
        // 子图像灰度的平均值
        static int nAvg ;
        nAvg = 0 ; //初始化为0
        // 对左下图像逐点扫描：
        for(j = 0; j <lmageHeight; j++)
        {
                for(i = 0;i <lmageWidth; i++)
                {
                        // 指向源图像倒数第j行，第i个象素的指针                 
                        lpSrc = (unsigned char *)lpDIBBits + lmageWidth * j + i;
                        //取得当前指针处的像素值
                        pixeltemp = (unsigned char)*lpSrc;
                        //灰度的累计
                        nAvg = (int)pixeltemp+nAvg;
                }
        }
        // 计算均值
        // adjustValue is best for blank image,because the diff is very small on blank image,it can filter many invalid data.
        //it is -12 for upek,-16 for glass that have cover
        nAvg = nAvg /((lmageHeight) * (lmageWidth))+adjustValue ;
        *AvgPixel = 255-nAvg;
        // 对左下图像逐点扫描：
        for(j = 0; j <lmageHeight; j++)
        {
                for(i = 0;i <lmageWidth; i++)
                {
                        // 指向源图像倒数第j行，第i个象素的指针                 
                        lpSrc = (unsigned char *)lpDIBBits + lmageWidth * j + i;
                        //取得当前指针处的像素值
                        pixeltemp = (unsigned char)*lpSrc;
                        //目标图像中灰度值小于门限则设置为黑点
                        if(pixeltemp <= nAvg)
                        {
                                *lpSrc=(unsigned char)0;
                        }
                        else    //否则设置为白点
                        {
                                *lpSrc=(unsigned char)255;
                        }
                }
        }
}


/*
Cut a area for detect from original image
*/
static int CutDetectArea(BYTE* SrcImgBuf,BYTE* DstImgBuf,int X,int Y,int SrcWidth,int SrcHeight, int Width,int Height)
{
        static int i,j;
        if(Width>SrcWidth || Height>SrcHeight || !SrcImgBuf || !DstImgBuf)
        {
                return FALSE;
        }

        for(j=Y;j<Y+Height;j++)
        {
                for(i=X;i<X+Width;i++)
                {
                        *DstImgBuf= *(SrcImgBuf+SrcWidth * j +i);
                        DstImgBuf++; 
                }
        }
        return TRUE;
}


#define DETECT_WIDTH            128
#define DETECT_HEIGHT           64
#define DETECT_IMG_SIZE         (DETECT_WIDTH*DETECT_HEIGHT)


BYTE prev_fp[DETECT_IMG_SIZE]={0};
BYTE diff_fp[DETECT_IMG_SIZE]={0};
BYTE cur_fp[DETECT_IMG_SIZE]={0};

int DetectFP(BYTE *ImgBuf,int Width,int Height, int HasFingerThreshold,int NoFingerThreshold,int Reverse,int DetectCount,int IsSingle)
{
	int v,avg_cur,avg_cur_div,avg_diff,m1,m2,size,pixel_diff,whiteSum=0,iTemp=0;
	int avgPixel=0;
/*	int ImgSize= Width*Height;

	BYTE *prev_fp = ImgBuf + (ImgSize),
		 *diff_fp = ImgBuf + (ImgSize) + DETECT_IMG_SIZE,
		 *cur_fp  = ImgBuf + (ImgSize) + (DETECT_IMG_SIZE <<1); */
//	printf(" ********** DetectFP ImgBuf=%p,prev_fp=%p,diff_fp=%p,cur_fp=%p \n",ImgBuf,prev_fp,diff_fp,cur_fp);
	static int LeaveFinger=0,
		   validCount=0,
		   pre_avg_cur_div=0;

	if(!CutDetectArea(ImgBuf,cur_fp,(Width>>1)-64 ,(Height>>1),Width,Height,DETECT_WIDTH,DETECT_HEIGHT))
	{
		return 0;
	}
	if(Reverse)
	{
		ReverseImage(cur_fp,DETECT_WIDTH,DETECT_HEIGHT);
	}
#ifdef DEBUGME
	WriteBitmap(cur_fp,DETECT_WIDTH,DETECT_HEIGHT,"fp_cut.bmp");
	WriteBitmap(ImgBuf,Width,Height,"fp_cut_src.bmp");
#endif
	CalcVar(cur_fp,DETECT_WIDTH,DETECT_HEIGHT,&avg_cur,&m1,&whiteSum,0);
	RegionDivideAdaptive((BYTE*)cur_fp,DETECT_WIDTH,DETECT_HEIGHT,-12,&avgPixel);
	size = DETECT_IMG_SIZE;
	for(v=0;v<size;v++)
	{
		if(cur_fp[v]==prev_fp[v])
			pixel_diff=255;
		else
			pixel_diff=0;
		diff_fp[v]=(BYTE)pixel_diff;
	}
	CalcVar(cur_fp,DETECT_WIDTH,DETECT_HEIGHT,&avg_cur_div,&m1,&iTemp,0);
	CalcVar(diff_fp,DETECT_WIDTH, DETECT_HEIGHT,&avg_diff,&m2,&iTemp,0);
	
#ifdef DEBUGME
	WriteBitmap(cur_fp, DETECT_WIDTH, DETECT_HEIGHT,"cur.bmp");
	WriteBitmap( prev_fp, DETECT_WIDTH, DETECT_HEIGHT,"prev.bmp");
	WriteBitmap( diff_fp,DETECT_WIDTH, DETECT_HEIGHT,"diff.bmp");
#endif
	if(!IsSingle && !LeaveFinger && (avg_cur_div<NoFingerThreshold || avg_diff>NoFingerThreshold))
	{
		LeaveFinger=TRUE;
		memcpy(prev_fp,cur_fp,DETECT_WIDTH*DETECT_HEIGHT);
		validCount=0; 
		pre_avg_cur_div=0;
	//	printf("No finger\n"); 
		return 0;
	}else if (IsSingle && ((avg_cur_div>HasFingerThreshold ) ||			//正常探测
			(avgPixel>180 && avg_cur_div>HasFingerThreshold-1200 ) || //适当考虑过湿指纹
			(avgPixel<80 &&  avg_cur_div>HasFingerThreshold+800 )))
	{
		return TRUE;
		
	}else if(LeaveFinger && ((avg_cur_div>HasFingerThreshold && avg_diff>NoFingerThreshold) ||			//正常探测
			(avgPixel>180 && avg_cur_div>HasFingerThreshold-1200 && avg_diff>NoFingerThreshold-1200) || //适当考虑过湿指纹
			(avgPixel<80 &&  avg_cur_div>HasFingerThreshold+800 && avg_diff>NoFingerThreshold+800)))	//适当过滤残留指纹
	{
	//	printf("debug me avg_cur_div=%d,count=%d,avgPixel=%d\n",avg_cur_div,validCount,avgPixel);
		validCount++;
		if(validCount==DetectCount)
		{
			//filter black background
			if(avgPixel>216 || (avg_cur<696 && avgPixel>136 && whiteSum<104) || (avg_cur<480 && avgPixel>120))
			{
				validCount=0;
				DBPRINTF("a fake finger\n");
				//write_bitmap("/mnt/ramdisk/finger_fake.bmp",ImgBuf,Width,Height);
				return FALSE;
			}
			//write_bitmap("/mnt/ramdisk/finger.bmp",ImgBuf,Width,Height);
					
			LeaveFinger=FALSE;
			memcpy(prev_fp,cur_fp,DETECT_IMG_SIZE);
#ifdef DEBUGME 	//get a best finger image
			if(validCount>1 && pre_avg_cur_div> avg_cur_div)
				memcpy(ImgBuf,ImgBuf+ImgSize,ImgSize);
#endif
			pre_avg_cur_div = 0;
			validCount=0;

			return TRUE;
		}else
		{
			pre_avg_cur_div = avg_cur_div;
#ifdef DEBUGME
			memcpy(ImgBuf+ImgSize,ImgBuf,ImgSize);
#endif
			return 0;
		}
	}else
	{
		//_RPT3(0,"else debug me avg_cur_div=%d,avg_diff=%d,avgPixel=%d\n",avg_cur_div,avg_diff,avgPixel);
	//	printf("else debug me avg_cur_div=%d,avg_diff=%d,avgPixel=%d\n",avg_cur_div,avg_diff,avgPixel);
		validCount=0;
		pre_avg_cur_div=0;
		if(LeaveFinger && avgPixel>180)
			return FALSE;// -1;
		else
			return FALSE;
	}
}

#endif

#if 0
U32 FPTest( char *ImgBuf)
{
        if(DetectFP((void *)ImgBuf, gOptions.OImageWidth,gOptions.OImageHeight, hasFingerThreshold, noFingerThreshold,0,1,0))
        {
                ExBeep(1);
                return TRUE;
        }else
        {
                return FALSE;
        }
}
#else
extern BOOL GC0303_NoCoat;
//修改以动态支持有膜无膜指纹头
U32 FPTest( char *ImgBuf)
{
       // int    // reverse = GC0303_NoCoat,  //是否反色
        int        numdetect=1,  //探测图像帧数
                issingle=0;  //是否允许指纹不离开连续比对

         if(DetectFP(ImgBuf, gOptions.OImageWidth,gOptions.OImageHeight, hasFingerThreshold, noFingerThreshold,GC0303_NoCoat,numdetect,issingle))//,sensortype))
        {
                ExBeep(1);
                return TRUE;
        }
        else
        {
                return FALSE;
        }
}

#endif
//up newfp test

#if 0
#define BSIZE 16

int CalcVar(BYTE *img, int width, int height, int *var, int *mean, int *whiteSum, int FrameWidth)
{
	int msum, vsum, i, j, bc, sum, m, n, v,t, bsize;
	BYTE *p;
	bsize=BSIZE*BSIZE;
	msum=0;bc=0;vsum=0;
	width-=FrameWidth*2;
	height-=FrameWidth*2;
	*whiteSum=0;
	for(i=0;i<height/BSIZE;i++)
	for(j=0;j<width/BSIZE;j++)
	{
		sum=0;
		for(m=i*BSIZE;m<i*BSIZE+BSIZE;m++)
		{
			p=img+FrameWidth+(m+FrameWidth)*(width+FrameWidth*2)+j*BSIZE;
			for(n=0;n<BSIZE;n++)
			{
				if((*p)>136)
					(*whiteSum)++;
				sum+=(int)*p++;
			}
		}
		sum=(sum+bsize)/bsize;
		msum+=sum;
		v=0;
		for(m=i*BSIZE;m<i*BSIZE+BSIZE;m++)
		{
			p=img+FrameWidth+(m+FrameWidth)*(width+FrameWidth*2)+j*BSIZE;
			for(n=0;n<BSIZE;n++)
			{
				t=(int)*p++-sum;
				t=t*t;
				v+=t;
			}
		}
		v=(v+bsize)/bsize;
		vsum+=v;
		bc++;
	}
	*var=(vsum+bc/2)/bc;
	if(gOptions.NewFPReader & CORRECT_REVERSE)
		*mean=255-(msum+bc/2)/bc;
	else
		*mean=(msum+bc/2)/bc;
	
	return 1;
}
#endif
