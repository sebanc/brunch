/*
 * cros_ec_sensors_ring - Driver for Chrome OS EC Sensor hub FIFO.
 *
 * Copyright (C) 2015 Google, Inc
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * This driver uses the cros-ec interface to communicate with the Chrome OS
 * EC about accelerometer data. Accelerometer access is presented through
 * iio sysfs.
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/iio/buffer.h>
#include <linux/iio/common/cros_ec_sensors_core.h>
#include <linux/iio/iio.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/trigger.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/sysfs.h>
#include <linux/kernel.h>
#include <linux/mfd/cros_ec.h>
#include <linux/module.h>
#include <linux/platform_data/cros_ec_commands.h>
#include <linux/platform_data/cros_ec_proto.h>
#include <linux/platform_device.h>
#include <linux/sort.h>
#include <linux/slab.h>

#define DRV_NAME "cros-ec-ring"

/*
 * The ring is a FIFO that return sensor information from
 * the single EC FIFO.
 * There are always 5 channels returned:
 * | ID | FLAG | X | Y | Z | Timestamp |
 * ID is the EC sensor id
 * FLAG are extra information provided by the EC.
 */

enum {
	CHANNEL_SENSOR_ID,
	CHANNEL_SENSOR_FLAG,
	CHANNEL_X,
	CHANNEL_Y,
	CHANNEL_Z,
	CHANNEL_TIMESTAMP,
	MAX_CHANNEL,
};

enum {
	LAST_TS,
	NEW_TS,
	ALL_TS
};

#define CROS_EC_SENSOR_MAX 16

struct __ec_todo_packed cros_ec_fifo_info {
	struct ec_response_motion_sense_fifo_info info;
	uint16_t lost[CROS_EC_SENSOR_MAX];
};

struct cros_ec_sensors_ring_sample {
	uint8_t sensor_id;
	uint8_t flag;
	int16_t  vector[CROS_EC_SENSOR_MAX_AXIS];
	s64      timestamp;
} __packed;

/* State used for cros_ec_ring_fix_overflow */
struct cros_ec_sensors_ec_overflow_state {
	s64 offset;
	s64 last;
};

/* Precision of fixed point for the m values from the filter */
#define M_PRECISION (1 << 23)

/* Length of the filter, how long to remember entries for */
#define TS_HISTORY_SIZE 64

/* Only activate the filter once we have at least this many elements. */
#define TS_HISTORY_THRESHOLD 8

/*
 * If we don't have any history entries for this long, empty the filter to
 * make sure there are no big discontinuities.
 */
#define TS_HISTORY_BORED_US 500000

struct cros_ec_sensors_ts_filter_state {
	s64 x_offset, y_offset;
	s64 x_history[TS_HISTORY_SIZE]; /* stored relative to x_offset */
	s64 y_history[TS_HISTORY_SIZE]; /* stored relative to y_offset */
	s64 m_history[TS_HISTORY_SIZE]; /* stored as *M_PRECISION */
	int history_len;

	s64 temp_buf[TS_HISTORY_SIZE];

	s64 median_m;
	s64 median_error;
};

#define FUTURE_TS_ANALYTICS_COUNT_MAX 100

/* State data for ec_sensors iio driver. */
struct cros_ec_sensors_ring_state {
	/* Shared by all sensors */
	struct cros_ec_sensors_core_state core;

	/* Notifier to kick to the interrupt */
	struct notifier_block notifier;

	/* Preprocessed ring to send to kfifo */
	struct cros_ec_sensors_ring_sample *ring;

	s64    fifo_timestamp[ALL_TS];
	struct cros_ec_fifo_info fifo_info;
	int    fifo_size;

	/* Used for timestamp spreading calculations when a batch shows up */
	s64 penultimate_batch_timestamp[CROS_EC_SENSOR_MAX];
	int penultimate_batch_len[CROS_EC_SENSOR_MAX];
	s64 last_batch_timestamp[CROS_EC_SENSOR_MAX];
	int last_batch_len[CROS_EC_SENSOR_MAX];
	s64 newest_sensor_event[CROS_EC_SENSOR_MAX];

	struct cros_ec_sensors_ec_overflow_state overflow_a;
	struct cros_ec_sensors_ec_overflow_state overflow_b;

	struct cros_ec_sensors_ts_filter_state filter;

	/*
	 * The timestamps reported from the EC have low jitter.
	 * Timestamps also come before every sample.
	 * Set either by feature bits coming from the EC or userspace.
	 */
	bool tight_timestamps;

	/*
	 * Statistics used to compute shaved time. This occures when
	 * timestamp interpolation from EC time to AP time accidentally
	 * puts timestamps in the future. These timestamps are clamped
	 * to `now` and these count/total_ns maintain the statistics for
	 * how much time was removed in a given period..
	 */
	s32 future_timestamp_count;
	s64 future_timestamp_total_ns;
};

static ssize_t cros_ec_ring_attr_tight_timestamps_show(
						struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	struct cros_ec_sensors_ring_state *state =
		iio_priv(dev_to_iio_dev(dev));

	return sprintf(buf, "%d\n", state->tight_timestamps);
}

