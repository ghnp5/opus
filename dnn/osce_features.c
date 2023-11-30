#ifdef HAVE_CONFIG_H
#include "config.h"
#endif



/*DEBUG*/
//#define WRITE_FEATURES
//#define DEBUG_PRING
/*******/

#include "stack_alloc.h"
#include "osce_features.h"
#include "kiss_fft.h"
#include "os_support.h"
#include "osce.h"
#include "freq.h"


#if defined(WRITE_FEATURES) || defined(DEBUG_PRING)
#include <stdio.h>
#include <stdlib.h>
#endif

static int center_bins_clean[64] = {
      0,      2,      5,      8,     10,     12,     15,     18,
     20,     22,     25,     28,     30,     33,     35,     38,
     40,     42,     45,     48,     50,     52,     55,     58,
     60,     62,     65,     68,     70,     73,     75,     78,
     80,     82,     85,     88,     90,     92,     95,     98,
    100,    102,    105,    108,    110,    112,    115,    118,
    120,    122,    125,    128,    130,    132,    135,    138,
    140,    142,    145,    148,    150,    152,    155,    160
};

static int center_bins_noisy[18] = {
      0,      4,      8,     12,     16,     20,     24,     28,
     32,     40,     48,     56,     64,     80,     96,    112,
    136,    160
};

static float band_weights_clean[64] = {
     0.666666666667,     0.400000000000,     0.333333333333,     0.400000000000,
     0.500000000000,     0.400000000000,     0.333333333333,     0.400000000000,
     0.500000000000,     0.400000000000,     0.333333333333,     0.400000000000,
     0.400000000000,     0.400000000000,     0.400000000000,     0.400000000000,
     0.500000000000,     0.400000000000,     0.333333333333,     0.400000000000,
     0.500000000000,     0.400000000000,     0.333333333333,     0.400000000000,
     0.500000000000,     0.400000000000,     0.333333333333,     0.400000000000,
     0.400000000000,     0.400000000000,     0.400000000000,     0.400000000000,
     0.500000000000,     0.400000000000,     0.333333333333,     0.400000000000,
     0.500000000000,     0.400000000000,     0.333333333333,     0.400000000000,
     0.500000000000,     0.400000000000,     0.333333333333,     0.400000000000,
     0.500000000000,     0.400000000000,     0.333333333333,     0.400000000000,
     0.500000000000,     0.400000000000,     0.333333333333,     0.400000000000,
     0.500000000000,     0.400000000000,     0.333333333333,     0.400000000000,
     0.500000000000,     0.400000000000,     0.333333333333,     0.400000000000,
     0.500000000000,     0.400000000000,     0.250000000000,     0.333333333333
};

static float band_weights_noisy[18] = {
     0.400000000000,     0.250000000000,     0.250000000000,     0.250000000000,
     0.250000000000,     0.250000000000,     0.250000000000,     0.250000000000,
     0.166666666667,     0.125000000000,     0.125000000000,     0.125000000000,
     0.083333333333,     0.062500000000,     0.062500000000,     0.050000000000,
     0.041666666667,     0.080000000000
};

