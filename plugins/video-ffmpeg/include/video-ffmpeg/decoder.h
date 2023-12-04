#include <stdint.h>

#include <libavutil/rational.h>

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

/*!
 * Seeks to the first keyframe on or before the specified target timestamp.
 * The timestamp is in the codec's time base. @see decoder_time_base()
 */
int
decoder_seek_near_keyframe(struct decoder* decoder, int64_t target_ts);

/*!
 * Gets the offset of the current frame relative to the beginning of the
 * video stream.
 * The timestamp is in the codec's time base. @see decoder_time_base()
 */
int64_t
decoder_offset(const struct decoder* decoder);

/*!
 * Gets the video codec's frame rate.
 */
AVRational
decoder_frame_rate(const struct decoder* decoder);

/*!
 * Gets the video codec's time base.
 */
AVRational
decoder_time_base(const struct decoder* decoder);

int64_t
decoder_duration(const struct decoder* decoder);

int
decode_next_frame(struct decoder* decoder);

void
decoder_frame_size(const struct decoder* decoder, int* w, int* h);

const void*
decoder_rgb24_data(const struct decoder* decoder);