static ssize_t cros_ec_ring_attr_tight_timestamps_store(
						struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t len)
{
	struct cros_ec_sensors_ring_state *state =
		iio_priv(dev_to_iio_dev(dev));
	int ret;

	ret = strtobool(buf, &state->tight_timestamps);
	return ret ? ret : len;
}

static IIO_DEVICE_ATTR(tight_timestamps, 0644,
			cros_ec_ring_attr_tight_timestamps_show,
			cros_ec_ring_attr_tight_timestamps_store,
			0);

static struct attribute *cros_ec_ring_attributes[] = {
	&iio_dev_attr_tight_timestamps.dev_attr.attr,
	NULL,
};

static const struct attribute_group cros_ec_ring_attribute_group = {
	.attrs = cros_ec_ring_attributes,
};

static const struct iio_info ec_sensors_info = {
	.attrs = &cros_ec_ring_attribute_group,
};

static int cros_ec_ring_fifo_toggle(struct cros_ec_sensors_ring_state *state,
				    bool on)
{
	int i, ret;

	mutex_lock(&state->core.cmd_lock);
	for (i = 0; i < CROS_EC_SENSOR_MAX; i++)
		state->last_batch_len[i] = 0;
	state->core.param.cmd = MOTIONSENSE_CMD_FIFO_INT_ENABLE;
	state->core.param.fifo_int_enable.enable = on;
	ret = cros_ec_motion_send_host_cmd(&state->core, 0);
	mutex_unlock(&state->core.cmd_lock);
	return ret;
}

static int cros_ec_ring_median_cmp(const void *pv1, const void *pv2)
{
	s64 v1 = *(s64 *)pv1;
	s64 v2 = *(s64 *)pv2;

	if (v1 > v2)
		return 1;
	else if (v1 < v2)
		return -1;
	else
		return 0;
}

/*
 * cros_ec_ring_median: Gets median of an array of numbers
 *
 * For now it's implemented using an inefficient > O(n) sort then return
 * the middle element. A more optimal method would be something like
 * quickselect, but given that n = 64 we can probably live with it in the
 * name of clarity.
 *
 * Warning: the input array gets modified (sorted)!
 */
static s64 cros_ec_ring_median(s64 *array, size_t length)
{
	sort(array, length, sizeof(s64), cros_ec_ring_median_cmp, NULL);
	return array[length / 2];
}

/*
 * IRQ Timestamp Filtering
 *
 * Lower down in cros_ec_ring_process_event(), for each sensor event we have to
 * calculate it's timestamp in the AP timebase. There are 3 time points:
 *   a - EC timebase, sensor event
 *   b - EC timebase, IRQ
 *   c - AP timebase, IRQ
 *   a' - what we want: sensor even in AP timebase
 *
 * While a and b are recorded at accurate times (due to the EC real time
 * nature); c is pretty untrustworthy, even though it's recorded the
 * first thing in ec_irq_handler(). There is a very good change we'll get
 * added lantency due to:
 *   other irqs
 *   ddrfreq
 *   cpuidle
 *
 * Normally a' = c - b + a, but if we do that naive math any jitter in c
 * will get coupled in a', which we don't want. We want a function
 * a' = cros_ec_ring_ts_filter(a) which will filter out outliers in c.
 *
 * Think of a graph of AP time(b) on the y axis vs EC time(c) on the x axis.
 * The slope of the line won't be exactly 1, there will be some clock drift
 * between the 2 chips for various reasons (mechanical stress, temperature,
 * voltage). We need to extrapolate values for a future x, without trusting
 * recent y values too much.
 *
 * We use a median filter for the slope, then another median filter for the
 * y-intercept to calculate this function:
 *   dx[n] = x[n-1] - x[n]
 *   dy[n] = x[n-1] - x[n]
 *   m[n] = dy[n] / dx[n]
 *   median_m = median(m[n-k:n])
 *   error[i] = y[n-i] - median_m * x[n-i]
 *   median_error = median(error[:k])
 *   predicted_y = median_m * x + median_error
 *
 * Implementation differences from above:
 * - Redefined y to be actually c - b, this gives us a lot more precision
 * to do the math. (c-b)/b variations are more obvious than c/b variations.
 * - Since we don't have floating point, any operations involving slope are
 * done using fixed point math (*M_PRECISION)
 * - Since x and y grow with time, we keep zeroing the graph (relative to
 * the last sample), this way math involving *x[n-i] will not overflow
 * - EC timestamps are kept in us, it improves the slope calculation precision
 */

/*
 * cros_ec_ring_ts_filter_update: Given a new IRQ timestamp pair (EC and
 * AP timebases), add it to the filter history.
 *
 * @b IRQ timestamp, EC timebase (us)
 * @c IRQ timestamp, AP timebase (ns)
 */
