/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 *
 ******************************************************************************
 *
 * File: test_vqec_utest_bitmap.c
 *
 * Description: Unit tests for the vqe_bitmap module.
 *
 * Documents:
 *
 *****************************************************************************/

#include <string.h>
#include <stdio.h>
#include "vqe_bitmap.h"
#include "vam_types.h"
#include "vqec_pcm.h"
#include "test_vqec_utest_main.h"
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"

#ifdef _VQEC_UTEST_INTERPOSERS
#define UT_STATIC
#else
#define UT_STATIC static
#endif

/* data structure so that unit tests can peer inside */
struct vqe_bitmap_ {
    uint32_t *data;
    uint32_t size;
};

boolean vqec_pcm_gap_get_gaps_internal(vqec_pcm_t *pcm, 
                                       vqec_seq_num_t seq1,
                                       vqec_seq_num_t seq2,
                                       vqec_dp_gap_t *gapbuff,
                                       uint32_t buf_len,
                                       vqec_seq_num_t *highest_seq,
                                       boolean *more);

/*
 *  Unit tests for vqe_bitmap
 */

#define VQE_TEST_BITMAP_LARGE_SIZE 65536
#define VQE_TEST_BITMAP_SMALL_SIZE 32768

vqec_pcm_t *tpcm, *tpcms;
uint32_t tblock;
boolean tbit;
const int add_ranges[] = {3, 10, 25, 35, 60, 120, -1};
const int fills[] = {6, 31, 32, 115, 26, 71, 74, 66, 83, -1};
const int sets[] = {37, 38, 39, 42, -1};
const int clear_ranges[] = {69, 72, 74, 86, -1};

/* init */
int test_vqec_bitmap_init (void)
{
    tpcm = malloc(sizeof(vqec_pcm_t));
    tpcms = malloc(sizeof(vqec_pcm_t));

    return 0;
}

/* helper funcs here */

/* 
 * Print the contents of the data for a given length -- used for testing.
 */
void vqe_bitmap_print_map (vqe_bitmap_t *map,
                           uint16_t buf_len)
{
    int i, j;
    for(i = 0; i < buf_len; i++) 
    {     
        if(i % 4 == 0) {  /* print bit ruler */
            printf("%35d %35d %35d %35d",
                   (i+1)*32-1, (i+2)*32-1, (i+3)*32-1, (i+4)*32-1);
            printf("\n");
        }
        for(j = 0; j < 32; j++) {  /* print bit array */
            printf("%d", (map->data[i] >> (31 - j)) & 1);
            if((j + 1) % 8 == 0)
                printf(" ");
        }
    } 
    printf("\n");
}

/*
 * Invert all bits before and after get_gaps, since get_gaps now looks for 0's
 * instead of 1's.
 */
void vqe_bitmap_invert_bits (vqe_bitmap_t *map)
{
    int i;
    for (i = 0;
         i < (VQE_TEST_BITMAP_LARGE_SIZE >> 5);
         i++) {
        map->data[i] = ~(map->data[i]);
    }
}

/* end helper funcs here */

void test_vqec_bitmap_create (void)
{
    tpcm->gapmap = vqe_bitmap_create(VQE_TEST_BITMAP_LARGE_SIZE + 1);
    CU_ASSERT(tpcm->gapmap == NULL);

    tpcm->gapmap = vqe_bitmap_create(VQE_TEST_BITMAP_LARGE_SIZE);
    
    CU_ASSERT(tpcm->gapmap != NULL);

    tpcms->gapmap = vqe_bitmap_create(VQE_TEST_BITMAP_SMALL_SIZE);
    CU_ASSERT(tpcms->gapmap != NULL);
}

void test_vqec_bitmap_destroy (void)
{
    CU_ASSERT(VQE_BITMAP_ERR_INVALIDARGS ==
              vqe_bitmap_destroy(NULL));
    
    CU_ASSERT(VQE_BITMAP_OK ==
              vqe_bitmap_destroy(tpcm->gapmap));

    CU_ASSERT(VQE_BITMAP_OK ==
              vqe_bitmap_destroy(tpcms->gapmap));
}

