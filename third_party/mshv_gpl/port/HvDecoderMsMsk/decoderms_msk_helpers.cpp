#include "decoderms.h"

#include <algorithm>
#include <cmath>
#include <cstring>

DecoderMs::DecoderMs(QString)
{
    pomAll.initPomAll();
    TGenMsk = new GenMsk(true);
    DecFt8_0 = new DecoderFt8();
    DecFt4_0 = new DecoderFt4();
    DecQ65 = new DecoderQ65();
    DEC_SAMPLE_RATE = 12000.0;
    twopi = 8.0 * std::atan(1.0);
    pi = 4.0 * std::atan(1.0);
    s_mode = 0;
    s_period_time = 15;
    s_decoder_deep = 2;
    G_DfTolerance = 100;
    G_MinSigdB = 0;
    G_ShOpt = false;
    G_SwlOpt = false;
    s_fopen = true;
    s_f_rtd = false;
    s_end_rtd = false;
    s_mousebutton = 0;
    s_time = QStringLiteral("00:00:00");
    first_dec_msk = true;
    first_sq = true;

    // IMPORTANT: upstream MSHV uses the oddly named flags f_first_msk144/40
    // as "already initialised" guards. first_msk144()/first_msk40() return
    // immediately when the flag is true, and build the sync templates only
    // when it is false. The previous port constructor set these flags true,
    // leaving cb_msk144/cbr_msk40, pulse windows and sync words all zero.
    // Result: the adapter ran, but detected and decoded nothing.
    f_first_msk144 = false;
    f_first_msk40 = false;

    first_rtd_msk = true;
    s_trained_msk144 = false;
    ss_msk144ms = false;

    // Match MSHV defaults: equalisation starts off and the correlation
    // history is initialised once at construction.
    s_msk144rxequal_s = false;
    s_msk144rxequal_d = false;
    nfft0_msk144_2 = 0;
    pomAll.zero_double_beg_end(dpclast_msk144_2, 0, 3);
    dpclast_msk144_2[0] = 1.0;
    pomAll.zero_double_comp_beg_end(s_corrd, 0, 524300);
    analytic_msk144_2_init_s_corrs_full();

    for (int i = 0; i < HASH_CALLS_COUNT; ++i) { hash_msk40_calls[i].hash = -101; hash_msk40_calls[i].calls = QStringLiteral("FALSE"); }
}

void DecoderMs::SetPerodTime(int p)
{
    if (p <= 0)
        p = 15;
    s_period_time = p;
}

DecoderMs::~DecoderMs()
{
    delete DecFt8_0; DecFt8_0 = nullptr;
    delete DecFt4_0; DecFt4_0 = nullptr;
    delete DecQ65; DecQ65 = nullptr;
    delete TGenMsk;
    TGenMsk = nullptr;
    f2a.DestroyPlansAll(true);
}

void DecoderMs::SetDecode(int *dat,int count,QString time,int t_istart,int mousebutton,bool f_rtd,bool end_rtd,bool fopen)
{
    QVector<double> tmp(count);
    for (int i = 0; i < count; ++i) tmp[i] = static_cast<double>(dat[i]) / 32768.0;
    s_time = time;
    s_mousebutton = mousebutton;
    s_f_rtd = f_rtd;
    s_end_rtd = end_rtd;
    s_fopen = fopen;
    msk_144_40_decode(tmp.data(), count, static_cast<double>(t_istart), fopen);
}

void DecoderMs::DecodeMsk144Buffer(double *dat, int count, const QString &time, int mouseButton, bool msk144ms)
{
    s_time = time;
    s_mousebutton = mouseButton;
    s_f_rtd = false;
    s_end_rtd = true;
    s_fopen = true;
    msk_144_40_decode(dat, count, 0.0, msk144ms);
}

QString DecoderMs::RemBegEndWSpaces(QString str)
{
    return str.trimmed();
}

void DecoderMs::SetNexColor(bool f) { f_back_color = f; }
void DecoderMs::SetBackColor()
{
    f_back_color = !f_back_color;
    emit EmitBackColor(f_back_color);
}

void DecoderMs::move_da_to_da(double*x,int begin_x, double*y,int begin_y,int y_end)
{
    for (int i = 0; i < y_end; ++i) y[i + begin_y] = x[i + begin_x];
}