static void cros_ec_ring_ts_filter_update(
			struct cros_ec_sensors_ts_filter_state *state,
			s64 b, s64 c)
{
	s64 x, y;
	s64 dx, dy;
	s64 m; /* stored as *M_PRECISION */
	s64 *m_history_copy = state->temp_buf;
	s64 *error = state->temp_buf;
	int i;

	/* we trust b the most, that'll be our independent variable */
	x = b;
	/* y is the offset between AP and EC times, in ns */
	y = c - b * 1000;

	dx = (state->x_history[0] + state->x_offset) - x;
	if (dx == 0)
		return; /* we already have this irq in the history */
	dy = (state->y_history[0] + state->y_offset) - y;
	m = div64_s64(dy * M_PRECISION, dx);

	/* Empty filter if we haven't seen any action in a while. */
	if (-dx > TS_HISTORY_BORED_US)
		state->history_len = 0;

	/* Move everything over, also update offset to all absolute coords .*/
	for (i = state->history_len - 1; i >= 1; i--) {
		state->x_history[i] = state->x_history[i-1] + dx;
		state->y_history[i] = state->y_history[i-1] + dy;

		state->m_history[i] = state->m_history[i-1];
		/*
		 * Also use the same loop to copy m_history for future
		 * median extraction.
		 */
		m_history_copy[i] = state->m_history[i-1];
	}

	/* Store the x and y, but remember offset is actually last sample. */
	state->x_offset = x;
	state->y_offset = y;
	state->x_history[0] = 0;
	state->y_history[0] = 0;

	state->m_history[0] = m;
	m_history_copy[0] = m;

	if (state->history_len < TS_HISTORY_SIZE)
		state->history_len++;

	/* Precalculate things for the filter. */
	if (state->history_len > TS_HISTORY_THRESHOLD) {
		state->median_m =
		    cros_ec_ring_median(m_history_copy, state->history_len - 1);

		/*
		 * Calculate y-intercepts as if m_median is the slope and
		 * points in the history are on the line. median_error will
		 * still be in the offset coordinate system.
		 */
		for (i = 0; i < state->history_len; i++)
			error[i] = state->y_history[i] -
				div_s64(state->median_m * state->x_history[i],
					M_PRECISION);
		state->median_error =
			cros_ec_ring_median(error, state->history_len);
	} else {
		state->median_m = 0;
		state->median_error = 0;
	}
}

/*
 * cros_ec_ring_ts_filter: Translate EC timebase timestamp to AP timebase
 *
 * @x any ec timestamp (us):
 *
 * cros_ec_ring_ts_filter(a) => a' event timestamp, AP timebase
 * cros_ec_ring_ts_filter(b) => calculated timestamp when the EC IRQ
 *                           should have happened on the AP, with low jitter
 *
 * @returns timestamp in AP timebase (ns)
 *
 * Note: The filter will only activate once state->history_len goes
 * over TS_HISTORY_THRESHOLD. Otherwise it'll just do the naive c - b + a
 * transform.
 *
 * How to derive the formula, starting from:
 *   f(x) = median_m * x + median_error
 * That's the calculated AP - EC offset (at the x point in time)
 * Undo the coordinate system transform:
 *   f(x) = median_m * (x - x_offset) + median_error + y_offset
 * Remember to undo the "y = c - b * 1000" modification:
 *   f(x) = median_m * (x - x_offset) + median_error + y_offset + x * 1000
 */
static s64 cros_ec_ring_ts_filter(struct cros_ec_sensors_ts_filter_state *state,
				  s64 x)
{
	return div_s64(state->median_m * (x - state->x_offset), M_PRECISION)
	       + state->median_error + state->y_offset + x * 1000;
}

/*
 * Since a and b were originally 32 bit values from the EC,
 * they overflow relatively often, casting is not enough, so we need to
 * add an offset.
 */
static void cros_ec_ring_fix_overflow(s64 *ts,
		const s64 overflow_period,
		struct cros_ec_sensors_ec_overflow_state *state)
{
	s64 adjust;

	*ts += state->offset;
	if (abs(state->last - *ts) > (overflow_period / 2)) {
		adjust = state->last > *ts ? overflow_period : -overflow_period;
		state->offset += adjust;
		*ts += adjust;
	}
	state->last = *ts;
}

static void cros_ec_ring_check_for_past_timestamp(
				struct cros_ec_sensors_ring_state *state,
				struct cros_ec_sensors_ring_sample *sample)
{
	const u8 sensor_id = sample->sensor_id;

	// if this event is earlier than one we saw before...
	if (state->newest_sensor_event[sensor_id] > sample->timestamp)
		// mark it for spreading
		sample->timestamp = state->last_batch_timestamp[sensor_id];
	else
		state->newest_sensor_event[sensor_id] = sample->timestamp;
}

/*
 * cros_ec_ring_process_event: process one EC FIFO event
 *
 * Process one EC event, add it in the ring if necessary.
 *
 * Return true if out event has been populated.
 *
 * fifo_info: fifo information from the EC (includes b point, EC timebase).
 * fifo_timestamp: EC IRQ, kernel timebase (aka c)
 * current_timestamp: calculated event timestamp, kernel timebase (aka a')
 * in: incoming FIFO event from EC (includes a point, EC timebase)
 * out: outgoing event to user space (includes a')
 */