void test_vqec_bitmap_wraparound (void)
{
    vqe_bitmap_error_t err;
    vqec_seq_num_t maxn = (tpcm->gapmap->size - 1);

    vqec_dp_gap_t gapbuff[100];
    uint16_t buf_len = 10;
    vqec_seq_num_t hseq;
    boolean more;

    /* add a gap that wraps around */
    err = vqe_bitmap_modify_bitrange(tpcm->gapmap, maxn - 5, 3, TRUE);
    CU_ASSERT(err == VQE_BITMAP_OK);

    /* we should have (65530,9) ==> (65530,5), (0,3) */
    vqe_bitmap_get_block(tpcm->gapmap, &tblock, maxn);
    CU_ASSERT(tblock == 0x0000003f); /* 0 ... 0011 1111 */
    vqe_bitmap_get_block(tpcm->gapmap, &tblock, 0);
    CU_ASSERT(tblock == 0xf0000000); /* 1111 0000 ... 0 */

    vqe_bitmap_invert_bits(tpcm->gapmap);
    buf_len = 10;
    buf_len = vqec_pcm_gap_get_gaps_internal(tpcm, maxn - 10, maxn + 11,
                                             gapbuff, buf_len, &hseq, &more);
    vqe_bitmap_invert_bits(tpcm->gapmap);
/*
    int i;
    printf("got gaps between seqs %u - %u:\n"
           " buf_len = %d (passed in as 10)\n"
           " more = %d\n", maxn - 10, maxn + 11, buf_len, (int)more);
    for(i = 0; i < buf_len; i++) {
        printf("  %d - %d\n", gapbuff[i].start_seq,
               gapbuff[i].start_seq + gapbuff[i].extent);
    }
*/
    /*
     * should produce:
     *  buf_len = 1
     *  more = 0
     *  65530 - 65539
     */
    CU_ASSERT(buf_len == 1);
    CU_ASSERT(more == FALSE);
    CU_ASSERT(gapbuff[0].start_seq == 65530);
    CU_ASSERT(gapbuff[0].extent == 9);
//    CU_ASSERT(gapbuff[1].start_seq == 65536);
//    CU_ASSERT(gapbuff[1].extent == 3);


    /* remove things around the boundaries */
    err = vqe_bitmap_clear_bit(tpcm->gapmap, maxn - 5);
    CU_ASSERT(err == VQE_BITMAP_OK);
    err = vqe_bitmap_clear_bit(tpcm->gapmap, maxn - 3);
    CU_ASSERT(err == VQE_BITMAP_OK);
    err = vqe_bitmap_clear_bit(tpcm->gapmap, 0);
    CU_ASSERT(err == VQE_BITMAP_OK);
    err = vqe_bitmap_clear_bit(tpcm->gapmap, 2);
    CU_ASSERT(err == VQE_BITMAP_OK);
    err = vqe_bitmap_clear_bit(tpcm->gapmap, maxn);
    CU_ASSERT(err == VQE_BITMAP_OK);

    /* so now we should have: (1,0), (3,0), (65531,0), (65533,1) */
    vqe_bitmap_get_block(tpcm->gapmap, &tblock, maxn);
    CU_ASSERT(tblock == 0x00000016); /* 0 ... 0001 0110 */
    vqe_bitmap_get_block(tpcm->gapmap, &tblock, 0);
    CU_ASSERT(tblock == 0x50000000); /* 0101 0000 ... 0 */

    buf_len = 10;
    vqe_bitmap_invert_bits(tpcm->gapmap);
    buf_len = vqec_pcm_gap_get_gaps_internal(tpcm, maxn - 10, maxn + 11,
                                   gapbuff, buf_len, &hseq, &more);
    vqe_bitmap_invert_bits(tpcm->gapmap);
/*
    {
        int i;
        printf("got gaps between seqs %u - %u:\n"
               " buf_len = %d (passed in as 10)\n"
               " more = %d\n", maxn - 10, maxn + 11, buf_len, (int)more);
        for(i = 0; i < buf_len; i++) {
            printf("  %d - %d\n", gapbuff[i].start_seq,
                   gapbuff[i].start_seq + gapbuff[i].extent);
        }
    }
*/
    /*
     * should produce:
     *  buf_len = 4
     *  more = 0
     *  65531 - 65531
     *  65533 - 65534
     *  65537 - 65537
     *  65539 - 65539
     */
    CU_ASSERT(buf_len == 4);
    CU_ASSERT(more == FALSE);
    CU_ASSERT(gapbuff[0].start_seq == 65531);
    CU_ASSERT(gapbuff[0].extent == 0);
    CU_ASSERT(gapbuff[1].start_seq == 65533);
    CU_ASSERT(gapbuff[1].extent == 1);
    CU_ASSERT(gapbuff[2].start_seq == 65537);
    CU_ASSERT(gapbuff[2].extent == 0);
    CU_ASSERT(gapbuff[3].start_seq == 65539);
    CU_ASSERT(gapbuff[3].extent == 0);

    /* make sure gap tree is empty for rest of tests */
    memset(tpcm->gapmap->data, 0, sizeof(uint32_t) * (tpcm->gapmap->size >> 5));
}

