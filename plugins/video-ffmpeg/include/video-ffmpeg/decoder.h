#include <stdint.h>

typedef struct AVFormatContext AVFormatContext;
typedef struct AVCodecContext AVCodecContext;
typedef struct SwsContext SwsContext;
typedef struct AVPacket AVPacket;
typedef struct AVFrame AVFrame;

struct decoder
{
    AVFormatContext* input_ctx;
    AVCodecContext* vcodec_ctx;
    SwsContext* reformat_ctx;
    AVPacket* current_packet;
    AVFrame* current_frame;
    AVFrame* current_frame_rgba;

    int vstream_idx, astream_idx;
};

int
decoder_open_file(struct decoder* decoder, const char* file_name, int pause);

void
decoder_close(struct decoder* decoder);

int
decoder_seek_near_keyframe(struct decoder* decoder, int64_t target_ts);

int
decode_next_frame(struct decoder* decoder);

const void*
decoder_rgb24_data(const struct decoder* decoder);