static bool cros_ec_ring_process_event(
				struct cros_ec_sensors_ring_state *state,
				const struct cros_ec_fifo_info *fifo_info,
				const s64 fifo_timestamp,
				s64 *current_timestamp,
				struct ec_response_motion_sensor_data *in,
				struct cros_ec_sensors_ring_sample *out)
{
	int axis;
	/* Do not populate the filter based on asynchronous events. */
	const int async_flags = in->flags &
		(MOTIONSENSE_SENSOR_FLAG_ODR | MOTIONSENSE_SENSOR_FLAG_FLUSH);
	const s64 now = cros_ec_get_time_ns();

	if (in->flags & MOTIONSENSE_SENSOR_FLAG_TIMESTAMP && !async_flags) {
		s64 a = in->timestamp;
		s64 b = fifo_info->info.timestamp;
		s64 c = fifo_timestamp;

		cros_ec_ring_fix_overflow(&a, 1LL << 32, &state->overflow_a);
		cros_ec_ring_fix_overflow(&b, 1LL << 32, &state->overflow_b);

		if (state->tight_timestamps) {
			cros_ec_ring_ts_filter_update(&state->filter, b, c);
			*current_timestamp =
				cros_ec_ring_ts_filter(&state->filter, a);
		} else {
			s64 new_timestamp;
			/*
			 * disable filtering since we might add more jitter
			 * if b is in a random point in time
			 */
			new_timestamp = c - b * 1000 + a * 1000;
			/*
			 * The timestamp can be stale if we had to use the fifo
			 * info timestamp.
			 */
			if (new_timestamp - *current_timestamp > 0)
				*current_timestamp = new_timestamp;
		}
	}

	if (in->flags & MOTIONSENSE_SENSOR_FLAG_ODR) {
		state->last_batch_len[in->sensor_num] =
			state->penultimate_batch_len[in->sensor_num] = 0;
		/*
		 * ODR change is only useful for the sensor_ring, it does not
		 * convey information to clients.
		 */
		return false;
	}

	if (in->flags & MOTIONSENSE_SENSOR_FLAG_FLUSH) {
		out->sensor_id = in->sensor_num;
		out->timestamp = *current_timestamp;
		out->flag = in->flags;
		state->last_batch_len[out->sensor_id] = 0;
		/*
		 * No other payload information provided with
		 * flush ack.
		 */
		return true;
	}
	if (in->flags & MOTIONSENSE_SENSOR_FLAG_TIMESTAMP)
		/* If we just have a timestamp, skip this entry. */
		return false;

	/* Regular sample */
	out->sensor_id = in->sensor_num;
	if (*current_timestamp - now > 0) {
		/*
		 * This fix is needed to overcome the timestamp filter putting
		 * events in the future.
		 */
		state->future_timestamp_total_ns += *current_timestamp - now;
		if (++state->future_timestamp_count ==
				FUTURE_TS_ANALYTICS_COUNT_MAX) {
			s64 avg = div_s64(state->future_timestamp_total_ns,
					state->future_timestamp_count);
			dev_warn(&state->core.indio_dev->dev,
					"100 timestamps in the future, %lldns shaved on average\n",
					avg);
			state->future_timestamp_count = 0;
			state->future_timestamp_total_ns = 0;
		}
		out->timestamp = now;
	} else {
		out->timestamp = *current_timestamp;
	}
	out->flag = in->flags;
	for (axis = CROS_EC_SENSOR_X; axis < CROS_EC_SENSOR_MAX_AXIS; axis++)
		out->vector[axis] = in->data[axis];
	if (state->tight_timestamps)
		cros_ec_ring_check_for_past_timestamp(state, out);
	return true;
}

/*
 * cros_ec_ring_spread_add: Calculate proper timestamps then add to ringbuffer.
 *
 * Note: This is the new spreading code, assumes every sample's timestamp
 * preceeds the sample. Run if tight_timestamps == true.
 *
 * Sometimes the EC receives only one interrupt (hence timestamp) for
 * a batch of samples. Only the first sample will have the correct
 * timestamp. So we must interpolate the other samples.
 * We use the previous batch timestamp and our current batch timestamp
 * as a way to calculate period, then spread the samples evenly.
 *
 * s0 int, 0ms
 * s1 int, 10ms
 * s2 int, 20ms
 * 30ms point goes by, no interrupt, previous one is still asserted
 * downloading s2 and s3
 * s3 sample, 20ms (incorrect timestamp)
 * s4 int, 40ms
 *
 * The batches are [(s0), (s1), (s2, s3), (s4)]. Since the 3rd batch
 * has 2 samples in them, we adjust the timestamp of s3.
 * s2 - s1 = 10ms, so s3 must be s2 + 10ms => 20ms. If s1 would have
 * been part of a bigger batch things would have gotten a little
 * more complicated.
 *
 * Note: we also assume another sensor sample doesn't break up a batch
 * in 2 or more partitions. Example, there can't ever be a sync sensor
 * in between S2 and S3. This simplifies the following code.
 */
