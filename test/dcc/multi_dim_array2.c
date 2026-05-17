#include "test.h"

int a[2][3][4][5][6];

int main() {
  for (int i = 0; i < 2; i++)
    for (int j = 0; j < 3; j++)
      for (int k = 0; k < 4; k++)
        for (int l = 0; l < 5; l++)
          for (int m = 0; m < 6; m++)
            a[i][j][k][l][m] = i * 10000 + j * 1000 + k * 100 + l * 10 + m;

  ASSERT(12345, a[1][2][3][4][5]);
  ASSERT(0, a[0][0][0][0][0]);
  ASSERT(10203, a[1][0][2][0][3]);

  ASSERT(1234 + 11111, a[0][1][2][3][4] + a[1][1][1][1][1]);

  ASSERT(10000, a[1][2][3][4][5] - a[0][2][3][4][5]);

  ASSERT(22468, a[1][1][2][3][4] * 2);

  ASSERT(1000, a[1][0][0][0][0] / 10);

  int *p = &a[0][0][0][0][0];
  ASSERT(10045, p[1 * 360 + 4 * 6 + 5]);
  ASSERT(0, p[0]);

  int *p_mid = p + 360;
  ASSERT(10000, *p_mid);
  ASSERT(10022, *(p_mid + 14));
  ASSERT(1000, *(p_mid - 240));

  int *p_a = &a[1][2][3][4][5];
  int *p_b = &a[0][0][0][0][0];
  ASSERT(719, p_a - p_b);
  ASSERT(-719, p_b - p_a);
  ASSERT(360, &a[1][0][0][0][0] - &a[0][0][0][0][0]);
  ASSERT(120, &a[0][2][0][0][0] - &a[0][1][0][0][0]);

  int (*p2)[6] = &a[0][0][0][0];
  ASSERT(5, (*p2)[5]);
  ASSERT(13, p2[1][3]);
  ASSERT(202, p2[10][2]);
  p2++;
  ASSERT(10, (*p2)[0]);
  p2 += 4;
  ASSERT(104, (*p2)[4]);

  int (*p3)[5][6] = &a[0][0][0];
  ASSERT(45, p3[0][4][5]);
  ASSERT(100, p3[1][0][0]);
  ASSERT(232, (*(p3 + 2))[3][2]);

  int (*p4)[4][5][6] = &a[0][0];
  ASSERT(234, p4[0][2][3][4]);
  ASSERT(1111, p4[1][1][1][1]);

  int (*p5)[3][4][5][6] = &a[0];
  ASSERT(1234, p5[0][1][2][3][4]);
  ASSERT(12345, p5[1][2][3][4][5]);
  p5++;
  ASSERT(10000, p5[0][0][0][0][0]);

  ASSERT(12345, *(p + 719));
  ASSERT(12345, *(719 + p));

  int *p_cmp1 = &a[0][1][2][3][4];
  int *p_cmp2 = &a[0][1][2][3][5];
  ASSERT(1, p_cmp1 < p_cmp2);
  ASSERT(0, p_cmp1 > p_cmp2);
  ASSERT(1, p_cmp1 == p_cmp1);
  ASSERT(1, p_cmp1 != p_cmp2);
  ASSERT(0, p_cmp2 <= p_cmp1);

  long long p_int = (long long)p_a;
  int *p_from_int = (int *)(p_int + 8);
  ASSERT(12345, *(p_from_int - 2));

  char *p_c = (char *)p_a;
  char *p_c_next = p_c + 4;
  ASSERT(*(int *)p_c_next, a[1][2][3][4][6]);

  printf("OK\n");
}
