#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vp_engine.h"
#include "avi_demuxer.h"
#include "adpcm.h"
#include "video_mem.h"

int read_data(void *stream, void *buf, unsigned int len)
{
	int rdnum;
	
	rdnum = stream_read(stream, buf, len); 
	
	return rdnum;
}

int read_skip(void *stream, int len)
{
	stream_seek(stream, len, SEEK_DIR_CUR);
	return 0;
}

int file_seek(void *stream, int len, int orig)
{
	stream_seek(stream, len, orig);
	return 0;
}

int file_seek_back(void *stream, int len)
{
	stream_seek(stream, len, SEEK_DIR_CUR);
	return 0;
}

int file_seek_set(void *stream, int len)
{
	stream_seek(stream, len, SEEK_DIR_BEG);
	return 0;
}

int file_tell(void *stream)
{
	return stream_tell(stream);
}

static int set_app_info(void *fhandle, void *media_info)
{
	avi_contex_t *avi_handle; 
	ve_media_info_t *ve_info;
		
	if ((fhandle == NULL)||(media_info == NULL))
	{
		return MEMERROR;
	}
	
	avi_handle = (avi_contex_t *)fhandle;
	ve_info = (ve_media_info_t *)media_info;
	
	ve_info->video_info.width  = avi_handle->bmpheader.biWidth;
	ve_info->video_info.height = avi_handle->bmpheader.biHeight;
	ve_info->index_flag = avi_handle->index_flag;

	if (avi_handle->dwScale == 0)	
		ve_info->video_info.frame_rate = 15;
	else
		ve_info->video_info.frame_rate = avi_handle->dwRate / avi_handle->dwScale;

	avi_handle->framerate = ve_info->video_info.frame_rate;

	//printf("video framerate: %ld, dwRate: %ld, dwScale: %ld\n", avi_handle->framerate, avi_handle->dwRate, avi_handle->dwScale);

	ve_info->video_info.vtotal_time = ((avi_handle->TotalFrames * TIMESCALE * 10) / avi_handle->framerate )/10;

	
	//avi_handle->framenum = 0;
	//app.type = 
	//if (avi_handle->filetype == formtypeAVI)
	//	app.type = VIDEO_AVI;
	//if (avi_handle->filetype == formtypeAMV)
	//	app.type = VIDEO_AMV;
	
	//file_seek(avi_handle->io, avi_handle->movitagpos, IDSEEK_SET);

	return NORMAL;
}

static void *open(io_stream_t stream, ve_media_info_t *file_info)
{
	avi_contex_t *avi_handle;
	ve_media_info_t *ve_m_info;
	int rtvl;
	ve_m_info = (ve_media_info_t *)file_info;

	avi_handle = video_mem_malloc(sizeof(avi_contex_t));
	if (avi_handle == NULL)
	{
		return NULL;
	}
	
	memset(avi_handle, 0, sizeof(avi_contex_t));
	avi_handle->stream = stream;
	rtvl = read_header(avi_handle);
	if (rtvl != NORMAL)
	{
		video_mem_free(avi_handle);
		return NULL;
	}
	
	set_app_info(avi_handle, ve_m_info);
	
//default to amv
	//avi_handle->amvformat = 1;
/*
	ve_m_info->video_info.frame_rate
	ve_m_info->video_info.width		
	ve_m_info->video_info.height
	ve_m_info->video_info.video
	ve_m_info->video_info.video_bitrate
	ve_m_info->video_info.vtotal_time

	ve_m_info->audio_info.atotal_time
	ve_m_info->audio_info.audio
	ve_m_info->audio_info.audio_bitrate
	ve_m_info->audio_info.channels
	ve_m_info->audio_info.sample_rate
*/
	return avi_handle;
}