static void cros_ec_ring_spread_add(
				struct cros_ec_sensors_ring_state *state,
				unsigned long sensor_mask,
				struct cros_ec_sensors_ring_sample *last_out)
{
	struct iio_dev *indio_dev = state->core.indio_dev;
	struct cros_ec_sensors_ring_sample *batch_start, *next_batch_start;
	int id;

	for_each_set_bit(id, &sensor_mask, BITS_PER_LONG) {
		for (batch_start = state->ring; batch_start < last_out;
		     batch_start = next_batch_start) {
			/*
			 * For each batch (where all samples have the same
			 * timestamp).
			 */
			int batch_len, sample_idx;
			struct cros_ec_sensors_ring_sample *batch_end =
				batch_start;
			struct cros_ec_sensors_ring_sample *s;
			s64 batch_timestamp = batch_start->timestamp;
			s64 sample_period;

			/*
			 * Skip over batches that start with the sensor types
			 * we're not looking at right now.
			 */
			if (batch_start->sensor_id != id) {
				next_batch_start = batch_start + 1;
				continue;
			}

			/*
			 * Send out flush packets, but do not start a batch
			 * from a flush, as it happens asynchronously to the
			 * regular flow of events.
			 */
			if (batch_start->flag &
				MOTIONSENSE_SENSOR_FLAG_FLUSH) {
				iio_push_to_buffers(indio_dev,
					(u8 *)batch_start);
				next_batch_start = batch_start + 1;
				continue;
			}

			if (batch_start->timestamp <=
				state->last_batch_timestamp[id]) {

				batch_timestamp =
					state->last_batch_timestamp[id];
				batch_len = state->last_batch_len[id];

				sample_idx = batch_len;

				state->last_batch_timestamp[id] =
					state->penultimate_batch_timestamp[id];
				state->last_batch_len[id] =
					state->penultimate_batch_len[id];
			} else {
				/*
				 * Push first sample in the batch to the,
				 * kifo, it's guaranteed to be correct, the
				 * rest will follow later on.
				 */
				sample_idx = batch_len = 1;
				iio_push_to_buffers(indio_dev,
					(u8 *)batch_start);
				batch_start++;
			}

			/* Find all samples have the same timestamp. */
			for (s = batch_start; s < last_out; s++) {
				if (s->sensor_id != id)
					/*
					 * Skip over other sensor types that
					 * are interleaved, don't count them.
					 */
					continue;
				if (s->timestamp != batch_timestamp)
					/* we discovered the next batch */
					break;
				if (s->flag & MOTIONSENSE_SENSOR_FLAG_FLUSH)
					/* break on flush packets */
					break;
				batch_end = s;
				batch_len++;
			}

			if (batch_len == 1)
				goto done_with_this_batch;

			/* Can we calculate period? */
			if (state->last_batch_len[id] == 0) {
				dev_warn(&indio_dev->dev, "Sensor %d: lost %d samples when spreading\n",
						id, batch_len - 1);
				goto done_with_this_batch;
				/*
				 * Note: we're dropping the rest of the samples
				 * in this batch since we have no idea where
				 * they're supposed to go without a period
				 * calculation.
				 */
			}

			sample_period = div_s64(batch_timestamp -
					state->last_batch_timestamp[id],
					state->last_batch_len[id]);
			dev_dbg(&indio_dev->dev,
					"Adjusting %d samples, sensor %d last_batch @%lld (%d samples) batch_timestamp=%lld => period=%lld\n",
					batch_len, id,
					state->last_batch_timestamp[id],
					state->last_batch_len[id],
					batch_timestamp,
					sample_period);

			/*
			 * Adjust timestamps of the samples then push them to
			 * kfifo.
			 */
			for (s = batch_start; s <= batch_end; s++) {
				if (s->sensor_id != id)
					/*
					 * Skip over other sensor types that
					 * are interleaved, don't change them.
					 */
					continue;

				s->timestamp = batch_timestamp +
					sample_period * sample_idx;
				sample_idx++;

				iio_push_to_buffers(indio_dev, (u8 *)s);
			}

done_with_this_batch:
			state->penultimate_batch_timestamp[id] =
				state->last_batch_timestamp[id];
			state->penultimate_batch_len[id] =
				state->last_batch_len[id];

			state->last_batch_timestamp[id] = batch_timestamp;
			state->last_batch_len[id] = batch_len;

			next_batch_start = batch_end + 1;
		}
	}
}

