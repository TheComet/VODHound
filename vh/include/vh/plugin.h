#pragma once

#include "vh/config.h"
#include <stdint.h>

C_BEGIN

struct plugin_ctx;

struct ui_interface
{
    void* (*create)(struct plugin_ctx* plugin);
    void (*destroy)(struct plugin_ctx* plugin, void* view);

    /* TODO: Debug only, will be removed in the future */
    void (*main)(struct plugin_ctx* plugin, void* view);
};

struct video_player_interface
{
    /*!
     * \brief Open a video file and decode the first frame. If pause is
     * true, then the video player should be in a paused state. Otherwise,
     * resume normal playback.
     * \note VODHound will guarantee that this function won't be called
     * twice in a row. close() will always be called first if necessary.
     */
    int (*open_file)(struct plugin_ctx* plugin, const char* file_name, int pause);
    
    /*!
     * \brief Close the video. Player should reset everything.
     * \note VODHound will guarantee that this function won't be called
     * twice in a row.
     */
    void (*close)(struct plugin_ctx* plugin);
    
    /*!
     * \brief Return true if a video is currently open. If the video is closed,
     * then this should return false.
     */
    int (*is_open)(struct plugin_ctx* plugin);
    
    /*!
     * \brief Begin normal playback of the video stream.
     */
    void (*play)(struct plugin_ctx* plugin);
    
    /*!
     * \brief Pause the video stream.
     */
    void (*pause)(struct plugin_ctx* plugin);
    
    /*!
     * \brief Advance by N number of video-frames (not game-frames).
     * \note N can be negative, which means to go backwards N frames.
     *
     * The video player should avoid seeking here if possible, but instead,
     * decode each successive frame as needed. In the case of advancing
     * backwards (negative value for N), the video player can seek
     * if required to buffer the previous frames, but typical implementations
     * will maintain a cache of decoded frames to avoid this if possible.
     *
     * \param frames The number of frames to seek. Can be negative. This
     * value is guaranteed to be "small", i.e. in the range of -30 to 30.
     */
    void (*step)(struct plugin_ctx* plugin, int frames);
    
    /*!
     * \brief Seek to a specific timestamp in the video.
     *
     * Seeking should not pause playback. If playback is paused, then the frame
     * seeked to should be decoded and displayed.
     *
     * The offset is specified in units of num/den. For example, if you want to
     * seek to 00:05 in the video, you could call seek(5, 1, 1), or
     * equivalently if you know the offset in "game frames" where the game is
     * running at 60 fps, seek(300, 1, 60) would achieve the same result.
     * \note The video offset does NOT correspond linearly with the game's
     * frame data. The most obvious example of why this is not true is if the
     * game is paused, the video will continue but there will be a large gap in
     * between the timestamps of the frames where the game was paused.
     */
    int (*seek)(struct plugin_ctx* plugin, uint64_t offset, int num, int den);
    
    /*!
     * \brief Get the current video offset in units of num/den.
     */
    uint64_t (*offset)(struct plugin_ctx* plugin, int num, int den);
    
    /*!
     * \brief Get the total video duration in units of num/den.
     */
    uint64_t (*duration)(struct plugin_ctx* plugin, int num, int den);
    
    /*!
     * \brief Return true if the video is currently playing, otherwise false.
     */
    int (*is_playing)(struct plugin_ctx* plugin);
    
    /*!
     * \brief Set the volume in percent.
     */
    void (*set_volume)(struct plugin_ctx* plugin, int percent);
    
    /*!
     * \brief Get the current volume in percent.
     */
    int (*volume)(struct plugin_ctx* plugin);
};

struct plugin_interface
{
    uint32_t plugin_version;
    uint32_t vh_version;
    struct plugin_ctx* (*create)(void);
    void (*destroy)(struct plugin_ctx* plugin);
    struct ui_interface* ui;
    struct video_player_interface* video;
    const char* name;
    const char* category;
    const char* author;
    const char* contact;
    const char* description;
};

C_END