void test_vqec_bitmap_set_range (void)
{
    int i;

    CU_ASSERT(VQE_BITMAP_ERR_INVALIDARGS ==
              vqe_bitmap_modify_bitrange(NULL, 1, 5, TRUE));

    CU_ASSERT(VQE_BITMAP_OK ==
              vqe_bitmap_modify_bitrange(tpcm->gapmap, 65530, 2, TRUE));

    CU_ASSERT(VQE_BITMAP_OK ==
              vqe_bitmap_modify_bitrange(tpcms->gapmap, 42343, 42345, TRUE));

    for (i = 0; add_ranges[i] != -1; i = i + 2) {
//        printf("setting bit range %d - %d\n",
//               add_ranges[i], add_ranges[i + 1]);
        CU_ASSERT(VQE_BITMAP_OK ==
                  vqe_bitmap_modify_bitrange(tpcm->gapmap, add_ranges[i], add_ranges[i + 1], TRUE));
    }
}

void test_vqec_bitmap_get_bit (void)
{
    CU_ASSERT(VQE_BITMAP_ERR_INVALIDARGS ==
              vqe_bitmap_get_bit(NULL, &tbit, 1));

    CU_ASSERT(VQE_BITMAP_OK ==
              vqe_bitmap_get_bit(tpcm->gapmap, &tbit, 1));
    CU_ASSERT(tbit);

    CU_ASSERT(VQE_BITMAP_OK ==
              vqe_bitmap_get_bit(tpcm->gapmap, &tbit, 5));
    CU_ASSERT(tbit);

    CU_ASSERT(VQE_BITMAP_OK ==
              vqe_bitmap_get_bit(tpcm->gapmap, &tbit, 15));
    CU_ASSERT(!tbit);

    CU_ASSERT(VQE_BITMAP_OK ==
              vqe_bitmap_get_bit(tpcm->gapmap, &tbit, 119));
    CU_ASSERT(tbit);

    CU_ASSERT(VQE_BITMAP_OK ==
              vqe_bitmap_get_bit(tpcm->gapmap, &tbit, 65565));
    CU_ASSERT(tbit);

    CU_ASSERT(VQE_BITMAP_OK ==
              vqe_bitmap_get_bit(tpcm->gapmap, &tbit, 65585));
    CU_ASSERT(!tbit);

    CU_ASSERT(VQE_BITMAP_OK ==
              vqe_bitmap_get_bit(tpcm->gapmap, &tbit, 68234));
    CU_ASSERT(!tbit);
}

void test_vqec_bitmap_clear_bit (void)
{
    int i;

    CU_ASSERT(VQE_BITMAP_OK ==
              vqe_bitmap_clear_bit(tpcms->gapmap, 42343));

    for (i = 0; fills[i] != -1; i++) {
//        printf("clearing bit %d\n", fills[i]);
        CU_ASSERT(VQE_BITMAP_OK ==
                  vqe_bitmap_clear_bit(tpcm->gapmap, fills[i]));
    }
}

