// Porres 2016
 
#include "m_pd.h"
#include <math.h>

static t_class *fold_class;

typedef struct _fold
{
    t_object    x_obj;
    t_int       x_bytes;
    t_atom      *output_list;
    t_outlet    *x_outlet;
    t_float     x_f;
    t_float     x_min;
    t_float     x_max;
} t_fold;

t_float convert(t_float f, min, max)
{
    float result;
    if(min > max)
    { // swap values
        float temp;
        temp = max;
        max = min;
        min = temp;
    };
    if(min == max)
        result = min;
    else if(f <= max && f >= min)
        result = f; // if f range, = in
    else
    { // folding
        float range = max - min;
        if(f < min)
        {
            float diff = min - f; // positive diff between f and min
            int mag = (int)(diff/range); // f is > range from min
            if(mag % 2 == 0)
            { // even # of ranges away = counting up from min
                diff = diff - ((float)mag * range);
                result = diff + min;
            }
            else
            { // odd # of ranges away = counting down from max
                diff = diff - ((float)mag * range);
                result = max - diff;
            };
        }
        else // f > max
        {
            float diff = f - max; // positive diff between f and max
            int mag  = (int)(diff/range); // f is > range from max
            if(mag % 2 == 0)
            { // even # of ranges away = counting down from max
                diff = diff - ((float)mag * range);
                result = max - diff;
            }
            else
            { // odd # of ranges away = counting up from min
                diff = diff - ((float)mag * range);
                result = diff + min;
            };
        };
    }
    return result;
}

void fold_float(t_fold *x, t_floatarg f)
{
  x->x_f = f;
  outlet_float(x->x_outlet, convert(f, x->x_min, x->x_max));
}

void fold_list(t_fold *x, t_symbol *s, int argc, t_atom *argv)
{
  int old_bytes = x->x_bytes, i = 0;
  x->x_bytes = argc*sizeof(t_atom);
  x->output_list = (t_atom *)t_resizebytes(x->output_list,old_bytes,x->x_bytes);
  for(i=0;i<argc;i++)
    SETFLOAT(x->output_list+i,convert(atom_getfloatarg(i,argc,argv), x->x_min, x->x_max));
  outlet_list(x->x_outlet,0,argc,x->output_list);
}

void fold_set(t_fold *x, t_float f)
{
  x->x_f = f;
}

void fold_bang(t_fold *x)
{
  outlet_float(x->x_outlet,convert(x->x_f, x->x_min, x->x_max));
}

void *fold_new(t_floatarg f1, t_floatarg f2)
{
  t_fold *x = (t_fold *) pd_new(fold_class);
  x->x_min = f1;
  x->x_max = f2;
  x->x_outlet = outlet_new(&x->x_obj, 0);
  x->x_bytes = sizeof(t_atom);
  x->output_list = (t_atom *)getbytes(x->x_bytes);
  if(x->output_list==NULL)
    {
    pd_error(x,"fold: memory allocation failure");
    return NULL;
    }
  return (x);
}

void fold_free(t_fold *x)
{
  t_freebytes(x->output_list,x->x_bytes);
}

void fold_setup(void)
{
  fold_class = class_new(gensym("fold"), (t_newmethod)fold_new,
			  (t_method)fold_free,sizeof(t_fold),0, A_DEFFLOAT, A_DEFFLOAT, 0);
  class_addfloat(fold_class,(t_method)fold_float);
  class_addlist(fold_class,(t_method)fold_list);
  class_addmethod(fold_class,(t_method)fold_set,gensym("set"),A_DEFFLOAT,0);
  class_addbang(fold_class,(t_method)fold_bang);
}
