// Porres 2017

#include "m_pd.h"

static t_class *coin_class;

typedef struct _coin{
    t_object   x_obj;
    int        x_val;
    t_float    x_random;
    t_float    x_lastin;
    t_inlet   *x_prob_let;
    t_outlet  *x_outlet;
} t_coin;

static void coin_seed(t_coin *x, t_floatarg f){
    x->x_val = (int)f * 1319;
}

static t_int *coin_perform(t_int *w){
    t_coin *x = (t_coin *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    t_float *out = (t_sample *)(w[5]);
    int val = x->x_val;
    t_float random = x->x_random;
    t_float lastin = x->x_lastin;
    while (nblock--){
        float trig = *in1++;
        float prob = *in2++;
        if (prob < 0)
            prob = 0;
        if (prob > 100)
            prob = 100;
        if (trig != 0 && lastin == 0){
            if (prob == 0)
                *out++ = 0;
            else if (prob == 100)
                *out++ = trig;
            else { // toss coin
                random = ((float)((val & 0x7fffffff) - 0x40000000)) * (float)(1.0 / 0x40000000);
                random = ((random + 1) / 2) * 100; // 0-100
                val = val * 435898247 + 382842987;
                if (random < prob)
                    *out++ = trig;
                else
                    *out++ = 0;
                }
            }
        else
            *out++ = 0;
        lastin = trig;
        }
    x->x_val = val;
    x->x_random = random; // current output
    x->x_lastin = lastin; // last input
    return (w + 6);
}

static void coin_dsp(t_coin *x, t_signal **sp)
{
    dsp_add(coin_perform, 5, x, sp[0]->s_n,
            sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void *coin_free(t_coin *x){
    inlet_free(x->x_prob_let);
    outlet_free(x->x_outlet);
    return (void *)x;
}

static void *coin_new(t_symbol *s, int ac, t_atom *av){
    t_coin *x = (t_coin *)pd_new(coin_class);
// default
    static int init_seed = 234599;
    init_seed *= 1319;
    t_int seed = init_seed;
    t_float init_prob = 50;
/////////////////////////////////////////////////////////////////////////////////////
int argnum = 0;
while(ac){
    if(av -> a_type != A_FLOAT)
        goto errstate;
    else{
        t_float curf = atom_getfloatarg(0, ac, av);
        switch(argnum){
            case 0:
                init_prob = curf;
                break;
            case 1:
                seed = (int)curf * 1319;
            break;
            default:
            break;
            };
        };
    argnum++;
    ac--;
    av++;
    };
/////////////////////////////////////////////////////////////////////////////////////
    x->x_val = seed; // load seed value
    x->x_lastin = 0;
    if (init_prob < 0)
        init_prob = 0;
    if (init_prob > 100)
        init_prob = 100;
//
    x->x_prob_let = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_prob_let, init_prob);
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    return (x);
    errstate:
        pd_error(x, "coin~: improper args");
        return NULL;
}

void coin_tilde_setup(void){
    coin_class = class_new(gensym("coin~"), (t_newmethod)coin_new,
        (t_method)coin_free, sizeof(t_coin), CLASS_DEFAULT, A_GIMME, 0);
    class_addmethod(coin_class, nullfn, gensym("signal"), 0);
    class_addmethod(coin_class, (t_method)coin_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(coin_class, (t_method)coin_seed, gensym("seed"), A_DEFFLOAT, 0);
}
