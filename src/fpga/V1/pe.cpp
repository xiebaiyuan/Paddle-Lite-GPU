/* Copyright (c) 2018 PaddlePaddle Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#include "fpga/common/pe.h"
#include <unistd.h>
#include <iomanip>
#include <iostream>
#include "common/types.h"
#include "fpga/V1/filter.h"
#include "fpga/V1/image.h"
#include "fpga/common/config.h"
#include "fpga/common/driver.h"
using namespace std;
using namespace paddle_mobile::fpga::driver;  // NOLINT

namespace paddle_mobile {
namespace fpga {

#define IMAGE_ALIGN 16
#define FILTER_ALIGN 16
#define FILTER_NUM_ALIGN 32
#define USE_RELU 1
#define USE_BIAS 2

// bypass cmd
#define CMD_FP16_TO_FP16 0
#define CMD_FP16_TO_FP32 1
#define CMD_FP32_TO_FP16 2
#define CMD_FP32_TO_FP32 3

// bypass macro
#define SIZE_FP16 2
#define SIZE_FP32 4

#define PE_IRQ_TIMEOUT 1000000

/* Interrupt bit-set offset*/
#define INTERRUPT_RSVD 0x0001
#define INTERRUPT_BYPASS 0x0002
#define INTERRUPT_CONV 0x0004
#define INTERRUPT_POOLING 0x0008
#define INTERRUPT_EW 0x0010
//#define INTERRUPT_RESIZE                        0x0020

/* Register offset */
#define REG_INTERRUPT 0x000
#define REG_VERSION 0x008
#define REG_TEMPERATURE 0x010
#define REG_FPGA_RESET 0x018
#define REG_TEST_REGISTER 0x048
#define REG_HARDWARE_STATUS 0x050

#define REG_TIMER_COUNTER 0x070

#define REG_SCALE_PARAMETER 0x080

#define REG_FLASH_CMD 0x200
#define REG_FLASH_DATA 0x208
#define REG_FLASH_CONFIG 0x210
#define REG_FLASH_STATUS 0x218
#define REG_SN 0x220

//#define REG_READ_SCALE
//#define REG_WRITE_SCALE

/*bypass*/
#define REG_CONVERT_CMD 0x400
#define REG_CONVERT_SRC_ADDR 0x408
#define REG_CONVERT_DST_ADDR 0x410
#define REG_CONVERT_LENGTH 0x418

/*resize*/
#define REG_RESIZE_CMD 0x600
#define REG_RESIZE_CHANNEL_NUMBER 0x608
#define REG_RESIZE_INPUT_IMAGE_PIXEL 0x610
#define REG_RESIZE_OUTPUT_IMAGE_PIXEL 0x618
#define REG_RESIZE_INPUT_BASE_ADDR 0x620
#define REG_RESIZE_WEIGHT_BASE_ADDR 0x628
#define REG_RESIZE_SRC_POS_BASE_ADDR 0x630
#define REG_RESIZE_OUTPUT_BASE_ADDR 0x638

/*pooling*/
#define REG_POOLING_CMD 0x800
#define REG_POOLING_IMAGE_BASE_ADDR 0x808
#define REG_POOLING_RESULT_BASE_ADDR 0x810
#define REG_POOLING_IMAGE_PIXEL 0x818
#define REG_POOLING_WINDOW_SIZE 0x820
#define REG_POOLING_RESULT_PIXEL 0x828
#define REG_POOLING_PAD_PIXEL 0x830
#define REG_POOLING_STEP_PIXEL 0x838
#define REG_POOLING_CHANNEL_NUMBER 0x840
#define REG_POOLING_IMAGE_AMOUNT_PER_ROW 0x848
#define REG_POOLING_IMAGE_ONE_PAD_PER_ROW 0x850
#define REG_POOLING_IMAGE_TWO_PAD_PER_ROW 0x858
#define REG_POOLING_IMAGE_ROW_MUL_WINDOW_HEIGHT 0x860
#define REG_POOLING_IMAGE_ROW_MUL_PAD_HEIGHT 0x868
#define REG_POOLING_IMAGE_ROW_MUL_STEP_HEIGHT 0x870
#define REG_POOLING_RESULT_AMOUNT_ALIGN_32 0x878
#define REG_POOLING_RESULT_AMOUNT_ALIGN_64 0x880
#define REG_POOLING_IMAGE_CALCU_HEIGHT 0x888
#define REG_POOLING_IMAGE_PADLEFT_SKIPWINDOW 0x898
#define REG_POOLING_MODE_RECIPROCAL 0x890

