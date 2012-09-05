/* 
 * RTCP XR unit test program.
 * This file holds all of the RTCP XR tests.
 *
 * Copyright (c) 2008 by cisco Systems, Inc.
 * All rights reserved.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/utils/vam_types.h"
#include "rtp_header.h"
#include "rtcp_xr.h"
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"
/* CU_ASSERT_* return values are not used */
/* sa_ignore DISABLE_RETURNS CU_assertImplementation */

#define BINARY_SIZE 16

uint16_t rtp_seq[] = {
65000,
65001,
65002,
65003,
65004,
65005,
65006,
65007,
65008,
65009,
65010,
65011,
65012,
65013,
65014,
65015,
65016,
65017,
65018,
65019,
65020,
65021,
65022,
65023,
65024,
65025,
65026,
65027,
65028,
65030,
65031,
65032,
65033,
65034,
65035,
65036,
65037,
65038,
65039,
65040,
65041,
65042,
65043,
65044,
65045,
65046,
65047,
65048,
65049,
65050,
65051,
65052,
65053,
65054,
65055,
65056,
65057,
65058,
65059,
65060,
65061,
65062,
65063,
65064,
65065,
65066,
65067,
65068,
65069,
65070,
65071,
65072,
65073,
65074,
65075,
65076,
65077,
65078,
65079,
65080,
65081,
65082,
65083,
65084,
65085,
65086,
65087,
65088,
65089,
65029,
65090,
65091,
65092,
65093,
65094,
65095,
65096,
65097,
65098,
65099,
65100,
65101,
65102,
65103,
65104,
65105,
65106,
65107,
65108,
65109,
65110,
65111,
65112,
65113,
65114,
65115,
65116,
65117,
65118,
65119,
65120,
65121,
65122,
65123,
65124,
65125,
65126,
65127,
65128,
65130,
65131,
65132,
65133,
65134,
65135,
65136,
65137,
65138,
65139,
65140,
65141,
65142,
65143,
65144,
65145,
65146,
65147,
65148,
65149,
65150,
65151,
65152,
65153,
65154,
65155,
65156,
65157,
65158,
65159,
65160,
65161,
65162,
65163,
65164,
65165,
65166,
65167,
65168,
65169,
65170,
65171,
65172,
65173,
65174,
65175,
65176,
65177,
65178,
65179,
65180,
65181,
65182,
65183,
65184,
65185,
65186,
65187,
65188,
65189,
65129,
65190,
65191,
65192,
65193,
65194,
65195,
65196,
65197,
65198,
65199,
65200,
65201,
65202,
65203,
65204,
65205,
65206,
65207,
65208,
65209,
65210,
65211,
65212,
65213,
65214,
65215,
65216,
65217,
65218,
65219,
65220,
65221,
65222,
65223,
65224,
65225,
65226,
65227,
65228,
65230,
65231,
65232,
65233,
65234,
65235,
65236,
65237,
65238,
65239,
65240,
65241,
65242,
65243,
65244,
65245,
65246,
65247,
65248,
65249,
65250,
65251,
65252,
65253,
65254,
65255,
65256,
65257,
65258,
65259,
65260,
65261,
65262,
65263,
65264,
65265,
65266,
65267,
65268,
65269,
65270,
65271,
65272,
65273,
65274,
65275,
65276,
65277,
65278,
65279,
65280,
65281,
65282,
65283,
65284,
65285,
65286,
65287,
65288,
65289,
65229,
65290,
65291,
65292,
65293,
65294,
65295,
65296,
65297,
65298,
65299,
65300,
65301,
65302,
65303,
65304,
65305,
65306,
65307,
65308,
65309,
65310,
65311,
65312,
65313,
65314,
65315,
65316,
65317,
65318,
65319,
65320,
65321,
65322,
65323,
65324,
65325,
65326,
65327,
65328,
65330,
65331,
65332,
65333,
65334,
65335,
65336,
65337,
65338,
65339,
65340,
65341,
65342,
65343,
65344,
65345,
65346,
65347,
65348,
65349,
65350,
65351,
65352,
65353,
65354,
65355,
65356,
65357,
65358,
65359,
65360,
65361,
65362,
65363,
65364,
65365,
65366,
65367,
65368,
65369,
65370,
65371,
65372,
65373,
65374,
65375,
65376,
65377,
65378,
65379,
65380,
65381,
65382,
65383,
65384,
65385,
65386,
65387,
65388,
65389,
65329,
65390,
65391,
65392,
65393,
65394,
65395,
65396,
65397,
65398,
65399,
65400,
65401,
65402,
65403,
65404,
65405,
65406,
65407,
65408,
65409,
65410,
65411,
65412,
65413,
65414,
65415,
65416,
65417,
65418,
65419,
65420,
65421,
65422,
65423,
65424,
65425,
65426,
65427,
65428,
65430,
65431,
65432,
65433,
65434,
65435,
65436,
65437,
65438,
65439,
65440,
65441,
65442,
65443,
65444,
65445,
65446,
65447,
65448,
65449,
65450,
65451,
65452,
65453,
65454,
65455,
65456,
65457,
65458,
65459,
65460,
65461,
65462,
65463,
65464,
65465,
65466,
65467,
65468,
65469,
65470,
65471,
65472,
65473,
65474,
65475,
65476,
65477,
65478,
65479,
65480,
65481,
65482,
65483,
65484,
65485,
65486,
65487,
65488,
65489,
65429,
65490,
65491,
65492,
65493,
65494,
65495,
65496,
65497,
65498,
65499,
65500,
65501,
65502,
65503,
65504,
65505,
65506,
65507,
65508,
65509,
65510,
65511,
65512,
65513,
65514,
65515,
65516,
65517,
65518,
65519,
65520,
65521,
65522,
65523,
65524,
65525,
65526,
65527,
65528,
65530,
65531,
65532,
65533,
65534,
65535,
0,
1,
2,
3,
4,
5,
6,
7,
8,
9,
10,
11,
12,
13,
14,
15,
16,
17,
18,
19,
20,
21,
22,
23,
24,
25,
26,
27,
28,
29,
30,
31,
32,
33,
34,
35,
36,
37,
38,
39,
40,
41,
42,
43,
44,
45,
46,
47,
48,
49,
50,
51,
52,
53,
65529,
54,
55,
56,
57,
58,
59,
60,
61,
62,
63,
64,
65,
66,
67,
68,
69,
70,
71,
72,
73,
74,
75,
76,
77,
78,
79,
80,
81,
82,
83,
84,
85,
86,
87,
88,
89,
90,
91,
92,
94,
95,
96,
97,
98,
99,
100,
101,
102,
103,
104,
105,
106,
107,
108,
109,
110,
111,
112,
113,
114,
115,
116,
117,
118,
119,
120,
121,
122,
123,
124,
125,
126,
127,
128,
129,
130,
131,
132,
133,
134,
135,
136,
137,
138,
139,
140,
141,
142,
143,
144,
145,
146,
147,
148,
149,
150,
151,
152,
153,
93,
154,
155,
156,
157,
158,
159,
160,
161,
162,
163,
164,
165,
166,
167,
168,
169,
170,
171,
172,
173,
174,
175,
176,
177,
178,
179,
180,
181,
182,
183,
184,
185,
186,
187,
188,
189,
190,
191,
192,
194,
195,
196,
197,
198,
199,
200,
201,
202,
203,
204,
205,
206,
207,
208,
209,
210,
211,
212,
213,
214,
215,
216,
217,
218,
219,
220,
221,
222,
223,
224,
225,
226,
227,
228,
229,
230,
231,
232,
233,
234,
235,
236,
237,
238,
239,
240,
241,
242,
243,
244,
245,
246,
247,
248,
249,
250,
251,
252,
253,
193,
254,
255,
256,
257,
258,
259,
260,
261,
262,
263,
264,
265,
266,
267,
268,
269,
270,
271,
272,
273,
274,
275,
276,
277,
278,
279,
280,
281,
282,
283,
284,
285,
286,
287,
288,
289,
290,
291,
292,
294,
295,
296,
297,
298,
299,
300,
301,
302,
303,
304,
305,
306,
307,
308,
309,
310,
311,
312,
313,
314,
315,
316,
317,
318,
319,
320,
321,
322,
323,
324,
325,
326,
327,
328,
329,
330,
331,
332,
333,
334};

