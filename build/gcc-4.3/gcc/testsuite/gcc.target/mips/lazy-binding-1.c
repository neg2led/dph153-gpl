/* { dg-do compile { target nomips16 } } */
/* { dg-mips-options "-mabicalls -mshared -mexplicit-relocs -O2 -fno-delayed-branch -fpic" } */

void bar (void);

void
foo (int n)
{
  while (n--)
    {
      bar ();
      bar ();
    }
}

/* There should be exactly five uses of $25: one to set up $gp, two to
   load the address of bar (), and two to call it.  */
/* { dg-final { scan-assembler-times "\tl.\t\\\$25,%call16\\\(bar\\\)" 2 } } */
/* { dg-final { scan-assembler-times "\tjalr\t\\\$25" 2 } } */
/* { dg-final { scan-assembler "(\\\$28,|\t.cpload\t)\\\$25" } } */
/* { dg-final { scan-assembler-times "\\\$25" 5 } } */