/*conv*/
#define REG_CONV_CMD 0xC00
#define REG_CONV_IMAGE_BASE_ADDR 0xC08
#define REG_CONV_FILTER_BASE_ADDR 0xC10
#define REG_CONV_SB_BASE_ADDR 0xC18
#define REG_CONV_RESULT_BASE_ADDR 0xC20
#define REG_CONV_IMAGE_PIXEL 0xC28
#define REG_CONV_FILTER_PIXEL 0xC30
#define REG_CONV_RESULT_PIXEL 0xC38
#define REG_CONV_PAD_PIXEL 0xC40
#define REG_CONV_STEP_PIXEL 0xC48
#define REG_CONV_GROUP_NUMBER 0xC50
#define REG_CONV_FILTER_NUMBER 0xC58
#define REG_CONV_CHANNEL_NUMBER 0xC60
#define REG_CONV_FILTER_PER_GROUP 0xC68
#define REG_CONV_CHANNEL_PER_GROUP 0xC70
#define REG_CONV_IMAGE_AMOUNT_PER_ROW 0xC78
#define REG_CONV_IMAGE_ONE_PAD_PER_ROW 0xC80
#define REG_CONV_IMAGE_TWO_PAD_PER_ROW 0xC88
#define REG_CONV_FILTER_AMOUNT_ALL 0xC90
#define REG_CONV_RESULT_AMOUNT_PER_ROW 0xC98
#define REG_CONV_RESULT_LAST_VALID 0xCA0

#define REG_CONV_BLOCK_AMOUNT_PER_ROW 0xCA8
#define REG_CONV_FILTER_PAD_WIDTH_MUL_CH 0xCB0
#define REG_CONV_IMAGE_AMOUNT_PER_ROW_MUL_WIN_F 0xCB8
#define REG_CONV_IMAGE_AMOUNT_PER_ROW_MUL_WIN 0xCC0
#define REG_CONV_IMAGE_BLOCK_NUM 0xCC8
#define REG_CONV_IMAGE_BLOCK_LEN 0xCD0
#define REG_CONV_IMAGE_BLOCK_LEN_LAST 0xCD8
#define REG_CONV_IMAGE_WIN_CNT 0xCE0
#define REG_CONV_IMAGE_WIN_CNT_LAST 0xCE8
#define REG_CONV_RES_ROW_DATA_ALIGN4_PAD 0xCF8
#define REG_CONV_PROG_FULL_CNT 0xD08
#define REG_CONV_POST_PROG_FULL_CNT 0xD10
#define REG_CONV_FPGA_BIAS_SCALE_LEN 0xD20

#define REG_CONV_IMAGE_SCALE 0xD28
#define REG_CONV_FILTER_SCALE 0xD30

/*ew*/
#define REG_EW_CMD 0x0F00
#define REG_EW_IMAGE0_BASE_ADDR 0x0F08
#define REG_EW_IMAGE1_BASE_ADDR 0x0F10
#define REG_EW_RESULT_BASE_ADDR 0x0F18
#define REG_EW_DATA_LEN 0x0F20
#define REG_EW_COEFFICIENT 0x0F28
#define REG_EW_IMAGE_PIXEL 0x0F30
#define REG_EW_IMAGE_AMOUNT_PER_ROW 0x0F38

int ComputeFpgaConv(const struct SplitConvArgs &args) {
//  ComputeBasicConv(args.conv_arg[0]);
#ifdef FPGA_PRINT_MODE
  DLOG << "=============ComputeFPGAConv===========";
  DLOG << "   filter_num:" << args.filter_num
       << "   group_num:" << args.group_num
       << "   split_num:" << args.split_num;
#endif

  int split_num = args.split_num;
  for (int i = 0; i < split_num; i++) {
    ComputeBasicConv(args.conv_arg[i]);
  }

  if (split_num > 1) {
    ComputeFPGAConcat(args.concat_arg);
  }
}

