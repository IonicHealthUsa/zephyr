/** @file
 *  @brief Bluetooth LC3 codec handling
 */

/*
 * Copyright (c) 2020 Intel Corporation
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_LC3_H_
#define ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_LC3_H_

/**
 * @brief LC3
 * @defgroup BT_AUDIO_CODEC_LC3 AUDIO
 * @ingroup bluetooth
 * @{
 */

#include <zephyr/sys/util_macro.h>
#include <zephyr/bluetooth/byteorder.h>
#include <zephyr/bluetooth/hci_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Codec capability types
 *
 * Used to build and parse codec capabilities as specified in the PAC specification.
 * Source is assigned numbers for Generic Audio, bluetooth.com.
 */
enum bt_audio_codec_cap_type {
	/** Supported sampling frequencies */
	BT_AUDIO_CODEC_CAP_TYPE_FREQ = 0x01,

	/** Supported frame durations */
	BT_AUDIO_CODEC_CAP_TYPE_DURATION = 0x02,

	/** Supported audio channel counts */
	BT_AUDIO_CODEC_CAP_TYPE_CHAN_COUNT = 0x03,

	/** Supported octets per codec frame */
	BT_AUDIO_CODEC_CAP_TYPE_FRAME_LEN = 0x04,

	/** Supported maximum codec frames per SDU  */
	BT_AUDIO_CODEC_CAP_TYPE_FRAME_COUNT = 0x05,
};

/** @brief Supported frequencies bitfield */
enum bt_audio_codec_cap_freq {
	/** 8 Khz sampling frequency */
	BT_AUDIO_CODEC_CAP_FREQ_8KHZ = BIT(0),

	/** 11.025 Khz sampling frequency */
	BT_AUDIO_CODEC_CAP_FREQ_11KHZ = BIT(1),

	/** 16 Khz sampling frequency */
	BT_AUDIO_CODEC_CAP_FREQ_16KHZ = BIT(2),

	/** 22.05 Khz sampling frequency */
	BT_AUDIO_CODEC_CAP_FREQ_22KHZ = BIT(3),

	/** 24 Khz sampling frequency */
	BT_AUDIO_CODEC_CAP_FREQ_24KHZ = BIT(4),

	/** 32 Khz sampling frequency */
	BT_AUDIO_CODEC_CAP_FREQ_32KHZ = BIT(5),

	/** 44.1 Khz sampling frequency */
	BT_AUDIO_CODEC_CAP_FREQ_44KHZ = BIT(6),

	/** 48 Khz sampling frequency */
	BT_AUDIO_CODEC_CAP_FREQ_48KHZ = BIT(7),

	/** 88.2 Khz sampling frequency */
	BT_AUDIO_CODEC_CAP_FREQ_88KHZ = BIT(8),

	/** 96 Khz sampling frequency */
	BT_AUDIO_CODEC_CAP_FREQ_96KHZ = BIT(9),

	/** 176.4 Khz sampling frequency */
	BT_AUDIO_CODEC_CAP_FREQ_176KHZ = BIT(10),

	/** 192 Khz sampling frequency */
	BT_AUDIO_CODEC_CAP_FREQ_192KHZ = BIT(11),

	/** 384 Khz sampling frequency */
	BT_AUDIO_CODEC_CAP_FREQ_384KHZ = BIT(12),

	/** Any frequency capability */
	BT_AUDIO_CODEC_CAP_FREQ_ANY =
		(BT_AUDIO_CODEC_CAP_FREQ_8KHZ | BT_AUDIO_CODEC_CAP_FREQ_11KHZ |
		 BT_AUDIO_CODEC_CAP_FREQ_16KHZ | BT_AUDIO_CODEC_CAP_FREQ_22KHZ |
		 BT_AUDIO_CODEC_CAP_FREQ_24KHZ | BT_AUDIO_CODEC_CAP_FREQ_32KHZ |
		 BT_AUDIO_CODEC_CAP_FREQ_44KHZ | BT_AUDIO_CODEC_CAP_FREQ_48KHZ |
		 BT_AUDIO_CODEC_CAP_FREQ_88KHZ | BT_AUDIO_CODEC_CAP_FREQ_96KHZ |
		 BT_AUDIO_CODEC_CAP_FREQ_176KHZ | BT_AUDIO_CODEC_CAP_FREQ_192KHZ |
		 BT_AUDIO_CODEC_CAP_FREQ_384KHZ),
};

