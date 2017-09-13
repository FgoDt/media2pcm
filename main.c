#include <stdio.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <getopt.h>

typedef struct {
	char *ifile_path;
	char *ofile_path;
	int  obit;
	int  rate;
	int  channels;
	int  print;
}Data;

void printHelp()
{
	printf("audio2pcm use (-i ./tst.aac) to set in put file path\n \
	-i to set inputfile path\n \
	-o to set outputfile path\n \
	-c to set outputfile channel\n \
	-r to set outputfile samplerate\n \
	-b to set outputfile bitrate\n \
	-h see help \n");
}

int parsearg(int argc, char *argv[], Data *data)
{
	char arg = 'e';
	while((arg = getopt(argc,argv,"i:o:r:c:b:hp"))!=-1)
	{
		switch (arg){
			case 'i':
				data->ifile_path = optarg;
				break;
			case 'o':
				data->ofile_path = optarg;
				break;
			case 'r':
				data->rate = atoi(optarg);
				break;
			case 'c':
				data->channels = atoi(optarg);
				break;
			case 'b':
				data->obit = atoi(optarg);
				break;
			case 'h':
				printHelp();
				break;
			case 'p':
				data->print = 1;
				break;
			default:
				printHelp();
				break;
		}
	}
	return 0;
}

int decode(Data *data)
{

	AVFormatContext *pFormatCtx;
	AVCodecContext	*pCodecCtx;
	AVCodec			*pCodec;

	printf("try to decode pcm :\n \
		output file path:%s\n \
		output bit: %d\n \
		output sample rate: %d\n \
		output channels:%d\n ",data->ofile_path,data->obit,data->rate,data->channels);

	av_register_all();
	avformat_network_init();

	pFormatCtx = avformat_alloc_context();

	if(avformat_open_input(&pFormatCtx,data->ifile_path,NULL,NULL)!=0)
	{
		printf("Can not open input stream\n");
		return -1;
	}

	if(avformat_find_stream_info(pFormatCtx,NULL)<0)
	{
		printf("can not find stream info");
		return -1;
	}

	if(data->print>0)
	{
		av_dump_format(pFormatCtx,0,data->ifile_path,0);
	}

	int audiostream = -1;

	for(int i = 0;i<pFormatCtx->nb_streams;i++)
	{
		if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO){
			audiostream = i;
			break;
		}
	}

	if(audiostream == -1)
	{
		printf("no audio stream\n");
		return -1;
	}

	pCodecCtx = pFormatCtx->streams[audiostream]->codec;

	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if(pCodec == NULL)
	{
		printf("no codec \n");
		return -1;
	}

	if(avcodec_open2(pCodecCtx,pCodec,NULL)<0)
	{
		printf("Can not open codec\n");
		return -1;
	}


	FILE *pFile = NULL;

	pFile = fopen(data->ofile_path,"wb");

	AVPacket *packet = (AVPacket*)malloc(sizeof(AVPacket));
	
	av_init_packet(packet);

	enum AVSampleFormat out_fmt = AV_SAMPLE_FMT_U8;
	if(data->obit==16)
		out_fmt = AV_SAMPLE_FMT_S16;
	if(data->obit==32)
		out_fmt = AV_SAMPLE_FMT_S32;
	
	uint64_t out_channel_layout = av_get_default_channel_layout(data->channels);

	int out_frame_size = pCodecCtx->frame_size;
	int out_buffer_size = av_samples_get_buffer_size(NULL,
	data->channels,out_frame_size,out_fmt,1);

	uint8_t *out_buf = (uint8_t*)av_malloc(out_buffer_size*2);

	AVFrame *pFrame;
	pFrame = avcodec_alloc_frame();

	int64_t in_ch_layout = av_get_default_channel_layout(pCodecCtx->channels);
	
	struct SwrContext *au_convert_ctx;
	au_convert_ctx = swr_alloc();

	au_convert_ctx = swr_alloc_set_opts(au_convert_ctx,
	out_channel_layout,out_fmt,data->rate,in_ch_layout,
	pCodecCtx->sample_fmt,pCodecCtx->sample_rate,0,NULL);

	swr_init(au_convert_ctx);

	int get_audio=0;
	int ret = 0;
	while(av_read_frame(pFormatCtx, packet)>=0)
	{
		if(packet->stream_index == audiostream)
		{
			ret = avcodec_decode_audio4(pCodecCtx,pFrame,&get_audio,packet);
			if(ret<0){
				printf("decoding audio error\n");
				return -1;
			}

			if(get_audio>0){
				swr_convert(au_convert_ctx,&out_buf,out_buffer_size,
					(const uint8_t**)pFrame->data,pFrame->nb_samples);
				fwrite(out_buf,1,out_buffer_size,pFile);
			}
		}
		av_free_packet(packet);
	}

	swr_free(&au_convert_ctx);
	fclose(pFile);
	av_free(out_buf);
	avcodec_close(pCodecCtx);

	return 0;
}


int checkarg(Data *data)
{
	if(data->ifile_path == NULL)
	{
		printf("no input file use -i xx.xx to set input file,use -h to set help! \n");
		return -1;
	}
	if(data->ifile_path==NULL||strlen(data->ifile_path)==0)
	{
		printf("input file path error\n");
		return -1;
	}
	if(data->ofile_path==NULL || strlen(data->ofile_path)==0)
	{
		printf("output file path error\n");
		return -1;
	}
	if(data->obit!=8&&data->obit!=16&&data->obit!=32)
	{
		printf("out put bit must be 8 16  or 32!\n");
		return -1;
	}
}

int main(int argc, char **argv)
{
	Data data;
	data.ofile_path = "a.pcm";
	data.obit = 16;
	data.channels = 2;
	data.rate = 44100;
	data.print = -1;
	parsearg(argc,argv,&data);
	if(checkarg(&data)<0)
	{
		return -1;
	}

	decode(&data);
	return 0;
}