int ComputeBasicConv(const struct ConvArgs &args) {
#ifdef FPGA_PRINT_MODE
  DLOG << "======Compute Basic Conv======";
  DLOG << "   relu_enabled:" << args.relu_enabled
       << "   sb_address:" << args.sb_address
       << "   filter_address:" << args.filter_address
       << "   filter_num:" << args.filter_num
       << "   group_num:" << args.group_num;
  DLOG << "   image_address:" << args.image.address
       << "   image_scale_address:" << args.image.scale_address
       << "   image_channels:" << args.image.channels
       << "   image_height:" << args.image.height
       << "   image_width:" << args.image.width
       << "   pad_height:" << args.image.pad_height
       << "   pad_width:" << args.image.pad_width;
  DLOG << "   kernel_height:" << args.kernel.height
       << "   kernel_width:" << args.kernel.width
       << "   stride_h:" << args.kernel.stride_h
       << "   stride_w:" << args.kernel.stride_w;
  DLOG << "   out_address:" << args.output.address
       << "   out_scale_address:" << args.output.scale_address;
#endif

#ifdef PADDLE_MOBILE_ZU5
  int ret = 0;
  uint64_t output_scale = 0;
  /*
  uint64_t output_scale;
  uint64_t image_scale;
  uint64_t filter_scale;
  uint64_t image_address_phy = 0;
  uint64_t sb_address_phy = 0;
  uint64_t filter_address_phy = 0;
  uint64_t output_address_phy = 0;


  fpga_copy(&image_scale, args.image.scale_address, 2 * sizeof(float));
  fpga_copy(&filter_scale, args.filter_scale_address, 2 * sizeof(float));
  uint64_t filterlen = (uint64_t)args.kernel.width *
                       (uint64_t)args.kernel.height *
                       (uint64_t)args.image.channels;
  filterlen = align_to_x(filterlen, FILTER_ALIGN);
  filterlen *= align_to_x((uint64_t)args.filter_num, FILTER_NUM_ALIGN);
  uint64_t fpga_bias_scale_len =
      align_to_x(args.filter_num / args.group_num, 8) * args.group_num;

  uint64_t output_height =
      (args.image.height + args.image.pad_height * 2 - args.kernel.height) /
          args.kernel.stride_h +
      1;
  uint64_t output_width =
      (args.image.width + args.image.pad_width * 2 - args.kernel.width) /
          args.kernel.stride_w +
      1;
  uint64_t output_size =
      output_height * output_width * (uint64_t)args.filter_num;

  uint64_t filter_per_group = (uint64_t)(args.filter_num / args.group_num);
  uint64_t channel_per_group = (uint64_t)(args.image.channels / args.group_num);

  uint64_t image_row_count = ((uint64_t)args.image.width) *
                             ((uint64_t)args.image.channels);  // without align
  uint64_t image_amount_per_row = align_to_x(image_row_count, IMAGE_ALIGN);
  uint64_t image_one_pad_per_row =
      align_to_x(image_row_count, IMAGE_ALIGN) +
      ((uint64_t)args.image.pad_width) * ((uint64_t)args.image.channels);
  uint64_t filter_amount_all =
      align_to_x(((uint64_t)args.kernel.height) *
                     ((uint64_t)args.kernel.width) * channel_per_group,
                 FILTER_ALIGN);

  uint64_t output_amount_per_row =
      align_to_x(output_width * ((uint64_t)args.filter_num), IMAGE_ALIGN);

  // find the opt partition strategy
  uint64_t res_win;
  uint64_t res_fit = 0;
  for (res_win = 1; res_win <= output_width; res_win = res_win + 1) {
    if ((align_to_x(
             (args.image.channels *
              (args.kernel.width + (res_win - 1) * args.kernel.stride_w)),
             IMAGE_ALIGN) /
             16 +
         1) *
            args.kernel.height >
        2048) {
      break;
    }
  }

  if (res_win != output_width) {
    res_win -= 1;
  }

  if (((res_win % 2) != 0) && (res_win != 1)) {
    res_win = res_win - 1;
  }
  res_fit = res_win;

  uint64_t block_num = (output_width + res_fit - 1) / res_fit;
  uint64_t block_len = res_fit;
  uint64_t block_last = output_width - res_fit * (block_num - 1);

  uint64_t res_amount_per_row = output_width * args.filter_num;
  uint64_t res_amount_per_row_pad = output_amount_per_row - res_amount_per_row;

  uint64_t image_block_amount_per_row =
      args.kernel.stride_w * (res_fit)*args.image.channels;
  uint64_t filter_pad_width_mul_channel =
      args.image.pad_width * args.image.channels;
  uint64_t image_amount_per_row_multi_win_first =
      image_amount_per_row * (4 * args.kernel.stride_h - args.image.pad_height);
  uint64_t image_amount_per_row_multi_win =
      image_amount_per_row * (4 * args.kernel.stride_h);

  uint64_t image_block_num = block_num;
  uint64_t image_block_len =
      align_to_x((args.image.channels *
                  (args.kernel.width + (block_len - 1) * args.kernel.stride_w)),
                 IMAGE_ALIGN) /
          16 +
      1;
  uint64_t image_block_len_last =
      align_to_x(
          (args.image.channels *
           (args.kernel.width + (block_last - 1) * args.kernel.stride_w)),
          IMAGE_ALIGN) /
          16 +
      1;
  uint64_t image_win_cnt = block_len;
  uint64_t image_win_cnt_last = block_last;
  uint64_t res_row_data_align4_pad = res_amount_per_row_pad / 8;
  uint64_t prog_full_cnt = 2048 / (filter_amount_all / 16 * 2) - 1;
  if (prog_full_cnt == 1023) {
    prog_full_cnt--;
  }
  uint64_t post_prog_full_cnt =
      (512 / (align_to_x(args.filter_num, 4) / 4 * 2) > 2)
          ? (512 / (align_to_x(args.filter_num, 4) / 4 * 2) - 2)
          : 0;

  image_address_phy = vaddr_to_paddr(args.image.address);
  sb_address_phy = vaddr_to_paddr(args.sb_address);
  filter_address_phy = vaddr_to_paddr(args.filter_address);
  output_address_phy = vaddr_to_paddr(args.output.address);

  uint64_t cmd = 0UL | (args.relu_enabled ? USE_RELU : 0) | USE_BIAS;
*/

  pthread_mutex_lock(&g_fpgainfo.pe_data->mutex);
  if (ERROR == g_fpgainfo.pe_data->pes[PE_IDX_CONV]->status) {
    ret = -EIO;
    DLOG << "Conv Status Error!";
    pthread_mutex_unlock(&g_fpgainfo.pe_data->mutex);
    return ret;
  }
  reg_writeq(output_scale, REG_SCALE_PARAMETER);
  reg_writeq(
      ((uint64_t)args.image.height) | (((uint64_t)args.image.width) << 32),
      REG_CONV_IMAGE_PIXEL);
  reg_writeq(
      ((uint64_t)args.kernel.height) | (((uint64_t)args.kernel.width) << 32),
      REG_CONV_FILTER_PIXEL);
  reg_writeq(args.driver.output_height | (args.driver.output_width << 32),
             REG_CONV_RESULT_PIXEL);
  reg_writeq(((uint64_t)args.image.pad_height) |
                 (((uint64_t)args.image.pad_width) << 32),
             REG_CONV_PAD_PIXEL);
  reg_writeq(((uint64_t)args.kernel.stride_h) |
                 (((uint64_t)args.kernel.stride_w) << 32),
             REG_CONV_STEP_PIXEL);
  reg_writeq((uint64_t)args.group_num, REG_CONV_GROUP_NUMBER);
  reg_writeq((uint64_t)args.filter_num, REG_CONV_FILTER_NUMBER);
  reg_writeq((uint64_t)args.image.channels, REG_CONV_CHANNEL_NUMBER);
  reg_writeq(*(uint64_t *)args.image.scale_address, REG_CONV_IMAGE_SCALE);
  reg_writeq(*(uint64_t *)args.filter_scale_address, REG_CONV_FILTER_SCALE);

  reg_writeq(args.driver.image_address_phy, REG_CONV_IMAGE_BASE_ADDR);
  reg_writeq(args.driver.filter_address_phy, REG_CONV_FILTER_BASE_ADDR);
  reg_writeq(args.driver.sb_address_phy, REG_CONV_SB_BASE_ADDR);
  reg_writeq(args.driver.output_address_phy, REG_CONV_RESULT_BASE_ADDR);
  reg_writeq(args.driver.filter_per_group, REG_CONV_FILTER_PER_GROUP);
  reg_writeq(args.driver.channel_per_group, REG_CONV_CHANNEL_PER_GROUP);
  reg_writeq(args.driver.image_amount_per_row, REG_CONV_IMAGE_AMOUNT_PER_ROW);
  reg_writeq(args.driver.image_one_pad_per_row, REG_CONV_IMAGE_ONE_PAD_PER_ROW);
  reg_writeq(args.driver.filter_amount_all, REG_CONV_FILTER_AMOUNT_ALL);
  reg_writeq(args.driver.output_amount_per_row, REG_CONV_RESULT_AMOUNT_PER_ROW);
  reg_writeq(args.driver.image_block_amount_per_row, 0xca8);
  reg_writeq(args.driver.filter_pad_width_mul_channel, 0xcb0);
  reg_writeq(args.driver.image_amount_per_row_multi_win_first, 0xcb8);
  reg_writeq(args.driver.image_amount_per_row_multi_win, 0xcc0);
  reg_writeq(args.driver.image_block_num, 0xcc8);
  reg_writeq(args.driver.image_block_len, 0xcd0);
  reg_writeq(args.driver.image_block_len_last, 0xcd8);
  reg_writeq(args.driver.image_win_cnt, 0xce0);
  reg_writeq(args.driver.image_win_cnt_last, 0xce8);
  reg_writeq(args.driver.res_row_data_align4_pad, 0xcf8);
  reg_writeq(args.driver.prog_full_cnt, 0xd08);
  reg_writeq(args.driver.post_prog_full_cnt, 0xd10);
  reg_writeq(args.driver.fpga_bias_scale_len / 4, 0xd20);

  reg_writeq(args.driver.cmd, REG_CONV_CMD);

  if (0 != fpga_regpoll(REG_INTERRUPT, INTERRUPT_CONV, PE_IRQ_TIMEOUT)) {
    g_fpgainfo.pe_data->pes[PE_IDX_CONV]->status = ERROR;
    ret = -EIO;
    DLOG << "Conv Wait Irq Timeout!";
  }

  output_scale = reg_readq(REG_SCALE_PARAMETER);
  output_scale = (output_scale << 32) | (output_scale >> 32);
  fpga_copy(args.output.scale_address, &output_scale, sizeof(float) * 2);

  pthread_mutex_unlock(&g_fpgainfo.pe_data->mutex);

  return ret;
#endif

  return 0;
}