void DecoderMs::zero_int_beg_end(int*d,int begin,int end)
{
    for (int i = begin; i < end; ++i) d[i] = 0;
}

void DecoderMs::smooth(double *x,int nz)
{
    if (nz < 3) return;
    double x0 = x[0];
    for (int i = 1; i < nz - 1; ++i) {
        const double x1 = x[i];
        x[i] = 0.5 * x[i] + 0.25 * (x0 + x[i + 1]);
        x0 = x1;
    }
}


double DecoderMs::dot_product_da_da(double *a, double *b, int size, int offset_b)
{
    double sum = 0.0;
    if (a == nullptr || b == nullptr || size <= 0)
        return sum;
    for (int i = 0; i < size; ++i)
        sum += a[i] * b[i + offset_b];
    return sum;
}

void DecoderMs::analytic(double *d,int d_count_begin,int npts,int nfft,double *s,mshv_complex *c)
{
    const int nh = nfft / 2;
    const double fac = 2.0 / static_cast<double>(nfft);
    for (int i = 0; i < npts; ++i) c[i] = fac * d[i + d_count_begin];
    for (int j = npts; j < nfft; ++j) c[j] = 0.0;
    f2a.four2a_c2c(c, nfft, -1, 1);
    for (int x = 0; x < nh; ++x) s[x] = pomAll.ps_hv(c[x]);
    c[0] = 0.5 * c[0];
    for (int y = nh + 1; y < nfft; ++y) c[y] = 0.0 + 0.0 * mshv_i();
    f2a.four2a_c2c(c, nfft, 1, 1);
}

void DecoderMs::tweak1(mshv_complex *ca,int jz,double f0,mshv_complex *cb)
{
    mshv_complex w = 1.0 + 1.0 * mshv_i();
    const double dphi = twopi * f0 / DEC_SAMPLE_RATE;
    const mshv_complex wstep = std::cos(dphi) + std::sin(dphi) * mshv_i();
    for (int i = 0; i < jz; ++i) {
        w = w * wstep;
        cb[i] = w * ca[i];
    }
}

void DecoderMs::ssort(double *x,double *y,int n,int kflag)
{
    QVector<int> idx(n);
    for (int i = 0; i < n; ++i) idx[i] = i;
    std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) {
        return (kflag >= 0) ? (x[a] < x[b]) : (x[a] > x[b]);
    });
    QVector<double> xs(n), ys(n);
    for (int i = 0; i < n; ++i) { xs[i] = x[idx[i]]; ys[i] = y[idx[i]]; }
    for (int i = 0; i < n; ++i) { x[i] = xs[i]; y[i] = ys[i]; }
}

void DecoderMs::indexx(int n,double *arr,int *indx)
{
    QVector<int> idx(n);
    for (int i = 0; i < n; ++i) idx[i] = i;
    std::stable_sort(idx.begin(), idx.end(), [&](int a, int b){ return arr[a] < arr[b]; });
    for (int i = 0; i < n; ++i) indx[i] = idx[i];
}

int DecoderMs::ping(double *s,int nz,double dtbuf,int slim,double wmin,double pingdat_[100][3])
{
    int nping = 0;
    double peak = 0.0;
    bool inside = false;
    const double snrlim = std::pow(10.0, 0.1 * static_cast<double>(slim)) - 1.0;
    const double sdown = 10.0 * std::log10(0.25 * snrlim + 1.0);
    int i0 = 0;
    double tstart = 0.0;
    for (int i = 1; i < nz; ++i) {
        if (s[i] >= static_cast<double>(slim) && !inside) {
            i0 = i;
            tstart = static_cast<double>(i0) * dtbuf;
            inside = true;
            peak = 0.0;
        }
        if (inside && s[i] > peak) peak = s[i];
        if (inside && (s[i] < sdown || i == nz - 1)) {
            if (i > i0 && dtbuf * static_cast<double>(i - i0) >= wmin) {
                pingdat_[nping][0] = tstart;
                pingdat_[nping][1] = dtbuf * static_cast<double>(i - i0);
                pingdat_[nping][2] = peak;
                if (nping < 99) ++nping;
            }
            inside = false;
            peak = 0.0;
        }
    }
    return nping;
}