/** @brief Supported frame durations bitfield */
enum bt_audio_codec_cap_frame_dur {
	/** 7.5 msec frame duration capability */
	BT_AUDIO_CODEC_CAP_DURATION_7_5 = BIT(0),

	/** 10 msec frame duration capability */
	BT_AUDIO_CODEC_CAP_DURATION_10 = BIT(1),

	/** Any frame duration capability */
	BT_AUDIO_CODEC_CAP_DURATION_ANY =
		(BT_AUDIO_CODEC_CAP_DURATION_7_5 | BT_AUDIO_CODEC_CAP_DURATION_10),

	/**
	 * @brief 7.5 msec preferred frame duration capability.
	 *
	 * This shall only be set if @ref BT_AUDIO_CODEC_CAP_DURATION_7_5 is also set, and if @ref
	 * BT_AUDIO_CODEC_CAP_DURATION_PREFER_10 is not set.
	 */
	BT_AUDIO_CODEC_CAP_DURATION_PREFER_7_5 = BIT(4),

	/**
	 * @brief 10 msec preferred frame duration capability
	 *
	 * This shall only be set if @ref BT_AUDIO_CODEC_CAP_DURATION_10 is also set, and if @ref
	 * BT_AUDIO_CODEC_CAP_DURATION_PREFER_7_5 is not set.
	 */
	BT_AUDIO_CODEC_CAP_DURATION_PREFER_10 = BIT(5),
};

enum bt_audio_codec_cap_chan_count {
	/** Supporting 1 channel */
	BT_AUDIO_CODEC_CAP_CHAN_COUNT_1 = BIT(0),

	/** Supporting 2 channel */
	BT_AUDIO_CODEC_CAP_CHAN_COUNT_2 = BIT(1),

	/** Supporting 3 channel */
	BT_AUDIO_CODEC_CAP_CHAN_COUNT_3 = BIT(2),

	/** Supporting 4 channel */
	BT_AUDIO_CODEC_CAP_CHAN_COUNT_4 = BIT(3),

	/** Supporting 5 channel */
	BT_AUDIO_CODEC_CAP_CHAN_COUNT_5 = BIT(4),

	/** Supporting 6 channel */
	BT_AUDIO_CODEC_CAP_CHAN_COUNT_6 = BIT(5),

	/** Supporting 7 channel */
	BT_AUDIO_CODEC_CAP_CHAN_COUNT_7 = BIT(6),

	/** Supporting 8 channel */
	BT_AUDIO_CODEC_CAP_CHAN_COUNT_8 = BIT(7),

	/** Supporting all channels */
	BT_AUDIO_CODEC_CAP_CHAN_COUNT_ANY =
		(BT_AUDIO_CODEC_CAP_CHAN_COUNT_1 | BT_AUDIO_CODEC_CAP_CHAN_COUNT_2 |
		 BT_AUDIO_CODEC_CAP_CHAN_COUNT_3 | BT_AUDIO_CODEC_CAP_CHAN_COUNT_4 |
		 BT_AUDIO_CODEC_CAP_CHAN_COUNT_5 | BT_AUDIO_CODEC_CAP_CHAN_COUNT_6 |
		 BT_AUDIO_CODEC_CAP_CHAN_COUNT_7 | BT_AUDIO_CODEC_CAP_CHAN_COUNT_8),
};

/** Minimum supported channel counts */
#define BT_AUDIO_CODEC_CAP_CHAN_COUNT_MIN 1
/** Maximum supported channel counts */
#define BT_AUDIO_CODEC_CAP_CHAN_COUNT_MAX 8

/**
 *  @brief Channel count support capability
 *
 *  Macro accepts variable number of channel counts.
 *  The allowed channel counts are defined by specification and have to be in range from
 *  @ref BT_AUDIO_CODEC_CAP_CHAN_COUNT_MIN to @ref BT_AUDIO_CODEC_CAP_CHAN_COUNT_MAX inclusive.
 *
 *  Example to support 1 and 3 channels:
 *    BT_AUDIO_CODEC_CAP_CHAN_COUNT_SUPPORT(1, 3)
 */
#define BT_AUDIO_CODEC_CAP_CHAN_COUNT_SUPPORT(...)                                                 \
	((enum bt_audio_codec_cap_chan_count)((FOR_EACH(BIT, (|), __VA_ARGS__)) >> 1))