int ComputeFpgaPool(const struct PoolingArgs &args) {
#ifdef FPGA_PRINT_MODE
  DLOG << "=============ComputeFpgaPool===========";
  DLOG << "   mode:" << args.mode
       << "   kernel_reciprocal:" << fp16_2_fp32(args.kernel_reciprocal);
  DLOG << "   image_address:" << args.image.address
       << "   image_scale_address:" << args.image.scale_address
       << "   image_channels:" << args.image.channels
       << "   image_height:" << args.image.height
       << "   image_width:" << args.image.width
       << "   pad_height:" << args.image.pad_height
       << "   pad_width:" << args.image.pad_width;
  DLOG << "   kernel_height:" << args.kernel.height
       << "   kernel_width:" << args.kernel.width
       << "   stride_h:" << args.kernel.stride_h
       << "   stride_w:" << args.kernel.stride_w;
  DLOG << "   out_address:" << args.output.address
       << "   out_scale_address:" << args.output.scale_address;
#endif
#ifdef PADDLE_MOBILE_ZU5
  DLOG << "Polling";
  // return 0;
  uint64_t output_scale = 0;
  uint64_t timer_cnt = 0;
  int ret = 0;
  uint64_t cmd = 0;

  uint64_t image_physical_address = 0;
  uint64_t output_physical_address = 0;

  image_physical_address = vaddr_to_paddr(args.image.address);
  output_physical_address = vaddr_to_paddr(args.output.address);

  uint32_t output_height = (uint32_t)(
      (args.image.height + args.image.pad_height * 2 - args.kernel.height) /
          args.kernel.stride_h +
      1);
  uint32_t output_width = (uint32_t)(
      (args.image.width + args.image.pad_width * 2 - args.kernel.width) /
          args.kernel.stride_w +
      1);

  uint64_t image_amount_per_row = align_to_x(
      (uint64_t)args.image.width * (uint64_t)args.image.channels, IMAGE_ALIGN);
  uint64_t image_one_pad_per_row =
      align_to_x((uint64_t)args.image.width * (uint64_t)args.image.channels,
                 FILTER_ALIGN) +
      (uint64_t)args.image.pad_width * (uint64_t)args.image.channels;
  uint64_t image_two_pad_per_row = align_to_x(
      ((uint64_t)args.image.width + (uint64_t)args.image.pad_width * 2) *
          (uint64_t)args.image.channels,
      IMAGE_ALIGN);
  uint64_t image_row_mul_pooling_hight =
      image_amount_per_row * (uint64_t)args.kernel.height;
  uint64_t image_row_mul_pad_hight =
      image_amount_per_row * (uint64_t)args.image.pad_height;
  uint64_t image_row_mul_step_hight =
      image_amount_per_row * (uint64_t)args.kernel.stride_h;
  uint64_t result_amount_align_32 = align_to_x(
      (uint64_t)output_width * (uint64_t)args.image.channels, FILTER_ALIGN);
  uint64_t result_amount_align_64 = align_to_x(
      (uint64_t)output_width * (uint64_t)args.image.channels, IMAGE_ALIGN);
  uint64_t image_calcu_height =
      (uint64_t)args.kernel.height +
      ((uint64_t)output_height - 1) * (uint64_t)args.kernel.stride_h;

  uint64_t image_pad_left = args.image.channels * args.image.pad_width;
  uint64_t image_skip_window = args.image.channels * args.kernel.stride_w;

  uint64_t image_padleft_skipwindow =
      (image_skip_window << 32) | image_pad_left;

  uint64_t mode_reciprocal = (uint64_t)0 | ((uint64_t)args.mode) << 16 |
                             (((uint64_t)args.kernel_reciprocal));

  pthread_mutex_lock(&g_fpgainfo.pe_data->mutex);
  if (ERROR == g_fpgainfo.pe_data->pes[PE_IDX_POOLING]->status) {
    ret = -EIO;
    DLOG << "Conv Status Error!";
    pthread_mutex_unlock(&g_fpgainfo.pe_data->mutex);
    return ret;
  }

  /*restart scale*/
  reg_writeq(output_scale, REG_SCALE_PARAMETER);

  reg_writeq(image_physical_address, REG_POOLING_IMAGE_BASE_ADDR);
  reg_writeq(output_physical_address, REG_POOLING_RESULT_BASE_ADDR);

  reg_writeq(
      ((uint64_t)args.image.height) | (((uint64_t)args.image.width) << 32),
      REG_POOLING_IMAGE_PIXEL);
  reg_writeq(
      ((uint64_t)args.kernel.height) | (((uint64_t)args.kernel.width) << 32),
      REG_POOLING_WINDOW_SIZE);

  reg_writeq(((uint64_t)output_height) | (((uint64_t)output_width) << 32),
             REG_POOLING_RESULT_PIXEL);

  reg_writeq(((uint64_t)args.image.pad_height) |
                 (((uint64_t)args.image.pad_width) << 32),
             REG_POOLING_PAD_PIXEL);
  reg_writeq(((uint64_t)args.kernel.stride_h) |
                 (((uint64_t)args.kernel.stride_w) << 32),
             REG_POOLING_STEP_PIXEL);

  reg_writeq((uint64_t)args.image.channels, REG_POOLING_CHANNEL_NUMBER);

  reg_writeq(image_amount_per_row, REG_POOLING_IMAGE_AMOUNT_PER_ROW);
  reg_writeq(image_one_pad_per_row, REG_POOLING_IMAGE_ONE_PAD_PER_ROW);
  reg_writeq(image_two_pad_per_row, REG_POOLING_IMAGE_TWO_PAD_PER_ROW);

  reg_writeq(image_row_mul_pooling_hight,
             REG_POOLING_IMAGE_ROW_MUL_WINDOW_HEIGHT);
  reg_writeq(image_row_mul_pad_hight, REG_POOLING_IMAGE_ROW_MUL_PAD_HEIGHT);
  reg_writeq(image_row_mul_step_hight, REG_POOLING_IMAGE_ROW_MUL_STEP_HEIGHT);

  reg_writeq(result_amount_align_32, REG_POOLING_RESULT_AMOUNT_ALIGN_32);
  reg_writeq(result_amount_align_64, REG_POOLING_RESULT_AMOUNT_ALIGN_64);

  reg_writeq(image_calcu_height, REG_POOLING_IMAGE_CALCU_HEIGHT);

  reg_writeq(image_padleft_skipwindow, REG_POOLING_IMAGE_PADLEFT_SKIPWINDOW);
  reg_writeq(mode_reciprocal, REG_POOLING_MODE_RECIPROCAL);

  /*SDK刷Cache保证数据一致性*/

  reg_writeq(cmd, REG_POOLING_CMD);

  DLOG << "before reg poll";
  if (0 != fpga_regpoll(REG_INTERRUPT, INTERRUPT_POOLING, PE_IRQ_TIMEOUT)) {
    g_fpgainfo.pe_data->pes[PE_IDX_POOLING]->status = ERROR;
    ret = -EIO;
    DLOG << "Pooling Wait Irq Timeout!";
  }
  DLOG << "after reg poll";

  // *(args.output.scale_address) = reg_readq(REG_SCALE_PARAMETER);
  output_scale = reg_readq(REG_SCALE_PARAMETER);
  output_scale = (output_scale << 32) | (output_scale >> 32);
  fpga_copy(args.output.scale_address, &output_scale, sizeof(float) * 2);
  //*(args.output.timer_cnt) = reg_readq(REG_TIMER_COUNTER);
  pthread_mutex_unlock(&g_fpgainfo.pe_data->mutex);

  return ret;
#endif
  return 0;
}