static int big_total = 870;

#ifdef DEBUG
static char *convert2binary(uint16_t data)
{
    static char binary[BINARY_SIZE];
    int i = 0;

    memset(binary, 0, BINARY_SIZE);
    data = data << 1;
    while(i != BINARY_SIZE-1) {
        if ((data & 0x8000) == 0) {
            binary[i] = '0';
        } else {
            binary[i] = '1';
        }

        i++;
        data = data << 1;
    }

    return binary;
}
#endif

/***********************************************************************
 *
 *     RTCP XR SUITE TESTS START HERE
 *
 * RTCP XR Suite tests the basic functions (rtcp_xr_update_seq)
 *
 **********************************************************************/

int init_rtcp_xr_suite(void)
{
    return(0);
}

int clean_rtcp_xr_suite(void)
{
    return(0);
}

void test_rtcp_xr_no_loss(void)
{
    uint32_t i, eseq;
    uint32_t start_seq = 1000;
    uint16_t totals = 65533;
    rtcp_xr_stats_t xr_stats;
    rtcp_xr_cfg_t cfg;

    rtcp_init_xr_cfg(&cfg);
    cfg.max_loss_rle = 1;
    CU_ASSERT_EQUAL(FALSE, is_rtcp_xr_enabled(&cfg));
    rtcp_xr_set_size(&xr_stats, cfg.max_loss_rle);
    rtcp_xr_update_seq(&xr_stats, 0);
    cfg.max_loss_rle = 2;
    CU_ASSERT_EQUAL(TRUE, is_rtcp_xr_enabled(&cfg));

    rtcp_xr_set_size(&xr_stats, 120);
    rtcp_xr_init_seq(&xr_stats, start_seq, FALSE);
    /* Start the seq from 1000 with 65534 consecutive numbers */
    for (i = 0; i < totals; i++) {
        eseq = start_seq + i;
        rtcp_xr_update_seq(&xr_stats, eseq);
    }
#ifdef DEBUG
    printf("\nNo loss occurred ...");
    for (i = 0; i <= xr_stats.cur_chunk_in_use; i++) {
        printf("\nchunk[%d] = 0x%04x", i, xr_stats.chunk[i]);
    }
#endif
    CU_ASSERT_EQUAL(totals, xr_stats.totals);
    CU_ASSERT_EQUAL(start_seq, xr_stats.eseq_start);

    CU_ASSERT_EQUAL(0, xr_stats.lost_packets);
    CU_ASSERT_EQUAL(0, xr_stats.dup_packets);
    CU_ASSERT_EQUAL(0, xr_stats.late_arrivals);

    CU_ASSERT_EQUAL(4, xr_stats.cur_chunk_in_use);
    CU_ASSERT_EQUAL(60, xr_stats.max_chunks_allow);

    CU_ASSERT_EQUAL(0, xr_stats.not_reported);
    CU_ASSERT_EQUAL(0, xr_stats.before_intvl);
}