struct bt_audio_codec_octets_per_codec_frame {
	/** Minimum number of octets supported per codec frame */
	uint16_t min;
	/** Maximum number of octets supported per codec frame */
	uint16_t max;
};

/**
 * @brief Codec configuration types
 *
 * Used to build and parse codec configurations as specified in the ASCS and BAP specifications.
 * Source is assigned numbers for Generic Audio, bluetooth.com.
 */
enum bt_audio_codec_cfg_type {
	/** Sampling frequency */
	BT_AUDIO_CODEC_CFG_FREQ = 0x01,

	/** Frame duration */
	BT_AUDIO_CODEC_CFG_DURATION = 0x02,

	/** Audio channel allocation */
	BT_AUDIO_CODEC_CFG_CHAN_ALLOC = 0x03,

	/** Octets per codec frame */
	BT_AUDIO_CODEC_CFG_FRAME_LEN = 0x04,

	/** Codec frame blocks per SDU */
	BT_AUDIO_CODEC_CFG_FRAME_BLKS_PER_SDU = 0x05,
};

enum bt_audio_codec_cfg_freq {
	/** 8 Khz codec sampling frequency */
	BT_AUDIO_CODEC_CFG_FREQ_8KHZ = 0x01,

	/** 11.025 Khz codec sampling frequency */
	BT_AUDIO_CODEC_CFG_FREQ_11KHZ = 0x02,

	/** 16 Khz codec sampling frequency */
	BT_AUDIO_CODEC_CFG_FREQ_16KHZ = 0x03,

	/** 22.05 Khz codec sampling frequency */
	BT_AUDIO_CODEC_CFG_FREQ_22KHZ = 0x04,

	/** 24 Khz codec sampling frequency */
	BT_AUDIO_CODEC_CFG_FREQ_24KHZ = 0x05,

	/** 32 Khz codec sampling frequency */
	BT_AUDIO_CODEC_CFG_FREQ_32KHZ = 0x06,

	/** 44.1 Khz codec sampling frequency */
	BT_AUDIO_CODEC_CFG_FREQ_44KHZ = 0x07,

	/** 48 Khz codec sampling frequency */
	BT_AUDIO_CODEC_CFG_FREQ_48KHZ = 0x08,

	/** 88.2 Khz codec sampling frequency */
	BT_AUDIO_CODEC_CFG_FREQ_88KHZ = 0x09,

	/** 96 Khz codec sampling frequency */
	BT_AUDIO_CODEC_CFG_FREQ_96KHZ = 0x0a,

	/** 176.4 Khz codec sampling frequency */
	BT_AUDIO_CODEC_CFG_FREQ_176KHZ = 0x0b,

	/** 192 Khz codec sampling frequency */
	BT_AUDIO_CODEC_CFG_FREQ_192KHZ = 0x0c,

	/** 384 Khz codec sampling frequency */
	BT_AUDIO_CODEC_CFG_FREQ_384KHZ = 0x0d,
};

enum bt_audio_codec_cfg_frame_dur {
	/** 7.5 msec Frame Duration configuration */
	BT_AUDIO_CODEC_CFG_DURATION_7_5 = 0x00,

	/** 10 msec Frame Duration configuration */
	BT_AUDIO_CODEC_CFG_DURATION_10 = 0x01,
};

/**
 *  @brief Helper to declare LC3 codec capability
 *
 *  ``_max_frames_per_sdu`` value is optional and will be included only if != 1
 */
/* COND_CODE_1 is used to omit an LTV entry in case the _frames_per_sdu is 1.
 * COND_CODE_1 will evaluate to second argument if the flag parameter(first argument) is 1
 * - removing one layer of paranteses.
 * If the flags argument is != 1 it will evaluate to the third argument which inserts a LTV
 * entry for the max_frames_per_sdu value.
 */