static int getframe(void *fhandle, demuxer_packet_t *de_pac)
{
	avi_packet_t packet_val;
	avi_packet_t *packet;	
	int rdnum;
	avi_contex_t *avi_handle;
	int rtval;
	int padding;
	wave_head_t *wave_h;
	
	if ((fhandle == NULL)||(de_pac == NULL))
	{
		//PRINT_LOG_ERROR("getframe err_1!", 0, 0);
		return EN_MEMERR;
	}
	
	if (de_pac->data == NULL)
	{
		//PRINT_LOG_ERROR("getframe err_2!", 0, 0);
		return EN_MEMERR;		
	}
	
	packet = &packet_val;
	
	packet->data = de_pac->data;
	avi_handle = (avi_contex_t *)fhandle;
	
	rtval = get_es_chunk(avi_handle, packet);

	if (rtval == MEMERROR)
	{
		return EN_MEMERR;
	}

	if (packet->data_len == 0 || rtval == STREAMEND)
	{
		return EN_FILEISEND;
	}
	
	de_pac->packet_type = UNKOWN_ES;
	
	if (packet->es_type == streamtypeVIDEO)
	{
		de_pac->packet_type = VIDEO_ES;
		avi_handle->framecounter++;
	}
	
	if (packet->es_type == streamtypeAUDIO)
	{
		de_pac->packet_type = AUDIO_ES;
	}
	
	if ((packet->es_type == ckidAVINEWINDEX)||(packet->es_type == ckidAMVEND))
	{
		return EN_FILEISEND;
	}
	
	if (de_pac->packet_type == UNKOWN_ES)
	{
		return EN_MEMERR;
	}
	
	padding = CHUNKPADD(packet->data_len, avi_handle->amvformat);
	
	if (de_pac->packet_type == AUDIO_ES)
	{
		if ((packet->data_len + padding + sizeof(wave_head_t) > MAX_PACKET_LEN))
		{
			printf("audio pkt too long!\n");
			return EN_MEMERR;
		}
	}
	
	if (de_pac->packet_type == AUDIO_ES)
	{
		wave_h = (wave_head_t *)(packet->data);
		wave_h->dwSamples_per_sec = avi_handle->dwSamples_per_sec;//avi_handle->waveheader.nSamplesPerSec;
		wave_h->wChannels = avi_handle->wChannels;//avi_handle->waveheader.nChannels;
		packet->data += sizeof(wave_head_t);
		if(avi_handle->amvformat == 0)
		{
			// avi audio stream
		    rdnum = read_data(avi_handle->stream, packet->data, 4);	
		    if (rdnum != 4)
		    {
		    	return EN_FILEISEND;
		    }
			packet->data += 4;			
			*((unsigned int *)packet->data) = (packet->data_len - 4)*2;
			packet->data += 4;
		    rdnum = read_data(avi_handle->stream, packet->data, packet->data_len + padding - 4);	
		    if (rdnum != (packet->data_len + padding - 4))
		    {
		    	return EN_FILEISEND;
		    }
			packet->data_len += 4;
		}
		else
		{
			// amv audio stream
	    	rdnum = read_data(avi_handle->stream, packet->data, packet->data_len + padding);	
	    	if (rdnum != (packet->data_len + padding))
	    	{
	    		return EN_FILEISEND;
	    	}
		}
	}

	de_pac->pts = packet->pts;

	if (de_pac->packet_type == AUDIO_ES)
	{
		packet->data_len += sizeof(wave_head_t);
	}
	
	de_pac->data_len = packet->data_len;

	return EN_NORMAL;
}

static int dispose(void *fhandle)
{
	if (fhandle)
	{
		video_mem_free(fhandle);
	}

	return 0;
}