int ComputeFpgaEWAdd(const struct EWAddArgs &args) {
#ifdef FPGA_PRINT_MODE
  DLOG << "=============ComputeFpgaEWAdd===========";
  DLOG << "   relu_enabled:" << args.relu_enabled
       << "   const0:" << fp16_2_fp32(int16_t(args.const0))
       << "   const1:" << fp16_2_fp32(int16_t(args.const1));
  DLOG << "   image0_address:" << args.image0.address
       << "   image0_scale_address:" << args.image0.scale_address
       << "   image0_channels:" << args.image0.channels
       << "   image0_height:" << args.image0.height
       << "   image0_width:" << args.image0.width
       << "   pad0_height:" << args.image0.pad_height
       << "   pad0_width:" << args.image0.pad_width;
  DLOG << "   image1_address:" << args.image1.address
       << "   image1_scale_address:" << args.image1.scale_address
       << "   image1_channels:" << args.image1.channels
       << "   image1_height:" << args.image1.height
       << "   image1_width:" << args.image1.width
       << "   pad1_height:" << args.image1.pad_height
       << "   pad_width:" << args.image1.pad_width;
  DLOG << "   out_address:" << args.output.address
       << "   out_scale_address:" << args.output.scale_address;
#endif
#ifdef PADDLE_MOBILE_ZU5
  int ret = 0;
  uint64_t output_scale = 0;
  /*uint64_t timer_cnt = 0;
  uint64_t image0_address_phy = 0;
  uint64_t image1_address_phy = 0;
  uint64_t output_address_phy = 0;

  uint64_t cmd = args.relu_enabled ? USE_RELU : 0;
  uint64_t datalen = (uint64_t)args.image0.width *
                     (uint64_t)args.image0.height *
                     (uint64_t)args.image0.channels;
  uint64_t coefficient = (uint64_t)args.const0 << 32 | (uint64_t)args.const1;
  image0_address_phy = vaddr_to_paddr(args.image0.address);
  image1_address_phy = vaddr_to_paddr(args.image1.address);
  output_address_phy = vaddr_to_paddr(args.output.address);

  uint64_t image_amount_per_row =
  align_to_x((uint64_t)args.image0.width * (uint64_t)args.image0.channels,
             IMAGE_ALIGN);
  uint64_t image_image_pixel = ((uint64_t)args.image0.channels << 32) |
                               ((uint64_t)args.image0.width << 16) |
                               (uint64_t)args.image0.height;*/

  pthread_mutex_lock(&g_fpgainfo.pe_data->mutex);
  if (ERROR == g_fpgainfo.pe_data->pes[PE_IDX_EW]->status) {
    ret = -EIO;
    DLOG << "EW Status Error!";
    pthread_mutex_unlock(&g_fpgainfo.pe_data->mutex);
    return ret;
  }

  reg_writeq(output_scale, REG_SCALE_PARAMETER);
  reg_writeq(args.driver.image0_address_phy, REG_EW_IMAGE0_BASE_ADDR);
  reg_writeq(args.driver.image1_address_phy, REG_EW_IMAGE1_BASE_ADDR);
  reg_writeq(args.driver.datalen, REG_EW_DATA_LEN);
  reg_writeq(args.driver.image_image_pixel, REG_EW_IMAGE_PIXEL);
  reg_writeq(args.driver.image_amount_per_row, REG_EW_IMAGE_AMOUNT_PER_ROW);
  reg_writeq(args.driver.output_address_phy, REG_EW_RESULT_BASE_ADDR);
  reg_writeq(args.driver.coefficient, REG_EW_COEFFICIENT);
  reg_writeq(args.driver.cmd, REG_EW_CMD);

  if (0 != fpga_regpoll(REG_INTERRUPT, INTERRUPT_POOLING, PE_IRQ_TIMEOUT)) {
    g_fpgainfo.pe_data->pes[PE_IDX_EW]->status = ERROR;
    ret = -EIO;
    DLOG << "EW Wait Irq Timeout!";
  }

  output_scale = reg_readq(REG_SCALE_PARAMETER);
  output_scale = (output_scale << 32) | (output_scale >> 32);
  fpga_copy(args.output.scale_address, &output_scale, sizeof(float) * 2);
  //*(args.output.scale_address) = reg_readq(REG_SCALE_PARAMETER);
  //*(args.output.timer_cnt) = reg_readq(REG_TIMER_COUNTER);
  pthread_mutex_unlock(&g_fpgainfo.pe_data->mutex);
  return ret;
#endif
  return 0;
}