void test_vqec_bitmap_clear_range (void)
{
    int i;
    for (i = 0; clear_ranges[i] != -1; i = i + 2) {
//        printf("clearing bit range %d - %d\n",
//               clear_ranges[i], clear_ranges[i + 1]);
        CU_ASSERT(VQE_BITMAP_OK ==
                  vqe_bitmap_modify_bitrange(tpcm->gapmap,
                                             clear_ranges[i],
                                             clear_ranges[i + 1],
                                             FALSE));
    }
}

void test_vqec_bitmap_get_block (void)
{
    CU_ASSERT(VQE_BITMAP_ERR_INVALIDARGS ==
              vqe_bitmap_get_block(NULL, NULL, 41));

    CU_ASSERT(VQE_BITMAP_OK ==
              vqe_bitmap_get_block(tpcms->gapmap, &tblock, 42343));
    CU_ASSERT(tblock == 0x00c00000);

    CU_ASSERT(VQE_BITMAP_OK ==
              vqe_bitmap_get_block(tpcm->gapmap, &tblock, 41));
    CU_ASSERT(tblock);
    CU_ASSERT(tblock == tpcm->gapmap->data[1]);
}

void test_vqec_bitmap_clear_block (void)
{
    CU_ASSERT(VQE_BITMAP_ERR_INVALIDARGS ==
              vqe_bitmap_clear_block(NULL, 41));

    CU_ASSERT(VQE_BITMAP_OK ==
              vqe_bitmap_clear_block(tpcms->gapmap, 42343));

    CU_ASSERT(VQE_BITMAP_OK ==
              vqe_bitmap_clear_block(tpcm->gapmap, 41));
    CU_ASSERT(0 == tpcm->gapmap->data[1]);
}

void test_vqec_bitmap_set_bit (void)
{
    int i;

    CU_ASSERT(VQE_BITMAP_OK ==
              vqe_bitmap_set_bit(tpcms->gapmap, 42343));

    for (i = 0; sets[i] != -1; i++) {
//        printf("setting bit %d\n", sets[i]);
        CU_ASSERT(VQE_BITMAP_OK ==
                  vqe_bitmap_set_bit(tpcm->gapmap, sets[i]));
    }
}

void test_vqec_bitmap_gap_search (void)
{
    vqec_seq_num_t gap_seqs[50];
    uint16_t num_gaps;

    vqe_bitmap_invert_bits(tpcm->gapmap);

    CU_ASSERT(0 == vqec_pcm_gap_search(NULL, 9, 5, 5, gap_seqs));
    CU_ASSERT(0 == vqec_pcm_gap_search(tpcm, 9, 0, 5, gap_seqs));

    num_gaps = vqec_pcm_gap_search(tpcm, 9, 5, 5, gap_seqs);
    /* search 9, 14, 19, 24, 29 */
    CU_ASSERT(num_gaps == 2);
    CU_ASSERT(gap_seqs[0] == 9);
    CU_ASSERT(gap_seqs[1] == 29);

    num_gaps = vqec_pcm_gap_search(tpcm, 4, 44, 5, gap_seqs);
    /* search 4, 48, 92, 136, 180 */
    CU_ASSERT(num_gaps == 2);
    CU_ASSERT(gap_seqs[0] == 4);
    CU_ASSERT(gap_seqs[1] == 92);

    num_gaps = vqec_pcm_gap_search(tpcm, 28, 1, 2, gap_seqs);
    /* search 9, 14, 19, 24, 29 */
    CU_ASSERT(num_gaps == 2);
    CU_ASSERT(gap_seqs[0] == 28);
    CU_ASSERT(gap_seqs[1] == 29);

    vqe_bitmap_invert_bits(tpcm->gapmap);
}

