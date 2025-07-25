#include <stdlib.h> // abs
#include <math.h>   // sqrt

#include "pitch.h"

static void xcorr_kernel(const float_t *x, const float_t *y, float_t sum[4], int len)
{
   int j;
   float_t y_0, y_1, y_2, y_3;

   y_3 = 0; /* gcc doesn't realize that y_3 can't be used uninitialized */
   y_0 = *y++;
   y_1 = *y++;
   y_2 = *y++;
   for (j = 0; j < len - 3; j += 4)
   {
      float_t tmp;
      tmp = *x++;
      y_3 = *y++;
      sum[0] = MAC(sum[0], tmp, y_0);
      sum[1] = MAC(sum[1], tmp, y_1);
      sum[2] = MAC(sum[2], tmp, y_2);
      sum[3] = MAC(sum[3], tmp, y_3);
      tmp = *x++;
      y_0 = *y++;
      sum[0] = MAC(sum[0], tmp, y_1);
      sum[1] = MAC(sum[1], tmp, y_2);
      sum[2] = MAC(sum[2], tmp, y_3);
      sum[3] = MAC(sum[3], tmp, y_0);
      tmp = *x++;
      y_1 = *y++;
      sum[0] = MAC(sum[0], tmp, y_2);
      sum[1] = MAC(sum[1], tmp, y_3);
      sum[2] = MAC(sum[2], tmp, y_0);
      sum[3] = MAC(sum[3], tmp, y_1);
      tmp = *x++;
      y_2 = *y++;
      sum[0] = MAC(sum[0], tmp, y_3);
      sum[1] = MAC(sum[1], tmp, y_0);
      sum[2] = MAC(sum[2], tmp, y_1);
      sum[3] = MAC(sum[3], tmp, y_2);
   }
   if (j++ < len)
   {
      float_t tmp = *x++;
      y_3 = *y++;
      sum[0] = MAC(sum[0], tmp, y_0);
      sum[1] = MAC(sum[1], tmp, y_1);
      sum[2] = MAC(sum[2], tmp, y_2);
      sum[3] = MAC(sum[3], tmp, y_3);
   }
   if (j++ < len)
   {
      float_t tmp = *x++;
      y_0 = *y++;
      sum[0] = MAC(sum[0], tmp, y_1);
      sum[1] = MAC(sum[1], tmp, y_2);
      sum[2] = MAC(sum[2], tmp, y_3);
      sum[3] = MAC(sum[3], tmp, y_0);
   }
   if (j < len)
   {
      float_t tmp = *x++;
      y_1 = *y++;
      sum[0] = MAC(sum[0], tmp, y_2);
      sum[1] = MAC(sum[1], tmp, y_3);
      sum[2] = MAC(sum[2], tmp, y_0);
      sum[3] = MAC(sum[3], tmp, y_1);
   }
}

static float_t celt_inner_prod(const float_t *x, const float_t *y, int N)
{
   int i;
   float_t xy = 0;
   for (i = 0; i < N; i++)
      xy = MAC(xy, x[i], y[i]);
   return xy;
}

void rnn_pitch_xcorr(const float_t *_x, const float_t *_y, float_t *xcorr, int len, int max_pitch)
{
   int i;
   /*The EDSP version requires that max_pitch is at least 1, and that _x is
      32-bit aligned.
     Since it's hard to put asserts in assembly, put them here.*/
   for (i = 0; i < max_pitch - 3; i += 4)
   {
      float_t sum[4] = {0, 0, 0, 0};
      xcorr_kernel(_x, _y + i, sum, len);
      xcorr[i] = sum[0];
      xcorr[i + 1] = sum[1];
      xcorr[i + 2] = sum[2];
      xcorr[i + 3] = sum[3];
   }
   /* In case max_pitch isn't a multiple of 4, do non-unrolled version. */
   for (; i < max_pitch; i++)
   {
      float_t sum;
      sum = celt_inner_prod(_x, _y + i, len);
      xcorr[i] = sum;
   }
}

