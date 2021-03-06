// Porres 2016

#include "m_pd.h"
#include <math.h>

static t_class *hz2rad_class;

typedef struct _hz2rad {
  t_object x_obj;
  t_float   x_radps;
  t_inlet *x_inlet;
  t_outlet *x_outlet;
} t_hz2rad;

void *hz2rad_new(void);
static t_int * hz2rad_perform(t_int *w);
static void hz2rad_dsp(t_hz2rad *x, t_signal **sp);

static t_int * hz2rad_perform(t_int *w)
{
  t_hz2rad *x = (t_hz2rad *)(w[1]);
  int n = (int)(w[2]);
  t_float *in = (t_float *)(w[3]);
  t_float *out = (t_float *)(w[4]);
  t_float radps = x->x_radps;
  while(n--)
*out++ = *in++ * radps;
  return (w + 5);
}

static void hz2rad_dsp(t_hz2rad *x, t_signal **sp)
{
  dsp_add(hz2rad_perform, 4, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec);
}

void *hz2rad_new(void)
{
  t_hz2rad *x = (t_hz2rad *)pd_new(hz2rad_class);
  x->x_radps = 2 * M_PI / sys_getsr();
  x->x_inlet = inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
  x->x_outlet = outlet_new(&x->x_obj, &s_signal);
  return (void *)x;
}

void hz2rad_tilde_setup(void) {
  hz2rad_class = class_new(gensym("hz2rad~"),
    (t_newmethod) hz2rad_new, 0, sizeof (t_hz2rad), CLASS_NOINLET, 0);
  class_addmethod(hz2rad_class, (t_method) hz2rad_dsp, gensym("dsp"), 0);
}