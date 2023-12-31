#include "video-ffmpeg/decoder.h"

#include "vh/log.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

int
decoder_open_file(struct decoder* decoder, const char* file_name)
{
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
        log_err("Failed to open file '%s': Failed to allocate avformat context\n", file_name);
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
        log_err("Failed to open file '%s': %s\n", file_name, buf);
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
        log_err("Failed to open file '%s': Failed to find stream info\n", file_name);
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
            log_err("Failed to open file '%s': Unsupported codec\n", file_name);
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
        log_err("Failed to open file '%s': Input does not contain a video stream\n", file_name);
        goto video_stream_not_found;
    }
    if (decoder->astream_idx == -1)
        log_note("Failed to open file '%s': Input does not contain an audio stream\n", file_name);

    /* https://ffmpeg.org/doxygen/trunk/structAVCodecContext.html */
    decoder->vcodec_ctx = avcodec_alloc_context3(vcodec);
    if (decoder->vcodec_ctx == NULL)
    {
        log_err("Failed to open file '%s': Failed to allocate video codec context\n", file_name);
        goto alloc_video_codec_context_failed;
    }

    /*
     * Fill the codec context based on the values from the supplied codec parameters
     * https://ffmpeg.org/doxygen/trunk/group__lavc__core.html#gac7b282f51540ca7a99416a3ba6ee0d16
     */
    if (avcodec_parameters_to_context(decoder->vcodec_ctx, decoder->input_ctx->streams[decoder->vstream_idx]->codecpar) < 0)
    {
        log_err("Failed to open file '%s': Failed to copy video codec params to video codec context\n", file_name);
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
        log_err("Failed to open file '%s': Failed to open video codec\n", file_name);
        goto open_video_codec_failed;
    }

    decoder->current_packet = av_packet_alloc();
    if (decoder->current_packet == NULL)
    {
        log_err("Failed to open file '%s': Failed to allocated packet\n", file_name);
        goto alloc_packet_failed;
    }

    decoder->current_frame = av_frame_alloc();
    if (decoder->current_frame == NULL)
    {
        log_err("Failed to open file '%s': Failed to allocated frame\n", file_name);
        goto alloc_frame_failed;
    }

    /*
     * Allocates the raw image buffer and fills in the frame's data
     * pointers and line sizes.
     *
     * NOTE #1: The image buffer has to be manually freed with
     *          av_free(qEntry->frame.data[0]), as av_frame_unref()
     *          does not take care of this.
     */
    decoder->current_frame_rgba = av_frame_alloc();
    if (decoder->current_frame_rgba == NULL)
    {
        log_err("Failed to open file '%s': Failed to allocate RGBA frame\n", file_name);
        goto alloc_rgba_frame_failed;
    }
    if (av_image_alloc(
        decoder->current_frame_rgba->data,      /* Data pointers to be filled in */
        decoder->current_frame_rgba->linesize,  /* linesizes for the image in dst_data to be filled in */
        decoder->vcodec_ctx->width,
        decoder->vcodec_ctx->height,
        AV_PIX_FMT_RGBA,
        1                                        /* Alignment */
    ) < 0)
    {
        log_err("Failed to open file '%s': Failed to allocate RGBA buffer\n", file_name);
        goto alloc_rgba_buffer_failed;
    }

    /*sourceWidth_ = decoder->vcodec_ctx->width;
    sourceHeight_ = decoder->vcodec_ctx->height;*/

    decoder->reformat_ctx = NULL;

    log_dbg("Opened video file '%s'\n", file_name);

    return 0;

    alloc_rgba_buffer_failed         : av_frame_free(&decoder->current_frame_rgba);
    alloc_rgba_frame_failed          : av_frame_free(&decoder->current_frame);
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
    log_dbg("Closing video stream\n");

    if (decoder->reformat_ctx)
    {
        sws_freeContext(decoder->reformat_ctx);
        decoder->reformat_ctx = NULL;
    }

    /* NOTE #1: This is the picture buffer we manually allocated */
    av_free(decoder->current_frame_rgba->data[0]);
    av_frame_free(&decoder->current_frame_rgba);

    av_frame_unref(decoder->current_frame);
    av_packet_unref(decoder->current_packet);
    av_frame_free(&decoder->current_frame);
    av_packet_free(&decoder->current_packet);
    avcodec_close(decoder->vcodec_ctx);
    avcodec_free_context(&decoder->vcodec_ctx);
    avformat_close_input(&decoder->input_ctx);
    avformat_free_context(decoder->input_ctx);

    decoder->current_frame_rgba = NULL;
}

int
decoder_is_open(const struct decoder* decoder)
{
    return decoder->current_frame_rgba != NULL;
}

// ----------------------------------------------------------------------------
int
decoder_seek_near_keyframe(struct decoder* decoder, int64_t target_ts)
{
    int seek_result;

    /*
     * AVSEEK_FLAG_BACKWARD should seek to the first keyframe that occurs
     * before the specific time_stamp, however, people online have said that
     * sometimes it will seek to a keyframe after the timestamp specified,
     * if the timestamp is one frame before the keyframe. To fix this, we
     * just subtract 1 more frame
     * https://stackoverflow.com/questions/20734814/ffmpeg-av-seek-frame-with-avseek-flag-any-causes-grey-screen
     */

    /* Seek and decode frame */
    seek_result = av_seek_frame(decoder->input_ctx, decoder->vstream_idx, target_ts, AVSEEK_FLAG_BACKWARD);

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

int64_t
decoder_offset(const struct decoder* decoder)
{
    return decoder->current_frame->pts;
}

AVRational
decoder_frame_rate(const struct decoder* decoder)
{
    return decoder->input_ctx->streams[decoder->vstream_idx]->r_frame_rate;
}

AVRational
decoder_time_base(const struct decoder* decoder)
{
    return decoder->input_ctx->streams[decoder->vstream_idx]->time_base;
}

int64_t
decoder_duration(const struct decoder* decoder)
{
    return decoder->input_ctx->streams[decoder->vstream_idx]->duration;
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

            /* Convert frame to RGB24 format */
            decoder->reformat_ctx = sws_getCachedContext(decoder->reformat_ctx,
                decoder->current_frame->width, decoder->current_frame->height, (enum AVPixelFormat)decoder->current_frame->format,
                decoder->current_frame->width, decoder->current_frame->height, AV_PIX_FMT_RGBA,
                SWS_POINT, NULL, NULL, NULL);

            sws_scale(decoder->reformat_ctx,
                (const uint8_t*const*)decoder->current_frame->data, decoder->current_frame->linesize, 0, decoder->vcodec_ctx->height,
                decoder->current_frame_rgba->data, decoder->current_frame_rgba->linesize);

            /*
             * sws_scale() doesn't appear to copy over this data. We make use
             * of pts, width and height in later stages
             */
            decoder->current_frame_rgba->best_effort_timestamp = decoder->current_frame->best_effort_timestamp;
            decoder->current_frame_rgba->pts                   = decoder->current_frame->pts;
            decoder->current_frame_rgba->width                 = decoder->current_frame->width;
            decoder->current_frame_rgba->height                = decoder->current_frame->height;

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
        /*else if (decoder->current_packet->stream_index == decoder->astream_idx)
        {

        }*/
        else
        {
            av_packet_unref(decoder->current_packet);
        }
    }

    return 0;
}

void
decoder_frame_size(const struct decoder* decoder, int* w, int* h)
{
    *w = decoder->vcodec_ctx->width;
    *h = decoder->vcodec_ctx->height;
}

const void*
decoder_rgb24_data(const struct decoder* decoder)
{
    return decoder->current_frame_rgba->data[0];
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