static void celt_fir5(const float_t *x, const float_t *num, float_t *y, int N, float_t *mem)
{
   float_t mem0 = mem[0], mem1 = mem[1], mem2 = mem[2], mem3 = mem[3], mem4 = mem[4];
   float_t num0 = num[0], num1 = num[1], num2 = num[2], num3 = num[3], num4 = num[4];

   for (int i = 0; i < N; i++)
   {
      float_t xi = x[i];
      float_t sum = VAL("SHL32", xi, SIG_SHIFT);
      sum = MAC(sum, num0, mem0);
      sum = MAC(sum, num1, mem1);
      sum = MAC(sum, num2, mem2);
      sum = MAC(sum, num3, mem3);
      sum = MAC(sum, num4, mem4);
      y[i] = VAL("ROUND16", sum, SIG_SHIFT);
      mem4 = mem3;
      mem3 = mem2;
      mem2 = mem1;
      mem1 = mem0;
      mem0 = xi;
   }

   mem[0] = mem0;
   mem[1] = mem1;
   mem[2] = mem2;
   mem[3] = mem3;
   mem[4] = mem4;
}

int rnn_autocorr(const float_t *x, float_t *ac, const float_t *window, int overlap, int lag, int n)
{
   int i, k;
   int fastN = n - lag;
   int shift;
   const float_t *xptr;
   float_t d;
   float_t xx[PITCH_BUF_SIZE / 2];

   if (overlap == 0)
   {
      xptr = x;
   }
   else
   {
      for (i = 0; i < n; i++)
         xx[i] = x[i];
      for (i = 0; i < overlap; i++)
      {
         xx[i] = MULT(x[i], window[i]);
         xx[n - i - 1] = MULT(x[n - i - 1], window[i]);
      }
      xptr = xx;
   }
   shift = 0;
   rnn_pitch_xcorr(xptr, xptr, ac, fastN, lag + 1);
   for (k = 0; k <= lag; k++)
   {
      for (i = k + fastN, d = 0; i < n; i++)
         d = MAC(d, xptr[i], xptr[i - k]);
      ac[k] += d;
   }
   return shift;
}

void rnn_lpc(float_t *_lpc, const float_t *ac, int p)
{
   float_t r;
   float_t error = ac[0];
   float *lpc = _lpc;

   RNN_CLEAR(lpc, p);
   if (ac[0] > EPSILON)
   {
      for (int i = 0; i < p; i++)
      {
         /* Sum up this iteration's reflection coefficient */
         float_t rr = 0;
         for (int j = 0; j < i; j++)
            rr += MULT(lpc[j], ac[i - j]);
         rr += VAL("SHR32", ac[i + 1], 3);
         r = -VAL("SHL32", rr, 3) / error;
         /*  Update LPC coefficients and total error */
         lpc[i] = VAL("SHR32", r, 3);
         for (int j = 0; j < (i + 1) >> 1; j++)
         {
            float_t tmp1, tmp2;
            tmp1 = lpc[j];
            tmp2 = lpc[i - 1 - j];
            lpc[j] = tmp1 + MULT(r, tmp2);
            lpc[i - 1 - j] = tmp2 + MULT(r, tmp1);
         }

         error = error - MULT(MULT(r, r), error);
         /* Bail out once we get 30 dB gain */
         if (error < .001f * ac[0])
            break;
      }
   }
}

void rnn_pitch_downsample(float_t *x[], float_t *x_lp, int len, int C)
{
   int i;
   float_t ac[5];
   float_t tmp = Q15ONE;
   float_t lpc[4], mem[5] = {0, 0, 0, 0, 0};
   float_t lpc2[5];
   float_t c1 = VAL("QCONST16", .8f, 15);
   for (i = 1; i < len >> 1; i++)
      x_lp[i] = VAL("SHR32", HALF(HALF(x[0][(2 * i - 1)] + x[0][(2 * i + 1)]) + x[0][2 * i]), shift);
   x_lp[0] = VAL("SHR32", HALF(HALF(x[0][1]) + x[0][0]), shift);
   if (C == 2)
   {
      for (i = 1; i < len >> 1; i++)
         x_lp[i] += VAL("SHR32", HALF(HALF(x[1][(2 * i - 1)] + x[1][(2 * i + 1)]) + x[1][2 * i]), shift);
      x_lp[0] += VAL("SHR32", HALF(HALF(x[1][1]) + x[1][0]), shift);
   }

   rnn_autocorr(x_lp, ac, NULL, 0, 4, len >> 1);

   /* Noise floor -40 dB */
   ac[0] *= 1.0001f;
   /* Lag windowing */
   for (i = 1; i <= 4; i++)
   {
      float f = (.008f * (float)i);
      ac[i] -= ac[i] * f * f;
   }

   rnn_lpc(lpc, ac, 4);
   for (i = 0; i < 4; i++)
   {
      tmp = MULT(VAL("QCONST16", .9f, 15), tmp);
      lpc[i] = MULT(lpc[i], tmp);
   }
   /* Add a zero */
   lpc2[0] = lpc[0] + VAL("QCONST16", .8f, SIG_SHIFT);
   lpc2[1] = lpc[1] + MULT(c1, lpc[0]);
   lpc2[2] = lpc[2] + MULT(c1, lpc[1]);
   lpc2[3] = lpc[3] + MULT(c1, lpc[2]);
   lpc2[4] = MULT(c1, lpc[3]);
   celt_fir5(x_lp, lpc2, x_lp, len >> 1, mem);
}

