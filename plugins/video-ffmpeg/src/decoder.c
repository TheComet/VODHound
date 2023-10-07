#include "video-ffmpeg/decoder.h"

#include "vh/log.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

int
decoder_open_file(struct decoder* decoder, const char* file_name, int pause)
{
    log_info("Opening file '%s'\n", file_name);

    int result;
    const AVCodec* vcodec;
    const AVCodec* acodec;

    /*
     * AVFormatContext holds the header information from the format(Container)
     * Allocating memory for this component
     * http://ffmpeg.org/doxygen/trunk/structAVFormatContext.html
     */
    decoder->input_ctx = avformat_alloc_context();
    if (decoder->input_ctx == NULL)
    {
        log_err("Failed to allocate avformat context\n");
        goto alloc_context_failed;
    }

    /*
     * Open the file and read its header.The codecs are not opened.
     * The function arguments are:
     * AVFormatContext (the component we allocated memory for),
     * url (filename), or some non-empty placeholder for when we use a custom IO
     * AVInputFormat (if you pass NULL it'll do the auto detect)
     * and AVDictionary (which are options to the demuxer)
     * http://ffmpeg.org/doxygen/trunk/group__lavf__decoding.html#ga31d601155e9035d5b0e7efedc894ee49
     */
    if ((result = avformat_open_input(&decoder->input_ctx, file_name, NULL, NULL)) != 0)
    {
        char buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(result, buf, AV_ERROR_MAX_STRING_SIZE);
        log_err("%s\n", buf);
        goto open_input_failed;
    }

    /*
     * read Packets from the Format to get stream information
     * this function populates pFormatContext->streams
     * (of size equals to pFormatContext->nb_streams)
     * the arguments are:
     * the AVFormatContext
     * and options contains options for codec corresponding to i-th stream.
     * On return each dictionary will be filled with options that were not found.
     * https://ffmpeg.org/doxygen/trunk/group__lavf__decoding.html#gad42172e27cddafb81096939783b157bb
     */
    if (avformat_find_stream_info(decoder->input_ctx, NULL) < 0)
    {
        log_err("Failed to find stream info\n");
        goto find_stream_info_failed;
    }

    vcodec = NULL;
    acodec = NULL;
    for (int i = 0; i < decoder->input_ctx->nb_streams; ++i)
    {
        AVStream* stream = decoder->input_ctx->streams[i];
        AVCodecParameters* params = stream->codecpar;
        const AVCodec* codec = avcodec_find_decoder(params->codec_id);
        if (codec == NULL)
        {
            log_err("Unsupported codec\n");
            continue;
        }

        switch (params->codec_type)
        {
            case AVMEDIA_TYPE_VIDEO: {
                decoder->vstream_idx = i;
                vcodec = codec;
            } break;

            case AVMEDIA_TYPE_AUDIO: {
                decoder->astream_idx = i;
                acodec = codec;
            } break;

            default: break;
        }
    }
    if (decoder->vstream_idx == -1)
    {
        log_err("Input does not contain a video stream\n");
        goto video_stream_not_found;
    }
    if (decoder->astream_idx == -1)
        log_note("Input does not contain an audio stream\n");

    /* https://ffmpeg.org/doxygen/trunk/structAVCodecContext.html */
    decoder->vcodec_ctx = avcodec_alloc_context3(vcodec);
    if (decoder->vcodec_ctx == NULL)
    {
        log_err("Failed to allocate video codec context\n");
        goto alloc_video_codec_context_failed;
    }

    /*
     * Fill the codec context based on the values from the supplied codec parameters
     * https://ffmpeg.org/doxygen/trunk/group__lavc__core.html#gac7b282f51540ca7a99416a3ba6ee0d16
     */
    if (avcodec_parameters_to_context(decoder->vcodec_ctx, decoder->input_ctx->streams[decoder->vstream_idx]->codecpar) < 0)
    {
        log_err("Failed to copy video codec params to video codec context\n");
        goto copy_video_codec_params_failed;
    }

    decoder->vcodec_ctx->pkt_timebase = decoder->input_ctx->streams[decoder->vstream_idx]->time_base;

    /* discard useless packets like 0 size packets in avi */
    decoder->input_ctx->streams[decoder->vstream_idx]->discard = AVDISCARD_DEFAULT;

    /*
     * Initialize the AVCodecContext to use the given AVCodec.
     * https://ffmpeg.org/doxygen/trunk/group__lavc__core.html#ga11f785a188d7d9df71621001465b0f1d
     */
    if (avcodec_open2(decoder->vcodec_ctx, vcodec, NULL) < 0)
    {
        log_err("Failed to open video codec\n");
        goto open_video_codec_failed;
    }

    decoder->current_packet = av_packet_alloc();
    if (decoder->current_packet == NULL)
    {
        log_err("Failed to allocated packet\n");
        goto alloc_packet_failed;
    }

    decoder->current_frame = av_frame_alloc();
    if (decoder->current_frame == NULL)
    {
        log_err("Failed to allocated frame\n");
        goto alloc_frame_failed;
    }

    /*sourceWidth_ = decoder->vcodec_ctx->width;
    sourceHeight_ = decoder->vcodec_ctx->height;*/

    log_info("Video stream initialized\n");

    return 0;

    alloc_frame_failed               : av_packet_free(&decoder->current_packet);
    alloc_packet_failed              : avcodec_close(decoder->vcodec_ctx);
    open_video_codec_failed          :
    copy_video_codec_params_failed   : avcodec_free_context(&decoder->vcodec_ctx);
    alloc_video_codec_context_failed :
    video_stream_not_found           :
    find_stream_info_failed          : avformat_close_input(&decoder->input_ctx);
    open_input_failed                : avformat_free_context(decoder->input_ctx);
    alloc_context_failed             : return -1;
}