#define BT_AUDIO_CODEC_CAP_LC3_DATA(_freq, _duration, _chan_count, _len_min, _len_max,             \
				    _max_frames_per_sdu)                                           \
	{                                                                                          \
		BT_AUDIO_CODEC_DATA(BT_AUDIO_CODEC_CAP_TYPE_FREQ, BT_BYTES_LIST_LE16(_freq)),      \
		BT_AUDIO_CODEC_DATA(BT_AUDIO_CODEC_CAP_TYPE_DURATION, (_duration)),                \
		BT_AUDIO_CODEC_DATA(BT_AUDIO_CODEC_CAP_TYPE_CHAN_COUNT, (_chan_count)),            \
		BT_AUDIO_CODEC_DATA(BT_AUDIO_CODEC_CAP_TYPE_FRAME_LEN,                             \
				    BT_BYTES_LIST_LE16(_len_min),                                  \
				    BT_BYTES_LIST_LE16(_len_max)),                                 \
		COND_CODE_1(_max_frames_per_sdu, (),                                               \
			    (BT_AUDIO_CODEC_DATA(BT_AUDIO_CODEC_CAP_TYPE_FRAME_COUNT,              \
						 (_max_frames_per_sdu))))                          \
	}

/**
 *  @brief Helper to declare LC3 codec metadata
 */
#define BT_AUDIO_CODEC_CAP_LC3_META(_prefer_context)                                               \
	{                                                                                          \
		BT_AUDIO_CODEC_DATA(BT_AUDIO_METADATA_TYPE_PREF_CONTEXT,                           \
				    BT_BYTES_LIST_LE16(_prefer_context))                           \
	}

/**
 * @brief Helper to declare LC3 codec
 *
 * @param _freq Supported Sampling Frequencies bitfield (see ``BT_AUDIO_CODEC_CAP_FREQ_*``)
 * @param _duration Supported Frame Durations bitfield (see ``BT_AUDIO_CODEC_CAP_DURATION_*``)
 * @param _chan_count Supported channels (see @ref BT_AUDIO_CODEC_CAP_CHAN_COUNT_SUPPORT)
 * @param _len_min Minimum number of octets supported per codec frame
 * @param _len_max Maximum number of octets supported per codec frame
 * @param _max_frames_per_sdu Supported maximum codec frames per SDU
 * @param _prefer_context Preferred contexts (@ref bt_audio_context)
 *
 */
#define BT_AUDIO_CODEC_CAP_LC3(_freq, _duration, _chan_count, _len_min, _len_max,                  \
			       _max_frames_per_sdu, _prefer_context)                               \
	BT_AUDIO_CODEC_CAP(BT_HCI_CODING_FORMAT_LC3, 0x0000, 0x0000,                               \
			   BT_AUDIO_CODEC_CAP_LC3_DATA(_freq, _duration, _chan_count, _len_min,    \
						       _len_max, _max_frames_per_sdu),             \
			   BT_AUDIO_CODEC_CAP_LC3_META(_prefer_context))

/**
 *  @brief Helper to declare LC3 codec data configuration
 *
 *  @param _freq            Sampling frequency (``BT_AUDIO_CODEC_CFG_FREQ_*``)
 *  @param _duration        Frame duration (``BT_AUDIO_CODEC_CFG_DURATION_*``)
 *  @param _loc             Audio channel location bitfield (@ref bt_audio_location)
 *  @param _len             Octets per frame (16-bit integer)
 *  @param _frames_per_sdu  Frames per SDU (8-bit integer). This value is optional and will be
 *                          included only if != 1
 */
#define BT_AUDIO_CODEC_CFG_LC3_DATA(_freq, _duration, _loc, _len, _frames_per_sdu)                 \
	{                                                                                          \
		BT_AUDIO_CODEC_DATA(BT_AUDIO_CODEC_CFG_FREQ, (_freq)),                             \
		BT_AUDIO_CODEC_DATA(BT_AUDIO_CODEC_CFG_DURATION, (_duration)),                     \
		BT_AUDIO_CODEC_DATA(BT_AUDIO_CODEC_CFG_CHAN_ALLOC, BT_BYTES_LIST_LE32(_loc)),      \
		BT_AUDIO_CODEC_DATA(BT_AUDIO_CODEC_CFG_FRAME_LEN, BT_BYTES_LIST_LE16(_len)),       \
		COND_CODE_1(_frames_per_sdu, (),                                                   \
			    (BT_AUDIO_CODEC_DATA(BT_AUDIO_CODEC_CFG_FRAME_BLKS_PER_SDU,            \
						 (_frames_per_sdu))))                              \
	}

/**
 *  @brief Helper to declare LC3 codec metadata configuration
 */
