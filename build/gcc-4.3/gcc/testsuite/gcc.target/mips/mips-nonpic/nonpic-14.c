/* { dg-options "nonpic-call.o pic-addr.o pic-receive-fn-addr.o pic-addr-call.o nonpic-receive-fn-addr.o nonpic-nothing.o pic-nothing.o" } */

int hit_pic_addr_call = 0;
int hit_nonpic_call = 0;
int hit_pic_addr = 0;
int hit_nonpic_nothing = 0;
int hit_pic_nothing = 0;
extern void nonpic_call ();
extern void pic_addr_call();
extern void abort (void);
extern void exit (int);
main ()
{
  nonpic_call ();
  pic_addr_call ();

  if (hit_nonpic_call != 1)
    abort ();

  if (hit_pic_addr_call != 1)
    abort ();

  exit (0);
} 