/*
 * cros_ec_ring_spread_add_legacy: Calculate proper timestamps then
 * add to ringbuffer (legacy).
 *
 * Note: This assumes we're running old firmware, where every sample's timestamp
 * is after the sample. Run if tight_timestamps == false.
 *
 * If there is a sample with a proper timestamp
 *                        timestamp | count
 * older_unprocess_out --> TS1      | 1
 *                         TS1      | 2
 * out -->                 TS1      | 3
 * next_out -->            TS2      |
 * We spread time for the samples [older_unprocess_out .. out]
 * between TS1 and TS2: [TS1+1/4, TS1+2/4, TS1+3/4, TS2].
 *
 * If we reach the end of the samples, we compare with the
 * current timestamp:
 *
 * older_unprocess_out --> TS1      | 1
 *                         TS1      | 2
 * out -->                 TS1      | 3
 * We know have [TS1+1/3, TS1+2/3, current timestamp]
 */
static void cros_ec_ring_spread_add_legacy(
				struct cros_ec_sensors_ring_state *state,
				unsigned long sensor_mask,
				s64 current_timestamp,
				struct cros_ec_sensors_ring_sample *last_out)
{
	struct cros_ec_sensors_ring_sample *out;
	struct iio_dev *indio_dev = state->core.indio_dev;
	int i;

	for_each_set_bit(i, &sensor_mask, BITS_PER_LONG) {
		s64 older_timestamp;
		s64 timestamp;
		struct cros_ec_sensors_ring_sample *older_unprocess_out =
			state->ring;
		struct cros_ec_sensors_ring_sample *next_out;
		int count = 1;

		for (out = state->ring; out < last_out; out = next_out) {
			s64 time_period;

			next_out = out + 1;
			if (out->sensor_id != i)
				continue;

			/* Timestamp to start with */
			older_timestamp = out->timestamp;

			/* find next sample */
			while (next_out < last_out && next_out->sensor_id != i)
				next_out++;

			if (next_out >= last_out) {
				timestamp = current_timestamp;
			} else {
				timestamp = next_out->timestamp;
				if (timestamp == older_timestamp) {
					count++;
					continue;
				}
			}

			/*
			 * The next sample has a new timestamp,
			 * spread the unprocessed samples.
			 */
			if (next_out < last_out)
				count++;
			time_period = div_s64(timestamp - older_timestamp,
					      count);

			for (; older_unprocess_out <= out;
					older_unprocess_out++) {
				if (older_unprocess_out->sensor_id != i)
					continue;
				older_timestamp += time_period;
				older_unprocess_out->timestamp =
					older_timestamp;
			}
			count = 1;
			/* The next_out sample has a valid timestamp, skip. */
			next_out++;
			older_unprocess_out = next_out;
		}
	}

	/* push the event into the kfifo */
	for (out = state->ring; out < last_out; out++)
		iio_push_to_buffers(indio_dev, (u8 *)out);
}

/*
 * cros_ec_ring_handler - the trigger handler function
 *
 * @state: device information.
 *
 * Called by the notifier, process the EC sensor FIFO queue.
 */
