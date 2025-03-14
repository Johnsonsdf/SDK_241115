demo 功能：
	当前demo 实现的是从mic 录取数据，然后opus 编码后将编码后的数据保存到文件中的功能，客户可以参考设置，实现mic 采集数据编码然后上传数据的功能。
	
demo 操作：
	假设平台的存储介质是nor flash，fat文件系统根目录为/NOR:，录音数据想保存到/NOR:/record.opus，
	1、若接着USB上电后会自动进入U盘模式，按on/off键退出U盘模式
	2、串口输入指令app nc start /NOR:/record.opus
	3、进行录音
	4、串口输入指令app nc stop
	5、接上USB重启，进入U盘模式，查看录制的文件record.opus

demo 说明：
	t_media_processor *processor = &nc_processor;
	io_stream_t enc_out_stream = NULL;
	media_init_param_t init_param;
	media_player_t *player = NULL;

	memset(processor, 0, sizeof(t_media_processor));
	memset(&init_param, 0, sizeof(init_param));

	enc_out_stream = open_file_stream(saved_url, MODE_OUT);
	if (!enc_out_stream) {
		SYS_LOG_ERR("stream open failed (%s)\n", saved_url);
		goto err_exit;
	}

	// 设置录音功能
	init_param.type = MEDIA_SRV_TYPE_CAPTURE;
	// 设置为录音功能类型，默认从mic采集数据
	init_param.stream_type = AUDIO_STREAM_AI;
	// 设置编码类型
	init_param.capture_format = OPUS_TYPE;
	// 设置mic数据采集采样率
	init_param.capture_sample_rate_input = 16;
	// 设置数据编码采样率
	init_param.capture_sample_rate_output = 16;
	// 设置数据位宽
	init_param.capture_sample_bits = 16;
	// 仅支持单声道数据编码
	init_param.capture_channels_input = 1;
	init_param.capture_channels_output = 1;
	//设置编码数据位宽
	init_param.capture_bit_rate = 16;
	init_param.capture_input_stream = NULL;
	//设置编码完成输出的数据流，demo输出到文件中，如果客户实现AI功能，可以用ringbuffer stream. 编码往ringbuffer stream中 写，
	// 数据上传线程从ring buffer stream 中读取并通过蓝牙上传。
	init_param.capture_output_stream = enc_out_stream;
	player = media_player_open(&init_param);
	if (!player) {
		SYS_LOG_ERR("media_player_open failed\n");
		goto err_exit;
	}

	media_player_play(player);
	processor->player = player;
	processor->enc_out_stream = enc_out_stream;
	return;

err_exit:
	if (enc_out_stream)
		close_stream(enc_out_stream);
}