static float osce_window[320] = {
     0.004908718808,     0.014725683311,     0.024541228523,     0.034354408400,     0.044164277127,
     0.053969889210,     0.063770299562,     0.073564563600,     0.083351737332,     0.093130877450,
     0.102901041421,     0.112661287575,     0.122410675199,     0.132148264628,     0.141873117332,
     0.151584296010,     0.161280864678,     0.170961888760,     0.180626435180,     0.190273572448,
     0.199902370753,     0.209511902052,     0.219101240157,     0.228669460829,     0.238215641862,
     0.247738863176,     0.257238206902,     0.266712757475,     0.276161601717,     0.285583828929,
     0.294978530977,     0.304344802381,     0.313681740399,     0.322988445118,     0.332264019538,
     0.341507569661,     0.350718204573,     0.359895036535,     0.369037181064,     0.378143757022,
     0.387213886697,     0.396246695891,     0.405241314005,     0.414196874117,     0.423112513073,
     0.431987371563,     0.440820594212,     0.449611329655,     0.458358730621,     0.467061954019,
     0.475720161014,     0.484332517110,     0.492898192230,     0.501416360796,     0.509886201809,
     0.518306898929,     0.526677640552,     0.534997619887,     0.543266035038,     0.551482089078,
     0.559644990127,     0.567753951426,     0.575808191418,     0.583806933818,     0.591749407690,
     0.599634847523,     0.607462493302,     0.615231590581,     0.622941390558,     0.630591150148,
     0.638180132051,     0.645707604824,     0.653172842954,     0.660575126926,     0.667913743292,
     0.675187984742,     0.682397150168,     0.689540544737,     0.696617479953,     0.703627273726,
     0.710569250438,     0.717442741007,     0.724247082951,     0.730981620454,     0.737645704427,
     0.744238692572,     0.750759949443,     0.757208846506,     0.763584762206,     0.769887082016,
     0.776115198508,     0.782268511401,     0.788346427627,     0.794348361383,     0.800273734191,
     0.806121974951,     0.811892519997,     0.817584813152,     0.823198305781,     0.828732456844,
     0.834186732948,     0.839560608398,     0.844853565250,     0.850065093356,     0.855194690420,
     0.860241862039,     0.865206121757,     0.870086991109,     0.874883999665,     0.879596685080,
     0.884224593137,     0.888767277786,     0.893224301196,     0.897595233788,     0.901879654283,
     0.906077149740,     0.910187315596,     0.914209755704,     0.918144082372,     0.921989916403,
     0.925746887127,     0.929414632439,     0.932992798835,     0.936481041442,     0.939879024058,
     0.943186419177,     0.946402908026,     0.949528180593,     0.952561935658,     0.955503880820,
     0.958353732530,     0.961111216112,     0.963776065795,     0.966348024735,     0.968826845041,
     0.971212287799,     0.973504123096,     0.975702130039,     0.977806096779,     0.979815820533,
     0.981731107599,     0.983551773378,     0.985277642389,     0.986908548290,     0.988444333892,
     0.989884851171,     0.991229961288,     0.992479534599,     0.993633450666,     0.994691598273,
     0.995653875433,     0.996520189401,     0.997290456679,     0.997964603026,     0.998542563469,
     0.999024282300,     0.999409713092,     0.999698818696,     0.999891571247,     0.999987952167,
     0.999987952167,     0.999891571247,     0.999698818696,     0.999409713092,     0.999024282300,
     0.998542563469,     0.997964603026,     0.997290456679,     0.996520189401,     0.995653875433,
     0.994691598273,     0.993633450666,     0.992479534599,     0.991229961288,     0.989884851171,
     0.988444333892,     0.986908548290,     0.985277642389,     0.983551773378,     0.981731107599,
     0.979815820533,     0.977806096779,     0.975702130039,     0.973504123096,     0.971212287799,
     0.968826845041,     0.966348024735,     0.963776065795,     0.961111216112,     0.958353732530,
     0.955503880820,     0.952561935658,     0.949528180593,     0.946402908026,     0.943186419177,
     0.939879024058,     0.936481041442,     0.932992798835,     0.929414632439,     0.925746887127,
     0.921989916403,     0.918144082372,     0.914209755704,     0.910187315596,     0.906077149740,
     0.901879654283,     0.897595233788,     0.893224301196,     0.888767277786,     0.884224593137,
     0.879596685080,     0.874883999665,     0.870086991109,     0.865206121757,     0.860241862039,
     0.855194690420,     0.850065093356,     0.844853565250,     0.839560608398,     0.834186732948,
     0.828732456844,     0.823198305781,     0.817584813152,     0.811892519997,     0.806121974951,
     0.800273734191,     0.794348361383,     0.788346427627,     0.782268511401,     0.776115198508,
     0.769887082016,     0.763584762206,     0.757208846506,     0.750759949443,     0.744238692572,
     0.737645704427,     0.730981620454,     0.724247082951,     0.717442741007,     0.710569250438,
     0.703627273726,     0.696617479953,     0.689540544737,     0.682397150168,     0.675187984742,
     0.667913743292,     0.660575126926,     0.653172842954,     0.645707604824,     0.638180132051,
     0.630591150148,     0.622941390558,     0.615231590581,     0.607462493302,     0.599634847523,
     0.591749407690,     0.583806933818,     0.575808191418,     0.567753951426,     0.559644990127,
     0.551482089078,     0.543266035038,     0.534997619887,     0.526677640552,     0.518306898929,
     0.509886201809,     0.501416360796,     0.492898192230,     0.484332517110,     0.475720161014,
     0.467061954019,     0.458358730621,     0.449611329655,     0.440820594212,     0.431987371563,
     0.423112513073,     0.414196874117,     0.405241314005,     0.396246695891,     0.387213886697,
     0.378143757022,     0.369037181064,     0.359895036535,     0.350718204573,     0.341507569661,
     0.332264019538,     0.322988445118,     0.313681740399,     0.304344802381,     0.294978530977,
     0.285583828929,     0.276161601717,     0.266712757475,     0.257238206902,     0.247738863176,
     0.238215641862,     0.228669460829,     0.219101240157,     0.209511902052,     0.199902370753,
     0.190273572448,     0.180626435180,     0.170961888760,     0.161280864678,     0.151584296010,
     0.141873117332,     0.132148264628,     0.122410675199,     0.112661287575,     0.102901041421,
     0.093130877450,     0.083351737332,     0.073564563600,     0.063770299562,     0.053969889210,
     0.044164277127,     0.034354408400,     0.024541228523,     0.014725683311,     0.004908718808
};

