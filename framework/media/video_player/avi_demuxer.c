#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "avi_demuxer.h"
#include <stream.h>

unsigned int Samples_per_sec;

/**
**	读取4  个字节
**/
int read4bytes(void *stream)
{
	DWORD val;
	int rdnum;
	
	rdnum = read_data(stream, &val, 4);
	if (rdnum != 4)
	{
		val = 0;
	}
	return val;
}


FOURCC check_junk(void *stream, FOURCC tag, FOURCC compare_tag)
{
	int curtag;
	int junklen;
	if (tag != compare_tag)
		return tag;
	
	junklen = read4bytes(stream);
	read_skip(stream, junklen);
	curtag = read4bytes(stream);
	curtag = check_junk(stream, curtag, compare_tag);
	return curtag;
}


/**
**	分析视频文件头，获取必要信息
**/
int read_header(void *avict)
{
//amv len is set to 0 so we can skip zero 
//make it simple just do the amv case
	//riff len avi 
	//list len hdrl
	//avih len mainAVIHeader
	//list len strl
	//strh len AVIStreamHeader
	//strf len BITMAPINFORHEADER or WAVEFORMATEX

	//riff len avi or riff 0 amv 
	FOURCC tag;
	MainAVIHeader mheader;
	AVIStreamHeader sheader;
	int rdnum;
	//BITMAPINFOHEADER bmpinfo;
	//WAVEFORMATEX waveinfo;
	io_stream_t stream;
	avi_contex_t *ac;
	//int junklen;
	FOURCC contag;	
	int strhlen;
	int strflen;
	int movi_len;

	if (avict == NULL)
	{
		printf("%s: %d\n", __FUNCTION__, __LINE__);
		return MEMERROR;
	}
	
	ac = (avi_contex_t *)avict;
		
	stream = ac->stream;
	
	read_skip(stream, 8);  //riff len
	
	tag = read4bytes(stream);

	if ((tag != formtypeAVI)&&(tag != formtypeAMV))
	{
		printf("%s: tag: 0x%x\n", __FUNCTION__, tag);
		return UNKNOWNCON;
	}
	
	contag = tag;
	
	if (contag == formtypeAMV)
	{
		// 视频文件为amv  格式
		ac->amvformat = 1;
	}
	
	read_skip(stream, 12);	//list len hdrl
	
	tag = read4bytes(stream);
	
	//if (tag != ckidAVIMAINHDR)	//avih len mainAVIHeader
	//	return UNKNOWNCON;	

	read_skip(stream, 4);
	
	rdnum = read_data(stream, &mheader, sizeof(MainAVIHeader));
	if (rdnum != sizeof(MainAVIHeader))
	{
		printf("%s: %d\n", __FUNCTION__, __LINE__);
		return STREAMEND;
	}
	
	if (ac->amvformat)
	{
		// AMV Format 
		int fps = 0;
		int timedur = 0;
		int timeval = mheader.dwReserved[3];

		// 帧率，每秒帧数
		fps = mheader.dwReserved[0]; //dwRate
		// 视频时长hh:mm:ss，转成秒
		timedur = (timeval&0xff) + ((timeval>>8)&0xff)*60 + ((timeval>>16)&0xff)*3600; 

		// 视频总帧数
		ac->TotalFrames = timedur * fps;
	 	ac->dwScale = mheader.dwReserved[1];
		ac->dwRate  = mheader.dwReserved[0];		
	}
	else
	{
		// AVI Format
		ac->TotalFrames = mheader.dwTotalFrames;
	}
	
	ac->index_flag = mheader.dwFlags;
	ac->bmpheader.biWidth  = mheader.dwWidth;
	ac->bmpheader.biHeight = mheader.dwHeight;	
	
    do{		
		
    	tag = read4bytes(stream);
		
		tag = check_junk(stream, tag, ckidAVIPADDING);
		
    	if (tag != LISTTAG)
    		break;	
    	
    	movi_len = read4bytes(stream);

		tag = read4bytes(stream);
    	if (tag == listtypeAVIMOVIE)
    	{
    		ac->movitagpos = file_tell(stream) ;
			ac->index_offset = movi_len+ac->movitagpos - 4;
			//相对偏移起始位置			
    		break;			
    	}
	
    	tag = read4bytes(stream);    	    	
    	if (tag != ckidSTREAMHEADER) //strh len AVIStreamHeader
    	{
			printf("%s: %d\n", __FUNCTION__, __LINE__);
    		return UNKNOWNCON;
    	}

		strhlen = read4bytes(stream);

		memset(&sheader, 0, sizeof(AVIStreamHeader));
			
		if (contag == formtypeAMV)
		{
			read_skip(stream, strhlen);
		}
		else
		{
    		rdnum = read_data(stream, &sheader, sizeof(AVIStreamHeader));
    
    		if (rdnum != sizeof(AVIStreamHeader))
    		{
				printf("%s: %d\n", __FUNCTION__, __LINE__);
    			return STREAMEND;	
    		}
		}
		
    	tag = read4bytes(stream); 
    	
    	if (tag != (ckidSTREAMFORMAT)) //strf len BITMAPINFORHEADER or WAVEFORMATEX
    	{
			printf("%s: %d\n", __FUNCTION__, __LINE__);
    		return UNKNOWNCON;	
    	}

		strflen = read4bytes(stream);
			
		if (contag == formtypeAMV)
		{
			if (strflen == 0x14)
			{
	    		rdnum = read_data(stream, &ac->amvwaveheader, sizeof(AMVAudioStreamFormat));	
	    		if (rdnum != sizeof(AMVAudioStreamFormat))
	    		{
					printf("%s: %d\n", __FUNCTION__, __LINE__);
	    			return STREAMEND;	
	    		}
				ac->dwSamples_per_sec = ac->amvwaveheader.dwSamples_per_sec;
				ac->wChannels = ac->amvwaveheader.wChannels;		
				Samples_per_sec = ac->dwSamples_per_sec;
			}
			else
			{
				read_skip(stream, strflen);
			}
		}
		else
		{
	    	if (sheader.fccType == streamtypeVIDEO)
	    	{
	 			ac->dwScale = sheader.dwScale;
				ac->dwRate  = sheader.dwRate;

	    		rdnum = read_data(stream, &ac->bmpheader, sizeof(BITMAPINFOHEADER));
	    		if (rdnum != sizeof(BITMAPINFOHEADER))
	    		{
					printf("%s: %d\n", __FUNCTION__, __LINE__);
	    			return STREAMEND;
	    		}
	    	}
	    
	    	if (sheader.fccType == streamtypeAUDIO)
	    	{
	    		rdnum = read_data(stream, &ac->waveheader, sizeof(WAVEFORMATEX));	
	    		if (rdnum != sizeof(WAVEFORMATEX))
	    		{
					printf("%s: %d\n", __FUNCTION__, __LINE__);
	    			return STREAMEND;
	    		}
				ac->dwSamples_per_sec = ac->waveheader.nSamplesPerSec;
				ac->wChannels = ac->waveheader.nChannels;
				Samples_per_sec = ac->dwSamples_per_sec;
				read_skip(stream, ac->waveheader.cbSize);
	    	}	
		}
    }while(1);

	printf("%s: %d\n", __FUNCTION__, __LINE__);
	return NORMAL;
}