void test_rtcp_xr_over_limit(void)
{
    uint32_t i, eseq;
    uint32_t start_seq = 1000;
    uint16_t totals = 65534;
    rtcp_xr_stats_t xr_stats;
    
    rtcp_xr_set_size(&xr_stats, 120);
    rtcp_xr_init_seq(&xr_stats, start_seq, FALSE);
    /* Start the seq from 1000 with 131068 consecutive numbers */
    for (i = 0; i < totals*2; i++) {
        eseq = start_seq + i;
        rtcp_xr_update_seq(&xr_stats, eseq);
    }

    /* Receive 100 seq starting from 700 */
    for (i = 0; i < 100; i++) {
        eseq = 700 + i;
        rtcp_xr_update_seq(&xr_stats, eseq);
    }

#ifdef DEBUG
    printf("\nOver the limits ...");
    for (i = 0; i <= xr_stats.cur_chunk_in_use; i++) {
        printf("\nchunk[%d] = 0x%04x", i, xr_stats.chunk[i]);
    }
#endif
    CU_ASSERT_EQUAL(totals, xr_stats.totals);
    CU_ASSERT_EQUAL(start_seq, xr_stats.eseq_start);

    CU_ASSERT_EQUAL(0, xr_stats.lost_packets);
    CU_ASSERT_EQUAL(0, xr_stats.dup_packets);
    CU_ASSERT_EQUAL(0, xr_stats.late_arrivals);

    CU_ASSERT_EQUAL(4, xr_stats.cur_chunk_in_use);
    CU_ASSERT_EQUAL(60, xr_stats.max_chunks_allow);

    /* There will be 65534 seq numbers not being reported */
    CU_ASSERT_EQUAL(totals, xr_stats.not_reported);
    /* There will be 100 seq numbers coming in before the interval */
    CU_ASSERT_EQUAL(100, xr_stats.before_intvl);
}