int PerformBypass(const struct BypassArgs &args) {
#ifdef FPGA_PRINT_MODE
  DLOG << "=============ComputeFpgaBypass===========";
  DLOG << "   input_type:" << args.input_data_type
       << "   output_type:" << args.output_data_type
       << "   input_layout_type:" << args.input_layout_type
       << "   output_layout_type:" << args.output_layout_type;
  DLOG << "   image_address:" << args.image.address
       << "   image_scale_address:" << args.image.scale_address
       << "   image_channels:" << args.image.channels
       << "   image_height:" << args.image.height
       << "   image_width:" << args.image.width
       << "   pad_height:" << args.image.pad_height
       << "   pad_width:" << args.image.pad_width;
  DLOG << "   out_address:" << args.output.address
       << "   out_scale_address:" << args.output.scale_address;
#endif
#ifdef PADDLE_MOBILE_ZU5
  DLOG << "Bypass";
  // return 0;
  struct fpga_pe *pe;
  uint64_t output_scale = 0;
  uint64_t timer_cnt = 0;
  uint64_t cmd = 0;
  uint64_t datalen = 0;
  uint64_t input_address_phy = 0;
  uint64_t output_address_phy = 0;
  uint8_t data_cell_in = 0;
  uint8_t data_cell_out = 0;

  int ret = 0;

  datalen = (uint64_t)args.image.width * (uint64_t)args.image.height *
            (uint64_t)args.image.channels;
  datalen = align_to_x(datalen, 16);

  input_address_phy = vaddr_to_paddr(args.image.address);
  output_address_phy = vaddr_to_paddr(args.output.address);
  DLOG << "input_phy:" << input_address_phy;
  DLOG << "output_phy:" << output_address_phy;

  switch (args.input_data_type) {
    case DATA_TYPE_FP16: {
      switch (args.output_data_type) {
        case DATA_TYPE_FP16:
          data_cell_in = SIZE_FP16;
          data_cell_out = SIZE_FP16;
          cmd = CMD_FP16_TO_FP16;
          break;

        case DATA_TYPE_FP32:
          data_cell_in = SIZE_FP16;
          data_cell_out = SIZE_FP32;
          cmd = CMD_FP16_TO_FP32;
          break;

        default:
          break;
      }
    } break;

    case DATA_TYPE_FP32: {
      switch (args.output_data_type) {
        case DATA_TYPE_FP16:
          data_cell_in = SIZE_FP32;
          data_cell_out = SIZE_FP16;
          cmd = CMD_FP32_TO_FP16;
          break;

        case DATA_TYPE_FP32:
          data_cell_in = SIZE_FP32;
          data_cell_out = SIZE_FP32;
          cmd = CMD_FP32_TO_FP32;
          break;

        default:
          break;
      }
    } break;

    default:
      break;
  }
  if (cmd != CMD_FP16_TO_FP16 && cmd != CMD_FP16_TO_FP32 &&
      cmd != CMD_FP32_TO_FP16 && cmd != CMD_FP32_TO_FP32) {
    return -EFAULT;
  }
  if ((data_cell_in != SIZE_FP16 && data_cell_in != SIZE_FP32) ||
      (data_cell_out != SIZE_FP16 && data_cell_out != SIZE_FP32)) {
    return -EFAULT;
  }

  pthread_mutex_lock(&g_fpgainfo.pe_data->mutex);
  if (ERROR == g_fpgainfo.pe_data->pes[PE_IDX_BYPASS]->status) {
    ret = -EIO;
    DLOG << "Bypass Status Error!";
    pthread_mutex_unlock(&g_fpgainfo.pe_data->mutex);
    return ret;
  }

  /*restart scale*/
  reg_writeq(output_scale, REG_SCALE_PARAMETER);

  reg_writeq(input_address_phy, REG_CONVERT_SRC_ADDR);
  reg_writeq(output_address_phy, REG_CONVERT_DST_ADDR);
  reg_writeq(datalen, REG_CONVERT_LENGTH);

  /*SDK刷Cache保证数据一致性*/
  reg_writeq(cmd, REG_CONVERT_CMD);

  DLOG << "before reg poll";
  if (0 != fpga_regpoll(REG_INTERRUPT, INTERRUPT_BYPASS, PE_IRQ_TIMEOUT)) {
    g_fpgainfo.pe_data->pes[PE_IDX_BYPASS]->status = ERROR;
    ret = -EIO;
    DLOG << "BYPASS Wait Irq Timeout!";
  }
  DLOG << "after reg poll";

  output_scale = reg_readq(REG_SCALE_PARAMETER);
  output_scale = (output_scale << 32) | (output_scale >> 32);
  fpga_copy(args.output.scale_address, &output_scale, sizeof(float) * 2);

  //*(args.output.scale_address) = reg_readq(REG_SCALE_PARAMETER);
  //*(args.output.timer_cnt) = reg_readq(REG_TIMER_COUNTER);
  pthread_mutex_unlock(&g_fpgainfo.pe_data->mutex);
  return ret;
#endif

  return 0;
}