static int dem_seek(void *fhandle, seek_info_t *seek_info)
{
	avi_contex_t *avi_handle;
	avi_packet_t packet;
	int rtval;
	int padding;
	int curr_index_offset;
	int relation_offset;
	int data_offset;
	unsigned int frame_num;
	
	if ((fhandle == NULL)||(seek_info == NULL))
	{
		return EN_MEMERR;
	}
	
	avi_handle = (avi_contex_t *)fhandle;
	
#if 0
	seek_info.seek_cmd  = GET_CONT_INFO;
	seek_info.curpos    = puserparam->pbreakinfor->bpbyte;
	seek_info.curframes = puserparam->pbreakinfor->framecnt;
	seek_info.curtime   = puserparam->pbreakinfor->time;
#endif

	switch(seek_info->seek_cmd)
	{
		case GET_CONT_INFO:
			avi_handle->framecounter = seek_info->curframes;
			if (seek_info->curpos != 0)
			{
				file_seek(avi_handle->stream, seek_info->curpos, SEEK_DIR_BEG);
			}
			break;
			
		case FAST_FORWORD:
			if(avi_handle->index_flag ||(avi_handle->amvformat == 0))
			{
				if((seek_info->curframes+avi_handle->framecounter) >=  avi_handle->TotalFrames)
				{
					return EN_FILEISEND;					
//					seek_info->curframes = avi_handle->TotalFrames - avi_handle->framecounter;
				}
				curr_index_offset = avi_handle->index_offset +8+((seek_info->curframes+avi_handle->framecounter)<<5);
				file_seek_set(avi_handle->stream, (curr_index_offset+8));
				relation_offset = read4bytes(avi_handle->stream);
				data_offset = relation_offset + avi_handle->movitagpos;
				file_seek_set(avi_handle->stream, data_offset-4);			
				avi_handle->framecounter += seek_info->curframes;
				packet.pts  = ((avi_handle->framecounter * TIMESCALE * 10) / avi_handle->framerate)/10;
			}
			else
			{
				packet.data_len = 0;
				packet.es_type  = -1;
				for(;;)
				{
					rtval = get_es_chunk(avi_handle, &packet);
					if (rtval == STREAMEND)
					{
						return EN_FILEISEND;
					}
				
					padding = CHUNKPADD(packet.data_len, avi_handle->amvformat);
					read_skip(avi_handle->stream, packet.data_len + padding);
				
					if (packet.es_type  == streamtypeVIDEO)
						break;
				}
				
				avi_handle->framecounter++;
				
				//skip audio chunck			
				rtval = get_es_chunk(avi_handle, &packet);
				if (rtval == STREAMEND)
				{
					return EN_FILEISEND;
				}
				
				padding = CHUNKPADD(packet.data_len, avi_handle->amvformat);
				read_skip(avi_handle->stream, packet.data_len + padding);
				//printf("f ts: %d\n", packet.pts);
			}
			break;
			
		case FAST_BACK:
			if(avi_handle->index_flag ||(avi_handle->amvformat == 0))
			{
				if(avi_handle->framecounter <= seek_info->curframes)
				{
					return EN_FILESTARTPOS;
				}
				curr_index_offset = avi_handle->index_offset +8+((avi_handle->framecounter - seek_info->curframes )<<5);
				file_seek_set(avi_handle->stream, (curr_index_offset+8));
				relation_offset = read4bytes(avi_handle->stream);
				data_offset = relation_offset + avi_handle->movitagpos;
				file_seek_set(avi_handle->stream, data_offset-4);			
				avi_handle->framecounter -= seek_info->curframes;
				packet.pts  = ((avi_handle->framecounter * TIMESCALE * 10) / avi_handle->framerate)/10;
			}
			else
			{
				packet.data = seek_info->searchbuf;
				rtval = search_es_chunk(avi_handle, &packet);
				if (rtval == STREAMSTART)
				{
					return EN_FILESTARTPOS;
				}
			}
			//printf("b ts: %d\n", packet.pts);
		
			break;

		case SEEK_TIME:
			frame_num = (seek_info->curtime *avi_handle->framerate*10/TIMESCALE)/10;
			printf("frame_num: %d, counter: %lu\n", frame_num, avi_handle->TotalFrames);
			if(frame_num > avi_handle->TotalFrames)
			{
				frame_num = avi_handle->TotalFrames;
			}
			curr_index_offset = avi_handle->index_offset +8+(frame_num<<5);
			file_seek_set(avi_handle->stream, (curr_index_offset+8));
			relation_offset = read4bytes(avi_handle->stream);
			data_offset = relation_offset + avi_handle->movitagpos;
			file_seek_set(avi_handle->stream, data_offset-4);			
			avi_handle->framecounter = frame_num;
			printf("seek: %d, tell: %d\n", data_offset-4, file_tell(avi_handle->stream));
			break;
			
		//search_es_chunk
		default:
			break;	
	}

	return 0;
}	

//support amv/avi	
demuxer_plugin_t demuxer_plugin = {
	"avi",
	open,
	dem_seek,
	getframe,
	dispose
};


demuxer_plugin_t * avi_api(void)   
{
	return &demuxer_plugin;
}