static void apply_filterbank(float *x_out, float *x_in, int *center_bins, float* band_weights, int num_bands)
{
    int b, i;
    float frac;

    celt_assert(x_in != x_out)

    x_out[0] = 0;
    for (b = 0; b < num_bands - 1; b++)
    {
        x_out[b+1] = 0;
        for (i = center_bins[b]; i < center_bins[b+1]; i++)
        {
            frac = (float) (center_bins[b+1] - i) / (center_bins[b+1] - center_bins[b]);
            x_out[b]   += band_weights[b] * frac * x_in[i];
            x_out[b+1] += band_weights[b+1] * (1 - frac) * x_in[i];

        }
    }
    x_out[num_bands - 1] += band_weights[num_bands - 1] * x_in[center_bins[num_bands - 1]];
#ifdef DEBUG_PRINT
    for (b = 0; b < num_bands; b++)
    {
        printf("band[%d]: %f\n", b, x_out[b]);
    }
#endif
}


static void mag_spec_320_onesided_slow(float *out, float *in)
/* for temporary use only */
{
    kiss_fft_cpx buffer[320];
    int k;
    forward_transform(buffer, in);

    for (k = 0; k < 161; k++)
    {
        out[k] = 320. * sqrt(buffer[k].r * buffer[k].r + buffer[k].i * buffer[k].i);
#ifdef DEBUG_PRINT
        printf("magspec[%d]: %f\n", k, out[k]);
#endif
    }
}


static void calculate_log_spectrum_from_lpc(float *spec, opus_int16 *a_q12, int lpc_order)
{
    float buffer[320] = {0};
    int i;

    /* zero expansion */
    buffer[0] = 1;
    for (i = 0; i < lpc_order; i++)
    {
        buffer[i+1] = - (float)a_q12[i] / (1U << 12);
    }

    /* calculate and invert magnitude spectrum */
    mag_spec_320_onesided_slow(buffer, buffer);

    for (i = 0; i < 161; i++)
    {
        buffer[i] = 1. / (buffer[i] + 1e-9f);
    }

    /* apply filterbank */
    apply_filterbank(spec, buffer, center_bins_clean, band_weights_clean, OSCE_CLEAN_SPEC_NUM_BANDS);

    /* log and scaling */
    for (i = 0; i < OSCE_CLEAN_SPEC_NUM_BANDS; i++)
    {
        spec[i] = 0.3f * log(spec[i] + 1e-9f);
    }
}