int ComputeFPGAConcat(const struct ConcatArgs &args) {
#ifdef FPGA_PRINT_MODE
  DLOG << "=============ComputeFpgaConcat===========";
  DLOG << "   Image_num: " << args.image_num
       << "   out_address:" << args.image_out
       << "   out_scale_address:" << args.scale_out
       << "   out_channel:" << args.out_channel;
  DLOG << "   image_height:" << args.height << "   image_width:" << args.width;
  for (int i = 0; i < args.image_num; i++) {
    DLOG << "   " << i << "th:        ";
    DLOG << "   channel_num:"
         << args.channel_num[i]
         // << "   aligned_channel_num:" << args.aligned_channel_num[i]
         << "   image_address:" << args.images_in[i]
         << "   image_scale_address:" << args.scales_in[i];
  }
#endif

  image::concat_images(args.images_in, args.scales_in, args.image_out,
                       args.scale_out, args.image_num, args.channel_num,
                       args.height, args.width);
  return 0;
}

void deconv_post_process(half **data_in, int sub_conv_n, int num, int channel,
                         int sub_height, int sub_width, int omit_size) {
  int origin_h = sub_height * sub_conv_n;
  int origin_w = sub_width * sub_conv_n;
  int align_origin_w = align_to_x(origin_w * channel, 16);
  int deconv_h = origin_h - 2 * omit_size;
  int deconv_w = origin_w - 2 * omit_size;
  int deconv_row_len = deconv_w * channel;
  int align_deconv_row_len = align_to_x(deconv_row_len, 16);
  half *ptr_tmp = *data_in;
  half *ptr_deconv =
      (half *)fpga_malloc(num * align_deconv_row_len * deconv_h * sizeof(half));
  memset(ptr_deconv, 0, num * align_deconv_row_len * deconv_h * sizeof(half));
  int deconv_idx = 0;
  for (int nn = 0; nn < num; ++nn) {
    for (int hh = 0; hh < origin_h; ++hh) {
      int hx = (hh % sub_conv_n);
      half *sub_t = ptr_tmp + hx * sub_height * align_origin_w;  // sub(hx,:);

      int hi = (hh / sub_conv_n);

      if ((hh < omit_size) || (hh >= (origin_h - omit_size))) continue;

      // for (int ww = 0; ww < origin_w; ++ww){

      //   if((ww < omit_size)  )// || (ww >= (origin_w-omit_size))
      //      continue;

      int sidx = (nn * origin_h * align_origin_w + hi * align_origin_w +
                  omit_size * channel);

      fpga_copy(ptr_deconv + deconv_idx, sub_t + sidx,
                sizeof(half) * deconv_row_len);
      deconv_idx += align_deconv_row_len;
      //}
    }
  }

  *data_in = ptr_deconv;
  fpga_free(ptr_tmp);
}