#define BT_AUDIO_CODEC_CFG_LC3_META(_stream_context)                                               \
	{                                                                                          \
		BT_AUDIO_CODEC_DATA(BT_AUDIO_METADATA_TYPE_STREAM_CONTEXT,                         \
				    BT_BYTES_LIST_LE16(_stream_context))                           \
	}

/**
 *  @brief Helper to declare LC3 codec configuration.
 *
 *  @param _freq            Sampling frequency (``BT_AUDIO_CODEC_CFG_FREQ_*``)
 *  @param _duration        Frame duration (``BT_AUDIO_CODEC_CFG_DURATION_*``)
 *  @param _loc             Audio channel location bitfield (@ref bt_audio_location)
 *  @param _len             Octets per frame (16-bit integer)
 *  @param _frames_per_sdu  Frames per SDU (8-bit integer)
 *  @param _stream_context  Stream context (``BT_AUDIO_CONTEXT_*``)
 */
#define BT_AUDIO_CODEC_LC3_CONFIG(_freq, _duration, _loc, _len, _frames_per_sdu, _stream_context)  \
	BT_AUDIO_CODEC_CFG(                                                                        \
		BT_HCI_CODING_FORMAT_LC3, 0x0000, 0x0000,                                          \
		BT_AUDIO_CODEC_CFG_LC3_DATA(_freq, _duration, _loc, _len, _frames_per_sdu),        \
		BT_AUDIO_CODEC_CFG_LC3_META(_stream_context))

/**
 *  @brief Helper to declare LC3 8.1 codec configuration
 *
 *  @param _loc             Audio channel location bitfield (@ref bt_audio_location)
 *  @param _stream_context  Stream context (``BT_AUDIO_CONTEXT_*``)
 */
#define BT_AUDIO_CODEC_LC3_CONFIG_8_1(_loc, _stream_context)                                       \
	BT_AUDIO_CODEC_LC3_CONFIG(BT_AUDIO_CODEC_CFG_FREQ_8KHZ, BT_AUDIO_CODEC_CFG_DURATION_7_5,   \
				  _loc, 26u, 1, _stream_context)
/**
 *  @brief Helper to declare LC3 8.2 codec configuration
 *
 *  @param _loc             Audio channel location bitfield (@ref bt_audio_location)
 *  @param _stream_context  Stream context (``BT_AUDIO_CONTEXT_*``)
 */
#define BT_AUDIO_CODEC_LC3_CONFIG_8_2(_loc, _stream_context)                                       \
	BT_AUDIO_CODEC_LC3_CONFIG(BT_AUDIO_CODEC_CFG_FREQ_8KHZ, BT_AUDIO_CODEC_CFG_DURATION_10,    \
				  _loc, 30u, 1, _stream_context)
/**
 *  @brief Helper to declare LC3 16.1 codec configuration
 *
 *  @param _loc             Audio channel location bitfield (@ref bt_audio_location)
 *  @param _stream_context  Stream context (``BT_AUDIO_CONTEXT_*``)
 */
#define BT_AUDIO_CODEC_LC3_CONFIG_16_1(_loc, _stream_context)                                      \
	BT_AUDIO_CODEC_LC3_CONFIG(BT_AUDIO_CODEC_CFG_FREQ_16KHZ, BT_AUDIO_CODEC_CFG_DURATION_7_5,  \
				  _loc, 30u, 1, _stream_context)
/**
 *  @brief Helper to declare LC3 16.2 codec configuration
 *
 *  @param _loc             Audio channel location bitfield (@ref bt_audio_location)
 *  @param _stream_context  Stream context (``BT_AUDIO_CONTEXT_*``)
 */
#define BT_AUDIO_CODEC_LC3_CONFIG_16_2(_loc, _stream_context)                                      \
	BT_AUDIO_CODEC_LC3_CONFIG(BT_AUDIO_CODEC_CFG_FREQ_16KHZ, BT_AUDIO_CODEC_CFG_DURATION_10,   \
				  _loc, 40u, 1, _stream_context)

/**
 *  @brief Helper to declare LC3 24.1 codec configuration
 *
 *  @param _loc             Audio channel location bitfield (@ref bt_audio_location)
 *  @param _stream_context  Stream context (``BT_AUDIO_CONTEXT_*``)
 */
#define BT_AUDIO_CODEC_LC3_CONFIG_24_1(_loc, _stream_context)                                      \
	BT_AUDIO_CODEC_LC3_CONFIG(BT_AUDIO_CODEC_CFG_FREQ_24KHZ, BT_AUDIO_CODEC_CFG_DURATION_7_5,  \
				  _loc, 45u, 1, _stream_context)
