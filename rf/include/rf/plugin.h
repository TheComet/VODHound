#pragma once

#include "rf/config.h"

C_BEGIN

struct video_player;

struct video_player_interface
{
    /*!
     * \brief Open a video file and decode the first frame. Video
     * player should pause.
     * \note ReFramed will guarantee that this function won't be called
     * twice in a row. close() will always be called first if necessary.
     */
    int (*open_file)(struct video_player* player, const char* file_name);
    
    /*!
     * \brief Close the video. Player should reset everything.
     * \note ReFramed will guarantee that this function won't be called
     * twice in a row.
     */
    void (*close)(struct video_player* player);
    
    /*!
     * \brief Return true if a video is currently open. If the video is closed,
     * then this should return false.
     */
    int (*is_open)(struct video_player* player);
    
    /*!
     * \brief Begin normal playback of the video stream.
     */
    void (*play)(struct video_player* player);
    
    /*!
     * \brief Pause the video stream.
     */
    void (*pause)(struct video_player* player);
    
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
    void (*step)(struct video_player* player, int frames);
    
    void (*seek)(struct video_player* player, int num, int den);
    
    /*!
     * \brief Return true if the video is currently playing, otherwise false.
     */
    int (*is_playing)(struct video_player* player);
    
    
    
    /*!
     * \brief Set the volume in percent.
     */
    void (*set_volume)(struct video_player* player, int percent);
};

struct plugin;

struct plugin_info
{
    const char* name;
    const char* category;
    const char* author;
    const char* contact;
    const char* description;
};

struct plugin_factory
{
    struct plugin* (*create)(void);
    void (*destroy)(struct plugin* plugin);
    struct plugin_info info;
};

struct plugin_interface
{
    uint32_t version;
    struct plugin_factory* factories;
    int (*start)(uint32_t version);
    void (*stop)(void);
};

C_END