int get_es_chunk(void *avict, avi_packet_t *raw_packet)
{
	FOURCC tag;
	//int rdnum;
	void *stream;
	avi_contex_t *ac;
	int chunklen;
	//int d[8];	
	//int i,n;
	//int size;	
	//unsigned char val;	
	
	if ((avict == NULL)||(raw_packet == NULL))
		return MEMERROR;

	ac = (avi_contex_t *)avict;
		
	stream = ac->stream;

	tag = read4bytes(stream);
	if (tag == 0)
		return STREAMEND;

	//tag = check_junk(io, tag, ckidAVIPADDING);
//get chunk type
	switch ((tag >> 16) & DATA_MASK)
	{
		case cktypeDIBbits:
		case cktypeDIBcompressed:
			raw_packet->es_type = streamtypeVIDEO;
			//printf("f pos: 0x%x\n", file_tell(io) - 4);
			break;
		case cktypeWAVEbytes:
			raw_packet->es_type = streamtypeAUDIO;
			break;
		default:
			raw_packet->es_type = tag;		
			break;
	}

//read chunk
	chunklen = read4bytes(stream);

	raw_packet->data_len = chunklen;
	raw_packet->pts  = ((ac->framecounter * TIMESCALE * 10) / ac->framerate)/10;

	return NORMAL;
	
}