static void calculate_cepstrum(float *cepstrum, float *signal)
{
    float buffer[320];
    float *spec = &buffer[164];
    int n;

    celt_assert(cepstrum != signal)

    for (n = 0; n < 320; n++)
    {
        buffer[n] = osce_window[n] * signal[n];
    }

    /* calculate magnitude spectrum */
    mag_spec_320_onesided_slow(buffer, buffer);

    /* accumulate bands */
    apply_filterbank(spec, buffer, center_bins_noisy, band_weights_noisy, OSCE_NOISY_SPEC_NUM_BANDS);

    /* log domain conversion */
    for (n = 0; n < OSCE_NOISY_SPEC_NUM_BANDS; n++)
    {
        spec[n] = log(spec[n] + 1e-9);
#ifdef DEBUG_PRINT
        printf("logspec[%d]: %f\n", n, spec[n]);
#endif
    }

    /* DCT-II (orthonormal) */
    celt_assert(OSCE_NOISY_SPEC_NUM_BANDS == NB_BANDS);
    dct(cepstrum, spec);
}

static void calculate_acorr(float *acorr, float *signal, int lag)
{
    int n, k;
    celt_assert(acorr != signal)

    for (k = -2; k <= 2; k++)
    {
        acorr[k+2] = 0;
        float xx = 0;
        float xy = 0;
        float yy = 0;
        for (n = 0; n < 80; n++)
        {
            /* obviously wasteful -> fix later */
            xx += signal[n] * signal[n];
            yy += signal[n - lag + k] * signal[n - lag + k];
            xy += signal[n] * signal[n - lag + k];
        }
        acorr[k+2] = xy / sqrt(xx * yy + 1e-9);
    }
}

static int pitch_postprocessing(OSCEFeatureState *psFeatures, int lag, int type)
{
    int new_lag;

#ifdef OSCE_HANGOVER_BUGFIX
#define TESTBIT 1
#else
#define TESTBIT 0
#endif

    /* hangover is currently disabled to reflect a bug in the python code. ToDo: re-evaluate hangover */
    if (type != TYPE_VOICED && psFeatures->last_type == TYPE_VOICED && TESTBIT)
    /* enter hangover */
    {
        new_lag = OSCE_NO_PITCH_VALUE;
        if (psFeatures->pitch_hangover_count < OSCE_PITCH_HANGOVER)
        {
            new_lag = psFeatures->last_lag;
            psFeatures->pitch_hangover_count = (psFeatures->pitch_hangover_count + 1) % OSCE_PITCH_HANGOVER;
        }
    }
    else if (type != TYPE_VOICED && psFeatures->pitch_hangover_count && TESTBIT)
    /* continue hangover */
    {
        new_lag = psFeatures->last_lag;
        psFeatures->pitch_hangover_count = (psFeatures->pitch_hangover_count + 1) % OSCE_PITCH_HANGOVER;
    }
    else if (type != TYPE_VOICED)
    /* unvoiced frame after hangover */
    {
        new_lag = OSCE_NO_PITCH_VALUE;
        psFeatures->pitch_hangover_count = 0;
    }
    else
    /* voiced frame: update last_lag */
    {
        new_lag = lag;
        psFeatures->last_lag = lag;
        psFeatures->pitch_hangover_count = 0;
    }

    /* buffer update */
    psFeatures->last_type = type;

    /* with the current setup this should never happen (but who knows...) */
    celt_assert(new_lag)

    return new_lag;
}

