/*
 * Microsemi Switchtec(tm) PCIe Management Command Line Interface
 * Copyright (c) 2017, Microsemi Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

/*
 * This code serves as a simple example for sending custom MRPC commands.
 * For the purposes of the example, we retrieve the die temperature
 * and do an echo command.
 *
 * More example MRPC command implementations can be found in the library
 * source code.
 */

#include <switchtec/switchtec.h>
#include <switchtec/mrpc.h>

#include <time.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

typedef unsigned int   UINT32;
typedef unsigned short  UINT16;
typedef unsigned char   UINT8;
typedef unsigned char   BOOL;

#define true 1
#define false 0

static int echo_cmd(struct switchtec_dev *dev)
{
	int ret;

	/*
	 * This is just some example command packet. Your structure
	 * could resemble whatever data you are passing for the custom
	 * MRPC command.
	 */
	struct my_cmd {
		uint32_t sub_cmd_id;
		uint16_t param1;
		uint16_t param2;
		uint64_t time_val;
	} __attribute__((packed)) incmd = {
		.sub_cmd_id = 0xAA55,
		.param1 = 0x1234,
		.param2 = 0x5678,
		.time_val = time(NULL),
	};
	struct my_cmd outdata = {};

	ret = switchtec_cmd(dev, MRPC_ECHO, &incmd, sizeof(incmd),
			    &outdata, sizeof(outdata));
	if (ret) {
		switchtec_perror("echo_cmd");
		return 2;
	}

	if (incmd.sub_cmd_id != ~outdata.sub_cmd_id) {
		fprintf(stderr, "Echo data did not match!\n");
		return 3;
	}

	return 0;
}

static int die_temp(struct switchtec_dev *dev)
{
	uint32_t sub_cmd_id = MRPC_DIETEMP_SET_MEAS;
	uint32_t temp;
	int ret;

	ret = switchtec_cmd(dev, MRPC_DIETEMP, &sub_cmd_id,
			    sizeof(sub_cmd_id), NULL, 0);
	if (ret) {
		switchtec_perror("dietemp_set_meas");
		return 4;
	}

	sub_cmd_id = MRPC_DIETEMP_GET;
	ret = switchtec_cmd(dev, MRPC_DIETEMP, &sub_cmd_id,
			    sizeof(sub_cmd_id), &temp, sizeof(temp));
	if (ret) {
		switchtec_perror("dietemp_get");
		return 5;
	}

	printf("Die Temp: %.1f°C\n", temp / 100.);
	return 0;
}

/** */
typedef struct sfm_raw_req_cmd_t {
    /** */
    UINT8 sub_cmd;

    UINT8 rsvd;

    /** Command Target PDFID */
    UINT16 func_pdfid;

} sfm_raw_req_cmd_struct;

/** Configuration Request Command Format */
typedef struct sfm_raw_cfg_req_cmd_t {
    /** Basic Request Command */
    struct sfm_raw_req_cmd_t;

    /** Configuration Space Offset */
    UINT16 csr_offset;
    /** Byte Count */
    UINT8 byte_count;
    /** cfg_rsvd_0 */
    UINT8 cfg_rsvd_0;

    /** Configuration Request Data */
    UINT32 cfg_wr_data;

} sfm_raw_cfg_req_cmd_struct;

/** Configuration Response Format */
typedef struct sfm_raw_cfg_rsp_t {
    /** Completion Status */
    UINT8 cpl_stat;
    /** Reserved Areas */
    UINT8 rsvd0;
    UINT8 rsvd1;
    UINT8 rsvd2;

    /** Cfg Read Data */
    UINT32 cfg_rd_data;
} sfm_raw_cfg_rsp_struct;


static int cfg_req( struct switchtec_dev *dev, BOOL is_read, UINT16 pdfid, UINT16 csr_off, UINT8 byte_count, UINT32 wr_data )
{
    int ret = 0;
    sfm_raw_cfg_req_cmd_struct * cfg_req = malloc( sizeof( sfm_raw_cfg_req_cmd_struct ) );

    if( NULL == cfg_req )
    {
        exit(1);
    }

    cfg_req->csr_offset = csr_off;
    cfg_req->byte_count = byte_count;
    cfg_req->func_pdfid = pdfid;

    if( true == is_read )
    {
        cfg_req->sub_cmd = 0;
    }
    else
    {
        cfg_req->sub_cmd = 1;
        cfg_req->cfg_wr_data = wr_data;
    }

    /* Output Buffer */
    sfm_raw_cfg_rsp_struct outdata;

	ret = switchtec_cmd(dev, 0x8E, cfg_req, sizeof(sfm_raw_cfg_req_cmd_struct),
			    &outdata, sizeof(outdata));
	if (ret)
  {
		switchtec_perror("error cfg request");
		return 2;
	}
    else
    {
        if( true == is_read )
        {
            printf("Cfg Rd: Data [%x] Stat[%d]\n", outdata.cfg_rd_data, outdata.cpl_stat );
        }
        else
        {
            printf("Cfg Wr: Stat[%d]\n", outdata.cpl_stat );
        }
    }

    return 0;
}

int main(int argc, char *argv[])
{
	struct switchtec_dev *dev;
	int ret = 0;
	const char *devpath;

    /* Cfg Req Parameters */
    BOOL is_read;
    UINT16 pdfid;
    UINT16 csr_off;
    UINT8 byte_cnt;
    UINT32 wr_dat;

	if (argc < 6) {
		fprintf(stderr, "USAGE: %s <pdfid> <1/0 Rd/Wr> <CSR_OFF> <BYTE_CNT> <WR_DAT>\n", argv[0]);
		return 1;
	} else if (argc == 2) {
		devpath = argv[1];
	} else {
		devpath = "/dev/switchtec0";

        sscanf( argv[1],"%x", & pdfid );
        is_read = atoi( argv[2] );
        csr_off = atoi( argv[3] );
        byte_cnt = atoi( argv[4] );
        wr_dat = atoi( argv[5] );
	}

	dev = switchtec_open(devpath);
	if (!dev) {
		switchtec_perror(devpath);
		return 1;
	}

    ret = cfg_req( dev, is_read, pdfid, csr_off, byte_cnt, wr_dat );

out:
	switchtec_close(dev);
	return ret;
}