typedef struct{
	int   frameth;
	DWORD pos;
}frame_info_t;

#define SCAN_BUF_LEN (MAX_PACKET_LEN-3)

int search_es_chunk(void *avict, avi_packet_t *raw_packet)
{
	FOURCC tag;
	//FOURCC Firsttag;
	//int rdnum;
	void *stream;
	avi_contex_t *ac;
	//int chunklen;
	//int d[8];	
	//int i,n;
	int size;	
	//unsigned char val;	
	int frame_num;
	frame_info_t frame_arr[10];
	int offsettostart;
	unsigned char paddbytes[3];
	unsigned char *tmpbuf = 0;
	int end;

	unsigned char *ptr;
	int buf_remain;
	int flag_start;
		
	if ((avict == NULL)||(raw_packet == NULL))
		return MEMERROR;

	ac = (avi_contex_t *)avict;
		
	stream = ac->stream;

	flag_start = 0;
	frame_num = 0;
	paddbytes[0] = 0;
	paddbytes[1] = 0;
	paddbytes[2] = 0;

//外部传入，就是av_buf里的缓冲区	
	tmpbuf = raw_packet->data;
GETMOREDATA:
	//ac->movitagpos;
	
	//offsettostart = file_tell(io);
	offsettostart = file_tell(stream);
	if (offsettostart <= ac->movitagpos)
		return STREAMSTART;

	offsettostart -= ac->movitagpos;
	
	if (offsettostart > SCAN_BUF_LEN)
	{
		file_seek_back(stream, -SCAN_BUF_LEN);
		read_data(stream, tmpbuf, SCAN_BUF_LEN);
		end = SCAN_BUF_LEN;
		file_seek_back(stream, -SCAN_BUF_LEN);
	}
	else
	{
		file_seek_back(stream, -offsettostart);
		read_data(stream, tmpbuf, offsettostart);
		end = offsettostart;
		flag_start = 1;
		file_seek_back(stream, -offsettostart);
	}
	
	//seek -(5k or less 5k)	
	tmpbuf[end + 0] =  paddbytes[0];
	tmpbuf[end + 1] =  paddbytes[1];	
	tmpbuf[end + 2] =  paddbytes[2];
	ptr = tmpbuf;
	buf_remain = end + 3;
				
	while(1)
	{
		if (buf_remain <= 3){
			paddbytes[0] = tmpbuf[0];
			paddbytes[1] = tmpbuf[1];
			paddbytes[2] = tmpbuf[2];			
			//reget buf;
			if (frame_num)
				goto SACN_END;
			
			if (flag_start)
				return STREAMSTART;
			
			goto GETMOREDATA;
		}
			
		if (*ptr != 0x30){	//30 is '0'
			ptr++;
			buf_remain--;
			continue;
		}
			
		if (buf_remain > 3)
			tag = ptr[0] + (ptr[1]<<8) + (ptr[2]<<16) + (ptr[3]<<24);
		else
			continue;
		
		switch (tag)
		{
			case 0x63643030:
				frame_arr[frame_num].pos = file_tell(stream) + (ptr - (unsigned char *)tmpbuf);
				
				if (frame_num >= 10)
					return NORMAL;
				frame_num++;
			
				if (buf_remain > 7){
					size = ptr[4] + (ptr[5]<<8) + (ptr[6]<<16) + (ptr[7]<<24);							
					if ((buf_remain - 8) >= size){
						ptr += (8 + size);
						buf_remain -= (8 + size);						
					}
					else
						buf_remain = 0;
				}
				else{
						ptr += 4;
						buf_remain -=4;
					}
				
			break;
			default:
				ptr++;
				buf_remain--;
			    break;
			
		}
	}

SACN_END:
	if (frame_num > 0)
	{
		int i;
		//get frameth
		for(i = 0; i < frame_num;i++)
		{
			frame_arr[i].frameth = ac->framecounter - (frame_num - i);
			//save to index
		}
		ac->framecounter = frame_arr[frame_num - 1].frameth;
		file_seek_set(stream, frame_arr[frame_num - 1].pos);
		//printf("b pos: 0x%x\n", frame_arr[frame_num - 1].pos);
		raw_packet->pts  = ((ac->framecounter * TIMESCALE * 10) / ac->framerate )/ 10;
	}
	
	return NORMAL;
	
}