/**
 *  @brief Helper to declare LC3 24.2 codec configuration
 *
 *  @param _loc             Audio channel location bitfield (@ref bt_audio_location)
 *  @param _stream_context  Stream context (``BT_AUDIO_CONTEXT_*``)
 */
#define BT_AUDIO_CODEC_LC3_CONFIG_24_2(_loc, _stream_context)                                      \
	BT_AUDIO_CODEC_LC3_CONFIG(BT_AUDIO_CODEC_CFG_FREQ_24KHZ, BT_AUDIO_CODEC_CFG_DURATION_10,   \
				  _loc, 60u, 1, _stream_context)
/**
 *  @brief Helper to declare LC3 32.1 codec configuration
 *
 *  @param _loc             Audio channel location bitfield (@ref bt_audio_location)
 *  @param _stream_context  Stream context (``BT_AUDIO_CONTEXT_*``)
 */
#define BT_AUDIO_CODEC_LC3_CONFIG_32_1(_loc, _stream_context)                                      \
	BT_AUDIO_CODEC_LC3_CONFIG(BT_AUDIO_CODEC_CFG_FREQ_32KHZ, BT_AUDIO_CODEC_CFG_DURATION_7_5,  \
				  _loc, 60u, 1, _stream_context)
/**
 *  @brief Helper to declare LC3 32.2 codec configuration
 *
 *  @param _loc             Audio channel location bitfield (@ref bt_audio_location)
 *  @param _stream_context  Stream context (``BT_AUDIO_CONTEXT_*``)
 */
#define BT_AUDIO_CODEC_LC3_CONFIG_32_2(_loc, _stream_context)                                      \
	BT_AUDIO_CODEC_LC3_CONFIG(BT_AUDIO_CODEC_CFG_FREQ_32KHZ, BT_AUDIO_CODEC_CFG_DURATION_10,   \
				  _loc, 80u, 1, _stream_context)
/**
 *  @brief Helper to declare LC3 441.1 codec configuration
 *
 *  @param _loc             Audio channel location bitfield (@ref bt_audio_location)
 *  @param _stream_context  Stream context (``BT_AUDIO_CONTEXT_*``)
 */
#define BT_AUDIO_CODEC_LC3_CONFIG_441_1(_loc, _stream_context)                                     \
	BT_AUDIO_CODEC_LC3_CONFIG(BT_AUDIO_CODEC_CFG_FREQ_44KHZ, BT_AUDIO_CODEC_CFG_DURATION_7_5,  \
				  _loc, 98u, 1, _stream_context)
/**
 *  @brief Helper to declare LC3 441.2 codec configuration
 *
 *  @param _loc             Audio channel location bitfield (@ref bt_audio_location)
 *  @param _stream_context  Stream context (``BT_AUDIO_CONTEXT_*``)
 */
#define BT_AUDIO_CODEC_LC3_CONFIG_441_2(_loc, _stream_context)                                     \
	BT_AUDIO_CODEC_LC3_CONFIG(BT_AUDIO_CODEC_CFG_FREQ_44KHZ, BT_AUDIO_CODEC_CFG_DURATION_10,   \
				  _loc, 130u, 1, _stream_context)
/**
 *  @brief Helper to declare LC3 48.1 codec configuration
 *
 *  @param _loc             Audio channel location bitfield (@ref bt_audio_location)
 *  @param _stream_context  Stream context (``BT_AUDIO_CONTEXT_*``)
 */
#define BT_AUDIO_CODEC_LC3_CONFIG_48_1(_loc, _stream_context)                                      \
	BT_AUDIO_CODEC_LC3_CONFIG(BT_AUDIO_CODEC_CFG_FREQ_48KHZ, BT_AUDIO_CODEC_CFG_DURATION_7_5,  \
				  _loc, 75u, 1, _stream_context)
/**
 *  @brief Helper to declare LC3 48.2 codec configuration
 *
 *  @param _loc             Audio channel location bitfield (@ref bt_audio_location)
 *  @param _stream_context  Stream context (``BT_AUDIO_CONTEXT_*``)
 */