static void cros_ec_ring_handler(struct cros_ec_sensors_ring_state *state)
{
	struct iio_dev *indio_dev = state->core.indio_dev;
	struct cros_ec_fifo_info *fifo_info = &state->fifo_info;
	s64    fifo_timestamp, current_timestamp;
	int    i, j, number_data, ret;
	unsigned long sensor_mask = 0;
	struct ec_response_motion_sensor_data *in;
	struct cros_ec_sensors_ring_sample *out, *last_out;

	mutex_lock(&state->core.cmd_lock);
	/* Get FIFO information */
	fifo_timestamp = state->fifo_timestamp[NEW_TS];
	/* Copy elements in the main fifo */
	if (fifo_info->info.total_lost) {
		/* Need to retrieve the number of lost vectors per sensor */
		state->core.param.cmd = MOTIONSENSE_CMD_FIFO_INFO;
		if (cros_ec_motion_send_host_cmd(&state->core, 0)) {
			mutex_unlock(&state->core.cmd_lock);
			return;
		}
		memcpy(fifo_info, &state->core.resp->fifo_info,
		       sizeof(*fifo_info));
		fifo_timestamp = cros_ec_get_time_ns();
	}
	if (fifo_info->info.count > state->fifo_size ||
	    fifo_info->info.size != state->fifo_size) {
		dev_warn(&indio_dev->dev,
			 "Mismatch EC data: count %d, size %d - expected %d",
			 fifo_info->info.count, fifo_info->info.size,
			 state->fifo_size);
		mutex_unlock(&state->core.cmd_lock);
		return;
	}

	current_timestamp = state->fifo_timestamp[LAST_TS];
	out = state->ring;
	for (i = 0; i < fifo_info->info.count; i += number_data) {
		state->core.param.cmd = MOTIONSENSE_CMD_FIFO_READ;
		state->core.param.fifo_read.max_data_vector =
			fifo_info->info.count - i;
		ret = cros_ec_motion_send_host_cmd(&state->core,
			       sizeof(state->core.resp->fifo_read) +
			       state->core.param.fifo_read.max_data_vector *
			       sizeof(struct ec_response_motion_sensor_data));
		if (ret != EC_RES_SUCCESS) {
			dev_warn(&indio_dev->dev, "Fifo error: %d\n", ret);
			break;
		}
		number_data =
			state->core.resp->fifo_read.number_data;
		if (number_data == 0) {
			dev_dbg(&indio_dev->dev, "Unexpected empty FIFO\n");
			break;
		} else if (number_data > fifo_info->info.count - i) {
			dev_warn(&indio_dev->dev,
				 "Invalid EC data: too many entry received: %d, expected %d",
				 number_data, fifo_info->info.count - i);
			break;
		} else if (out + number_data >
			   state->ring + fifo_info->info.count) {
			dev_warn(&indio_dev->dev,
				 "Too many samples: %d (%zd data) to %d entries for expected %d entries",
				 i, out - state->ring, i + number_data,
				 fifo_info->info.count);
			break;
		}
		for (in = state->core.resp->fifo_read.data, j = 0;
		     j < number_data; j++, in++) {
			if (cros_ec_ring_process_event(
					state, fifo_info, fifo_timestamp,
					&current_timestamp, in, out)) {
				sensor_mask |= (1 << in->sensor_num);
				out++;
			}
		}
	}
	mutex_unlock(&state->core.cmd_lock);
	last_out = out;

	if (out == state->ring)
		/* Unexpected empty FIFO. */
		goto ring_handler_end;

	/*
	 * Check if current_timestamp is ahead of the last sample.
	 * Normally, the EC appends a timestamp after the last sample, but if
	 * the AP is slow to respond to the IRQ, the EC may have added new
	 * samples. Use the FIFO info timestamp as last timestamp then.
	 */
	if (!state->tight_timestamps &&
	    (last_out-1)->timestamp == current_timestamp)
		current_timestamp = fifo_timestamp;

	/* Check if buffer is set properly. */
	if (!indio_dev->active_scan_mask ||
	    (bitmap_empty(indio_dev->active_scan_mask,
			  indio_dev->masklength)))
		goto ring_handler_end;

	/* Warn on lost samples. */
	for_each_set_bit(i, &sensor_mask, BITS_PER_LONG) {
		if (fifo_info->info.total_lost) {
			int lost = fifo_info->lost[i];

			if (lost) {
				dev_warn(&indio_dev->dev,
					"Sensor %d: lost: %d out of %d\n", i,
					lost, fifo_info->info.total_lost);
				state->last_batch_len[i] = 0;
			}
		}
	}

	/*
	 * Spread samples in case of batching, then add them to the ringbuffer.
	 */
	if (state->tight_timestamps)
		cros_ec_ring_spread_add(state, sensor_mask, last_out);
	else
		cros_ec_ring_spread_add_legacy(state, sensor_mask,
					       current_timestamp, last_out);

ring_handler_end:
	state->fifo_timestamp[LAST_TS] = current_timestamp;
}

static int cros_ec_ring_event(struct notifier_block *nb,
	unsigned long queued_during_suspend, void *_notify)
{
	struct cros_ec_sensors_ring_state *state;
	struct cros_ec_device *ec;

	state = container_of(nb, struct cros_ec_sensors_ring_state, notifier);
	ec = state->core.ec;

	if (ec->event_data.event_type != EC_MKBP_EVENT_SENSOR_FIFO)
		return NOTIFY_DONE;

	if (ec->event_size != sizeof(ec->event_data.data.sensor_fifo)) {
		dev_warn(ec->dev, "Invalid fifo info size\n");
		return NOTIFY_DONE;
	}

	if (queued_during_suspend)
		return NOTIFY_OK;

	state->fifo_info.info = ec->event_data.data.sensor_fifo.info;
	state->fifo_timestamp[NEW_TS] = ec->last_event_time;
	cros_ec_ring_handler(state);
	return NOTIFY_OK;
}

/*
 * When the EC is suspending, we must stop sending interrupt,
 * we may use the same interrupt line for waking up the device.
 * Tell the EC to stop sending non-interrupt event on the iio ring.
 */
static int __maybe_unused cros_ec_ring_prepare(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);

	return cros_ec_ring_fifo_toggle(iio_priv(indio_dev), false);
}

static void __maybe_unused cros_ec_ring_complete(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);

	cros_ec_ring_fifo_toggle(iio_priv(indio_dev), true);
}

/*
 * Once we are ready to receive data, enable the interrupt
 * that allow EC to indicate events are available.
 */
static int cros_ec_ring_postenable(struct iio_dev *indio_dev)
{
	return cros_ec_ring_fifo_toggle(iio_priv(indio_dev), true);
}

static int cros_ec_ring_predisable(struct iio_dev *indio_dev)
{
	return cros_ec_ring_fifo_toggle(iio_priv(indio_dev), false);
}

static const struct iio_buffer_setup_ops cros_ec_ring_buffer_ops = {
	.postenable = cros_ec_ring_postenable,
	.predisable = cros_ec_ring_predisable,
};

#define CROS_EC_RING_ID(_id, _name)		\
{						\
	.type = IIO_ACCEL,			\
	.scan_index = _id,			\
	.scan_type = {				\
		.sign = 'u',			\
		.realbits = 8,			\
		.storagebits = 8,		\
	},					\
	.extend_name = _name,			\
}

