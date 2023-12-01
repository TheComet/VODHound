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
decoder_open_file(struct decoder* decoder, const char* file_name);

void
decoder_close(struct decoder* decoder);

int
decoder_is_open(const struct decoder* decoder);

int
decoder_seek_near_keyframe(struct decoder* decoder, int64_t target_ts, int num, int den);

uint64_t
decoder_offset(const struct decoder* decoder, int num, int den);

int
decode_next_frame(struct decoder* decoder);

void
decoder_frame_size(const struct decoder* decoder, int* w, int* h);

const void*
decoder_rgb24_data(const struct decoder* decoder);
