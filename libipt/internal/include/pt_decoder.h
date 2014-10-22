/*
 * Copyright (c) 2013-2014, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __PT_DECODER_H__
#define __PT_DECODER_H__

#include "pt_last_ip.h"
#include "pt_tnt_cache.h"
#include "pt_time.h"
#include "pt_event_queue.h"

#include "intel-pt.h"

#include <stdint.h>

struct pt_decoder_function;


/* Intel(R) Processor Trace decoder flags. */
enum pt_decoder_flag {
	/* Tracing has temporarily been disabled. */
	pdf_pt_disabled		= 1 << 0,

	/* The packet will be consumed after all events have been processed. */
	pdf_consume_packet	= 1 << 1
};

struct pt_decoder {
	/* The decoder configuration. */
	struct pt_config config;

	/* The current position in the trace buffer. */
	const uint8_t *pos;

	/* The position of the last PSB packet. */
	const uint8_t *sync;

	/* The decoding function for the next packet. */
	const struct pt_decoder_function *next;

	/* The last-ip. */
	struct pt_last_ip ip;

	/* The cached tnt indicators. */
	struct pt_tnt_cache tnt;

	/* A bit-vector of decoder flags. */
	uint64_t flags;

	/* Timing information. */
	struct pt_time time;

	/* Pending (incomplete) events. */
	struct pt_event_queue evq;

	/* The current event. */
	struct pt_event *event;
};

/* Allocate an Intel(R) Processor Trace decoder.
 *
 * The decoder will work on the buffer defined via its base address and size.
 * The buffer shall contain raw trace data and remain valid for the lifetime of
 * the decoder.
 *
 * The decoder needs to be synchronized before it can be used.
 */
extern struct pt_decoder *pt_alloc_decoder(const struct pt_config *);

/* Free an Intel(R) Processor Trace decoder.
 *
 * The decoder object must not be used after a successful return.
 */
extern void pt_free_decoder(struct pt_decoder *);

/* Get the current decoder position.
 *
 * This is useful for reporting errors.
 *
 * Returns zero on success.
 *
 * Returns -pte_invalid if no @decoder is given.
 */
extern int pt_get_decoder_pos(struct pt_decoder *decoder, uint64_t *offset);

/* Get the position of the last synchronization point.
 *
 * This is useful when splitting a trace stream for parallel decoding.
 *
 * Returns zero on success.
 *
 * Returns -pte_invalid if no @decoder is given.
 */
extern int pt_get_decoder_sync(struct pt_decoder *decoder, uint64_t *offset);

/* Start querying.
 *
 * Read ahead until the first query-relevant packet and return the current
 * query status.
 *
 * This function must be called once after synchronizing the decoder.
 *
 * On success, if the second parameter is not NULL, provides the linear address
 * of the first instruction in it, unless the address has been suppressed. In
 * this case, the address is set to zero.
 *
 * Returns a non-negative pt_status_flag bit-vector on success.
 *
 * Returns -pte_invalid if no decoder is given.
 * Returns -pte_nosync if the decoder is out of sync.
 * Returns -pte_eos if the end of the trace buffer is reached.
 * Returns -pte_bad_opc if the decoder encountered unknown packets.
 * Returns -pte_bad_packet if the decoder encountered unknown packet payloads.
 */
extern int pt_query_start(struct pt_decoder *, uint64_t *);

/* Get the next unconditional branch destination.
 *
 * On success, provides the linear destination address of the next unconditional
 * branch in the second parameter, provided it is not null, and updates the
 * decoder state accordingly.
 *
 * Returns a non-negative pt_status_flag bit-vector on success.
 *
 * Returns -pte_invalid if no decoder or no address is given.
 * Returns -pte_nosync if the decoder is out of sync.
 * Returns -pte_bad_query if no unconditional branch destination is found.
 * Returns -pte_bad_opc if the decoder encountered unknown packets.
 * Returns -pte_bad_packet if the decoder encountered unknown packet payloads.
 */
extern int pt_query_uncond_branch(struct pt_decoder *, uint64_t *);

/* Query whether the next unconditional branch has been taken.
 *
 * On success, provides 1 (taken) or 0 (not taken) in the second parameter for
 * the next conditional branch and updates the decoder state accordingly.
 *
 * Returns a non-negative pt_status_flag bit-vector on success.
 *
 * Returns -pte_invalid if no decoder or no address is given.
 * Returns -pte_nosync if the decoder is out of sync.
 * Returns -pte_bad_query if no conditional branch is found.
 * Returns -pte_bad_opc if the decoder encountered unknown packets.
 * Returns -pte_bad_packet if the decoder encountered unknown packet payloads.
 */
extern int pt_query_cond_branch(struct pt_decoder *, int *);

/* Query the next pending event.
 *
 * On success, provides the next event in the second parameter and updates the
 * decoder state accordingly.
 *
 * Returns a non-negative pt_status_flag bit-vector on success.
 *
 * Returns -pte_invalid if no decoder or no address is given.
 * Returns -pte_nosync if the decoder is out of sync.
 * Returns -pte_bad_query if no event is found.
 * Returns -pte_bad_opc if the decoder encountered unknown packets.
 * Returns -pte_bad_packet if the decoder encountered unknown packet payloads.
 */
extern int pt_query_event(struct pt_decoder *, struct pt_event *);

/* Query the current time stamp count.
 *
 * This returns the time stamp count at the decoder's current position. Since
 * the decoder is reading ahead until the next unconditional branch or event,
 * the value matches the time stamp count for that branch or event.
 *
 * The time stamp count is similar to what an rdtsc instruction would return.
 *
 * Beware that the time stamp count is no fully accurate and that it is updated
 * irregularly.
 *
 * Returns zero on success.
 *
 * Returns -pte_invalid if no @decoder is given.
 */
extern int pt_query_time(struct pt_decoder *, uint64_t *time);

/* Query the current core:bus ratio.
 *
 * This returns the core:bus ratio at the decoder's current position. Since
 * the decoder is reading ahead until the next unconditional branch or event,
 * the value matches the core:bus ratio for that branch or event.
 *
 * The ratio is defined as core cycles per bus clock cycle.
 *
 * Returns zero on success.
 *
 * Returns -pte_invalid if no @decoder is given.
 */
extern int pt_query_core_bus_ratio(struct pt_decoder *decoder, uint32_t *cbr);

static inline const uint8_t *pt_begin(const struct pt_decoder *decoder)
{
	return decoder->config.begin;
}

static inline const uint8_t *pt_end(const struct pt_decoder *decoder)
{
	return decoder->config.end;
}

/* Initialize the decoder.
 *
 * Returns zero on success, a negative error code otherwise.
 */
extern int pt_decoder_init(struct pt_decoder *, const struct pt_config *);

/* Finalize the decoder. */
extern void pt_decoder_fini(struct pt_decoder *);

/* Check if decoding the next decoder function will result in an event.
 *
 * Returns 1 if it will result in an event.
 * Returns 0 if it will not result in an event.
 * Returns -pte_invalid if @decoder is NULL.
 */
extern int pt_will_event(const struct pt_decoder *decoder);

/* Reset the decoder state.
 *
 * This resets the cache fields of the decoder state. It does not modify
 * buffer-related fields.
 */
extern void pt_reset(struct pt_decoder *);

#endif /* __PT_DECODER_H__ */
