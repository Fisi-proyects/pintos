#include <stdint.h>
#define F (1 << 14) // factor de conversion

int int_to_fixed_point(int n);          // convertir entero a punto fijo
int fixed_point_to_int(int x);          // convertir punto fijo a entero
int fixed_point_to_int_round(int x);    // convertir punto fijo a entero redondeando
int add_fixed_points(int x, int y);     // suma de dos numeros en punto fijo
int subtract_fixed_points(int x, int y);// resta de dos numeros en punto fijo
int add_fixed_point_and_int(int x, int n);  // suma de un numero en punto fijo y un entero
int subtract_fixed_point_and_int(int x, int n);  // resta de un numero en punto fijo y un entero
int multiply_fixed_points(int x, int y);     // multiplicacion de dos numeros en punto fijo
int multiply_fixed_point_and_int(int x, int y); // multiplicacion de un numero en punto fijo y un entero
int divide_fixed_points(int x, int y);      // division de dos numeros en punto fijo
int divide_fixed_point_and_int(int x, int n);  // division de un numero en punto fijo y un entero

int int_to_fixed_point(int n)
{
   return n * F;
}

int fixed_point_to_int(int x)
{
   return x / F;
}

int fixed_point_to_int_round(int x)
{
   if (x >= 0)
   {
      return (x + F / 2) / F;
   }
   else
   {
      return (x - F / 2) / F;
   }
}

int add_fixed_points(int x, int y)
{
   return x + y;
}

int subtract_fixed_points(int x, int y)
{
   return x - y;
}

int add_fixed_point_and_int(int x, int n)
{
   return x + (n * F);
}

int subtract_fixed_point_and_int(int x, int n)
{
   return x - (n * F);
}

int multiply_fixed_points(int x, int y)
{
   return ((int64_t)x) * y / F;
}

int multiply_fixed_point_and_int(int x, int n)
{
   return x * n;
}

int divide_fixed_points(int x, int y)
{
   return ((int64_t)x) * F / y;
}

int divide_fixed_point_and_int(int x, int n)
{
   return x / n;
}