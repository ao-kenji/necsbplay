/*
 * Copyright (c) 2014 Kenji Aoyama.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>	/* getprogname(3) */
#include <unistd.h>	/* getopt(3) */
#include <errno.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/audioio.h>
#include <string.h>

#define BUFFER_SIZE 32768
u_int8_t  buf[BUFFER_SIZE];

/* global variables */
int debug = 0;
int rate = 11025;	/* default sampling rate */
FILE *wav_fp = NULL;

/* prototypes */
int read_wav_data(u_int8_t *, int, struct audio_info*);
int wav_open(char *);
void wav_close(void);
void print_audio_info(struct audio_info *ai);
void usage(void);

int
main(int argc, char **argv)
{
	int fd;
	int bps, nsamples;

	/*
	 * parse options
	 */
	int ch;
	extern char *optarg;
	extern int optind, opterr;

	while ((ch = getopt(argc, argv, "dr:")) != -1) {
		switch (ch) {
		case 'd':	/* debug flag */
			debug = 1;
			break;
		case 'r':	/* sampling rate */
			rate = atoi(optarg);
			break;
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1) {
		usage();
		return 1;
	}

	struct audio_info ai;
	AUDIO_INITINFO(&ai);

	fd = open("/dev/sound", O_RDWR, 0666);

	if (fd == -1) {
		perror("open");
		return 1;
	}

	/*
	 * set audio parameters
	 */
	if (ioctl(fd, AUDIO_GETINFO, &ai) == -1) {
		perror("ioctl AUDIO_GETINFO");
		return 1;
	}

	if (debug) print_audio_info(&ai);

	/* we assume Linear PCM, 11025Hz, 16bit, stereo */
	ai.play.sample_rate =  rate;
	ai.play.encoding = AUDIO_ENCODING_SLINEAR;
	ai.play.channels = 2;	/* stereo */
	ai.play.precision = 16;	/* 16 bit sampling */
	ai.play.bps = 2;	/* 2 bytes per sample */
	ai.play.msb = 1;	/* msb = 1 ?? */

	if (ioctl(fd, AUDIO_SETINFO, &ai) == -1) {
		perror("ioctl AUDIO_SETINFO");
		return 1;
	}

	if (debug) print_audio_info(&ai);

	/* bytes per sample */
	bps = ai.play.channels * ai.play.precision / 8;

	/*
	 * process a WAV file
	 */
	printf("open %s\n", argv[0]);
	if (wav_open(argv[0]) != 0)
		return 1;

	for(;;) {
		nsamples = read_wav_data(buf, 8192, &ai);
		write(fd, buf, nsamples * bps);
		if (feof(wav_fp))
			break;
	}

	wav_close();

	close(fd);

	return 0;
}

int
read_wav_data(u_int8_t *p, int max_samples, struct audio_info* ai)
{
	/*
	 * We use signed 16 bit data here.
	 */
	int bps;
	size_t nbytes;

	/* bytes per sample */
	bps = ai->play.channels * ai->play.precision / 8;

	/* If specified sample size is larger than the buffer size, clip it. */
	if (max_samples > 32768 / bps)
		max_samples = 32768 / bps;

	nbytes = fread(p, sizeof(u_int8_t), max_samples * bps, wav_fp);
	printf("%s: read %d samples\n", __func__, nbytes / bps);

	return nbytes / bps;	/* in samples */
}

int
wav_open(char *wav_file)
{
	if ((wav_fp = fopen(wav_file, "rb")) == NULL)
		return 1;

	/* XXX: skip 44 bytes = typical hearder size */
	if (fseek(wav_fp, 44, SEEK_SET) != 0) {
		fclose(wav_fp);
		return 1;
	}
	return 0;
}

void
wav_close(void)
{
	fclose(wav_fp);
	wav_fp = NULL;
}

void
print_audio_info(struct audio_info *ai) {

	printf("audio_info:\n");
	printf("\t.play.sample_rate = %d\n", ai->play.sample_rate);
	printf("\t.play.channels = %d\n", ai->play.channels);
	printf("\t.play.precision = %d\n", ai->play.precision);
	printf("\t.play.bps = %d\n", ai->play.bps);
	printf("\t.play.msb = %d\n", ai->play.msb);
	printf("\t.play.encoding = %d\n", ai->play.encoding);
	printf("\t.play.buffer_size = %d\n", ai->play.buffer_size);
	printf("\t.play.block_size = %d\n", ai->play.block_size);
	printf("\t.blocksize = %d\n", ai->blocksize);
	printf("\t.hiwat = %d\n", ai->hiwat);
	printf("\t.lowat = %d\n", ai->lowat);

	return;
}

void
usage(void)
{
	printf("Usage: %s [options] wavfile.wav\n", getprogname());
	printf("\t-d	: debug flag\n");
	printf("\t-r #	: sampling rate\n");
	printf("\twavfile must be LE, 16bit, stereo\n");
	exit(1);
}