void osce_calculate_features(
    silk_decoder_state          *psDec,                         /* I/O  Decoder state                               */
    silk_decoder_control        *psDecCtrl,                     /* I    Decoder control                             */
    float                       *features,                      /* O    input features                              */
    float                       *numbits,                       /* O    numbits and smoothed numbits                */
    int                         *periods,                       /* O    pitch lags on subframe basis                */
    const opus_int16            xq[],                           /* I    Decoded speech                              */
    opus_int32                  num_bits                        /* I    Size of SILK payload in bits                */
)
{
    int num_subframes, num_samples;
    float buffer[OSCE_FEATURES_MAX_HISTORY + OSCE_MAX_FEATURE_FRAMES * 80];
    float *frame, *pfeatures;
    OSCEFeatureState *psFeatures;
    int i, n, k;
#ifdef WRITE_FEATURES
    static FILE *f_feat = NULL;
    if (f_feat == NULL)
    {
        f_feat = fopen("assembled_features.f32", "wb");
    }
#endif

    //OPUS_CLEAR(buffer, 1);
    memset(buffer, 0, sizeof(buffer));

    num_subframes = psDec->nb_subfr;
    num_samples = num_subframes * 80;
    psFeatures = &psDec->osce.features;

    /* smooth bit count */
    psFeatures->numbits_smooth = 0.9 * psFeatures->numbits_smooth + 0.1 * num_bits;
    numbits[0] = num_bits;
#ifdef OSCE_NUMBITS_BUGFIX
    numbits[1] = psFeatures->numbits_smooth;
#else
    numbits[1] = num_bits;
#endif

    for (n = 0; n < num_samples; n++)
    {
        buffer[OSCE_FEATURES_MAX_HISTORY + n] = (float) xq[n] / (1U<<15);
    }
    OPUS_COPY(buffer, psFeatures->signal_history, OSCE_FEATURES_MAX_HISTORY);

    for (k = 0; k < num_subframes; k++)
    {
        pfeatures = features + k * OSCE_FEATURE_DIM;
        frame = &buffer[OSCE_FEATURES_MAX_HISTORY + k * 80];
        memset(pfeatures, 0, OSCE_FEATURE_DIM); /* precaution */

        /* clean spectrum from lpcs (update every other frame) */
        if (k % 2 == 0)
        {
            calculate_log_spectrum_from_lpc(pfeatures + OSCE_CLEAN_SPEC_START, psDecCtrl->PredCoef_Q12[k >> 1], psDec->LPC_order);
        }
        else
        {
            OPUS_COPY(pfeatures + OSCE_CLEAN_SPEC_START, pfeatures + OSCE_CLEAN_SPEC_START - OSCE_FEATURE_DIM, OSCE_CLEAN_SPEC_LENGTH);
        }

        /* noisy cepstrum from signal (update every other frame) */
        if (k % 2 == 0)
        {
            calculate_cepstrum(pfeatures + OSCE_NOISY_CEPSTRUM_START, frame - 160);
        }
        else
        {
            OPUS_COPY(pfeatures + OSCE_NOISY_CEPSTRUM_START, pfeatures + OSCE_NOISY_CEPSTRUM_START - OSCE_FEATURE_DIM, OSCE_NOISY_CEPSTRUM_LENGTH);
        }

        /* pitch hangover and zero value replacement */
        periods[k] = pitch_postprocessing(psFeatures, psDecCtrl->pitchL[k], psDec->indices.signalType);

        /* auto-correlation around pitch lag */
        calculate_acorr(pfeatures + OSCE_ACORR_START, frame, periods[k]);

        /* ltp */
        celt_assert(OSCE_LTP_LENGTH == LTP_ORDER)
        for (i = 0; i < OSCE_LTP_LENGTH; i++)
        {
            pfeatures[OSCE_LTP_START + i] = (float) psDecCtrl->LTPCoef_Q14[k * LTP_ORDER + i] / (1U << 14);
        }

        /* frame gain */
        pfeatures[OSCE_LOG_GAIN_START] = log((float) psDecCtrl->Gains_Q16[k] / (1UL << 16) + 1e-9);

#ifdef WRITE_FEATURES
        fwrite(pfeatures, sizeof(*pfeatures), 93, f_feat);
#endif
    }

    /* buffer update */
    OPUS_COPY(psFeatures->signal_history, &buffer[num_samples], OSCE_FEATURES_MAX_HISTORY);
}