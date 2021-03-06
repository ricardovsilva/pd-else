// Porres 2017

#include <math.h>
#include <stdlib.h>
#include "m_pd.h"

#define PI M_PI
#define PLUCK_STACK 48000 // stack buf size, 1 sec at 48k
#define PLUCK_MAXD 4294967294 // max delay = 2**32 - 2

static t_class *pluck_class;

typedef struct _pluck
{
    t_object        x_obj;
    t_inlet         *x_hz_let;
    t_inlet         *x_alet;
    t_inlet         *x_inlet_cutoff;
    t_outlet        *x_outlet;
    float           x_sr;
    // pointers to the delay buf
    double          * x_ybuf;
    double          x_fbstack[PLUCK_STACK];
    int             x_alloc; // if we are using allocated buf
    unsigned int    x_sz; // actual size of each delay buffer
    t_float         x_maxdel;  // maximum delay in ms
    unsigned int    x_wh;     // writehead
    
    float     x_sum;
    float     x_last_trig;
    
    double  x_xnm1;
    double  x_xnm2;
    double  x_ynm1;
    double  x_ynm2;
} t_pluck;

static void pluck_clear(t_pluck *x){
    unsigned int i;
    for(i=0; i<x->x_sz; i++){
        x->x_ybuf[i] = 0.;
    };
    x->x_wh = 0;
    x->x_xnm1 = x->x_xnm2 = x->x_ynm1 = x->x_ynm2 = 0.;
}

static void pluck_sz(t_pluck *x){
    // helper function to deal with allocation issues if needed
    // ie if wanted size x->x_maxdel is bigger than stack, allocate
    
    // convert ms to samps
    unsigned int newsz = (unsigned int)ceil((double)x->x_maxdel*0.001*(double)x->x_sr);
    newsz++; // add a sample for good measure since say bufsize is 2048 and
    // you want a delay of 2048 samples,.. problem!
    
    int alloc = x->x_alloc;
    unsigned int cursz = x->x_sz; //current size
    
    if(newsz < 0)
        newsz = 0;
    else if(newsz > PLUCK_MAXD)
        newsz = PLUCK_MAXD;
    if(!alloc && newsz > PLUCK_STACK){
        x->x_ybuf = (double *)malloc(sizeof(double)*newsz);
        x->x_alloc = 1;
        x->x_sz = newsz;
    }
    else if(alloc && newsz > cursz){
        x->x_ybuf = (double *)realloc(x->x_ybuf, sizeof(double)*newsz);
        x->x_sz = newsz;
    }
    else if(alloc && newsz < PLUCK_STACK){
        free(x->x_ybuf);
        x->x_sz = PLUCK_STACK;
        x->x_ybuf = x->x_fbstack;
        x->x_alloc = 0;
    };
    pluck_clear(x);
}

static double pluck_getlin(double tab[], unsigned int sz, double idx){
    // linear interpolated reader, copied from Derek Kwan's library
    double output;
    unsigned int tabphase1 = (unsigned int)idx;
    unsigned int tabphase2 = tabphase1 + 1;
    double frac = idx - (double)tabphase1;
    if(tabphase1 >= sz - 1){
        tabphase1 = sz - 1; // checking to see if index falls within bounds
        output = tab[tabphase1];
    }
    else if(tabphase1 < 0){
        tabphase1 = 0;
        output = tab[tabphase1];
    }
    else{
        double yb = tab[tabphase2]; // linear interp
        double ya = tab[tabphase1];
        output = ya+((yb-ya)*frac);
    };
    return output;
}

static double pluck_read_delay(t_pluck *x, double arr[], t_float in_samps){
    //helper func, basically take desired ms delay, convert to samp, read from arr[]
    
    //eventual reading head
    double rh = (double)in_samps; // getting samp_size
    //bounds checking for minimum delay in samples
    if(rh < 1)
        rh = 1;
    rh = (double)x->x_wh+((double)x->x_sz-rh); //essentially subracting from writehead to find proper position in buffer
    //wrapping into length of delay buffer
    while(rh >= x->x_sz){
        rh -= (double)x->x_sz;
    };
    //now to read from the buffer!
    double output = pluck_getlin(arr, x->x_sz, rh);
    return output;
}