void test_rtcp_xr_missing_seq(void)
{
    uint32_t i, eseq;
    uint32_t start_seq = 1000;
    uint16_t totals = 101;
    rtcp_xr_stats_t xr_stats;
    
    /* Start the seq from 1000 with 101 numbers but skip every other one */
    rtcp_xr_set_size(&xr_stats, 120);
    rtcp_xr_init_seq(&xr_stats, start_seq, FALSE);
    for (i = 0; i < totals; i++) {
        eseq = start_seq + i;
        if (i % 2 == 0) {
            rtcp_xr_update_seq(&xr_stats, eseq);
        }
    }
#ifdef DEBUG
    printf("\nMissing every other packet ...");
    for (i = 0; i <= xr_stats.cur_chunk_in_use; i++) {
        printf("\nchunk[%d] = %s", i, convert2binary(xr_stats.chunk[i]));
    }
#endif
    CU_ASSERT_EQUAL(totals, xr_stats.totals);
    CU_ASSERT_EQUAL(start_seq, xr_stats.eseq_start);

    CU_ASSERT_EQUAL(totals/2, xr_stats.lost_packets);
    CU_ASSERT_EQUAL(0, xr_stats.dup_packets);
    CU_ASSERT_EQUAL(0, xr_stats.late_arrivals);

    /* Start the seq from 1000 with 1002 numbers but 
       skip every one out of 100 */
    rtcp_xr_init_seq(&xr_stats, start_seq, TRUE);
    CU_ASSERT_EQUAL(totals, xr_stats.re_init);
    totals = 1002;
    for (i = 0; i < totals; i++) {
        eseq = start_seq + i;
        if (i % 100 != 0) {
            rtcp_xr_update_seq(&xr_stats, eseq);
        }
    }
#ifdef DEBUG
    printf("\nMissing every 100th packet ...");
    for (i = 0; i <= xr_stats.cur_chunk_in_use; i++) {
        printf("\nchunk[%d] = 0x%04x", i, xr_stats.chunk[i]);
    }
#endif
    CU_ASSERT_EQUAL(totals, xr_stats.totals);
    CU_ASSERT_EQUAL(start_seq, xr_stats.eseq_start);

    CU_ASSERT_EQUAL(11, xr_stats.lost_packets);
    CU_ASSERT_EQUAL(0, xr_stats.dup_packets);
    CU_ASSERT_EQUAL(0, xr_stats.late_arrivals);

    /* Start the seq from 1000 but skip 14 packets */
    rtcp_xr_init_seq(&xr_stats, start_seq, TRUE);
    CU_ASSERT_EQUAL(totals, xr_stats.re_init);
    eseq = 1015;
    rtcp_xr_update_seq(&xr_stats, eseq);
    CU_ASSERT_EQUAL(eseq+1, xr_stats.eseq_start + xr_stats.totals);

    /* Skip 300 packets */
    eseq = 1300;
    rtcp_xr_update_seq(&xr_stats, eseq);
    CU_ASSERT_EQUAL(eseq+1, xr_stats.eseq_start + xr_stats.totals);

#ifdef DEBUG
    printf("\nMissing 15 packets ...");
    for (i = 0; i <= xr_stats.cur_chunk_in_use; i++) {
        printf("\nchunk[%d] = 0x%04x", i, xr_stats.chunk[i]);
    }
#endif
    CU_ASSERT_EQUAL(eseq-start_seq+1-2, xr_stats.lost_packets);
    CU_ASSERT_EQUAL(0, xr_stats.dup_packets);
    CU_ASSERT_EQUAL(0, xr_stats.late_arrivals);

    /* Start the seq from 1000 with 65534 numbers but 
       skip every one out of 10 */
    rtcp_xr_set_size(&xr_stats, 1400);
    rtcp_xr_init_seq(&xr_stats, start_seq, FALSE);
    totals = 10499;
    for (i = 0; i < totals; i++) {
        eseq = start_seq + i;
        if (i % 10 != 0) {
            rtcp_xr_update_seq(&xr_stats, eseq);
        }
    }

    eseq = start_seq + 10560;
    rtcp_xr_update_seq(&xr_stats, eseq);

#ifdef DEBUG
    printf("\nMissing every 10th packet ...");
    for (i = 0; i <= xr_stats.cur_chunk_in_use; i++) {
        if (xr_stats.chunk[i] & INITIAL_BIT_VECTOR) {
            printf("\nchunk[%d] = %s", i, convert2binary(xr_stats.chunk[i]));
        } else {
            printf("\nchunk[%d] = 0x%04x", i, xr_stats.chunk[i]);
        }
    }
#endif

    CU_ASSERT_EQUAL(699, xr_stats.cur_chunk_in_use);
}