static void find_best_pitch(float_t *xcorr, float_t *y, int len, int max_pitch, int *best_pitch)
{
   int i, j;
   float_t Syy = 1;
   float_t best_num[2];
   float_t best_den[2];

   best_num[0] = -1;
   best_num[1] = -1;
   best_den[0] = 0;
   best_den[1] = 0;
   best_pitch[0] = 0;
   best_pitch[1] = 1;
   for (j = 0; j < len; j++)
      Syy = ADD(Syy, VAL("SHR32", MULT(y[j], y[j]), yshift));
   for (i = 0; i < max_pitch; i++)
   {
      if (xcorr[i] > 0)
      {
         float_t num;
         float_t xcorr16;
         xcorr16 = VAL("VSHR32", xcorr[i], xshift);
         num = MULT(xcorr16, xcorr16);
         if (MULT(num, best_den[1]) > MULT(best_num[1], Syy))
         {
            if (MULT(num, best_den[0]) > MULT(best_num[0], Syy))
            {
               best_num[1] = best_num[0];
               best_den[1] = best_den[0];
               best_pitch[1] = best_pitch[0];
               best_num[0] = num;
               best_den[0] = Syy;
               best_pitch[0] = i;
            }
            else
            {
               best_num[1] = num;
               best_den[1] = Syy;
               best_pitch[1] = i;
            }
         }
      }
      Syy += VAL("SHR32", MULT(y[i + len], y[i + len]), yshift) - VAL("SHR32", MULT(y[i], y[i]), yshift);
      Syy = MAX(1, Syy);
   }
}

void rnn_pitch_search(const float_t *x_lp, float_t *y, int len, int max_pitch, int *pitch)
{
   int i, j;
   int lag;
   int best_pitch[2] = {0, 0};
   int offset;
   float_t x_lp4[PITCH_FRAME_SIZE >> 2];
   float_t y_lp4[(PITCH_FRAME_SIZE + PITCH_MAX_PERIOD) >> 2];
   float_t xcorr[PITCH_MAX_PERIOD >> 1];

   lag = len + max_pitch;

   /* Downsample by 2 again */
   for (j = 0; j < len >> 2; j++)
      x_lp4[j] = x_lp[2 * j];
   for (j = 0; j < lag >> 2; j++)
      y_lp4[j] = y[2 * j];

   /* Coarse search with 4x decimation */

   rnn_pitch_xcorr(x_lp4, y_lp4, xcorr, len >> 2, max_pitch >> 2);

   find_best_pitch(xcorr, y_lp4, len >> 2, max_pitch >> 2, best_pitch);

   /* Finer search with 2x decimation */

   for (i = 0; i < max_pitch >> 1; i++)
   {
      float_t sum;
      xcorr[i] = 0;
      if (abs(i - 2 * best_pitch[0]) > 2 && abs(i - 2 * best_pitch[1]) > 2)
         continue;
      sum = celt_inner_prod(x_lp, y + i, len >> 1);

      xcorr[i] = MAX(-1, sum);
   }
   find_best_pitch(xcorr, y, len >> 1, max_pitch >> 1, best_pitch);

   /* Refine by pseudo-interpolation */
   if (best_pitch[0] > 0 && best_pitch[0] < (max_pitch >> 1) - 1)
   {
      float_t a, b, c;
      a = xcorr[best_pitch[0] - 1];
      b = xcorr[best_pitch[0]];
      c = xcorr[best_pitch[0] + 1];
      if ((c - a) > MULT(VAL("QCONST16", .7f, 15), b - a))
         offset = 1;
      else if ((a - c) > MULT(VAL("QCONST16", .7f, 15), b - c))
         offset = -1;
      else
         offset = 0;
   }
   else
   {
      offset = 0;
   }
   *pitch = 2 * best_pitch[0] - offset;
}