#define BT_AUDIO_CODEC_LC3_CONFIG_48_2(_loc, _stream_context)                                      \
	BT_AUDIO_CODEC_LC3_CONFIG(BT_AUDIO_CODEC_CFG_FREQ_48KHZ, BT_AUDIO_CODEC_CFG_DURATION_10,   \
				  _loc, 100u, 1, _stream_context)
/**
 *  @brief Helper to declare LC3 48.3 codec configuration
 *
 *  @param _loc             Audio channel location bitfield (@ref bt_audio_location)
 *  @param _stream_context  Stream context (``BT_AUDIO_CONTEXT_*``)
 */
#define BT_AUDIO_CODEC_LC3_CONFIG_48_3(_loc, _stream_context)                                      \
	BT_AUDIO_CODEC_LC3_CONFIG(BT_AUDIO_CODEC_CFG_FREQ_48KHZ, BT_AUDIO_CODEC_CFG_DURATION_7_5,  \
				  _loc, 90u, 1, _stream_context)
/**
 *  @brief Helper to declare LC3 48.4 codec configuration
 *
 *  @param _loc             Audio channel location bitfield (@ref bt_audio_location)
 *  @param _stream_context  Stream context (``BT_AUDIO_CONTEXT_*``)
 */
#define BT_AUDIO_CODEC_LC3_CONFIG_48_4(_loc, _stream_context)                                      \
	BT_AUDIO_CODEC_LC3_CONFIG(BT_AUDIO_CODEC_CFG_FREQ_48KHZ, BT_AUDIO_CODEC_CFG_DURATION_10,   \
				  _loc, 120u, 1, _stream_context)
/**
 *  @brief Helper to declare LC3 48.5 codec configuration
 *
 *  @param _loc             Audio channel location bitfield (@ref bt_audio_location)
 *  @param _stream_context  Stream context (``BT_AUDIO_CONTEXT_*``)
 */
#define BT_AUDIO_CODEC_LC3_CONFIG_48_5(_loc, _stream_context)                                      \
	BT_AUDIO_CODEC_LC3_CONFIG(BT_AUDIO_CODEC_CFG_FREQ_48KHZ, BT_AUDIO_CODEC_CFG_DURATION_7_5,  \
				  _loc, 117u, 1, _stream_context)
/**
 *  @brief Helper to declare LC3 48.6 codec configuration
 *
 *  @param _loc             Audio channel location bitfield (@ref bt_audio_location)
 *  @param _stream_context  Stream context (``BT_AUDIO_CONTEXT_*``)
 */
#define BT_AUDIO_CODEC_LC3_CONFIG_48_6(_loc, _stream_context)                                      \
	BT_AUDIO_CODEC_LC3_CONFIG(BT_AUDIO_CODEC_CFG_FREQ_48KHZ, BT_AUDIO_CODEC_CFG_DURATION_10,   \
				  _loc, 155u, 1, _stream_context)
/**
 *  @brief Helper to declare LC3 codec QoS for 7.5ms interval
 */
#define BT_AUDIO_CODEC_LC3_QOS_7_5(_framing, _sdu, _rtn, _latency, _pd)                            \
	BT_AUDIO_CODEC_QOS(7500u, _framing, BT_AUDIO_CODEC_QOS_2M, _sdu, _rtn, _latency, _pd)
/**
 *  @brief Helper to declare LC3 codec QoS for 7.5ms interval unframed input
 */
#define BT_AUDIO_CODEC_LC3_QOS_7_5_UNFRAMED(_sdu, _rtn, _latency, _pd)                             \
	BT_AUDIO_CODEC_QOS_UNFRAMED(7500u, _sdu, _rtn, _latency, _pd)
/**
 *  @brief Helper to declare LC3 codec QoS for 10ms frame internal
 */
#define BT_AUDIO_CODEC_LC3_QOS_10(_framing, _sdu, _rtn, _latency, _pd)                             \
	BT_AUDIO_CODEC_QOS(10000u, _framing, BT_AUDIO_CODEC_QOS_2M, _sdu, _rtn, _latency, _pd)
/**
 *  @brief Helper to declare LC3 codec QoS for 10ms interval unframed input
 */
#define BT_AUDIO_CODEC_LC3_QOS_10_UNFRAMED(_sdu, _rtn, _latency, _pd)                              \
	BT_AUDIO_CODEC_QOS_UNFRAMED(10000u, _sdu, _rtn, _latency, _pd)

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_LC3_H_ */
