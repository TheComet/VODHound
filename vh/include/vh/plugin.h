#pragma once

#include "vh/config.h"
#include <stdint.h>

C_BEGIN

struct db;
struct db_interface;
struct plugin_ctx;
typedef struct _GtkWidget GtkWidget;
typedef struct _GTypeModule GTypeModule;

struct ui_center_interface
{
    GtkWidget* (*create)(struct plugin_ctx* ctx);
    void (*destroy)(struct plugin_ctx* ctx, GtkWidget* view);
};

struct ui_pane_interface
{
    GtkWidget* (*create)(struct plugin_ctx* ctx);
    void (*destroy)(struct plugin_ctx* ctx, GtkWidget* view);
};

struct replay_interface
{
    void (*select)(struct plugin_ctx* ctx, const int* game_ids, int count);
    void (*clear)(struct plugin_ctx* ctx);
};

struct video_player_interface
{
    /*!
     * \brief Open a video file.
     * The video decoder is not expected to decode or display anything
     * in this function. VODHound will follow up with a call to seek()
     * after a successful open to jump to (typically) the beginning of the
     * game within the video.
     * \note VODHound will guarantee that this function won't be called
     * twice in a row. close() will always be called first if necessary.
     */
    int (*open_file)(struct plugin_ctx* ctx, const char* file_name);

    void (*set_game_start)(struct plugin_ctx* ctx, int64_t game_start_ts, int num, int den);

    /*!
     * \brief Close the video. Player should reset everything, but keep the
     * last decoded frame visible on the canvas.
     * \note VODHound will guarantee that this function won't be called
     * twice in a row.
     */
    void (*close)(struct plugin_ctx* ctx);

    /*!
     * \brief Clears the canvas to its default background color.
     * When a video is closed, the last frame displayed will persist on
     * the canvas. This is to avoid flickering when switching videos.
     */
    void (*clear)(struct plugin_ctx* ctx);

    /*!
     * \brief Return true if a video is currently open. If the video is closed,
     * then this should return false.
     */
    int (*is_open)(const struct plugin_ctx* ctx);

    /*!
     * \brief Begin normal playback of the video stream.
     */
    void (*play)(struct plugin_ctx* ctx);

    /*!
     * \brief Pause the video stream.
     */
    void (*pause)(struct plugin_ctx* ctx);

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
    void (*step)(struct plugin_ctx* ctx, int frames);

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
    int (*seek)(struct plugin_ctx* ctx, uint64_t offset, int num, int den);

    /*!
     * \brief Return true if the video is currently playing, otherwise false.
     */
    int (*is_playing)(const struct plugin_ctx* ctx);

    /*!
     * \brief Get the current video offset in units of num/den.
     */
    uint64_t (*offset)(const struct plugin_ctx* ctx, int num, int den);

    /*!
     * \brief Get the total video duration in units of num/den.
     */
    uint64_t (*duration)(const struct plugin_ctx* ctx, int num, int den);

    void (*dimensions)(const struct plugin_ctx* ctx, int* width, int* height);

    /*!
     * \brief Set the volume in percent.
     */
    void (*set_volume)(struct plugin_ctx* ctx, int percent);

    /*!
     * \brief Get the current volume in percent.
     */
    int (*volume)(const struct plugin_ctx* ctx);
};

struct plugin_info
{
    const char* name;
    const char* category;
    const char* author;
    const char* contact;
    const char* description;
};

struct plugin_interface
{
    uint32_t plugin_version;
    uint32_t vh_version;
    struct plugin_info* info;
    struct plugin_ctx* (*create)(GTypeModule* type_module, struct db_interface* dbi, struct db* db);
    void (*destroy)(GTypeModule* type_module, struct plugin_ctx* ctx);
    struct ui_center_interface* ui_center;
    struct ui_pane_interface* ui_pane;
    struct replay_interface* replays;
    struct video_player_interface* video;
};

C_END