void
decoder_close(struct decoder* decoder)
{
    log_info("Closing video stream\n");
    av_frame_free(&decoder->current_frame);
    av_packet_free(&decoder->current_packet);
    avcodec_close(decoder->vcodec_ctx);
    avcodec_free_context(&decoder->vcodec_ctx);
    avformat_close_input(&decoder->input_ctx);
    avformat_free_context(decoder->input_ctx);
}

// ----------------------------------------------------------------------------
int
decoder_seek_near_keyframe(struct decoder* decoder, int64_t target_ts)
{
    /*
     * AVSEEK_FLAG_BACKWARD should seek to the first keyframe that occurs
     * before the specific time_stamp, however, people online have said that
     * sometimes it will seek to a keyframe after the timestamp specified,
     * if the timestamp is one frame before the keyframe. To fix this, we
     * just subtract 1 more frame
     * https://stackoverflow.com/questions/20734814/ffmpeg-av-seek-frame-with-avseek-flag-any-causes-grey-screen
     */

    // Seek and decode frame
    int seek_result = av_seek_frame(decoder->input_ctx, decoder->vstream_idx, target_ts, AVSEEK_FLAG_BACKWARD);

    /*
     * Some files don't start with a keyframe (mp4's created by Nintendo Switch)
     * in which case the above seek will fail. Try again and seek to any frame.
     */
    if (seek_result < 0)
        seek_result = av_seek_frame(decoder->input_ctx, decoder->vstream_idx, target_ts, AVSEEK_FLAG_ANY);

    if (seek_result < 0)
    {
        char buf[AV_ERROR_MAX_STRING_SIZE];
        log_err("av_seek_frame failed: %s\n", av_make_error_string(buf, AV_ERROR_MAX_STRING_SIZE, seek_result));
        return -1;
    }

    avcodec_flush_buffers(decoder->vcodec_ctx);

    return 0;
}

// ----------------------------------------------------------------------------
int64_t
to_codec_timestamp(struct decoder* decoder, int64_t ts, int num, int den)
{
    AVRational from = av_make_q(num, den);
    AVRational to = decoder->input_ctx->streams[decoder->vstream_idx]->time_base;
    return av_rescale_q(ts, from, to);
}

// ----------------------------------------------------------------------------
int64_t
from_codec_timestamp(struct decoder* decoder, int64_t codec_ts, int num, int den)
{
    AVRational from = decoder->input_ctx->streams[decoder->vstream_idx]->time_base;
    AVRational to = av_make_q(num, den);
    return av_rescale_q(codec_ts, from, to);
}

// ----------------------------------------------------------------------------
void
framerate(struct decoder* decoder, int* num, int* den)
{
    AVRational r = decoder->input_ctx->streams[decoder->vstream_idx]->r_frame_rate;
    *num = r.num;
    *den = r.den;
}

// ----------------------------------------------------------------------------
int64_t
duration(struct decoder* decoder)
{
    int64_t duration = decoder->input_ctx->streams[decoder->vstream_idx]->duration;
    return duration >= 0 ? duration : 0;
}

