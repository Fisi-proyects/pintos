#define FACTOR (1 << 14) // factor de conversion

int int_to_fp(int n);          // convertir entero a punto fijo
int fp_to_int(int x);          // convertir punto fijo a entero
int fp_to_int_round(int x);    // convertir punto fijo a entero redondeando
int add_fp(int x, int y);      // suma de dos numeros en punto fijo
int sub_fp(int x, int y);      // resta de dos numeros en punto fijo
int add_fp_int(int x, int n);  // suma de un numero en punto fijo y un entero
int sub_fp_int(int x, int n);  // resta de un numero en punto fijo y un entero
int mult_fp(int x, int y);     // multiplicacion de dos numeros en punto fijo
int mult_fp_int(int x, int y); // multiplicacion de un numero en punto fijo y un entero
int div_fp(int x, int y);      // division de dos numeros en punto fijo
int div_fp_int(int x, int n);  // division de un numero en punto fijo y un entero

int int_to_fp(int n)
{
   return n * FACTOR;
}

int fp_to_int(int x)
{
   return x / FACTOR;
}

int fp_to_int_round(int x)
{
   if (x >= 0)
   {
      return (x + FACTOR / 2) / FACTOR;
   }
   else
   {
      return (x - FACTOR / 2) / FACTOR;
   }
}

int add_fp(int x, int y)
{
   return x + y;
}

int sub_fp(int x, int y)
{
   return x - y;
}

int add_fp_int(int x, int n)
{
   return x + (n * FACTOR);
}

int sub_fp_int(int x, int n)
{
   return x - (n * FACTOR);
}

int mult_fp(int x, int y)
{
   return ((int64_t)x) * y / FACTOR;
}

int mult_fp_int(int x, int n)
{
   return x * n;
}

int div_fp(int x, int y)
{
   return ((int64_t)x) * FACTOR / y;
}

int div_fp_int(int x, int n)
{
   return x / n;
}