void test_vqec_bitmap_show_gaps (void)
{
    vqec_dp_gap_t gapbuff[100];
    uint32_t buf_len = 4;
    vqec_seq_num_t hseq;
    boolean more;

    vqe_bitmap_invert_bits(tpcm->gapmap);
    buf_len = vqec_pcm_gap_get_gaps_internal(tpcm, 9, 93, gapbuff, buf_len,
                                   &hseq, &more);
    vqe_bitmap_invert_bits(tpcm->gapmap);
/*
    int i;
    printf("got gaps between seqs 9 - 93:\n"
           " buf_len = %d (passed in as 4)\n"
           " more = %d\n", buf_len, (int)more);
    for(i = 0; i < buf_len; i++) {
        printf("  %d - %d\n", gapbuff[i].start_seq,
               gapbuff[i].start_seq + gapbuff[i].extent);
    }
*/
    /*
     * should produce:
     *  buf_len = 4
     *  more = 1
     *  9 - 10
     *  25 - 25
     *  27 - 30
     *  37 - 39
     */
    CU_ASSERT(buf_len == 4);
    CU_ASSERT(more == TRUE);
    CU_ASSERT(gapbuff[0].start_seq == 9);
    CU_ASSERT(gapbuff[0].extent == 1);
    CU_ASSERT(gapbuff[1].start_seq == 25);
    CU_ASSERT(gapbuff[1].extent == 0);
    CU_ASSERT(gapbuff[2].start_seq == 27);
    CU_ASSERT(gapbuff[2].extent == 3);
    CU_ASSERT(gapbuff[3].start_seq == 37);
    CU_ASSERT(gapbuff[3].extent == 2);


    buf_len = 50;  /* increase buf_len and try again */
    vqe_bitmap_invert_bits(tpcm->gapmap);
    buf_len = vqec_pcm_gap_get_gaps_internal(tpcm, 9, 93, gapbuff, buf_len,
                                   &hseq, &more);
    vqe_bitmap_invert_bits(tpcm->gapmap);
/*
    printf("got gaps between seqs 9 - 93:\n"
           " buf_len = %d (passed in as 50)\n"
           " more = %d\n", buf_len, (int)more);
    int i;
    for(i = 0; i < buf_len; i++) {
        printf("  %d - %d\n", gapbuff[i].start_seq,
               gapbuff[i].start_seq + gapbuff[i].extent);
    }
*/
    /*
     * should produce:
     *  buf_len = 9 (passed in as 50)
     *  more = 0
     *  9 - 10
     *  25 - 25
     *  27 - 30
     *  37 - 39
     *  42 - 42
     *  64 - 65
     *  67 - 68
     *  73 - 73
     *  87 - 93
     */
    CU_ASSERT(buf_len == 9);
    CU_ASSERT(more == FALSE);
    CU_ASSERT(gapbuff[0].start_seq == 9);
    CU_ASSERT(gapbuff[0].extent == 1);
    CU_ASSERT(gapbuff[1].start_seq == 25);
    CU_ASSERT(gapbuff[1].extent == 0);
    CU_ASSERT(gapbuff[2].start_seq == 27);
    CU_ASSERT(gapbuff[2].extent == 3);
    CU_ASSERT(gapbuff[3].start_seq == 37);
    CU_ASSERT(gapbuff[3].extent == 2);
    CU_ASSERT(gapbuff[4].start_seq == 42);
    CU_ASSERT(gapbuff[4].extent == 0);
    CU_ASSERT(gapbuff[5].start_seq == 64);
    CU_ASSERT(gapbuff[5].extent == 1);
    CU_ASSERT(gapbuff[6].start_seq == 67);
    CU_ASSERT(gapbuff[6].extent == 1);
    CU_ASSERT(gapbuff[7].start_seq == 73);
    CU_ASSERT(gapbuff[7].extent == 0);
    CU_ASSERT(gapbuff[8].start_seq == 87);
    CU_ASSERT(gapbuff[8].extent == 6);

//    vqe_bitmap_print_map(tpcm->gapmap, 4);
}

void test_vqec_bitmap_flush (void)
{
    CU_ASSERT(VQE_BITMAP_ERR_INVALIDARGS == vqe_bitmap_flush(NULL));
    CU_ASSERT(tpcm->gapmap->data[0] != 0);
    CU_ASSERT(tpcm->gapmap->data[2047] != 0);

    CU_ASSERT(VQE_BITMAP_OK == vqe_bitmap_flush(tpcm->gapmap));
    CU_ASSERT(tpcm->gapmap->data[0] == 0);
    CU_ASSERT(tpcm->gapmap->data[2047] == 0);
}