int ComputeFpgaDeconv(const struct DeconvArgs &args) {
#ifdef FPGA_PRINT_MODE
  DLOG << "=============ComputeFPGADeConv===========";
  DLOG << "   filter_num:" << args.filter_num
       << "   group_num:" << args.group_num
       << "   sub_conv_num:" << args.sub_conv_num;
#endif

  int sub_conv_num = args.sub_conv_num;

  for (int i = 0; i < sub_conv_num; i++) {
    //#if CPU_SIMULATE

    //#else
    ComputeBasicConv(args.conv_args[i]);
    //#endif
  }

  if (sub_conv_num > 1) {
    float max_scale = -1.0;
    for (int i = 0; i < sub_conv_num; i++) {
      float ptr_scale = (args.conv_args[i].output.scale_address)[0];
      if (ptr_scale > max_scale) {
        args.output.scale_address[0] = ptr_scale;
        args.output.scale_address[1] =
            (args.conv_args[i].output.scale_address)[1];
      }
    }
    deconv_post_process((half **)(&(args.output.address)), args.sub_conv_num, 1,
                        args.filter_num, (args.sub_output_height),
                        (args.sub_output_width), args.omit_size);
  }
  return 0;
}

int ComputeFPGASplit(const struct SplitArgs &args) {
#ifdef FPGA_PRINT_MODE
  DLOG << "=============ComputeFpgaSplit===========";
  DLOG << "   Image_num: " << args.image_num
       << "   in_address:" << args.image_in
       << "   in_scale_address:" << args.scale_in;
  DLOG << "   image_height:" << args.height << "   image_width:" << args.width;
  for (int i = 0; i < args.image_num; i++) {
    DLOG << "   " << i << "th:        ";
    DLOG << "   channel_num:" << args.out_channel_nums[i]
         << "   image_address:" << args.images_out[i]
         << "   image_scale_address:" << args.scales_out[i];
  }
#endif
  image::split_image(args.image_in, args.scale_in, args.images_out,
                     args.scales_out, args.image_num, args.out_channel_nums,
                     args.height, args.width);
  return 0;
}
}  // namespace fpga
}  // namespace paddle_mobile