static t_int *pluck_perform(t_int *w){
    t_pluck *x = (t_pluck *)(w[1]);
    int n = (int)(w[2]);
    t_float *xin = (t_float *)(w[3]);
    t_float *t_in = (t_float *)(w[4]);
    t_float *hz_in = (t_float *)(w[5]);
    t_float *ain = (t_float *)(w[6]);
    t_float *cut_in = (t_float *)(w[7]);
    t_float *out = (t_float *)(w[8]);
    t_float sr = x->x_sr;
    t_float last_trig = x->x_last_trig;
    t_float sum = x->x_sum;
    
    double xnm1 = x->x_xnm1;
    double xnm2 = x->x_xnm2;
    double ynm1 = x->x_ynm1;
    double ynm2 = x->x_ynm2;
    
    int i;
    for(i=0; i<n;i++){
        int wh = x->x_wh;
        
        double input = (double)xin[i];
        
        double output;
        
        t_float trig = t_in[i];
        t_float hz = hz_in[i];
        if(hz < 1)
            hz = 1;
        
        float period = 1./hz;
        
        float delms = period * 1000;
        
        t_int samps = (int)roundf(period * sr);
        
        double fb_del = pluck_read_delay(x, x->x_ybuf, samps); // get delayed vals
        if (ain[i] == 0)
            ain[i] = 0;
        else
            ain[i] = copysign(exp(log(0.001) * delms/fabs(ain[i])), ain[i]);
        
        if (trig > 0 && last_trig <= 0)
            sum = 0;
        
// Filter stuff
        double cuttoff = (double)cut_in[i];
        double omega, alphaQ, cos_w, a0, a1, a2, b0, b1, b2, yn;
        double nyq = (sr * 0.5);
        double hz2rad = PI/nyq;
        if (cuttoff < 0.000001)
            cuttoff = 0.000001;
        if (cuttoff > nyq - 0.000001)
            cuttoff = nyq - 0.000001;
        omega = cuttoff * hz2rad;
        alphaQ = sin(omega); // q = 0.5
        cos_w = cos(omega);
        b0 = alphaQ + 1;
        a0 = (1 - cos_w) / (2 * b0);
        a1 = (1 - cos_w) / b0;
        a2 = a0;
        b1 = 2*cos_w / b0;
        b2 = (alphaQ - 1) / b0;
// gate
        t_int gate = (sum += 1) <= samps;
// envelope
        double envelope = input * gate;
// output
        output = envelope + (double)ain[i] * fb_del;
        out[i] = output;
// filter
        yn = a0 * output + a1 * xnm1 + a2 * xnm2 + b1 * ynm1 + b2 * ynm2;
// put into delay buffer
        x->x_ybuf[wh] = yn;
        x->x_wh = (wh + 1) % x->x_sz; // increment writehead
      
        last_trig = trig;
        xnm2 = xnm1;
        xnm1 = output;
        ynm2 = ynm1;
        ynm1 = yn;
    };
    x->x_sum = sum; // next
    x->x_last_trig = last_trig;
    x->x_xnm1 = xnm1;
    x->x_xnm2 = xnm2;
    x->x_ynm1 = ynm1;
    x->x_ynm2 = ynm2;
    return (w + 9);
}

static void pluck_dsp(t_pluck *x, t_signal **sp)
{
    int sr = sp[0]->s_sr;
    if(sr != x->x_sr){
        // if new sample rate isn't old sample rate, need to realloc
        x->x_sr = sr;
        pluck_sz(x);
    };
    dsp_add(pluck_perform, 8, x, sp[0]->s_n, sp[0]->s_vec,
            sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec,
            sp[4]->s_vec, sp[5]->s_vec);
}

/* static void pluck_size(t_pluck *x, t_floatarg f1){
    if(f1 < 0)
        f1 = 0;
    x->x_maxdel = f1;
    pluck_sz(x);
} */

static void *pluck_new(t_symbol *s, int argc, t_atom *argv){
    t_pluck *x = (t_pluck *)pd_new(pluck_class);
/////////////////////////////////////////////////////////////////////////////////////
    float freq = 0;
    float decay = 0;
    float cut_freq = 15000;
////
    int argnum = 0;
    while(argc > 0){
        if(argv -> a_type == A_FLOAT)
        { //if current argument is a float
            t_float argval = atom_getfloatarg(0, argc, argv);
            switch(argnum)
            {
                case 0:
                    freq = argval;
                    break;
                case 1:
                    decay = argval;
                    break;
                case 2:
                    cut_freq = argval;
                    break;
                default:
                    break;
            };
            argnum++;
            argc--;
            argv++;
        }
    else if (argv -> a_type == A_SYMBOL)
        goto errstate;
    };
/////////////////////////////////////////////////////////////////////////////////////
    x->x_sr = sys_getsr();
    x->x_alloc = x->x_last_trig = 0;
    x->x_xnm1 = x->x_xnm2 = x->x_ynm1 = x->x_ynm2 = 0.;
    x->x_sum =  PLUCK_MAXD;
    x->x_sz = PLUCK_STACK;
// clear out stack buf, set pointer to stack
    x->x_ybuf = x->x_fbstack;
    pluck_clear(x);
    if (freq < 0)
        freq = 0;
    x->x_maxdel = 1000;
// ship off to the helper method to deal with allocation if necessary
    pluck_sz(x);
    
// inlets / outlet
    inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    x->x_hz_let = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_hz_let, freq);
    x->x_alet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_alet, decay);
    x->x_inlet_cutoff = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_cutoff, cut_freq);
    x->x_outlet = outlet_new((t_object *)x, &s_signal);
    return (x);
    errstate:
        pd_error(x, "pluck~: improper args");
        return NULL;
}

static void * pluck_free(t_pluck *x){
    if(x->x_alloc)
        free(x->x_ybuf);
    inlet_free(x->x_hz_let);
    inlet_free(x->x_alet);
    inlet_free(x->x_inlet_cutoff);
    outlet_free(x->x_outlet);
    return (void *)x;
}

void pluck_tilde_setup(void)
{
    pluck_class = class_new(gensym("pluck~"), (t_newmethod)pluck_new,
                (t_method)pluck_free, sizeof(t_pluck), 0, A_GIMME, 0);
    class_addmethod(pluck_class, nullfn, gensym("signal"), 0);
    class_addmethod(pluck_class, (t_method)pluck_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(pluck_class, (t_method)pluck_clear, gensym("clear"), 0);
}
        
