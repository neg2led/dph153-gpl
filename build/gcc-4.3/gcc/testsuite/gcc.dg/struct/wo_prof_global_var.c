/* { dg-do compile } */
/* { dg-do run } */

#include <stdlib.h>
typedef struct
{
  int a;
  float b;
}str_t;

#define N 1000

str_t *p;

int
main ()
{
  int i, sum;

  p = malloc (N * sizeof (str_t));

  for (i = 0; i < N; i++)
    p[i].b = i;

  for (i = 0; i < N; i++)
    p[i].b = p[i].a + 1;

  for (i = 0; i < N; i++)
    if (p[i].b != p[i].a + 1)
      abort ();

  return 0;
}

/*--------------------------------------------------------------------------*/
/* { dg-final { scan-ipa-dump "Number of structures to transform is 1" "ipa_struct_reorg" } } */
/* { dg-final { cleanup-ipa-dump "*" } } */