static void dual_inner_prod(const float_t *x, const float_t *y01, const float_t *y02,
                            int N, float_t *xy1, float_t *xy2)
{
   int i;
   float_t xy01 = 0;
   float_t xy02 = 0;
   for (i = 0; i < N; i++)
   {
      xy01 = MAC(xy01, x[i], y01[i]);
      xy02 = MAC(xy02, x[i], y02[i]);
   }
   *xy1 = xy01;
   *xy2 = xy02;
}

static float_t compute_pitch_gain(float_t xy, float_t xx, float_t yy)
{
   return xy / (float)sqrt(1 + xx * yy);
}

static const int second_check[16] = {0, 0, 3, 2, 3, 2, 5, 2, 3, 2, 3, 2, 5, 2, 3, 2};
float_t rnn_remove_doubling(float_t *x, int maxperiod, int minperiod,
                            int N, int *T0_, int prev_period, float_t prev_gain)
{
   int k, i, T, T0;
   float_t g, g0;
   float_t pg;
   float_t xy, xx, yy, xy2;
   float_t xcorr[3];
   float_t best_xy, best_yy;
   int offset;
   int minperiod0;
   float_t yy_lookup[PITCH_MAX_PERIOD + 1];

   minperiod0 = minperiod;
   maxperiod /= 2;
   minperiod /= 2;
   *T0_ /= 2;
   prev_period /= 2;
   N /= 2;
   x += maxperiod;
   if (*T0_ >= maxperiod)
      *T0_ = maxperiod - 1;

   T = T0 = *T0_;
   dual_inner_prod(x, x, x - T0, N, &xx, &xy);
   yy_lookup[0] = xx;
   yy = xx;
   for (i = 1; i <= maxperiod; i++)
   {
      yy = yy + MULT(x[-i], x[-i]) - MULT(x[N - i], x[N - i]);
      yy_lookup[i] = MAX(0, yy);
   }
   yy = yy_lookup[T0];
   best_xy = xy;
   best_yy = yy;
   g = g0 = compute_pitch_gain(xy, xx, yy);
   /* Look for any pitch at T/k */
   for (k = 2; k <= 15; k++)
   {
      int T1, T1b;
      float_t g1;
      float_t cont = 0;
      float_t thresh;
      T1 = (2 * T0 + k) / (2 * k);
      if (T1 < minperiod)
         break;
      /* Look for another strong correlation at T1b */
      if (k == 2)
      {
         if (T1 + T0 > maxperiod)
            T1b = T0;
         else
            T1b = T0 + T1;
      }
      else
      {
         T1b = (2 * second_check[k] * T0 + k) / (2 * k);
      }
      dual_inner_prod(x, &x[-T1], &x[-T1b], N, &xy, &xy2);
      xy = HALF(xy + xy2);
      yy = HALF(yy_lookup[T1] + yy_lookup[T1b]);
      g1 = compute_pitch_gain(xy, xx, yy);
      if (abs(T1 - prev_period) <= 1)
         cont = prev_gain;
      else if (abs(T1 - prev_period) <= 2 && 5 * k * k < T0)
         cont = HALF(prev_gain);
      else
         cont = 0;
      thresh = MAX(VAL("QCONST16", .3f, 15), MULT(VAL("QCONST16", .7f, 15), g0) - cont);
      /* Bias against very high pitch (very short period) to avoid false-positives
         due to short-term correlation */
      if (T1 < 3 * minperiod)
         thresh = MAX(VAL("QCONST16", .4f, 15), MULT(VAL("QCONST16", .85f, 15), g0) - cont);
      else if (T1 < 2 * minperiod)
         thresh = MAX(VAL("QCONST16", .5f, 15), MULT(VAL("QCONST16", .9f, 15), g0) - cont);
      if (g1 > thresh)
      {
         best_xy = xy;
         best_yy = yy;
         T = T1;
         g = g1;
      }
   }
   best_xy = MAX(0, best_xy);
   if (best_yy <= best_xy)
      pg = Q15ONE;
   else
      pg = best_xy / (best_yy + 1);

   for (k = 0; k < 3; k++)
      xcorr[k] = celt_inner_prod(x, x - (T + k - 1), N);
   if ((xcorr[2] - xcorr[0]) > MULT(VAL("QCONST16", .7f, 15), xcorr[1] - xcorr[0]))
      offset = 1;
   else if ((xcorr[0] - xcorr[2]) > MULT(VAL("QCONST16", .7f, 15), xcorr[1] - xcorr[2]))
      offset = -1;
   else
      offset = 0;
   if (pg > g)
      pg = g;
   *T0_ = 2 * T + offset;

   if (*T0_ < minperiod0)
      *T0_ = minperiod0;
   return pg;
}