void test_rtcp_xr_dup_seq(void)
{
    uint32_t i, eseq;
    uint32_t start_seq = 1000;
    uint16_t totals = 1000;
    rtcp_xr_stats_t xr_stats;
    
    /* Start the seq from 1000 with 1000 numbers but make
       a dup every 100 one */
    rtcp_xr_set_size(&xr_stats, 120);
    rtcp_xr_init_seq(&xr_stats, start_seq, FALSE);
    for (i = 0; i < totals; i++) {
        eseq = start_seq + i;
        rtcp_xr_update_seq(&xr_stats, eseq);
        if (i % 100 == 0) {
            rtcp_xr_update_seq(&xr_stats, eseq);
        }
    }

    /* Another run of dup packets */
    for (i = 0; i < totals; i++) {
        eseq = start_seq + i;
        if (i % 50 == 0) {
            rtcp_xr_update_seq(&xr_stats, eseq);
        }
    }

    CU_ASSERT_EQUAL(totals, xr_stats.totals);
    CU_ASSERT_EQUAL(start_seq, xr_stats.eseq_start);

    CU_ASSERT_EQUAL(0, xr_stats.lost_packets);
    CU_ASSERT_EQUAL(30, xr_stats.dup_packets);
    CU_ASSERT_EQUAL(0, xr_stats.late_arrivals);
}