#define CROS_EC_RING_AXIS(_axis)		\
{						\
	.type = IIO_ACCEL,			\
	.modified = 1,				\
	.channel2 = IIO_MOD_##_axis,		\
	.scan_index = CHANNEL_##_axis,		\
	.scan_type = {				\
		.sign = 's',			\
		.realbits = 16,			\
		.storagebits = 16,		\
	},					\
	.extend_name = "ring",			\
}

static const struct iio_chan_spec cros_ec_ring_channels[] = {
	CROS_EC_RING_ID(CHANNEL_SENSOR_ID, "id"),
	CROS_EC_RING_ID(CHANNEL_SENSOR_FLAG, "flag"),
	CROS_EC_RING_AXIS(X),
	CROS_EC_RING_AXIS(Y),
	CROS_EC_RING_AXIS(Z),
	IIO_CHAN_SOFT_TIMESTAMP(CHANNEL_TIMESTAMP)
};

static int cros_ec_ring_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct cros_ec_dev *ec_dev = dev_get_drvdata(dev->parent);
	struct cros_ec_device *ec_device;
	struct iio_dev *indio_dev;
	struct iio_buffer *buffer;
	struct cros_ec_sensors_ring_state *state;
	int ret;

	if (!ec_dev || !ec_dev->ec_dev) {
		dev_warn(&pdev->dev, "No CROS EC device found.\n");
		return -EINVAL;
	}
	ec_device = ec_dev->ec_dev;

	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*state));
	if (!indio_dev)
		return -ENOMEM;

	platform_set_drvdata(pdev, indio_dev);

	ret = cros_ec_sensors_core_init(pdev, indio_dev, false);
	if (ret)
		return ret;

	state = iio_priv(indio_dev);
	/*
	 * Disable the ring in case it was left enabled previously.
	 */
	ret = cros_ec_ring_fifo_toggle(state, false);
	if (ret)
		return ret;

	/* Retrieve FIFO information */
	state->core.param.cmd = MOTIONSENSE_CMD_FIFO_INFO;
	/* If it fails, just assume the FIFO is not supported.
	 * For other errors, the other sensor drivers would have noticed
	 * already.
	 */
	if (cros_ec_motion_send_host_cmd(&state->core, 0))
		return -ENODEV;

	/* Allocate the full fifo.
	 * We need to copy the whole FIFO to set timestamps properly *
	 */
	state->fifo_size = state->core.resp->fifo_info.size;
	state->ring = devm_kcalloc(&pdev->dev, state->fifo_size,
			sizeof(*state->ring), GFP_KERNEL);
	if (!state->ring)
		return -ENOMEM;

	state->fifo_timestamp[LAST_TS] = cros_ec_get_time_ns();

	indio_dev->channels = cros_ec_ring_channels;
	indio_dev->num_channels = ARRAY_SIZE(cros_ec_ring_channels);
	indio_dev->info = &ec_sensors_info;
	indio_dev->modes = INDIO_BUFFER_SOFTWARE;

	buffer = devm_iio_kfifo_allocate(indio_dev->dev.parent);
	if (!buffer)
		return -ENOMEM;

	iio_device_attach_buffer(indio_dev, buffer);
	indio_dev->setup_ops = &cros_ec_ring_buffer_ops;

	ret = devm_iio_device_register(indio_dev->dev.parent, indio_dev);
	if (ret < 0)
		return ret;

	state->tight_timestamps = cros_ec_check_features(ec_dev,
		EC_FEATURE_MOTION_SENSE_TIGHT_TIMESTAMPS);

	/* register the notifier that will act as a top half interrupt. */
	state->notifier.notifier_call = cros_ec_ring_event;
	ret = blocking_notifier_chain_register(&ec_device->event_notifier,
					       &state->notifier);
	if (ret < 0) {
		dev_warn(&indio_dev->dev, "failed to register notifier\n");
	}
	return ret;
}

static int cros_ec_ring_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct cros_ec_sensors_ring_state *state = iio_priv(indio_dev);
	struct cros_ec_device *ec = state->core.ec;

	/*
	 * Disable the ring, prevent EC interrupt to the AP for nothing.
	 */
	cros_ec_ring_fifo_toggle(state, false);
	blocking_notifier_chain_unregister(&ec->event_notifier,
					   &state->notifier);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static const struct dev_pm_ops cros_ec_ring_pm_ops = {
	.prepare = cros_ec_ring_prepare,
	.complete = cros_ec_ring_complete
};
#else
static const struct dev_pm_ops cros_ec_ring_pm_ops = { };
#endif

static struct platform_driver cros_ec_ring_platform_driver = {
	.driver = {
		.name	= DRV_NAME,
		.pm	= &cros_ec_ring_pm_ops,
	},
	.probe		= cros_ec_ring_probe,
	.remove		= cros_ec_ring_remove,
};
module_platform_driver(cros_ec_ring_platform_driver);

MODULE_DESCRIPTION("ChromeOS EC sensor hub ring driver");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_LICENSE("GPL v2");