// ----------------------------------------------------------------------------
int
decode_next_frame(struct decoder* decoder)
{
    int response;
    while (1)
    {
        response = av_read_frame(decoder->input_ctx, decoder->current_packet);
        if (response < 0)
        {
            if ((response == AVERROR_EOF || avio_feof(decoder->input_ctx->pb)))
                return -1;
            if (decoder->input_ctx->pb && decoder->input_ctx->pb->error)
                return -1;
            continue;
        }

        if (decoder->current_packet->stream_index == decoder->vstream_idx)
        {
            // Send packet to video decoder
            response = avcodec_send_packet(decoder->vcodec_ctx, decoder->current_packet);
            if (response < 0)
            {
                if ((response == AVERROR_EOF || avio_feof(decoder->input_ctx->pb)))
                    goto send_error;
                if (decoder->input_ctx->pb && decoder->input_ctx->pb->error)
                    goto send_error;
                goto send_need_next_pkt;
            }

            response = avcodec_receive_frame(decoder->vcodec_ctx, decoder->current_frame);
            if (response == AVERROR(EAGAIN))
                goto recv_need_next_pkt;
            else if (response == AVERROR_EOF)
                goto recv_eof;
            else if (response < 0)
            {
                log_err("Decoder error\n");
                goto recv_need_next_pkt;
            }

            // Frame is successfully decoded here

            av_frame_unref(decoder->current_frame);
            av_packet_unref(decoder->current_packet);
            break;

            recv_need_next_pkt      :
            send_need_next_pkt      : av_packet_unref(decoder->current_packet);
            continue;

            recv_eof                :
            alloc_frame_failed      :
            send_error              : av_packet_unref(decoder->current_packet);
            return -1;

            /*
            // Get a picture we can re-use from the freelist
            picEntry = picturePool_.take();
            if (picEntry == nullptr)
            {
                // Have to allocate a new entry
                picEntry = freeFrameEntries_.take();
                if (picEntry == nullptr)
                {
                    log_->error("AVDecoder::decodeNextPacket(): picturePool_.take(): Used up all free frame entries!");
                    goto alloc_picture_failed;
                }
                picEntry->frame = av_frame_alloc();

                // Allocates the raw image buffer and fills in the frame's data
                // pointers and line sizes.
                //
                // NOTE: The image buffer has to be manually freed with
                //       av_free(qEntry->frame.data[0]), as av_frame_unref()
                //       does not take care of this.
                av_image_alloc(
                    picEntry->frame->data,      // Data pointers to be filled in
                    picEntry->frame->linesize,  // linesizes for the image in dst_data to be filled in
                    sourceWidth_,
                    sourceHeight_,
                    AV_PIX_FMT_RGB24,
                    1                           // Alignment
                );
            }

            // Convert frame to RGB24 format
            videoScaleCtx_ = sws_getCachedContext(videoScaleCtx_,
                sourceWidth_, sourceHeight_, static_cast<AVPixelFormat>(frame->format),
                sourceWidth_, sourceHeight_, AV_PIX_FMT_RGB24,
                SWS_BILINEAR, nullptr, nullptr, nullptr);

            sws_scale(videoScaleCtx_,
                frame->data, frame->linesize, 0, sourceHeight_,
                picEntry->frame->data, picEntry->frame->linesize);

            // sws_scale() doesn't appear to copy over this data. We make use
            // of pts, width and height in later stages
            picEntry->frame->best_effort_timestamp = frame->best_effort_timestamp;
            picEntry->frame->pts                   = frame->pts;
            picEntry->frame->width                 = frame->width;
            picEntry->frame->height                = frame->height;

            videoQueue_.putFront(picEntry);

            av_frame_unref(frame);
            framePool_.put(frameEntry);
            av_packet_unref(decoder->current_packet);
            break;

            recv_need_next_pkt      : framePool_.put(frameEntry);
            send_need_next_pkt      : av_packet_unref(decoder->current_packet);
            continue;

            alloc_picture_failed    : av_frame_unref(frame);
            recv_eof                : framePool_.put(frameEntry);
            alloc_frame_failed      :
            send_error              : av_packet_unref(decoder->current_packet);
            return false;*/
        }
        else if (decoder->current_packet->stream_index == decoder->astream_idx)
        {

        }
        else
        {
            av_packet_unref(decoder->current_packet);
        }
    }

    return 0;
}

/*
// ----------------------------------------------------------------------------
void VideoDecoder::run()
{
    PROFILE(VideoDecoder, run);

    mutex_.lock();
    while (requestShutdown_ == false)
    {
        int expectedBackQueueCount = bufSize_ / 2;
        int expectedFrontQueueCount = (bufSize_ - 1) / 2;

        // Remove any excess entries from front and back queues and return them
        // to the freelist. This occurs when we move to the next or previous
        // frame.
        while (backQueue_.count() > expectedBackQueueCount)
        {
            FrameDequeEntry* entry = backQueue_.takeBack();
            av_frame_unref(entry->frame);
            frameFreeList_.put(entry);
        }
        while (frontQueue_.count() > expectedFrontQueueCount)
        {
            FrameDequeEntry* entry = frontQueue_.takeFront();
            av_frame_unref(entry->frame);
            frameFreeList_.put(entry);
        }

        out:
        if (requestShutdown_ == false)
            cond_.wait(&mutex_);
    }
    mutex_.unlock();
}*/