void test_vqec_bitmap_optimizations (void)
{
    vqec_dp_gap_t gapbuff[100];
    uint32_t buf_cnt;
    vqec_seq_num_t hseq;
    boolean more;

    /* case 1: single block of gaps, none else */
    vqe_bitmap_modify_bitrange(tpcm->gapmap, 128, 159, TRUE);
    CU_ASSERT(tpcm->gapmap->data[4] == (uint32_t)(-1));
    buf_cnt = 50;
    vqe_bitmap_invert_bits(tpcm->gapmap);
    buf_cnt = vqec_pcm_gap_get_gaps_internal(tpcm, 10, 300, gapbuff, buf_cnt,
                                   &hseq, &more);
    vqe_bitmap_invert_bits(tpcm->gapmap);
    CU_ASSERT(buf_cnt == 1);
    CU_ASSERT(more == FALSE);
    CU_ASSERT(gapbuff[0].start_seq == 128);
    CU_ASSERT(gapbuff[0].extent == 31);

    /* case 2: 2 consecutive blocks of gaps */
    vqe_bitmap_modify_bitrange(tpcm->gapmap, 160, 191, TRUE);
    CU_ASSERT(tpcm->gapmap->data[4] == (uint32_t)(-1));
    CU_ASSERT(tpcm->gapmap->data[5] == (uint32_t)(-1));
    buf_cnt = 50;
    vqe_bitmap_invert_bits(tpcm->gapmap);
    buf_cnt = vqec_pcm_gap_get_gaps_internal(tpcm, 10, 300, gapbuff, buf_cnt,
                                   &hseq, &more);
    vqe_bitmap_invert_bits(tpcm->gapmap);
    CU_ASSERT(buf_cnt == 1);
    CU_ASSERT(more == FALSE);
    CU_ASSERT(gapbuff[0].start_seq == 128);
    CU_ASSERT(gapbuff[0].extent == 63);

    /* case 3: partial block, 2 full block, partial block, all consecutive */
    vqe_bitmap_modify_bitrange(tpcm->gapmap, 120, 127, TRUE);
    vqe_bitmap_modify_bitrange(tpcm->gapmap, 192, 200, TRUE);
    buf_cnt = 50;
    vqe_bitmap_invert_bits(tpcm->gapmap);
    buf_cnt = vqec_pcm_gap_get_gaps_internal(tpcm, 10, 300, gapbuff, buf_cnt,
                                   &hseq, &more);
    vqe_bitmap_invert_bits(tpcm->gapmap);
    CU_ASSERT(buf_cnt == 1);
    CU_ASSERT(more == FALSE);
    CU_ASSERT(gapbuff[0].start_seq == 120);
    CU_ASSERT(gapbuff[0].extent == 80);


}

int test_vqec_bitmap_clean (void)
{
    return 0;
}

CU_TestInfo test_array_bitmap[] = {
    {"test vqec_bitmap_create",test_vqec_bitmap_create},
    {"test vqec_bitmap_wraparound",test_vqec_bitmap_wraparound},
    {"test vqec_bitmap_set_range",test_vqec_bitmap_set_range},
    {"test vqec_bitmap_get_bit",test_vqec_bitmap_get_bit},
    {"test vqec_bitmap_clear_bit",test_vqec_bitmap_clear_bit},
    {"test vqec_bitmap_get_block",test_vqec_bitmap_get_block},
    {"test vqec_bitmap_clear_block",test_vqec_bitmap_clear_block},
    {"test vqec_bitmap_clear_range",test_vqec_bitmap_clear_range},
    {"test vqec_bitmap_set_bit",test_vqec_bitmap_set_bit},
    {"test vqec_bitmap_gap_search",test_vqec_bitmap_gap_search},
    {"test vqec_bitmap_show_gaps",test_vqec_bitmap_show_gaps},
    {"test vqec_bitmap_flush",test_vqec_bitmap_flush},
    {"test vqec_bitmap_optimizations",test_vqec_bitmap_optimizations},
    {"test vqec_bitmap_destroy",test_vqec_bitmap_destroy},
    CU_TEST_INFO_NULL,
};
