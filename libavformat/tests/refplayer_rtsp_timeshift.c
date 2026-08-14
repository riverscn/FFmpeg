/*
 * RefPlayer RTSP time-shift profile parser tests
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <string.h>

#include "libavutil/avassert.h"
#include "libavutil/mathematics.h"
#include "libavformat/rtsp.h"

static void parse(RTSPMessageHeader *reply, RTSPState *rt, const char *line)
{
    ff_rtsp_parse_line(NULL, reply, line, rt, "PLAY");
}

static void test_hms(void)
{
    RTSPMessageHeader reply = { 0 };
    RTSPState rt = { 0 };

    parse(&reply, &rt, "Range: clock=0-");
    parse(&reply, &rt, "Timeshift-Status: 1");
    ff_rtsp_refplayer_update_timeshift(&rt, &reply);
    av_assert0(rt.refplayer_timeshift_profile == REFPLAYER_RTSP_TIMESHIFT_HMS);
    av_assert0(rt.refplayer_timeshift_range_kind == REFPLAYER_RTSP_RANGE_CLOCK);
    av_assert0(rt.refplayer_timeshift_horizon == INT64_C(604800) * AV_TIME_BASE);
}

static void test_hms_sdp_range_with_describe_status(void)
{
    RTSPMessageHeader reply = { 0 };
    RTSPState rt = { 0 };
    AVFormatContext format = { .priv_data = &rt };

    av_assert0(ff_sdp_parse(&format, "v=0\r\na=range:clock=0-\r\n") == 0);
    parse(&reply, &rt, "Timeshift-Status: 1");
    ff_rtsp_refplayer_update_timeshift(&rt, &reply);
    av_assert0(rt.refplayer_timeshift_profile == REFPLAYER_RTSP_TIMESHIFT_HMS);
    av_assert0(rt.refplayer_timeshift_range_kind == REFPLAYER_RTSP_RANGE_CLOCK);
    av_assert0(rt.refplayer_timeshift_horizon == INT64_C(604800) * AV_TIME_BASE);
}

static void test_duplicate_sdp_range_fails_closed(void)
{
    RTSPMessageHeader reply = { 0 };
    RTSPState rt = { 0 };
    AVFormatContext format = { .priv_data = &rt };

    av_assert0(ff_sdp_parse(&format,
               "v=0\r\na=range:clock=0-\r\na=range:clock=0-\r\n") == 0);
    parse(&reply, &rt, "Timeshift-Status: 1");
    ff_rtsp_refplayer_update_timeshift(&rt, &reply);
    av_assert0(rt.refplayer_timeshift_profile == REFPLAYER_RTSP_TIMESHIFT_NONE);
}

static void test_finite_clock(void)
{
    RTSPMessageHeader reply = { 0 };
    RTSPState rt = { 0 };

    parse(&reply, &rt,
          "Range: clock=20260812T110000Z-20260812T120000Z");
    ff_rtsp_refplayer_update_timeshift(&rt, &reply);
    av_assert0(rt.refplayer_timeshift_profile ==
               REFPLAYER_RTSP_TIMESHIFT_FINITE_RANGE);
    av_assert0(rt.refplayer_timeshift_range_kind == REFPLAYER_RTSP_RANGE_CLOCK);
    av_assert0(rt.refplayer_timeshift_range_end - rt.refplayer_timeshift_range_start
               == INT64_C(3600) * AV_TIME_BASE);
}

static void test_duplicate_range_fails_closed(void)
{
    RTSPMessageHeader reply = { 0 };
    RTSPState rt = { 0 };

    parse(&reply, &rt, "Range: clock=0-");
    parse(&reply, &rt, "Range: clock=0-");
    parse(&reply, &rt, "Timeshift-Status: 1");
    ff_rtsp_refplayer_update_timeshift(&rt, &reply);
    av_assert0(rt.refplayer_timeshift_profile == REFPLAYER_RTSP_TIMESHIFT_NONE);
}

static void test_3gpp_depth(void)
{
    RTSPMessageHeader reply = { 0 };
    RTSPState rt = { 0 };

    parse(&reply, &rt,
          "3GPP-TS-CurrentRecording-Time: clock=20260812T120000Z");
    parse(&reply, &rt, "3GPP-TS-Buffer: buffer-depth=3600");
    ff_rtsp_refplayer_update_timeshift(&rt, &reply);
    av_assert0(rt.refplayer_timeshift_profile == REFPLAYER_RTSP_TIMESHIFT_3GPP);
    av_assert0(rt.refplayer_timeshift_range_kind == REFPLAYER_RTSP_RANGE_CLOCK);
    av_assert0(rt.refplayer_timeshift_range_end - rt.refplayer_timeshift_range_start
               == INT64_C(3600) * AV_TIME_BASE);
}

static void test_3gpp_interval_and_depth(void)
{
    RTSPMessageHeader reply = { 0 };
    RTSPState rt = { 0 };

    parse(&reply, &rt, "3GPP-TS-CurrentRecording-Time: npt=600");
    parse(&reply, &rt,
          "3GPP-TS-Buffer: npt=100-;buffer-depth=300");
    ff_rtsp_refplayer_update_timeshift(&rt, &reply);
    av_assert0(rt.refplayer_timeshift_profile == REFPLAYER_RTSP_TIMESHIFT_3GPP);
    av_assert0(rt.refplayer_timeshift_range_kind == REFPLAYER_RTSP_RANGE_NPT);
    av_assert0(rt.refplayer_timeshift_range_start == INT64_C(300) * AV_TIME_BASE);
    av_assert0(rt.refplayer_timeshift_range_end == INT64_C(600) * AV_TIME_BASE);
}

int main(void)
{
    test_hms();
    test_hms_sdp_range_with_describe_status();
    test_duplicate_sdp_range_fails_closed();
    test_finite_clock();
    test_duplicate_range_fails_closed();
    test_3gpp_depth();
    test_3gpp_interval_and_depth();
    return 0;
}