void test_rtcp_xr_late_arrival(void)
{
    uint32_t i, eseq;
    uint32_t start_seq = 1000;
    uint16_t totals = 101;
    rtcp_xr_stats_t xr_stats;
    
    /* Start the seq from 1000 with 101 numbers but skip every other one */
    rtcp_xr_set_size(&xr_stats, 120);
    rtcp_xr_init_seq(&xr_stats, start_seq, FALSE);
    for (i = 0; i < totals; i++) {
        eseq = start_seq + i;
        if (i % 2 == 0) {
            rtcp_xr_update_seq(&xr_stats, eseq);
        }
    }

    CU_ASSERT_EQUAL(totals, xr_stats.totals);
    CU_ASSERT_EQUAL(start_seq, xr_stats.eseq_start);

    CU_ASSERT_EQUAL(totals/2, xr_stats.lost_packets);
    CU_ASSERT_EQUAL(0, xr_stats.dup_packets);
    CU_ASSERT_EQUAL(0, xr_stats.late_arrivals);

    /* All the packets have arrived */
    for (i = 0; i < totals; i++) {
        eseq = start_seq + i;
        if (i % 2 != 0) {
            rtcp_xr_update_seq(&xr_stats, eseq);
        }
    }
#ifdef DEBUG
    printf("\nLate arrival with every other packet ...");
    for (i = 0; i <= xr_stats.cur_chunk_in_use; i++) {
        printf("\nchunk[%d] = 0x%04x", i, xr_stats.chunk[i]);
    }
#endif
    CU_ASSERT_EQUAL(totals, xr_stats.totals);
    CU_ASSERT_EQUAL(start_seq, xr_stats.eseq_start);

    CU_ASSERT_EQUAL(0, xr_stats.lost_packets);
    CU_ASSERT_EQUAL(0, xr_stats.dup_packets);
    CU_ASSERT_EQUAL(totals/2, xr_stats.late_arrivals);

    /* Start the seq from 1000 with 1000 numbers but 
       skip every 90 consecutive seq */
    rtcp_xr_set_size(&xr_stats, 54);
    rtcp_xr_init_seq(&xr_stats, start_seq, TRUE);
    CU_ASSERT_EQUAL(27, xr_stats.max_chunks_allow);
    CU_ASSERT_EQUAL(totals, xr_stats.re_init);
    totals = 1000;
    int total_exp;
    for (i = 0; i <= totals; i++) {
        eseq = start_seq + i;
        if (i % 90 == 0) {
            rtcp_xr_update_seq(&xr_stats, eseq);
            total_exp = eseq - start_seq + 1;
        }
    }
#ifdef DEBUG
    printf("\nMissing 90 packets ...");
    for (i = 0; i <= xr_stats.cur_chunk_in_use; i++) {
        printf("\nchunk[%d] = 0x%04x", i, xr_stats.chunk[i]);
    }
#endif
    CU_ASSERT_EQUAL(total_exp, xr_stats.totals);
    CU_ASSERT_EQUAL(89*11, xr_stats.lost_packets);
    CU_ASSERT_EQUAL(0, xr_stats.dup_packets);
    CU_ASSERT_EQUAL(0, xr_stats.late_arrivals);

    /* The following packets have arrived */
    eseq = 1014;
    rtcp_xr_update_seq(&xr_stats, eseq);
    CU_ASSERT_EQUAL(0xC001, xr_stats.chunk[0]);
    CU_ASSERT_EQUAL(total_exp, xr_stats.totals);

    eseq = 1016;
    rtcp_xr_update_seq(&xr_stats, eseq);
    CU_ASSERT_EQUAL(0xa000, xr_stats.chunk[1]);
    CU_ASSERT_EQUAL(0x003c, xr_stats.chunk[2]);
    CU_ASSERT_EQUAL(total_exp, xr_stats.totals);

    eseq = 1089;
    rtcp_xr_update_seq(&xr_stats, eseq);
    CU_ASSERT_EQUAL(0x002d, xr_stats.chunk[2]);
    CU_ASSERT_EQUAL(0x8001, xr_stats.chunk[3]);
    CU_ASSERT_EQUAL(total_exp, xr_stats.totals);

    eseq = 1225;
    rtcp_xr_update_seq(&xr_stats, eseq);
    CU_ASSERT_EQUAL(0x001e, xr_stats.chunk[7]);
    CU_ASSERT_EQUAL(0xc000, xr_stats.chunk[8]);
    CU_ASSERT_EQUAL(0x001e, xr_stats.chunk[9]);
    CU_ASSERT_EQUAL(total_exp, xr_stats.totals);

    /* Push oevr the limit */
    CU_ASSERT_EQUAL(14, xr_stats.bit_idx);
    eseq = 1825;
    rtcp_xr_update_seq(&xr_stats, eseq);
    CU_ASSERT_EQUAL(0xc000, xr_stats.chunk[23]);
    CU_ASSERT_EQUAL(0x003c, xr_stats.chunk[24]);
    CU_ASSERT_EQUAL(total_exp-1, xr_stats.totals);
    CU_ASSERT_EQUAL(0, xr_stats.bit_idx);

    eseq = 1899;
    rtcp_xr_update_seq(&xr_stats, eseq);
    CU_ASSERT_EQUAL(0x002d, xr_stats.chunk[24]);
    CU_ASSERT_EQUAL(0x8001, xr_stats.chunk[25]);
    CU_ASSERT_EQUAL(total_exp-1-75, xr_stats.totals);
#ifdef DEBUG
    printf("\nBefore inserting the packet ...");
    for (i = 0; i <= xr_stats.cur_chunk_in_use; i++) {
        printf("\nchunk[%d] = 0x%04x", i, xr_stats.chunk[i]);
    }
#endif
    eseq = 1855;
    rtcp_xr_update_seq(&xr_stats, eseq);
    CU_ASSERT_EQUAL(0x000f, xr_stats.chunk[24]);
    CU_ASSERT_EQUAL(0xc000, xr_stats.chunk[25]);
    CU_ASSERT_EQUAL(0x000f, xr_stats.chunk[26]);
    CU_ASSERT_EQUAL(total_exp-1-75-30, xr_stats.totals);
#ifdef DEBUG
    printf("\nAfter inserting the packet ...");
    for (i = 0; i <= xr_stats.cur_chunk_in_use; i++) {
        printf("\nchunk[%d] = 0x%04x", i, xr_stats.chunk[i]);
    }
#endif

    rtp_hdr_session_t session;
    rtp_hdr_source_t  source;
    rtp_hdr_status_t ret;
    for (i = 0; i < big_total; i++) {
        ret = rtp_update_seq(&session, &source, &xr_stats, rtp_seq[i]);
    }

    CU_ASSERT_EQUAL(1, xr_stats.lost_packets);
    CU_ASSERT_EQUAL(0, xr_stats.dup_packets);
}
