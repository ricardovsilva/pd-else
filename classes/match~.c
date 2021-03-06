// porres 2017

#include "m_pd.h"
#include <stdlib.h>

typedef struct _match{
	t_object x_obj;
    t_float x_f;
    t_float x_lastin;
    t_int   x_n_outlets;
    t_int   x_first;
	t_float *matches; // numbers to match against
    t_float **ins;
    t_float **outs;
} t_match;

static t_class *match_class;

void *match_new(t_symbol *msg, short argc, t_atom *argv);
void match_free(t_match *x);
void match_dsp(t_match *x, t_signal **sp);

t_int *match_perform(t_int *w){
    int i, j;
    t_match *x = (t_match *) w[1]; // first is object
    t_float **ins = x->ins;
    t_float **outs = x->outs;
    t_float *main_input;
	t_float *inlet;
	t_float *match_outlet;
	t_float *matches = x->matches;
    t_int outlets = x->x_n_outlets;
    t_float last = x->x_lastin;
    t_int matched;
    int n = (int) w[outlets + 3]; // last is block
// copy main input vector
    main_input = (t_float *) w[2]; // main input
    for(i = 0; i < n; i++)
        ins[0][i] = main_input[i];
    inlet = ins[0];
// assign output vectors
    for(i = 0; i < outlets; i++)
        outs[i] = (t_float *) w[3 + i];
// clean outlets
    for(i = 0; i < outlets; i++){
		match_outlet = (double *) outs[i];
		for(j = 0; j < n; j++)
			match_outlet[j] = 0.0;
	}
// match
	for(i = 0; i < n; i++){
        matched = 0;
        if(!x->x_first){
            x->x_first = 1;
            for(j = 0; j < outlets - 1; j++){
                if(inlet[i] == matches[j]){ // if matched
                    match_outlet = (double *) outs[j];
                    match_outlet[i] = 1.0; // always send a unity click
                    matched = 1;
                }
            }
            if(!matched){
                match_outlet = (double *) outs[outlets - 1];
                match_outlet[i] = 1.0;
            }
        }
        else if (inlet[i] != last){ // if changed
			for(j = 0; j < outlets - 1; j++){
                if(inlet[i] == matches[j]){ // if matched
                    match_outlet = (double *) outs[j];
                    match_outlet[i] = 1.0; // always send a unity click
                    matched = 1;
                }
			}
            if(!matched){
                match_outlet = (double *) outs[outlets - 1];
                match_outlet[i] = 1.0;
           }
        }
        last = inlet[i];
	}
    x->x_lastin = inlet[n-1];
    return (w + outlets + 4);
}

void match_dsp(t_match *x, t_signal **sp)
{
	long i;
    t_int **sigvec;
    int pointer_count = x->x_n_outlets + 3; // output chans + object + inchan + vectorsize
    if(!sp[0]->s_sr)
        return;
    sigvec  = (t_int **) calloc(pointer_count, sizeof(t_int *));
	for(i = 0; i < pointer_count; i++)
		sigvec[i] = (t_int *) calloc(sizeof(t_int),1);
// first is the object
	sigvec[0] = (t_int *)x;
// last is vector size (n)
	sigvec[pointer_count - 1] = (t_int *)sp[0]->s_n;
// now I/O
	for(i = 1; i < pointer_count - 1; i++)
		sigvec[i] = (t_int *)sp[i-1]->s_vec;
    dsp_addv(match_perform, pointer_count, (t_int *)sigvec);
    free(sigvec);
}

void match_free(t_match *x){
    free(x->matches);
    free(x->outs);
    free(x->ins[0]);
    free(x->ins);
}

void *match_new(t_symbol *msg, short argc, t_atom *argv){
    t_match *x = (t_match *)pd_new(match_class);
    x->x_lastin = 0;
    x->x_first = 0;
    if(!argc){
        x->x_n_outlets = 2;
        outlet_new(&x->x_obj, gensym("signal"));
        outlet_new(&x->x_obj, gensym("signal"));
        x->matches = (double *) malloc(1 * sizeof(double));
        x->matches[0] = 0;
    }
    else{
        int i;
        x->x_n_outlets = (t_int)argc + 1;
        for(i=0; i < x->x_n_outlets ; i++)
            outlet_new(&x->x_obj, gensym("signal"));
        x->matches = (double *) malloc((x->x_n_outlets - 1) * sizeof(double));
        for(i = 0; i < argc; i++)
            x->matches[i] = (double)atom_getfloatarg(i,argc,argv);
    }
    x->ins = (t_float **) malloc(1 * sizeof(t_float *));
    x->outs = (t_float **) malloc(x->x_n_outlets * sizeof(t_float *));
    x->ins[0] = (t_float *) malloc(8192 * sizeof(t_float));
    return x;
}

void match_tilde_setup(void){
    match_class = class_new(gensym("match~"), (t_newmethod)match_new,
                            (t_method)match_free, sizeof(t_match),0,A_GIMME,0);
    CLASS_MAINSIGNALIN(match_class, t_match, x_f);
    class_addmethod(match_class, (t_method)match_dsp, gensym("dsp"), A_CANT, 0);
}
